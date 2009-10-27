/*
 *  -------------------------------------------------------------------------
 *  spi_stm_gpio.c SPI/GPIO driver for STMicroelectronics platforms
 *  -------------------------------------------------------------------------
 *
 *  Copyright (c) 2008 STMicroelectronics Limited
 *  Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 *  -------------------------------------------------------------------------
 *  Changelog:
 *  2008-01-24 Angus Clark <angus.clark@st.com>
 *    - chip_select modified to ignore devices with no chip_select, and keep
 *      hold of PIO pin (freeing pin selects STPIO_IN (high-Z) mode).
 *    - added spi_stmpio_setup() and spi_stmpio_setup_transfer() to enfore
 *	SPI_STMPIO_MAX_SPEED_HZ
 *  2008-08-28 Angus Clark <angus.clark@st.com>
 *    - Updated to fit with changes to 'ssc_pio_t'
 *    - Support for user-defined chip_select, specified in board setup
 *
 *  -------------------------------------------------------------------------
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/stm/platform.h>
#include <linux/io.h>
#include <linux/param.h>

#ifdef CONFIG_SPI_DEBUG
#define dgb_print(fmt, args...)  printk(KERN_INFO "%s: " \
					fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)	do { } while (0)
#endif

#define NAME "spi-stm-gpio"

/* Maybe this should be included in platform_data? */
#define SPI_STMPIO_MAX_SPEED_HZ		1000000

struct spi_stm_gpio {
	struct spi_bitbang	bitbang;
	struct platform_device	*pdev;
	unsigned int gpio_sck, gpio_mosi, gpio_miso;
	/* Max speed supported by STPIO bit-banging SPI controller */
	int max_speed_hz;
};

static inline void setsck(struct spi_device *dev, int on)
{
	struct spi_stm_gpio *spi_stm_gpio = spi_master_get_devdata(dev->master);

	gpio_set_value(spi_stm_gpio->gpio_sck, on);
}

static inline void setmosi(struct spi_device *dev, int on)
{
	struct spi_stm_gpio *spi_stm_gpio = spi_master_get_devdata(dev->master);

	gpio_set_value(spi_stm_gpio->gpio_mosi, on);
}

static inline u32 getmiso(struct spi_device *dev)
{
	struct spi_stm_gpio *spi_stm_gpio = spi_master_get_devdata(dev->master);

	return gpio_get_value(spi_stm_gpio->gpio_miso);
}

#define EXPAND_BITBANG_TXRX
#define spidelay(x) ndelay(x)
#include <linux/spi/spi_bitbang.h>

static u32 spi_gpio_txrx_mode0(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha0(spi, nsecs, 0, word, bits);
}

static u32 spi_gpio_txrx_mode1(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha1(spi, nsecs, 0, word, bits);
}

static u32 spi_gpio_txrx_mode2(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha0(spi, nsecs, 1, word, bits);
}

static u32 spi_gpio_txrx_mode3(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha1(spi, nsecs, 1, word, bits);
}

static void spi_stm_gpio_chipselect(struct spi_device *spi, int value)
{
	unsigned int out;

	dgb_print("\n");

	if (spi->chip_select == (typeof(spi->chip_select))(STM_GPIO_INVALID))
		return;

	if (value == BITBANG_CS_ACTIVE)
		out = spi->mode & SPI_CS_HIGH ? 1 : 0;
	else
		out = spi->mode & SPI_CS_HIGH ? 0 : 1;

	if (unlikely(!spi->controller_data)) {
		/* Allocate the GPIO when called for the first time.
		 * FIXME: This code leaks a gpio! (no gpio_free()) */
		if (gpio_request(spi->chip_select, "spi_stm_gpio CS") < 0) {
			printk(KERN_ERR NAME " Failed to allocate CS pin\n");
			return;
		}
		spi->controller_data = (void *)1;
		gpio_direction_output(spi->chip_select, out);
	} else {
		gpio_set_value(spi->chip_select, out);
	}

	dgb_print("%s PIO%d[%d] -> %d \n",
		  value == BITBANG_CS_ACTIVE ? "select" : "deselect",
		  stm_gpio_port(spi->chip_select),
		  stm_gpio_pin(spi->chip_select), out);

	return;
}

static int spi_stmpio_setup(struct spi_device *spi)
{
	struct spi_stm_gpio *spi_stm_gpio = spi_master_get_devdata(spi->master);

	dgb_print("\n");

	if (spi->max_speed_hz > spi_stm_gpio->max_speed_hz) {
		printk(KERN_ERR NAME " requested baud rate (%dhz) exceeds "
		       "max (%dhz)\n",
		       spi->max_speed_hz, spi_stm_gpio->max_speed_hz);
		return -EINVAL;
	}

	return spi_bitbang_setup(spi);
}

static int spi_stmpio_setup_transfer(struct spi_device *spi,
				     struct spi_transfer *t)
{
	dgb_print("\n");

	if (t)
		if (t->speed_hz > spi->max_speed_hz) {
			printk(KERN_ERR NAME " requested baud rate (%dhz) "
			       "exceeds max (%dhz)\n",
			       t->speed_hz, spi->max_speed_hz);
			return -EINVAL;
		}

	return spi_bitbang_setup_transfer(spi, t);
}

static int __init spi_probe(struct platform_device *pdev)
{
	struct stm_plat_ssc_data *plat_data = pdev->dev.platform_data;
	struct spi_master *master;
	struct spi_stm_gpio *spi_stm_gpio;

	dgb_print("\n");

	if (plat_data->gpio_sclk == STM_GPIO_INVALID ||
			plat_data->gpio_mtsr == STM_GPIO_INVALID ||
			plat_data->gpio_mrst == STM_GPIO_INVALID)
		return -1;

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_stm_gpio));
	if (!master)
		return -1;

	spi_stm_gpio = spi_master_get_devdata(master);
	if (!spi_stm_gpio)
		return -1;

	platform_set_drvdata(pdev, spi_stm_gpio);
	spi_stm_gpio->bitbang.master = master;
	spi_stm_gpio->bitbang.master->setup = spi_stmpio_setup;
	spi_stm_gpio->bitbang.setup_transfer = spi_stmpio_setup_transfer;
	spi_stm_gpio->bitbang.chipselect = spi_stm_gpio_chipselect;
	spi_stm_gpio->bitbang.txrx_word[SPI_MODE_0] = spi_gpio_txrx_mode0;
	spi_stm_gpio->bitbang.txrx_word[SPI_MODE_1] = spi_gpio_txrx_mode1;
	spi_stm_gpio->bitbang.txrx_word[SPI_MODE_2] = spi_gpio_txrx_mode2;
	spi_stm_gpio->bitbang.txrx_word[SPI_MODE_3] = spi_gpio_txrx_mode3;

	if (plat_data->spi_chipselect)
		spi_stm_gpio->bitbang.chipselect = plat_data->spi_chipselect;
	else
		spi_stm_gpio->bitbang.chipselect = spi_stm_gpio_chipselect;

	/* chip_select field of spi_device is declared as u8 and therefore
	 * limits number of GPIOs that can be used as a CS line. Sorry. */
	master->num_chipselect =
			sizeof(((struct spi_device *)0)->chip_select) * 256;

	master->bus_num = pdev->id;
	spi_stm_gpio->max_speed_hz = SPI_STMPIO_MAX_SPEED_HZ;

	spi_stm_gpio->gpio_sck = plat_data->gpio_sclk;
	if (gpio_request(spi_stm_gpio->gpio_sck, "spi-stm-gpio SCK") < 0) {
		printk(KERN_ERR NAME " Failed to allocate PIO%d[%d] for SCK\n",
				stm_gpio_port(spi_stm_gpio->gpio_sck),
				stm_gpio_pin(spi_stm_gpio->gpio_sck));
		return -1;
	}
	spi_stm_gpio->gpio_mosi = plat_data->gpio_mtsr;
	if (gpio_request(spi_stm_gpio->gpio_mosi, "spi-stm-gpio MOSI") < 0) {
		printk(KERN_ERR NAME " Failed to allocate PIO%d[%d] for MOSI\n",
				stm_gpio_port(spi_stm_gpio->gpio_mosi),
				stm_gpio_pin(spi_stm_gpio->gpio_mosi));
		return -1;
	}
	spi_stm_gpio->gpio_miso = plat_data->gpio_mrst;
	if (gpio_request(spi_stm_gpio->gpio_miso, "spi-stm-gpio MISO") < 0) {
		printk(KERN_ERR NAME " Failed to allocate PIO%d[%d] for MISO\n",
				stm_gpio_port(spi_stm_gpio->gpio_miso),
				stm_gpio_pin(spi_stm_gpio->gpio_miso));
		return -1;
	}

	gpio_direction_output(spi_stm_gpio->gpio_sck, 0);
	gpio_direction_output(spi_stm_gpio->gpio_mosi, 0);
	gpio_direction_output(spi_stm_gpio->gpio_miso, 0);

	if (spi_bitbang_start(&spi_stm_gpio->bitbang)) {
		printk(KERN_ERR NAME
		       "The SPI Core refuses the spi_stm_gpio adapter\n");
		return -1;
	}

	printk(KERN_INFO NAME ": Registered SPI Bus %d: "
	       "SCK [%d,%d], MOSI [%d,%d], MISO [%d, %d]\n",
	       master->bus_num,
	       stm_gpio_port(spi_stm_gpio->gpio_sck),
	       stm_gpio_pin(spi_stm_gpio->gpio_sck),
	       stm_gpio_port(spi_stm_gpio->gpio_mosi),
	       stm_gpio_pin(spi_stm_gpio->gpio_mosi),
	       stm_gpio_port(spi_stm_gpio->gpio_miso),
	       stm_gpio_pin(spi_stm_gpio->gpio_miso));

	return 0;
}

static int spi_remove(struct platform_device *pdev)
{
	struct spi_stm_gpio *spi_stm_gpio = platform_get_drvdata(pdev);

	dgb_print("\n");
	spi_bitbang_stop(&spi_stm_gpio->bitbang);
	gpio_free(spi_stm_gpio->gpio_sck);
	gpio_free(spi_stm_gpio->gpio_mosi);
	gpio_free(spi_stm_gpio->gpio_miso);
	spi_master_put(spi_stm_gpio->bitbang.master);

	return 0;
}

static struct platform_driver spi_sw_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.probe = spi_probe,
	.remove = spi_remove,
};

static int __init spi_gpio_init(void)
{
	printk(KERN_INFO NAME ": PIO based SPI Driver\n");
	return platform_driver_register(&spi_sw_driver);
}

static void __exit spi_gpio_exit(void)
{
	dgb_print("\n");
	platform_driver_unregister(&spi_sw_driver);
}

MODULE_AUTHOR("Francesco Virlinzi <francesco.virlinzi@st.com>");
MODULE_DESCRIPTION("GPIO based SPI Driver");
MODULE_LICENSE("GPL");

module_init(spi_gpio_init);
module_exit(spi_gpio_exit);

