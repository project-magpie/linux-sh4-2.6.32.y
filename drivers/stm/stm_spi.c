/*
   -------------------------------------------------------------------------
   stm_spi.c
   -------------------------------------------------------------------------
   STMicroelectronics


   ----------------------------------------------------------------------------

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, wrssc to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */
#include "stm_spi.h"
#include "stm_spi_ioctl.h"
#include <linux/stm/pio.h>
#include <linux/vmalloc.h>
#include <asm/semaphore.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/param.h>		/* for HZ */


#undef dgb_print

#ifdef  CONFIG_STM_SPI_DEBUG
#define SPI_LOOP_DEBUG
#define dgb_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#define NANOSEC_PER_SEC         1000000000

#define SPI_LINE_SHIFT      0x0
#define SPI_BANK_SHIFT      0x3
#define SPI_MODE_SHIFT      0x7

#define SPI_LINE_MASK       0x7
#define SPI_BANK_MASK       0xf
#define SPI_MODE_MASK       0x1
#define SPI_DEVICE_MASK     0xff

#define spi_get_mode(address)       ( (address >> SPI_MODE_SHIFT) & SPI_MODE_MASK )
#define spi_get_bank(address)       ( (address >> SPI_BANK_SHIFT) & SPI_BANK_MASK )
#define spi_get_line(address)       ( (address >> SPI_LINE_SHIFT) & SPI_LINE_MASK )
#define spi_get_device(address)     (  address & SPI_DEVICE_MASK )

enum spi_state_machine_e {
	SPI_FSM_VOID = 0,
	SPI_FSM_PREPARE,
	SPI_FSM_RUNNING_8BITS,
	SPI_FSM_RUNNING_16BITS,
	SPI_FSM_STOP,
	SPI_FSM_COMPLETE,
	SPI_FSM_ABORT
};

struct spi_client_t {
	struct spi_device_t *dev;	/* the bus device used */
	struct stpio_pin *pio_chip;
	enum spi_state_machine_e state;
	enum spi_state_machine_e next_state;
	unsigned int msg_length;	// in bytes
	unsigned int idx_write;
	unsigned int idx_read;
	char *buf_write;
	char *buf_read;
	unsigned long timeout;
#define SPI_PHASE_MASK            0x01
#define SPI_PHASE_HIGH            0x01
#define SPI_PHASE_LOW             0x00

#define SPI_POLARITY_MASK         0x02
#define SPI_POLARITY_HIGH         0x02
#define SPI_POLARITY_LOW          0x00

#define SPI_MSB_MASK              0x04
#define SPI_MSB                   0x04
#define SPI_LSB                   0x00

#define SPI_FULLDUPLEX_MASK       0x08
#define SPI_FULLDUPLEX            0x08
#define SPI_HALFDUPLEX            0x00

#define SPI_WIDE_MASK             0x10
#define SPI_WIDE_16BITS           0x10
#define SPI_WIDE_8BITS            0x00

#define SPI_CSACTIVE_MASK         0x20
#define SPI_CSACTIVE_HIGH         0x20
#define SPI_CSACTIVE_LOW          0x00

#define SPI_BAUDRATE_MASK         0xffff0000
#define SPI_BAUDRATE_SHIFT        0x10

/*
 *  * Virtual Configuration *
 *
 *  [  0: POLARITY  :0]
 *  [  1: PHASE     :1]
 *  [  2: MSB       :2]
 *  [  3: FULL/HALF :3]
 *  [  4: WIDE      :4]
 *  [  5: CS_ACTIVE :5]
 *  [ 15: FREE      :6]
 *  [ 31: BAUDRATE  :16]
 *
 */
	unsigned int virtual_configuration;
};

#define SPI_MAJOR_NUMBER 98

#define SPI_RDWR_OFFSET   8
static struct cdev spi_char_dev;

/*
 *  In this way i can manage no more than 5 bus spi
 *  but 5 it's enough for our platform
 */
#define MAX_NUMBER_SPI_BUSSES 5
/*
 *  This array is used to speed up the
 *  open device file
 */
struct spi_device_t *spi_busses_array[MAX_NUMBER_SPI_BUSSES];
//static LIST_HEAD(spi_busses);
/*
 * In this way the spi bus will be available
 * with the spi_busses_array array
 * or the spi_busses list
 */

#define spi_malloc(size)     vmalloc(size)
#define spi_free(addr)       vfree(addr)

#define jump_on_fsm_complete()     { spi->state = SPI_FSM_COMPLETE;    \
                                      goto be_fsm_complete;       }

void spi_algo_state_machine(struct spi_client_t *spi)
{

   struct device *dev    = spi->dev->dev.parent;
   struct ssc_t *ssc_bus = container_of(dev,struct ssc_t,dev);
	unsigned short status;
   unsigned int   idx;
   unsigned short tx_fifo_status;
   unsigned short rx_fifo_status;
	unsigned int config = spi->virtual_configuration;
	unsigned int phase, polarity;
	unsigned int hb, frame_size;
	unsigned char ctmp;

	union {
		char bytes[2];
		short word;
	} tmp;

	tmp.word = 0;
	spi->state = spi->next_state;

	switch (spi->state) {
	case SPI_FSM_PREPARE:
	   dgb_print("-SPI_FSM_PREPARE\n");
		spi->idx_write = 0;
		spi->idx_read = 0;
		phase = ((config & SPI_PHASE_MASK) ? 1 : 0);
		polarity = ((config & SPI_POLARITY_MASK) ? 1 : 0);
		hb = ((config & SPI_MSB_MASK) ? 1 : 0);
		frame_size = ((config & SPI_WIDE_MASK) ? 1 : 0) * 0x8 + 0x7;

		ssc_store16(ssc_bus, SSC_BRG,
			    (config & SPI_BAUDRATE_MASK) >> SPI_BAUDRATE_SHIFT);

		ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_SR | 0x1);
		ssc_store16(ssc_bus, SSC_I2C, 0x0);
		ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
			    (SSC_CTL_PO * polarity) |
			    (SSC_CTL_PH * phase) | (SSC_CTL_HB * hb) |
#ifdef SPI_LOOP_DEBUG
			    SSC_CTL_LPB |
#endif
			    frame_size);
		tmp.bytes[0] = spi->buf_write[spi->idx_write++];

		spi->next_state = SPI_FSM_RUNNING_8BITS;
		if (frame_size > 0x7) {
			spi->next_state = SPI_FSM_RUNNING_16BITS;

			ctmp = tmp.bytes[0];
			tmp.bytes[0] = spi->buf_write[spi->idx_write++];
			tmp.bytes[1] = ctmp;
		}
	   ssc_load16(ssc_bus, SSC_RBUF);/* only to clear the status register */
		ssc_store16(ssc_bus, SSC_TBUF, tmp.word);
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_TEEN | SSC_IEN_RIEN);

		break;

	case SPI_FSM_RUNNING_8BITS:
		dgb_print(" SPI_FSM_RUNNING:\n");

		status = ssc_load16(ssc_bus, SSC_STA);
		if ((status & SSC_STA_RIR) && spi->idx_read < spi->msg_length) {
			tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
			spi->buf_read[spi->idx_read++] = tmp.bytes[0];
			dgb_print(" Reading: %c\n", tmp.bytes[0]);
		}
		if ((status & SSC_STA_TIR) && spi->idx_write < spi->msg_length) {
                	dgb_print(" Writeing %c\n",
				spi->buf_write[spi->idx_write]);
			tmp.bytes[0] = spi->buf_write[spi->idx_write++];
			ssc_store16(ssc_bus, SSC_TBUF, tmp.word);
		}
		if (spi->idx_write >= spi->msg_length
		    && spi->idx_read >= spi->msg_length)
			jump_on_fsm_complete();
		break;
	case SPI_FSM_RUNNING_16BITS:
		dgb_print(" SPI_FSM_RUNNING_16BITS\n");
		status = ssc_load16(ssc_bus, SSC_STA);
		if ((status & SSC_STA_RIR) && spi->idx_read < spi->msg_length) {
			tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
			spi->buf_read[spi->idx_read++] = tmp.bytes[1];
			spi->buf_read[spi->idx_read++] = tmp.bytes[0];
			dgb_print(" Reading: %c %c\n", tmp.bytes[1],
				tmp.bytes[0]);
		}
		if ((status & SSC_STA_TIR) && spi->idx_write < spi->msg_length) {
			dgb_print(" Writeing %c %c\n",
				spi->buf_write[spi->idx_write],
				spi->buf_write[spi->idx_write + 1]);
			tmp.bytes[1] = spi->buf_write[spi->idx_write++];
			tmp.bytes[0] = spi->buf_write[spi->idx_write++];
			ssc_store16(ssc_bus, SSC_TBUF, tmp.word);
		}
		if (spi->idx_write >= spi->msg_length
		    && spi->idx_read >= spi->msg_length)
			jump_on_fsm_complete();
		break;
	case SPI_FSM_COMPLETE:
	      be_fsm_complete:
		dgb_print(" SPI_FSM_COMPLETE\n");
		ssc_store16(ssc_bus, SSC_IEN, 0x0);
		wake_up(&(ssc_bus->wait_queue));
		break;

	case SPI_FSM_VOID:
	default:
		;
	}
	return;
}

#define chip_asserted() if ( spi->virtual_configuration & SPI_CSACTIVE_MASK ) \
                             stpio_set_pin(spi->pio_chip, 0x1);               \
                        else stpio_set_pin(spi->pio_chip, 0x0);

#define chip_deasserted() if ( spi->virtual_configuration & SPI_CSACTIVE_MASK ) \
                               stpio_set_pin(spi->pio_chip, 0x0);               \
                          else stpio_set_pin(spi->pio_chip, 0x1);

static ssize_t spi_cdev_read(struct file *filp,
			     char __user * buff, size_t count, loff_t * offp)
{
	struct spi_client_t *spi = (struct spi_client_t *)filp->private_data;
        struct device *dev = spi->dev->dev.parent;
	struct ssc_t *ssc_bus = container_of(dev,struct ssc_t,dev);
	unsigned int local_flag;

	if (spi->pio_chip == NULL)
		return -ENODATA;

	if (spi->virtual_configuration & SPI_FULLDUPLEX_MASK) {
/*
 * In FullDuplex Mode
 * The Datas are already ready...
 */
		if (spi->buf_read == NULL)
			return 0;
		dgb_print("Reading in FullD\n");
		copy_to_user(buff, spi->buf_read, spi->msg_length);
		spi_free(spi->buf_read);
		spi->buf_read = NULL;
		return spi->msg_length;
	}

	/*
	 * the first step is request the bus access
	 */
	ssc_request_bus(ssc_bus, spi_algo_state_machine, (void *)spi);

	chip_asserted();

#ifdef SPI_LOOP_DEBUG
#define DUMMY   "dummy_string_only_for_test"
	count = strlen(DUMMY);
#endif

	dgb_print("Reading in Half/D %d bytes\n", count);
	spi->buf_read = (char *)spi_malloc(count+SPI_RDWR_OFFSET);
        spi->buf_write = spi->buf_read+SPI_RDWR_OFFSET;
	spi->msg_length = count;

#ifdef SPI_LOOP_DEBUG
	strcpy(spi->buf_write, DUMMY);
#endif

	spi->next_state = SPI_FSM_PREPARE;
	local_irq_save(local_flag);

/*
 *  When the data frame is 16 bits lenght
 *  the msg_length must be %2=0
 *
 */
	if (spi->virtual_configuration & SPI_WIDE_MASK)
		spi->msg_length &= ~0x1;

	spi_algo_state_machine(spi);
	interruptible_sleep_on_timeout(&(ssc_bus->wait_queue),
				       spi->timeout * HZ);
	local_irq_restore(local_flag);

	chip_deasserted();

	ssc_release_bus(ssc_bus);
	copy_to_user(buff, spi->buf_read, count);
	spi_free(spi->buf_read);
	spi->buf_read = NULL;
	spi->buf_write = NULL;
	return count;
}

static ssize_t spi_cdev_write(struct file *filp,
			      const char __user * buff,
			      size_t count, loff_t * offp)
{
	struct spi_client_t *spi = (struct spi_client_t *)filp->private_data;
        struct device *dev = spi->dev->dev.parent;
	struct ssc_t *ssc_bus = container_of(dev,struct ssc_t,dev);
	unsigned int local_flag;
	dgb_print("\n");

	if (spi->pio_chip == NULL)
		return -ENODATA;

	ssc_request_bus(ssc_bus, spi_algo_state_machine, (void *)spi);

	chip_asserted();

	if (spi->buf_read != NULL)
		spi_free(spi->buf_read);

	spi->buf_read  = spi_malloc(count+SPI_RDWR_OFFSET);
        spi->buf_write = spi->buf_read+SPI_RDWR_OFFSET;

	spi->msg_length = count;
	if (spi->virtual_configuration & SPI_WIDE_MASK)
		spi->msg_length &= ~0x1;

	copy_from_user(spi->buf_write, buff, spi->msg_length);

	spi->next_state = SPI_FSM_PREPARE;

	local_irq_save(local_flag);
	spi_algo_state_machine(spi);
	interruptible_sleep_on_timeout(&(ssc_bus->wait_queue),
				       spi->timeout * HZ);
	local_irq_restore(local_flag);
	chip_deasserted();
	spi->buf_write = NULL;
	ssc_release_bus(ssc_bus);

	if (!(spi->virtual_configuration & SPI_FULLDUPLEX)) {
#ifdef SPI_LOOP_DEBUG
		dgb_print("Read: %s\n", spi->buf_read);
#endif
		spi_free(spi->buf_read);
		spi->buf_read = NULL;
		spi->msg_length = 0;
	}
	return count;
}

static int spi_cdev_addressing(unsigned int address, struct spi_client_t *spi)
{
	unsigned int spi_device;

	spi_device = spi_get_device(address);

	dgb_print("Spi opening Slave 0x%x (%d)\n", spi_device, spi_device);

/* 1. release the Pio of previous addressing*/
        if ( spi->pio_chip)
            stpio_free_pin(spi->pio_chip);
       spi->pio_chip = NULL;
// 2. check if the pio[BANK][LINE] used for chip_selector is free
	spi->pio_chip =
	    stpio_request_pin(spi_get_bank(address), spi_get_line(address),
			      "spi-chip-selector", STPIO_OUT);

	if (!(spi->pio_chip)) {
/*
 * Somebody already requested the PIO[bank][line]
 * therefore we abort the addressing
 */
		dgb_print("Error Pio locked or not-exist\n");
		return -ENOSYS;
	}
	dgb_print("->with PIO [%d][%d]\n", spi_get_bank(address),
		spi_get_line(address));

	spi->virtual_configuration =
	    spi->virtual_configuration & ~SPI_FULLDUPLEX;
	dgb_print("->with FULLDUPLEX = 0x%x\n", spi_get_mode(address));
	spi->virtual_configuration |= SPI_FULLDUPLEX * spi_get_mode(address);
/*
 *  Free the data of prev addressing
 */
	if (spi->buf_read != NULL)
		spi_free(spi->buf_read);

	spi->buf_read = NULL;
	chip_deasserted();

	return 0;

}

static int spi_cdev_ioctl(struct inode *inode,
			  struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct spi_client_t *spi = (struct spi_client_t *)filp->private_data;

	dgb_print("\n");

	switch (cmd) {
	case SPI_IOCTL_WIDEFRAME:
		spi->virtual_configuration =
		    spi->virtual_configuration & ~SPI_WIDE_MASK;
		if (arg)
			spi->virtual_configuration |= SPI_WIDE_16BITS;
		break;
	case SPI_IOCTL_POLARITY:
		spi->virtual_configuration =
		    spi->virtual_configuration & ~SPI_POLARITY_MASK;
		if (arg)
			spi->virtual_configuration |= SPI_POLARITY_HIGH;
		break;
	case SPI_IOCTL_PHASE:
		spi->virtual_configuration =
		    spi->virtual_configuration & ~SPI_PHASE_MASK;
		if (arg)
			spi->virtual_configuration |= SPI_PHASE_HIGH;
		break;
	case SPI_IOCTL_HEADING:
		spi->virtual_configuration =
		    spi->virtual_configuration & ~SPI_MSB_MASK;
		if (arg)
			spi->virtual_configuration |= SPI_MSB;
		break;
	case SPI_IOCTL_BUADRATE:
		{
			unsigned long baudrate;
			baudrate =
			    ssc_get_clock() / (2 *arg);
			spi->virtual_configuration =
			    spi->virtual_configuration & ~SPI_BAUDRATE_MASK;
			spi->virtual_configuration =
			    spi->
			    virtual_configuration | (baudrate <<
						     SPI_BAUDRATE_SHIFT);
		}
		break;
	case SPI_IOCTL_CSACTIVE:
		spi->virtual_configuration =
		    spi->virtual_configuration & ~SPI_CSACTIVE_MASK;
		if (arg)
			spi->virtual_configuration |= SPI_CSACTIVE_HIGH;
		break;
	case SPI_IOCTL_ADDRESS:
		if (spi_cdev_addressing((unsigned int)arg, spi) != 0)
			return -1;
		break;
	case SPI_IOCTL_TIMEOUT:
		spi->timeout = arg;
		break;
	default:
		;
	}

#ifdef SPI_STM_DEBUG
	{
		unsigned int conf = spi->virtual_configuration;
		dgb_print("SPI - Virtual Config:\n");
		dgb_print(" - PHASE:    0x%x\n", (conf & SPI_PHASE_MASK) != 0);
		dgb_print(" - POLARITY: 0x%x\n", (conf & SPI_POLARITY_MASK) != 0);
		dgb_print(" - HEADING:  0x%x\n", (conf & SPI_MSB_MASK) != 0);
		dgb_print(" - FULLDUP:  0x%x\n",
			(conf & SPI_FULLDUPLEX_MASK) != 0);
		dgb_print(" - WIDE:     0x%x\n", (conf & SPI_WIDE_MASK) != 0);
		dgb_print(" - CSACTIVE: 0x%x\n", (conf & SPI_CSACTIVE_MASK) != 0);
		dgb_print(" - BUADRATE: 0x%x\n",
			(conf & SPI_BAUDRATE_MASK) >> SPI_BAUDRATE_SHIFT);
	}
#endif
	return 0;
}

static int spi_cdev_open(struct inode *inode, struct file *filp)
{
	unsigned int minor;
	struct spi_client_t *spi;

	dgb_print("\n");

	minor = iminor(inode);
	if (minor >= MAX_NUMBER_SPI_BUSSES)
		return -ENODEV;
        if (!spi_busses_array[minor])
                return -ENODEV;
        spi = (struct spi_client_t *)kmalloc(sizeof(struct spi_client_t),GFP_KERNEL);
	spi->dev = spi_busses_array[minor];
	spi->timeout = 5;	/* 5 seconds */
	spi->msg_length = 0;
	spi->buf_write = NULL;
	spi->buf_read = NULL;
	spi->pio_chip = NULL;
/*
 *  1 Phase
 *  1 Polarity
 *  1 Heading
 *  - Full/Half
 *  1 Wide (16bits)
 *  0 CSActive
 *  1 MHz (at 133MHz of common clock)
 */
	spi->virtual_configuration = 0x420017;

	filp->private_data = spi;
	return 0;
}

static int spi_cdev_release(struct inode *inode, struct file *filp)
{
	struct spi_client_t *spi = (struct spi_client_t *)filp->private_data;

   dgb_print("\n");
	if (spi->pio_chip != NULL) {
		stpio_free_pin(spi->pio_chip);
		spi->pio_chip = NULL;
	}
   dgb_print("PIO-chip released\n");
	if (spi->buf_read != NULL)
		spi_free(spi->buf_read);
   kfree(spi);
	filp->private_data = NULL;
	return 0;
}

struct file_operations spi_stm_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = spi_cdev_open,
	.release = spi_cdev_release,
	.read = spi_cdev_read,
	.write = spi_cdev_write,
	.ioctl = spi_cdev_ioctl
};

static int spi_stm_match(struct device *dev, struct device_driver *drv)
{
   dgb_print("\n");
	if (dev == NULL || drv == NULL)
		return 0;
	return !strncmp(dev->bus_id, drv->name, 3);
}

struct bus_type spi_bus_type = {
	.name = "spi",
	.match = spi_stm_match,
};

void spi_del_adapter(struct spi_device_t *spi_dev)
{
   dgb_print("\n");
   spi_busses_array[spi_dev->idx_dev]=0;
   kfree(spi_dev);
   return ;
}

#define SPI_DRIVER_BUS
#if   defined(SPI_DRIVER_BUS)
static int spi_bus_driver_probe(struct device *dev)
{
   struct spi_device_t *spi_dev;

   dgb_print("\n");
   spi_dev = container_of(dev,struct spi_device_t,dev);

   return spi_dev->dev_type == SPI_DEV_BUS_ADAPTER;
};

static void spi_bus_driver_remove(struct device *dev)
{
	struct spi_device_t *spi_dev;
   spi_dev = container_of(dev,struct spi_device_t,dev);
   dgb_print("\n");
//   spi_del_adapter(spi_dev);
//   dgb_print("..\n");
   return;
}
static void spi_bus_driver_shutdown(struct device *dev)
{
   struct spi_device_t *spi_dev;
   spi_dev = container_of(dev,struct spi_device_t,dev);
   dgb_print("\n");
   spi_del_adapter(spi_dev);
//   dgb_print("..\n");
   return;
}
static struct device_driver spi_bus_drv = {
   .owner = THIS_MODULE,
   .name = "spi_bus_drv",
   .bus = &spi_bus_type,
   .probe = spi_bus_driver_probe,
   .shutdown = spi_bus_driver_shutdown,
   .remove   = spi_bus_driver_remove,
};
#endif

int spi_add_adapter(struct spi_device_t *spi_dev)
{
	unsigned int ret;
   unsigned int idx_dev = spi_dev->idx_dev;

   dgb_print("\n");
   spi_dev->dev_type = SPI_DEV_BUS_ADAPTER;
	spi_dev->dev.bus = &(spi_bus_type);
   sprintf(spi_dev->dev.bus_id, "spi-%d", idx_dev);
   spi_dev->dev.release = spi_del_adapter;
#if defined(SPI_DRIVER_BUS)
   spi_dev->dev.driver = &spi_bus_drv;
#endif
	ret = device_register(&spi_dev->dev);
	if (ret) {
		printk(KERN_WARNING "Unable to register %s bus\n",
		       spi_dev->dev.bus_id);
		kfree(spi_dev);
	} else {
/*
 * with the spi_busses_array
 * i avoid to used the list
 */
     spi_busses_array[idx_dev] = spi_dev;
     //list_add(&spi_dev->list, &spi_busses);
	}
	return ret;
}

static int spi_stm_adapter_detect()
{
	unsigned int idx;
   unsigned int num_ssc_bus = ssc_device_available();
	unsigned int num_spi_bus;
	struct spi_device_t *spi_dev;
   struct ssc_t **ssc_busses;
   dgb_print("\n");
/*
 *  Check the ssc on the platform
 */
   ssc_busses = (struct ssc_t **) kmalloc(num_ssc_bus *
                        sizeof(struct ssc_t *), GFP_KERNEL);
   for (idx = 0, num_spi_bus = 0; idx < num_ssc_bus; ++idx)
       if ((ssc_capability(idx) & SSC_SPI_CAPABILITY))
            ssc_busses[num_spi_bus++] = ssc_device_request(idx);

   for (idx = 0; idx < num_spi_bus; ++idx) {
      spi_dev = (struct spi_device_t *)
               kmalloc(sizeof(struct spi_device_t), GFP_KERNEL);
      memset(&spi_dev->dev, 0, sizeof(struct device));
      spi_dev->dev.parent = &(ssc_busses[idx]->dev);
      spi_dev->idx_dev = idx;
      spi_add_adapter(spi_dev);
   };
   kfree(ssc_busses);
}

static int __init spi_stm_init(void)
{
   dev_t ch_device;
   unsigned int ret;
   unsigned int num_ssc_bus = ssc_device_available();
   dgb_print("\n");
	ret = bus_register(&spi_bus_type);
	if (ret) {
		printk(KERN_WARNING "Unable to register spi bus\n");
		return ret;
	}
#ifdef SPI_DRIVER_BUS
   ret = driver_register(&spi_bus_drv);
	if (ret) {
		printk(KERN_WARNING "Unable to register spi driver\n");
		return ret;
	}
#endif
   spi_stm_adapter_detect();

	ch_device = MKDEV(SPI_MAJOR_NUMBER, 0);
   register_chrdev_region(ch_device, num_ssc_bus, "spi");
	cdev_init(&(spi_char_dev), &(spi_stm_fops));
   cdev_add(&(spi_char_dev), ch_device, num_ssc_bus);

	printk(KERN_INFO "spi /dev layer initialized\n");
	return 0;
}

static int __exit spi_stm_exit(void)
{
	dev_t ch_device;
   struct spi_device_t *spi_dev;
   struct list_head *item;

   ch_device = MKDEV(SPI_MAJOR_NUMBER, 0);
   dgb_print("\n");
	cdev_del(&(spi_char_dev));
	unregister_chrdev_region(ch_device, ssc_device_available());
/*
   list_for_each(item,&spi_busses) {
   spi_dev = container_of(item, struct spi_device_t,list);
   spi_del_adapter(spi_dev);
   }
*/
   dgb_print("\n");

#if defined(SPI_DRIVER_BUS)
   driver_unregister(&spi_bus_drv);
#endif
	bus_unregister(&spi_bus_type);
	return 0;
}

late_initcall(spi_stm_init);
module_exit(spi_stm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("Module for stm spi device");
MODULE_LICENSE("GPL");
