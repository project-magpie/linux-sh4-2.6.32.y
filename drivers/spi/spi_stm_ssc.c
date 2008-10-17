/*
 *  ------------------------------------------------------------------------
 *  spi_stm_ssc.c SPI/SSC driver for STMicroelectronics platforms
 *  ------------------------------------------------------------------------
 *
 *  Copyright (c) 2008 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 *  ------------------------------------------------------------------------
 *  Changelog:
 *  2008-01-24 (angus.clark@st.com)
 *    - Initial version
 *  2008-08-28 (angus.clark@st.com)
 *    - Updates to fit with changes to 'ssc_pio_t'
 *    - SSC accesses now all 32-bit, for compatibility with 7141 Comms block
 *    - Updated to handle 7141 PIO ALT configuration
 *    - Support for user-defined, per-bus, chip_select function.  Specified
 *      in board setup
 *    - Bug fix for rx_bytes_pending updates
 *
 *  ------------------------------------------------------------------------
 */

#include <linux/stm/pio.h>
#include <asm/semaphore.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/stm/soc.h>
#include <linux/stm/stssc.h>
#include <linux/uaccess.h>
#include <linux/param.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/delay.h>

#undef dgb_print

#ifdef CONFIG_SPI_DEBUG
#define SPI_LOOP_DEBUG
#define dgb_print(fmt, args...)  printk(KERN_INFO "%s: " \
					fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)	do { } while (0)
#endif

#define NAME "spi_stm_ssc"

struct spi_stm_ssc {

	/* SSC SPI Controller */
	struct spi_bitbang	bitbang;
	unsigned long		base;
	unsigned int		fcomms;
	struct platform_device  *pdev;

	/* SSC SPI current transaction */
	const u8		*tx_ptr;
	u8			*rx_ptr;
	u16			bits_per_word;
	unsigned int		baud;
	unsigned int		tx_bytes_pending;
	unsigned int		rx_bytes_pending;
	struct completion	done;

};

static void spi_stpio_chipselect(struct spi_device *spi, int value)
{
	unsigned int out;

	dgb_print("\n");
	if (spi->chip_select == SPI_NO_CHIPSELECT)
		return;

	/* Request stpio_pin if not already done so */
	/*  (stored in spi_device->controller_data) */
	if (!spi->controller_data)
		spi->controller_data =
			stpio_request_pin(spi_get_bank(spi->chip_select),
					  spi_get_line(spi->chip_select),
					  "spi-cs", STPIO_OUT);
	if (!spi->controller_data) {
		printk(KERN_ERR NAME " Error spi-cs locked or not-exist\n");
		return;
	}

	if (value == BITBANG_CS_ACTIVE)
		out = spi->mode & SPI_CS_HIGH ? 1 : 0;
	else
		out = spi->mode & SPI_CS_HIGH ? 0 : 1;

	stpio_set_pin((struct stpio_pin *)spi->controller_data, out);

	dgb_print("%s PIO%d[%d] -> %d \n",
		  value == BITBANG_CS_ACTIVE ? "select" : "deselect",
		  spi_get_bank(spi->chip_select),
		  spi_get_line(spi->chip_select), out);

	return;
}

static int spi_stmssc_setup_transfer(struct spi_device *spi,
				     struct spi_transfer *t)
{
	struct spi_stm_ssc *st_ssc;
	u32 hz;
	u8 bits_per_word;
	u32 reg;
	u32 sscbrg;

	st_ssc = spi_master_get_devdata(spi->master);
	bits_per_word = (t) ? t->bits_per_word : 0;
	hz = (t) ? t->speed_hz : 0;

	/* If not specified, use defaults */
	if (!bits_per_word)
		bits_per_word = spi->bits_per_word;
	if (!hz)
		hz = spi->max_speed_hz;

	/* Actually, can probably support 2-16 without any other change!!! */
	if (bits_per_word != 8 && bits_per_word != 16) {
		printk(KERN_ERR NAME " unsupported bits_per_word=%d\n",
		       bits_per_word);
		return -EINVAL;
	}
	st_ssc->bits_per_word = bits_per_word;

	/* Set SSC_BRF */
	/* TODO: program prescaler for slower baudrates */
	sscbrg = st_ssc->fcomms/(2*hz);
	if (sscbrg < 0x07 || sscbrg > (0x1 << 16)) {
		printk(KERN_ERR NAME " baudrate outside valid range"
		       " %d (sscbrg = %d)\n", hz, sscbrg);
		return -EINVAL;
	}
	st_ssc->baud = st_ssc->fcomms/(2*sscbrg);
	if (sscbrg == (0x1 << 16)) /* 16-bit counter wraps */
		sscbrg = 0x0;
	dgb_print("setting baudrate: hz = %d, sscbrg = %d\n", hz, sscbrg);
	ssc_store32(st_ssc, SSC_BRG, sscbrg);

	 /* Set SSC_CTL and enable SSC */
	 reg = ssc_load32(st_ssc, SSC_CTL);
	 reg |= SSC_CTL_MS;

	 if (spi->mode & SPI_CPOL)
		 reg |= SSC_CTL_PO;
	 else
		 reg &= ~SSC_CTL_PO;

	 if (spi->mode & SPI_CPHA)
		 reg |= SSC_CTL_PH;
	 else
		 reg &= ~SSC_CTL_PH;

	 if ((spi->mode & SPI_LSB_FIRST) == 0)
		 reg |= SSC_CTL_HB;
	 else
		 reg &= ~SSC_CTL_HB;

	 if (spi->mode & SPI_LOOP)
		 reg |= SSC_CTL_LPB;
	 else
		 reg &= ~SSC_CTL_LPB;

	 reg &= 0xfffffff0;
	 reg |= (bits_per_word - 1);

	 /* CHECK!: are we always going to use FIFO or
	    do I need CONFIG_STM_SPI_HW_FIFO? */
	 reg |= SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO;
	 reg |= SSC_CTL_EN;

	 dgb_print("ssc_ctl = 0x%04x\n", reg);
	 ssc_store32(st_ssc, SSC_CTL, reg);

	 /* Clear the status register */
	 ssc_load32(st_ssc, SSC_RBUF);

	 return 0;
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS  (SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST | SPI_LOOP | SPI_CS_HIGH)
static int spi_stmssc_setup(struct spi_device *spi)
{
	struct spi_stm_ssc *st_ssc;
	int retval;

	st_ssc = spi_master_get_devdata(spi->master);

	if (spi->mode & ~MODEBITS) {
		printk(KERN_ERR NAME "unsupported mode bits %x\n",
			  spi->mode & ~MODEBITS);
		return -EINVAL;
	}

	if (!spi->max_speed_hz)  {
		printk(KERN_ERR NAME " max_speed_hz unspecified\n");
		return -EINVAL;
	}

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	retval = spi_stmssc_setup_transfer(spi, NULL);
	if (retval < 0)
		return retval;

	return 0;
}

/* For SSC SPI as MASTER, TX/RX is handled as follows:

   1. Fill the TX_FIFO with up to (SSC_TXFIFO_SIZE - 1) words, and enable
      TX_FIFO_EMPTY interrupts.
   2. When the last word of TX_FIFO is copied to the shift register,
      a TX_FIFO_EMPTY interrupt is issued, and the last word will *start* being
      shifted out/in.
   3. On receiving a TX_FIFO_EMPTY interrupt, copy all *available* received
      words from the RX_FIFO. Note, depending on the time taken to shift out/in
      the 'last' word compared to the IRQ latency, the 'last' word may not be
      available yet in the RX_FIFO.
   4. If there are more bytes to TX, refill the TX_FIFO.  Since the 'last' word
      from the previous iteration may still be (or about to be) in the RX_FIFO,
      only add up to (SSC_TXFIFO_SIZE - 1) words.  If all bytes have been
      transmitted, disable TX and set completion.
   5. If we are interested in the received data, check to see if the 'last' word
      has been received.  If not, then wait the period of shifting 1 word, then
      read the 'last' word from the RX_FIFO.

*/
static void spi_stmssc_fill_tx_fifo(struct spi_stm_ssc *st_ssc)
{
	union {
		u8 bytes[4];
		u32 dword;
	} tmp = {.dword = 0,};
	int i;

	for (i = 0;
	     i < SSC_TXFIFO_SIZE - 1 && st_ssc->tx_bytes_pending > 0; i++) {
		if (st_ssc->bits_per_word > 8) {
			if (st_ssc->tx_ptr) {
				tmp.bytes[1] = *st_ssc->tx_ptr++;
				tmp.bytes[0] = *st_ssc->tx_ptr++;
			} else {
				tmp.bytes[1] = 0;
				tmp.bytes[0] = 0;
			}

			st_ssc->tx_bytes_pending -= 2;

		} else {
			if (st_ssc->tx_ptr)
				tmp.bytes[0] = *st_ssc->tx_ptr++;
			else
				tmp.bytes[0] = 0;

			st_ssc->tx_bytes_pending--;
		}
		ssc_store32(st_ssc, SSC_TBUF, tmp.dword);
	}
}

static int spi_stmssc_rx_mopup(struct spi_stm_ssc *st_ssc)
{
	unsigned long word_period_ns;
	u32 rx_fifo_status;
	union {
		u8 bytes[4];
		u32 dword;
	} tmp = {.dword = 0,};

	dgb_print("\n");

	word_period_ns = 1000000000 / st_ssc->baud;
	word_period_ns *= st_ssc->bits_per_word;

	/* delay for period equivalent to shifting 1 complete word
	   out of and into shift register */
	ndelay(word_period_ns);

	/* Check 'last' word is actually there! */
	rx_fifo_status = ssc_load32(st_ssc, SSC_RX_FSTAT);
	if (rx_fifo_status == 1) {
		tmp.dword = ssc_load32(st_ssc, SSC_RBUF);

		if (st_ssc->bits_per_word > 8) {
			if (st_ssc->rx_ptr) {
				*st_ssc->rx_ptr++ = tmp.bytes[1];
				*st_ssc->rx_ptr++ = tmp.bytes[0];
			}
			st_ssc->rx_bytes_pending -= 2;
		} else {
			if (st_ssc->rx_ptr)
				*st_ssc->rx_ptr++ = tmp.bytes[0];
			st_ssc->rx_bytes_pending--;
		}
	} else {
		dgb_print("should only be one word in RX_FIFO"
			  "(rx_fifo_status = %d)\n", rx_fifo_status);
	}

	return 0;
}


static int spi_stmssc_txrx_bufs(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_stm_ssc *st_ssc;

	dgb_print("\n");

	st_ssc = spi_master_get_devdata(spi->master);

	st_ssc->tx_ptr = t->tx_buf;
	st_ssc->rx_ptr = t->rx_buf;
	st_ssc->tx_bytes_pending = t->len;
	st_ssc->rx_bytes_pending = t->len;
	INIT_COMPLETION(st_ssc->done);

	/* fill TX_FIFO */
	spi_stmssc_fill_tx_fifo(st_ssc);

	/* enable TX_FIFO_EMPTY interrupts */
	ssc_store32(st_ssc, SSC_IEN, SSC_IEN_TIEN);

	/* wait for all bytes to be transmitted*/
	wait_for_completion(&st_ssc->done);

	/* check 'last' byte has been received */
	/* NOTE: need to read rxbuf, even if ignoring the result! */
	if (st_ssc->rx_bytes_pending)
		spi_stmssc_rx_mopup(st_ssc);

	/* disable ints */
	ssc_store32(st_ssc, SSC_IEN, 0x0);

	return t->len - st_ssc->tx_bytes_pending;
}



static irqreturn_t spi_stmssc_irq(int irq, void *dev_id)
{
	struct spi_stm_ssc *st_ssc = (struct spi_stm_ssc *)dev_id;
	unsigned int rx_fifo_status;
	u32 ssc_status;

	union {
		u8 bytes[4];
		u32 dword;
	} tmp = {.dword = 0,};

	ssc_status = ssc_load32(st_ssc, SSC_STA);

	/* FIFO_TX_EMPTY */
	if (ssc_status & SSC_STA_TIR) {
		/* Find number of words available in RX_FIFO: 8 if RX_FIFO_FULL,
		   else SSC_RX_FSTAT (0-7)
		*/
		rx_fifo_status = (ssc_status & SSC_STA_RIR) ? 8 :
			ssc_load32(st_ssc, SSC_RX_FSTAT);

		/* Read all available words from RX_FIFO */
		while (rx_fifo_status) {
			tmp.dword = ssc_load32(st_ssc, SSC_RBUF);

			if (st_ssc->bits_per_word > 8) {
				if (st_ssc->rx_ptr) {
					*st_ssc->rx_ptr++ = tmp.bytes[1];
					*st_ssc->rx_ptr++ = tmp.bytes[0];
				}
				st_ssc->rx_bytes_pending -= 2;
			} else {
				if (st_ssc->rx_ptr)
					*st_ssc->rx_ptr++ = tmp.bytes[0];
				st_ssc->rx_bytes_pending--;
			}

			rx_fifo_status = ssc_load32(st_ssc, SSC_RX_FSTAT);
		}

		/* See if there is more data to send */
		if (st_ssc->tx_bytes_pending > 0)
			spi_stmssc_fill_tx_fifo(st_ssc);
		else {
			/* No more data to send */
			ssc_store32(st_ssc, SSC_IEN, 0x0);
			complete(&st_ssc->done);
		}
	}

	return IRQ_HANDLED;
}


static int __init spi_stm_probe(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
	struct spi_master *master;
	struct resource *res;
	struct spi_stm_ssc *st_ssc;

	u32 reg;

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_stm_ssc));
	if (!master)
		return -ENOMEM;

	platform_set_drvdata(pdev, master);

	st_ssc = spi_master_get_devdata(master);
	st_ssc->bitbang.master     = spi_master_get(master);
	st_ssc->bitbang.setup_transfer = spi_stmssc_setup_transfer;
	st_ssc->bitbang.txrx_bufs  = spi_stmssc_txrx_bufs;
	st_ssc->bitbang.master->setup = spi_stmssc_setup;

	if (pio_info->chipselect)
		st_ssc->bitbang.chipselect = (void (*)
					      (struct spi_device *, int))
			pio_info->chipselect;
	else
		st_ssc->bitbang.chipselect = spi_stpio_chipselect;

	master->num_chipselect = SPI_NO_CHIPSELECT + 1;
	master->bus_num = pdev->id;
	init_completion(&st_ssc->done);

	/* Get resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!devm_request_mem_region(&pdev->dev, res->start,
				     res->end - res->start, "spi")) {
		printk(KERN_ERR NAME " Request mem 0x%x region failed\n",
		       res->start);
		return -ENOMEM;
	}

	st_ssc->base =
		(unsigned long) devm_ioremap_nocache(&pdev->dev, res->start,
						     res->end - res->start);
	if (!st_ssc->base) {
		printk(KERN_ERR NAME " Request iomem 0x%x region failed\n",
		       res->start);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		printk(KERN_ERR NAME " Request irq %d failed\n", res->start);
		return -ENODEV;
	}

	if (devm_request_irq(&pdev->dev, res->start, spi_stmssc_irq,
		IRQF_DISABLED, "stspi", st_ssc) < 0) {
		printk(KERN_ERR NAME " Request irq failed\n");
		return -ENODEV;
	}

	/* Check for hard wired SSC which doesn't use PIO pins */
	if (pio_info->pio[0].pio_port == SSC_NO_PIO)
		goto ssc_hard_wired;

	/* Get PIO pins */
	pio_info->clk = stpio_request_set_pin(pio_info->pio[0].pio_port,
					  pio_info->pio[0].pio_pin,
					      "SPI Clock", STPIO_BIDIR, 0);
	if (!pio_info->clk) {
		printk(KERN_ERR NAME
		       " Failed to allocate clk pin (PIO%d[%d])\n",
		       pio_info->pio[0].pio_port, pio_info->pio[0].pio_pin);
		return -ENODEV;
	}
	pio_info->sdout = stpio_request_set_pin(pio_info->pio[1].pio_port,
					    pio_info->pio[1].pio_pin,
						"SPI Data out", STPIO_BIDIR, 0);
	if (!pio_info->sdout) {
		printk(KERN_ERR NAME
		       " Failed to allocate sdo pin (PIO%d[%d])\n",
		       pio_info->pio[1].pio_port, pio_info->pio[1].pio_pin);
		return -ENODEV;
	}
	pio_info->sdin = stpio_request_pin(pio_info->pio[2].pio_port,
					   pio_info->pio[2].pio_pin,
					   "SPI Data in", STPIO_IN);
	if (!pio_info->sdin) {
		printk(KERN_ERR NAME
		       " Failed to allocate sdi pin (PIO%d[%d])\n",
		       pio_info->pio[2].pio_port, pio_info->pio[2].pio_pin);
		return -ENODEV;
	}

ssc_hard_wired:

	/* Disable I2C and Reset SSC */
	ssc_store32(st_ssc, SSC_I2C, 0x0);
	reg = ssc_load16(st_ssc, SSC_CTL);
	reg |= SSC_CTL_SR;
	ssc_store32(st_ssc, SSC_CTL, reg);

	udelay(1);
	reg = ssc_load32(st_ssc, SSC_CTL);
	reg &= ~SSC_CTL_SR;
	ssc_store32(st_ssc, SSC_CTL, reg);

	/* Set SSC into slave mode before reconfiguring PIO pins */
	reg = ssc_load32(st_ssc, SSC_CTL);
	reg &= ~SSC_CTL_MS;
	ssc_store32(st_ssc, SSC_CTL, reg);

	if (pio_info->pio[0].pio_port == SSC_NO_PIO)
		goto ssc_hard_wired2;

#ifdef CONFIG_CPU_SUBTYPE_STX7141
	stpio_configure_pin(pio_info->clk, STPIO_OUT);
	stpio_configure_pin(pio_info->sdout, STPIO_OUT);
	stpio_configure_pin(pio_info->sdin, STPIO_IN);
#else
	stpio_configure_pin(pio_info->clk, STPIO_ALT_OUT);
	stpio_configure_pin(pio_info->sdout, STPIO_ALT_OUT);
	stpio_configure_pin(pio_info->sdin, STPIO_IN);
#endif

ssc_hard_wired2:

	st_ssc->fcomms = clk_get_rate(clk_get(NULL, "comms_clk"));;

	/* Start bitbang worker */
	if (spi_bitbang_start(&st_ssc->bitbang)) {
		printk(KERN_ERR NAME
		       " The SPI Core refuses the spi_stm_ssc adapter\n");
		return -1;
	}

	printk(KERN_INFO NAME ": Registered SPI Bus %d: "
	       "CLK[%d,%d] SDOUT[%d, %d] SDIN[%d, %d]\n", master->bus_num,
	       pio_info->pio[0].pio_port, pio_info->pio[0].pio_pin,
	       pio_info->pio[1].pio_port, pio_info->pio[1].pio_pin,
	       pio_info->pio[2].pio_port, pio_info->pio[2].pio_pin);

	return 0;
}

static int  spi_stm_remove(struct platform_device *pdev)
{
	struct spi_stm_ssc *st_ssc;
	struct spi_master *master;
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;

	master = platform_get_drvdata(pdev);
	st_ssc = spi_master_get_devdata(master);

	spi_bitbang_stop(&st_ssc->bitbang);

	if (pio_info->sdin) {
		stpio_free_pin(pio_info->sdin);
		stpio_free_pin(pio_info->clk);
		stpio_free_pin(pio_info->sdout);
	}

	return 0;
}

static struct platform_driver spi_hw_driver = {
	.driver.name = "spi_st_ssc",
	.driver.owner = THIS_MODULE,
	.probe = spi_stm_probe,
	.remove = spi_stm_remove,
};


static int __init spi_stm_ssc_init(void)
{
	printk(KERN_INFO NAME ": SSC SPI Driver\n");
	return platform_driver_register(&spi_hw_driver);
}

static void __exit spi_stm_ssc_exit(void)
{
	dgb_print("\n");
	platform_driver_unregister(&spi_hw_driver);
}

module_init(spi_stm_ssc_init);
module_exit(spi_stm_ssc_exit);

MODULE_AUTHOR("STMicroelectronics <www.st.com>");
MODULE_DESCRIPTION("STM SSC SPI driver");
MODULE_LICENSE("GPL");
