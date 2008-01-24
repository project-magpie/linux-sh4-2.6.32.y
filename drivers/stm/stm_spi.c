/*
   -------------------------------------------------------------------------
   stm_spi.c
   -------------------------------------------------------------------------
   STMicroelectronics
   Version: 2.0 (1 April 2007)
   ----------------------------------------------------------------------------
   May be copied or modified under the terms of the GNU General Public
   License V.2.  See linux/COPYING for more information.

   ------------------------------------------------------------------------- */

#include "stm_spi.h"
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <asm/semaphore.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/wait.h>
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
	SPI_FSM_RUNNING,
	SPI_FSM_STOP,
	SPI_FSM_COMPLETE,
	SPI_FSM_ABORT
};

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

struct spi_transaction_t {
	struct spi_client_t *client;	/* the transaction's owner */
	enum spi_state_machine_e state;
	enum spi_state_machine_e next_state;
	unsigned int msg_length;
	unsigned int idx_write;
	unsigned int idx_read;
};

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
/*
 * In this way the spi bus will be available
 * with the spi_busses_array array
 */

#define jump_on_fsm_complete(trsc)	{ (trsc)->state = SPI_FSM_COMPLETE;	\
					 goto be_fsm_complete;}

#define jump_on_fsm_abort(trsc)		{ (trsc)->state = SPI_FSM_ABORT;	\
					 goto be_fsm_abort;}

static int spi_state_machine(int this_irq,struct spi_device_t *ssc_bus)
{
	struct spi_transaction_t *transaction = ssc_bus->trns;
	struct spi_client_t *client = transaction->client;
	unsigned short status;
	short tx_fifo_status;
	short rx_fifo_status;
	unsigned int config = client->config;
	unsigned int phase, polarity;
	unsigned int hb;
	unsigned int wide_frame = (config & SPI_WIDE_MASK) ? 1 : 0;

	union {
		char bytes[2];
		short word;
	} tmp = {.word = 0,};

	transaction->state = transaction->next_state;

	dgb_print("\n");
	switch (transaction->state) {
	case SPI_FSM_PREPARE:
		dgb_print("-SPI_FSM_PREPARE\n");
		phase    = ((config & SPI_PHASE_MASK) ? 1 : 0);
		polarity = ((config & SPI_POLARITY_MASK) ? 1 : 0);
		hb       = ((config & SPI_MSB_MASK) ? 1 : 0);
		wide_frame = ((config & SPI_WIDE_MASK) ? 1 : 0) * 0x8 + 0x7;

		ssc_store32(ssc_bus, SSC_BRG,
			    (config & SPI_BAUDRATE_MASK) >> SPI_BAUDRATE_SHIFT);

		ssc_store32(ssc_bus, SSC_CTL, SSC_CTL_SR | 0x1);
		ssc_store32(ssc_bus, SSC_I2C, 0x0);
		ssc_store32(ssc_bus, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
			    (SSC_CTL_PO * polarity) |
			    (SSC_CTL_PH * phase) | (SSC_CTL_HB * hb) |
#ifdef SPI_LOOP_DEBUG
			    SSC_CTL_LPB |
#endif
#ifdef CONFIG_STM_SPI_HW_FIFO
			    SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO |
#endif
			    wide_frame);

		transaction->next_state = SPI_FSM_RUNNING;
		ssc_load32(ssc_bus, SSC_RBUF);	/* only to clear the status register */
#ifdef CONFIG_STM_SPI_HW_FIFO
		for (tx_fifo_status = 0;
		     tx_fifo_status < SSC_TXFIFO_SIZE - 1 &&
		     transaction->idx_write < transaction->msg_length;
		     ++tx_fifo_status)
#endif
		{
			if (wide_frame > 0x7) {
				dgb_print(" Writing %c %c\n",
					  client->wr_buf[transaction->
							 idx_write * 2],
					  client->wr_buf[transaction->
							 idx_write * 2 + 1]);
				tmp.bytes[1] =
				    client->wr_buf[transaction->idx_write * 2];
				tmp.bytes[0] =
				    client->wr_buf[transaction->idx_write * 2 +
						   1];
			} else {
				dgb_print(" Writing %c\n",
					  client->wr_buf[transaction->
							 idx_write]);
				tmp.bytes[0] =
				    client->wr_buf[transaction->idx_write];
			}
			++transaction->idx_write;
			ssc_store32(ssc_bus, SSC_TBUF, tmp.word);
		}
		ssc_store32(ssc_bus, SSC_IEN, SSC_IEN_TEEN | SSC_IEN_RIEN);
		break;

	case SPI_FSM_RUNNING:
		status = ssc_load32(ssc_bus, SSC_STA);
		dgb_print(" SPI_FSM_RUNNING 0x%x\n", status);
#ifndef CONFIG_STM_SPI_HW_FIFO
		if ((status & SSC_STA_RIR) &&
		    transaction->idx_read < transaction->msg_length) {
#else
		for (rx_fifo_status = ssc_load32(ssc_bus, SSC_RX_FSTAT);
		     rx_fifo_status &&
		     transaction->idx_read < transaction->msg_length;
		     --rx_fifo_status) {
#endif
			tmp.word = ssc_load32(ssc_bus, SSC_RBUF);
			if (wide_frame) {
				client->rd_buf[transaction->idx_read * 2] =
				    tmp.bytes[1];
				client->rd_buf[transaction->idx_read * 2 + 1] =
				    tmp.bytes[0];
				dgb_print(" Reading: %c %c\n", tmp.bytes[1],
					  tmp.bytes[0]);
			} else {
				client->rd_buf[transaction->idx_read] =
				    tmp.bytes[0];
				dgb_print(" Reading: %c\n", tmp.bytes[0]);
			}
			++transaction->idx_read;
		}
#ifndef CONFIG_STM_SPI_HW_FIFO
		if ((status & SSC_STA_TIR)
		    && transaction->idx_write < transaction->msg_length) {
#else
		for (tx_fifo_status = ssc_load32(ssc_bus, SSC_TX_FSTAT);
		     tx_fifo_status < SSC_TXFIFO_SIZE - 1 &&
		     transaction->idx_write < transaction->msg_length;
		     ++tx_fifo_status) {
#endif
			if (wide_frame) {
				dgb_print(" Writing %c %c\n",
					  client->wr_buf[transaction->
							 idx_write * 2],
					  client->wr_buf[transaction->
							 idx_write * 2 + 1]);
				tmp.bytes[1] =
				    client->wr_buf[transaction->idx_write * 2];
				tmp.bytes[0] =
				    client->wr_buf[transaction->idx_write * 2 +
						   1];
			} else {
				dgb_print(" Writing %c\n",
					  client->wr_buf[transaction->
							 idx_write]);
				tmp.bytes[0] =
				    client->wr_buf[transaction->idx_write];
			}
			++transaction->idx_write;
			ssc_store32(ssc_bus, SSC_TBUF, tmp.word);
		}

		if (transaction->idx_write >= transaction->msg_length &&
		    transaction->idx_read >= transaction->msg_length)
			jump_on_fsm_complete(transaction);
		break;
	case SPI_FSM_COMPLETE:
	      be_fsm_complete:
		dgb_print(" SPI_FSM_COMPLETE\n");
		ssc_store32(ssc_bus, SSC_IEN, 0x0);
		wake_up(&(ssc_bus->wait_queue));
		break;

	case SPI_FSM_VOID:
	default:
		;
	}
	return IRQ_HANDLED;
}

#define chip_asserted(client) if ((client)->config & SPI_CSACTIVE_MASK )	\
			     stpio_set_pin((client)->pio_chip, 0x1);		\
			else stpio_set_pin((client)->pio_chip, 0x0);

#define chip_deasserted(client) if ((client)->config & SPI_CSACTIVE_MASK )	\
			       stpio_set_pin((client)->pio_chip, 0x0);		\
			  else stpio_set_pin((client)->pio_chip, 0x1);

int spi_write(struct spi_client_t *client, char *wr_buffer, size_t count)
{
	unsigned long flag;
	int timeout;
	int result = (int)count;
	struct spi_device_t *spi_bus =client->dev;
	struct spi_transaction_t transaction = {.client = client,

		.msg_length = count,
		.next_state = SPI_FSM_PREPARE,
		.idx_write = 0,
		.idx_read = 0,
	};
	dgb_print("\n");

	if (client->pio_chip == NULL)
		return -ENODATA;

	mutex_lock(&spi_bus->mutex_bus);
	chip_asserted(client);

	client->rd_buf = kmalloc(count, GFP_KERNEL);
	client->wr_buf = wr_buffer;
	if (client->config & SPI_WIDE_MASK)
		transaction.msg_length >>= 1;

	spi_bus->trns = &transaction;
	spi_state_machine(NULL,spi_bus);
	timeout = wait_event_interruptible_timeout(spi_bus->wait_queue,
						   (transaction.state == SPI_FSM_COMPLETE),
						   client->timeout * HZ);
	if (timeout <= 0) {
		/* Terminate transaction */
		local_irq_save(flag);
		transaction.next_state = SPI_FSM_COMPLETE;
		spi_state_machine(NULL,spi_bus);
		local_irq_restore(flag);

		if (!timeout) {
			printk(KERN_ERR "stm_spi: timeout during SPI write\n");
			result = -ETIMEDOUT;
		} else {
			dgb_print
			    ("stm_spi: interrupt or error in wait event\n");
			result = timeout;
		}
	}

	chip_deasserted(client);
	mutex_unlock(&spi_bus->mutex_bus);
	kfree(client->rd_buf);
	client->rd_buf = NULL;
	client->wr_buf = NULL;
	return result;
}

int spi_read(struct spi_client_t *client, char *rd_buffer, size_t count)
{
	unsigned long flag;
	int timeout;
	int result = (int)count;
	struct spi_device_t *spi_bus =client->dev;
	unsigned int wide_frame =
	    (client->config & SPI_WIDE_MASK) ? 1 : 0;
	struct spi_transaction_t transaction = {.client = client,
		.msg_length = count,
		.next_state = SPI_FSM_PREPARE,
		.idx_write = 0,
		.idx_read = 0,
	};
	/*
	 * the first step is request the bus access
	 */
	mutex_lock(&spi_bus->mutex_bus);

	chip_asserted(client);

#ifdef SPI_LOOP_DEBUG
#define DUMMY   "dummy_string_only_for_test"
	count = strlen(DUMMY);
#endif

	client->rd_buf = rd_buffer;
	client->wr_buf = (char *)kmalloc(count, GFP_KERNEL);

#ifdef SPI_LOOP_DEBUG
	strcpy(client->wr_buf, DUMMY);
#endif

/*
 *  When the data frame is 16 bits long
 *  then msg_length must be %2=0
 *
 */
	if (wide_frame)
		transaction.msg_length >>= 1;	// frame oriented
	spi_bus->trns=&transaction;
	spi_state_machine(NULL,spi_bus);
	timeout = wait_event_interruptible_timeout(spi_bus->wait_queue,
						   (transaction.state == SPI_FSM_COMPLETE),
						   client->timeout * HZ);

	if (timeout <= 0) {
		/* Terminate transaction */
		local_irq_save(flag);
		transaction.next_state = SPI_FSM_COMPLETE;
		spi_state_machine(NULL,spi_bus);
		local_irq_restore(flag);

		if (!timeout) {
			printk(KERN_ERR "stm_spi: timeout during SPI read\n");
			result = -ETIMEDOUT;
		} else {
			dgb_print
			    ("stm_spi: interrupt or error in read wait event\n");
			result = timeout;
		}
	}

	chip_deasserted(client);

	mutex_unlock(&spi_bus->mutex_bus);
	kfree(client->wr_buf);
	client->rd_buf = NULL;
	client->wr_buf = NULL;
	return result;
}

int spi_write_then_read(struct spi_client_t *client, char *wr_buffer,
			char *rd_buffer, size_t count)
{
	unsigned long flag;
	int timeout;
	int result = (int)count;
	struct spi_device_t *spi_bus =client->dev;
	struct spi_transaction_t transaction = {.client = client,
		.msg_length = count,
		.next_state = SPI_FSM_PREPARE,
		.idx_write = 0,
		.idx_read = 0,
	};
	dgb_print("\n");

	if (client->pio_chip == NULL)
		return -ENODATA;

	mutex_lock(&spi_bus->mutex_bus);

	chip_asserted(client);

	client->rd_buf = rd_buffer;
	client->wr_buf = wr_buffer;

	if (client->config & SPI_WIDE_MASK)
		transaction.msg_length >>= 1;	// frame oriented...

	spi_bus->trns=&transaction;
	spi_state_machine(NULL,spi_bus);
	timeout = wait_event_interruptible_timeout(spi_bus->wait_queue,
						  (transaction.state == SPI_FSM_COMPLETE),
						  client->timeout * HZ);
	if (timeout <= 0) {
		/* Terminate transaction */
		local_irq_save(flag);
		transaction.next_state = SPI_FSM_COMPLETE;
		spi_state_machine(NULL,spi_bus);
		local_irq_restore(flag);

		if (!timeout) {
			printk(KERN_ERR "stm_spi: timeout during SPI read\n");
			result = -ETIMEDOUT;
		} else {
			dgb_print
			    ("stm_spi: interrupt or error in read wait event\n");
			result = timeout;
		}
	}

	chip_deasserted(client);
	mutex_unlock(&spi_bus->mutex_bus);

	return count;
}

struct spi_client_t *spi_create_client(int bus_number)
{
	struct spi_client_t *client;

	dgb_print("\n");

	if (bus_number >= MAX_NUMBER_SPI_BUSSES)
		return NULL;
	if (!spi_busses_array[bus_number])
		return NULL;
	client =
	    (struct spi_client_t *)kzalloc(sizeof(struct spi_client_t),
					   GFP_KERNEL);
	if (!client)
		return NULL;
	client->dev = spi_busses_array[bus_number];
	client->timeout = 5;	/* 5 seconds */
/*
 *  1 Phase
 *  1 Polarity
 *  1 Heading
 *  - Full/Half
 *  1 Wide (16bits)
 *  0 CSActive
 *  1 MHz (at 100MHz of common clock)
 */
	client->config = 0x420017;

	return client;
}

int spi_client_release(struct spi_client_t *client)
{
	dgb_print("\n");
	if (!client)
		return 0;
	if (client->pio_chip != NULL) {
		stpio_free_pin(client->pio_chip);
		client->pio_chip = NULL;
	}
	dgb_print("PIO-chip released\n");
	if (client->rd_buf != NULL)
		kfree(client->rd_buf);
	kfree(client);
	return 1;
}

int spi_client_addressing(struct spi_client_t *client, unsigned int slave_address)
{
	unsigned int spi_device;

	spi_device = spi_get_device(slave_address);

	dgb_print("Spi opening Slave 0x%x (%d)\n", spi_device, spi_device);

/* 1. release the Pio of previous addressing*/
	if (client->pio_chip)
		stpio_free_pin(client->pio_chip);
	client->pio_chip = NULL;
// 2. check if the pio[BANK][LINE] used for chip_selector is free
	client->pio_chip =
	    stpio_request_pin(spi_get_bank(slave_address),
			      spi_get_line(slave_address), "spi-chip-selector",
			      STPIO_OUT);

	if (!(client->pio_chip)) {
/*
 * Somebody already requested the PIO[bank][line]
 * therefore we abort the addressing
 */
		dgb_print("Error Pio locked or not-exist\n");
		return -ENOSYS;
	}
	dgb_print("->with PIO [%d][%d]\n", spi_get_bank(slave_address),
		  spi_get_line(slave_address));

	client->config &= ~SPI_FULLDUPLEX;
	dgb_print("->with FULLDUPLEX = 0x%x\n", spi_get_mode(slave_address));
	client->config |= ( SPI_FULLDUPLEX * spi_get_mode(slave_address));
/*
 *  Free the data of prev addressing
 */
	if (client->rd_buf != NULL)
		kfree(client->rd_buf);

	client->rd_buf = NULL;
	chip_deasserted(client);

	return 0;

}

int spi_client_control(struct spi_client_t *client, int cmd, int arg)
{
	dgb_print("\n");
	switch (cmd) {
	case SPI_IOCTL_WIDEFRAME:
		client->config &= ~SPI_WIDE_MASK;
		if (arg)
			client->config |= SPI_WIDE_16BITS;
		break;
	case SPI_IOCTL_POLARITY:
		client->config &=  ~SPI_POLARITY_MASK;
		if (arg)
			client->config |= SPI_POLARITY_HIGH;
		break;
	case SPI_IOCTL_PHASE:
		client->config &= ~SPI_PHASE_MASK;
		if (arg)
			client->config |= SPI_PHASE_HIGH;
		break;
	case SPI_IOCTL_HEADING:
		client->config &= ~SPI_MSB_MASK;
		if (arg)
			client->config |= SPI_MSB;
		break;
	case SPI_IOCTL_BUADRATE:
		{
			unsigned long baudrate;
			baudrate = clk_get_rate(clk_get(NULL,"comms_clk")) / (2 * arg);
			client->config &= ~SPI_BAUDRATE_MASK;
			client->config |= (baudrate << SPI_BAUDRATE_SHIFT);
		}
		break;
	case SPI_IOCTL_CSACTIVE:
		client->config &= ~SPI_CSACTIVE_MASK;
		if (arg)
			client->config |= SPI_CSACTIVE_HIGH;
		break;
	case SPI_IOCTL_ADDRESS:
		if (spi_client_addressing(client, (unsigned int)arg) != 0)
			return -1;
		break;
	case SPI_IOCTL_TIMEOUT:
		client->timeout = arg;
		break;
	default:
		;
	}
#ifdef SPI_STM_DEBUG
	{
		unsigned int conf = client->config;
		dgb_print("SPI - Virtual Config:\n");
		dgb_print(" - PHASE:    0x%x\n", (conf & SPI_PHASE_MASK) != 0);
		dgb_print(" - POLARITY: 0x%x\n",
			  (conf & SPI_POLARITY_MASK) != 0);
		dgb_print(" - HEADING:  0x%x\n", (conf & SPI_MSB_MASK) != 0);
		dgb_print(" - FULLDUP:  0x%x\n",
			  (conf & SPI_FULLDUPLEX_MASK) != 0);
		dgb_print(" - WIDE:     0x%x\n", (conf & SPI_WIDE_MASK) != 0);
		dgb_print(" - CSACTIVE: 0x%x\n",
			  (conf & SPI_CSACTIVE_MASK) != 0);
		dgb_print(" - BUADRATE: 0x%x\n",
			  (conf & SPI_BAUDRATE_MASK) >> SPI_BAUDRATE_SHIFT);
	}
#endif

}

#ifdef CONFIG_STM_SPI_CHAR_DEV
#define SPI_MAJOR 153
static struct class *spi_dev_class;
static struct cdev spi_cdev;

static ssize_t spi_cdev_read(struct file *filp,
			     char __user * buff, size_t count, loff_t * offp)
{
	struct spi_client_t *client = (struct spi_client_t *)filp->private_data;
	unsigned int wide_frame =
	    (client->config & SPI_WIDE_MASK) ? 1 : 0;
	char *read_buffer;

	if (client->pio_chip == NULL)
		return -ENODATA;

	if (client->config & SPI_FULLDUPLEX_MASK) {
/*
 * In FullDuplex Mode
 * The Datas are already ready...
 */
		if (!client->rd_buf)
			return 0;
		dgb_print("Reading in FullD\n");
		if (wide_frame)
			count &= ~0x1;
		copy_to_user(buff, client->rd_buf, count);
		kfree(client->rd_buf);
		client->rd_buf = NULL;
		return count;
	}

	dgb_print("Reading in Half/D %d bytes\n", count);
	read_buffer = (char *)kmalloc(count, GFP_KERNEL);
	spi_read(client, read_buffer, count);
	copy_to_user(buff, read_buffer, count);
	kfree(read_buffer);
	return count;
}

static ssize_t spi_cdev_write(struct file *filp,
			      const char __user * buff,
			      size_t count, loff_t * offp)
{
	struct spi_client_t *client = (struct spi_client_t *)filp->private_data;
	char *wr_buffer;
	char *rd_buffer;
	int result;
	dgb_print("\n");

	wr_buffer = kmalloc(count, GFP_KERNEL);
	if (!wr_buffer)
		return -ENOMEM;
	rd_buffer = kmalloc(count, GFP_KERNEL);
	if (!rd_buffer){
		kfree(wr_buffer);
		return -ENOMEM;
	}

	copy_from_user(wr_buffer, buff, count);

	result = spi_write_then_read(client, wr_buffer, rd_buffer, count);

	if (result >= 0)
		result = count;

	if (!(client->config & SPI_FULLDUPLEX)) {
#ifdef SPI_LOOP_DEBUG
		dgb_print("Read: %s\n", rd_buffer);
#endif
		kfree(rd_buffer);
		client->rd_buf = NULL;
	}

	return result;
}

static int spi_cdev_ioctl(struct inode *inode,
			  struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	dgb_print("\n");
	spi_client_control((struct spi_client_t *)filp->private_data, cmd, arg);
	return 0;
}

static int spi_cdev_open(struct inode *inode, struct file *filp)
{
	unsigned int minor;
	struct spi_client_t *client;

	dgb_print("\n");
	minor = iminor(inode);
	client = spi_create_client(minor);
	filp->private_data = client;
	if (client)
		return 0;
	else
		return -ENODEV;
}

static int spi_cdev_release(struct inode *inode, struct file *filp)
{
	dgb_print("\n");
	spi_client_release((struct spi_client_t *)filp->private_data);
	filp->private_data = NULL;
	return 0;
}

struct file_operations spi_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = spi_cdev_open,
	.release = spi_cdev_release,
	.read = spi_cdev_read,
	.write = spi_cdev_write,
	.ioctl = spi_cdev_ioctl
};
#endif

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
	spi_busses_array[spi_dev->idx_dev] = 0;
#ifdef CONFIG_STM_SPI_CHAR_DEV
	class_device_destroy(spi_dev_class, MKDEV(SPI_MAJOR, spi_dev->idx_dev));
#endif
	kfree(spi_dev);
	return;
}

static int spi_bus_driver_probe(struct device *dev)
{
	struct spi_device_t *spi_dev;

	dgb_print("\n");
	spi_dev = container_of(dev, struct spi_device_t, dev);

	return spi_dev->dev_type == SPI_DEV_BUS_ADAPTER;
};

static void spi_bus_driver_shutdown(struct device *dev)
{
	struct spi_device_t *spi_dev;
	spi_dev = container_of(dev, struct spi_device_t, dev);
	dgb_print("\n");
	spi_del_adapter(spi_dev);
	return;
}
static struct device_driver spi_bus_drv = {
	.owner = THIS_MODULE,
	.name = "spi_bus_drv",
	.bus = &spi_bus_type,
	.probe = spi_bus_driver_probe,
	.shutdown = spi_bus_driver_shutdown,
};

int spi_add_adapter(struct spi_device_t *spi_dev)
{
	unsigned int ret;
	unsigned int idx_dev = spi_dev->idx_dev;
	struct device *dev;

	dgb_print("\n");
	spi_dev->dev_type = SPI_DEV_BUS_ADAPTER;
	spi_dev->dev.bus = &(spi_bus_type);
	sprintf(spi_dev->dev.bus_id, "spi-%d", idx_dev);
	spi_dev->dev.driver = &spi_bus_drv;
	ret = device_register(&spi_dev->dev);
	if (ret) {
		printk(KERN_WARNING "Unable to register %s bus\n",
		       spi_dev->dev.bus_id);
		kfree(spi_dev);
	} else
		spi_busses_array[idx_dev] = spi_dev;
#ifdef CONFIG_STM_SPI_CHAR_DEV
	dev = spi_dev->dev.parent;
	spi_dev->class_dev = class_device_create(spi_dev_class, NULL,
						 MKDEV(SPI_MAJOR,
						 spi_dev->idx_dev), dev,
						 "spi-%d", spi_dev->idx_dev);
#endif
	return ret;
}

static void __init spi_core_init(void)
{
	unsigned int ret;
	dgb_print("\n");
	ret = bus_register(&spi_bus_type);
	if(ret){
		printk(KERN_WARNING "Unable to register spi bus\n");
		return ;
		}
	ret = driver_register(&spi_bus_drv);
	if (ret) {
		printk(KERN_WARNING "Unable to register spi driver\n");
		return ;
        }
        printk(KERN_INFO "spi layer initialized\n");
}

#ifdef CONFIG_STM_SPI_CHAR_DEV
static void __init spi_cdev_init(void)
{
	dev_t ch_device;
	dgb_print("\n");

	spi_dev_class = class_create(THIS_MODULE, "spi-dev");
	if (IS_ERR(spi_dev_class))
		return 0;

	ch_device = MKDEV(SPI_MAJOR, 0);
	register_chrdev_region(ch_device, 255, "spi");
	cdev_init(&(spi_cdev), &(spi_fops));
	cdev_add(&(spi_cdev), ch_device, 255);
	printk(KERN_INFO "spi /dev layer initialized\n");
	return 0;
}
fs_initcall(spi_cdev_init);
#endif
#define NAME "spi_stm_probe"
static int __init spi_stm_probe(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
        struct resource *res;
	struct spi_device_t *dev;

	dev = devm_kzalloc(&pdev->dev,sizeof(struct spi_device_t), GFP_KERNEL);

	if(!dev)
		return -ENOMEM;

	if (!(res=platform_get_resource(pdev, IORESOURCE_MEM, 0)))
		return -ENODEV;
	if (!devm_request_mem_region(&pdev->dev, res->start, res->end - res->start, "spi")){
		printk(KERN_ERR NAME " Request mem 0x%x region not done\n",res->start);
		return -ENOMEM;
	}
	if (!(dev->base =
		devm_ioremap_nocache(&pdev->dev, res->start, res->end - res->start))){
		printk(KERN_ERR NAME " Request iomem 0x%x region not done\n",res->start);
		return -ENOMEM;
	}
	if (!(res=platform_get_resource(pdev, IORESOURCE_IRQ, 0))){
		printk(KERN_ERR NAME " Request irq %d not done\n",res->start);
		return -ENODEV;
	}
	if(devm_request_irq(&pdev->dev,res->start, spi_state_machine,
		IRQF_DISABLED, "spi", dev)<0){
		printk(KERN_ERR NAME " Request irq not done\n");
		return -ENODEV;
	}
	pio_info->clk = stpio_request_pin(pio_info->pio_port,pio_info->pio_pin[0],
                                "SPI Clock", STPIO_OUT);
	if(!pio_info->clk){
		printk(KERN_ERR "Faild to clk pin allocation\n");
		return -ENODEV;
	}
	pio_info->sdout = stpio_request_pin(pio_info->pio_port,pio_info->pio_pin[1],
				"SPI Data out", STPIO_OUT);
	if(!pio_info->sdout){
		printk(KERN_ERR "Faild to sda pin allocation\n");
		stpio_free_pin(pio_info->clk);
		return -ENODEV;
		}
	pio_info->sdin = stpio_request_pin(pio_info->pio_port,pio_info->pio_pin[2],
				"SPI Data in", STPIO_IN);
	if(!pio_info->sdin){
		printk(KERN_ERR "Faild to sdo pin allocation\n");
		stpio_free_pin(pio_info->sdout);
		stpio_free_pin(pio_info->clk);
		return -ENODEV;
		}

	init_waitqueue_head(&dev->wait_queue);
        mutex_init(&dev->mutex_bus);
	dev->idx_dev = pdev->id;
	dev->dev.parent = &pdev->dev;
	pdev->dev.driver_data = dev;
	if (spi_add_adapter(dev) < 0) {
		printk(KERN_ERR
			"spi/stm: The SPI Core refuses the spi/stm adapter\n");
		return -ENODEV;
	}
	return 0;
}

static int  spi_stm_remove(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
        struct resource *res;
        struct spi_device_t *dev = pdev->dev.driver_data;

	spi_del_adapter(dev);
	devm_iounmap(&pdev->dev,dev->base);
	devm_free_irq(&pdev->dev,res->start, dev);
	devm_kfree(&pdev->dev,dev);
	stpio_free_pin(pio_info->sdin);
	stpio_free_pin(pio_info->clk);
	stpio_free_pin(pio_info->sdout);
        return 0;
}

#ifdef CONFIG_PM
static int spi_stm_suspend(struct platform_device *pdev,pm_message_t state)
{
	struct spi_device_t *dev = pdev->dev.driver_data;
	return 0;
}

static int spi_stm_resume(struct platform_device *pdev)
{
	struct spi_device_t *dev = pdev->dev.driver_data;
	return 0;
}
#else
#define spi_stm_suspend		NULL
#define spi_stm_resume		NULL
#endif

static struct platform_driver spi_hw_driver = {
        .driver.name = "spi_st",
        .driver.owner = THIS_MODULE,
        .probe = spi_stm_probe,
        .remove = spi_stm_remove,
	.suspend = spi_stm_suspend,
	.resume = spi_stm_resume,
};


static int __init spi_init(void)
{
        platform_driver_register(&spi_hw_driver);
        return 0;
}

static int __exit spi_exit(void)
{
	dev_t ch_device;

	dgb_print("\n");
#ifdef CONFIG_STM_SPI_CHAR_DEV
	ch_device = MKDEV(SPI_MAJOR, 0);
	cdev_del(&(spi_cdev));
	unregister_chrdev_region(ch_device, 255);
#endif

	driver_unregister(&spi_bus_drv);
	bus_unregister(&spi_bus_type);
	return 0;
}

subsys_initcall(spi_core_init);
module_init(spi_init);
module_exit(spi_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("Module for stm spi device");
MODULE_LICENSE("GPL");
