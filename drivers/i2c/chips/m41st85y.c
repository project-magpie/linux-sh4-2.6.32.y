/* STMicroelectronics

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, wrssc to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/stm/pio.h>
#include <asm/io.h>

/* General debugging */
#undef M41ST85Y_DEBUG
#ifdef  M41ST85Y_DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define M41ST85Y_NAME		"m41st85y"
#define M41ST85Y_NREGMAP	0x14	/* no of RTC's registers */
#define M41ST85Y_ADDR		0x68	/* MY41ST85Y slave address */
#define M41ST85Y_ISOPEN		0x01	/* means /dev/rtc is in use */
#define M41ST85Y_RD		0x01	/* read flag for a i2c transfer */
#define M41ST85Y_WR		0x00	/* write flag for a i2c transfer */
#define M41ST85Y_INVALID	0xff	/* invalid value */
#define M41ST85Y_IRQ_LEVEL	0x01	/* default value. 1=High, 0=Low */
#define M41ST85Y_SQW_LEVEL	0x00	/* default value. 1=High, 0=Low */
#if defined(CONFIG_CPU_SUBTYPE_STB7100)
#define M41ST85Y_NOBUS		0x03	/* number of I2C busses */
#else
#error platform not supported.
#endif

/*
 * Addressing compliante to SPI PIO address mechanism
 * Address = [7:Not used:7][6:PIO-Port:3][2:PIO-Pin:0]
 */
#define m41st85y_get_pioport(address)	((address >> 0x03) & 0x0f)
#define m41st85y_get_piopin(address)	(address & 0x07)

struct m41st85y_s {
	struct i2c_adapter *adapter;
	long data_queue;
	wait_queue_head_t wait_queue;
	unsigned long status;
	unsigned long epoch;
	unsigned long irqp;
	spinlock_t lock;
	spinlock_t task_lock;
	rtc_task_t *task_callback;
	unsigned int cmd;
} m41st85y;

static __u8 rbuf[M41ST85Y_NREGMAP];
static __u8 wbuf[M41ST85Y_NREGMAP];
static __u32 busid = M41ST85Y_NOBUS;
static __u32 irqpio = CONFIG_SENSORS_M41ST85Y_IRQPIO, sqwpio =
    CONFIG_SENSORS_M41ST85Y_SQWPIO;
static struct stpio_pin *m41st85y_irqpio, *m41st85y_sqwpio;

static int m41st85y_transfer(struct m41st85y_s *instance,
			     __u8 * buf, __u8 len, __u8 oper, __u8 at_addr)
{
	struct i2c_msg msg[2];
	__u8 n_msg;

	if (oper == M41ST85Y_WR) {
		msg[0].addr = M41ST85Y_ADDR;
		msg[0].flags = oper;
		msg[0].len = len;
		msg[0].buf = buf;
		n_msg = 1;
	} else {
		rbuf[0] = at_addr;
		msg[0].addr = M41ST85Y_ADDR;
		msg[0].flags = M41ST85Y_WR;
		msg[0].len = 1;
		msg[0].buf = rbuf;

		msg[1].addr = M41ST85Y_ADDR;
		msg[1].flags = M41ST85Y_RD;
		msg[1].len = len;
		msg[1].buf = buf;
		n_msg = 2;
	}
	return i2c_transfer(instance->adapter, msg, n_msg);
}

static int m41st85y_power_up(void)
{
	__u8 RegsMap[M41ST85Y_NREGMAP];

	while (1) {
		m41st85y_transfer(&m41st85y, rbuf, 1, M41ST85Y_RD, 0x0F);
		if ((rbuf[0] & 0x40) == 0x00)
			break;
		printk(KERN_INFO
		       "There was an alarm during the back-up mode AF 0x%x\n",
		       rbuf[0]);
	}

	if (m41st85y_transfer(&m41st85y,
			      RegsMap + 1, M41ST85Y_NREGMAP - 1,
			      M41ST85Y_RD, 0x01) >= 0) {
		RegsMap[0x00] = 0x01;	/* address offset */
		RegsMap[0x01] &= ~0x80;	/* ST bit, wake up the oscillator */
		RegsMap[0x08] = 0x80;	/* IRQ/FT/OUT line is driven low */
		RegsMap[0x0A] &= ~0x40;	/* Swq disable */
		RegsMap[0x0C] &= ~0x40;	/* Update the TIMEKEEPER registers */
		RegsMap[0x13] &= 0x00;	/* Default Square wave output is 1Hz */
		m41st85y.irqp = 1;	/* 1Hz */
		if (m41st85y_transfer(&m41st85y,
				      RegsMap, M41ST85Y_NREGMAP,
				      M41ST85Y_WR, M41ST85Y_INVALID) < 0) {
			printk(KERN_ERR "I2C transfer write failure\n");
			return -EIO;
		}
		/* waiting RTC hardware restart */
		ssleep(1);
		return 0;
	}
	printk(KERN_ERR "I2C transfer read failure.\n");
	return -EIO;
}

static int m41st85y_alarmset(unsigned int ioctl_cmd, struct rtc_time *ltime)
{
	if ((ioctl_cmd != RTC_UIE_ON) && (ioctl_cmd != RTC_ALM_SET))
		return -1;

	if (ioctl_cmd == RTC_UIE_ON) {
		rtc_get_rtc_time(ltime);

		/* alarm update */
		wbuf[0] = 0x0A;
		wbuf[1] = BIN2BCD(ltime->tm_mon);
		wbuf[2] = 0xC0 | BIN2BCD(ltime->tm_mday);
		wbuf[3] = 0x80 | BIN2BCD(ltime->tm_hour);
		wbuf[4] = 0x80 | BIN2BCD(ltime->tm_min);
		wbuf[5] = 0x80 | BIN2BCD(ltime->tm_sec + 1);
		if (m41st85y_transfer(&m41st85y,
				      wbuf, 6, M41ST85Y_WR,
				      M41ST85Y_INVALID) >= 0) {
			wbuf[0] = 0x0A;
			wbuf[1] = (wbuf[1] | 0x80);
			DPRINTK("enable AFE writing %#x\n", wbuf[1]);
			if (m41st85y_transfer(&m41st85y,
					      wbuf, 2, M41ST85Y_WR,
					      M41ST85Y_INVALID) >= 0)
				return 0;
		}
	} else {
		if (m41st85y_transfer(&m41st85y,
				      rbuf, 6, M41ST85Y_RD, 0x0A) >= 0) {
			/* alarm update */
			wbuf[0] = 0x0A;
			wbuf[1] = (rbuf[0] & 0xE0) | BIN2BCD(ltime->tm_mon);
			wbuf[2] = (rbuf[1] & 0xC0) | BIN2BCD(ltime->tm_mday);
			wbuf[3] = (rbuf[2] & 0xC0) | BIN2BCD(ltime->tm_hour);
			wbuf[4] = (rbuf[3] & 0x80) | BIN2BCD(ltime->tm_min);
			wbuf[5] = (rbuf[4] & 0x80) | BIN2BCD(ltime->tm_sec);
			DPRINTK("writing alarm date\n");
			if (m41st85y_transfer(&m41st85y,
					      wbuf, 6, M41ST85Y_WR,
					      M41ST85Y_INVALID) >= 0)
				return 0;
		}
	}
	return -EIO;
}

void m41st85y_handler(struct stpio_pin *pin, void *dev)
{
	struct m41st85y_s *instance = dev;
	char skip = 0;

	stpio_disable_irq(pin);

	if ((instance->cmd == RTC_PIE_ON) || (instance->cmd == RTC_UIE_ON)) {
		if (stpio_get_pin(pin) == M41ST85Y_IRQ_LEVEL) {
			skip = 1;
			stpio_enable_irq(pin, M41ST85Y_IRQ_LEVEL);
		} else
			stpio_enable_irq(pin, !M41ST85Y_IRQ_LEVEL);
	}

	if (!skip) {
		/* Now do the rest of the actions */
		spin_lock(&instance->task_lock);
		if (instance->task_callback)
			instance->task_callback->func(instance->task_callback->
						      private_data);
		spin_unlock(&instance->task_lock);

		instance->data_queue++;
		wake_up(&instance->wait_queue);
	}
}

static int m41st85y_ioctl(struct inode *inode,
			  struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct rtc_time ltime;

	memset(&ltime, 0, sizeof(struct rtc_time));
	m41st85y.cmd = cmd;

	switch (cmd) {
	case RTC_UIE_OFF:	/* Mask ints from RTC updates.  */
		DPRINTK("RTC_UIE_OFF\n");
	case RTC_AIE_OFF:	/* Mask alarm int. enab. bit    */
		DPRINTK("RTC_AIE_OFF\n");
	case RTC_AIE_ON:	/* Allow alarm interrupts.      */
		{
			DPRINTK("RTC_AIE_ON\n");

			/* reading AFE bits */
			if (m41st85y_transfer(&m41st85y,
					      rbuf, 1, M41ST85Y_RD, 0x0A) >= 0)
			{
				char n_data = 2;

				wbuf[0] = 0x0A;
				if (cmd == RTC_AIE_ON) {
					stpio_enable_irq(m41st85y_irqpio,
							 M41ST85Y_IRQ_LEVEL);
					wbuf[1] = (rbuf[0] | 0x80);
				} else {
					stpio_disable_irq(m41st85y_irqpio);
					m41st85y.cmd = 0;	/* disable status */
					n_data = 6;
					wbuf[1] = (rbuf[0] & ~0xA0);	/* disabling AFE flag bit */
					wbuf[2] = wbuf[3] = wbuf[4] = wbuf[5] = 0x00;	/* disabling RPT5-RPT1 */
				}

				DPRINTK(" writing AFE %#x\n", wbuf[1]);
				if (m41st85y_transfer(&m41st85y,
						      wbuf, n_data, M41ST85Y_WR,
						      M41ST85Y_INVALID) >= 0) {
					return 0;
				}
			}
			return -EIO;
		}
	case RTC_PIE_OFF:	/* Mask periodic int. enab. bit */
		DPRINTK("RTC_PIE_OFF\n");
	case RTC_PIE_ON:	/* Allow periodic ints          */
		{
			DPRINTK("RTC_PIE_ON\n");
			if (m41st85y_transfer(&m41st85y,
					      rbuf, 1, M41ST85Y_RD, 0x0A) >= 0)
			{
				wbuf[0] = 0x0A;
				if (cmd == RTC_PIE_OFF) {
					stpio_disable_irq(m41st85y_sqwpio);
					wbuf[1] = (rbuf[0] & ~0x40);
				} else {
					stpio_enable_irq(m41st85y_sqwpio,
							 M41ST85Y_SQW_LEVEL);
					wbuf[1] = (rbuf[0] | 0x40);
				}

				DPRINTK("writing on SWQE %#x\n", wbuf[1]);
				if (m41st85y_transfer(&m41st85y,
						      wbuf, 2, M41ST85Y_WR,
						      M41ST85Y_INVALID) >= 0)
					return 0;
			}
			return -EIO;
		}
	case RTC_UIE_ON:	/* Allow ints for RTC updates. (one per second) */
		{
			DPRINTK("RTC_UIE_ON\n");
			stpio_enable_irq(m41st85y_irqpio, M41ST85Y_IRQ_LEVEL);
			return m41st85y_alarmset(cmd, &ltime);
		}
	case RTC_ALM_READ:	/* Read the present alarm time */
		{
			DPRINTK("RTC_ALM_READ\n");
			if (m41st85y_transfer(&m41st85y,
					      rbuf, 6, M41ST85Y_RD, 0x0A) >= 0)
			{
				ltime.tm_mon = BCD2BIN(rbuf[0] & 0x1f);
				ltime.tm_mday = BCD2BIN(rbuf[1] & 0x3f);
				ltime.tm_hour = BCD2BIN(rbuf[2] & 0x3f);
				ltime.tm_min = BCD2BIN(rbuf[3] & 0x7f);
				ltime.tm_sec = BCD2BIN(rbuf[4] & 0x7f);
				return copy_to_user((void __user *)arg,
						    &ltime,
						    sizeof ltime) ? -EFAULT : 0;
			}
			return -EIO;
		}
	case RTC_ALM_SET:	/* Store a time into the alarm */
		{
			DPRINTK("RTC_ALM_SET\n");
			if (copy_from_user
			    (&ltime, (struct rtc_time __user *)arg,
			     sizeof ltime))
				return -EFAULT;
			return m41st85y_alarmset(cmd, &ltime);
		}
	case RTC_RD_TIME:	/* Read the time/date from RTC  */
		{
			DPRINTK("RTC_RD_TIME\n");
			rtc_get_rtc_time(&ltime);
			return copy_to_user((void __user *)arg,
					    &ltime, sizeof ltime) ? -EFAULT : 0;
		}
	case RTC_SET_TIME:	/* Set the RTC */
		{
			DPRINTK("RTC_SET_TIME\n");
			if (copy_from_user
			    (&ltime, (struct rtc_time __user *)arg,
			     sizeof ltime))
				return -EFAULT;

			if (m41st85y_transfer(&m41st85y,
					      rbuf, 8, M41ST85Y_RD, 0x00) >= 0)
			{
				/* time update */
				wbuf[0] = 0x00;
				wbuf[1] = 0x00;
				wbuf[2] =
				    (rbuf[1] & 0x80) | BIN2BCD(ltime.tm_sec);
				wbuf[3] =
				    (rbuf[2] & 0x80) | BIN2BCD(ltime.tm_min);
				wbuf[4] =
				    (rbuf[3] & 0xC0) | BIN2BCD(ltime.tm_hour);
				memcpy(&wbuf[5], &rbuf[4], sizeof(char));
				wbuf[6] =
				    (rbuf[5] & 0xC0) | BIN2BCD(ltime.tm_mday);
				wbuf[7] =
				    (rbuf[6] & 0xE0) | BIN2BCD(ltime.tm_mon);
				wbuf[8] =
				    BIN2BCD((ltime.tm_year - m41st85y.epoch));

				if (m41st85y_transfer(&m41st85y,
						      wbuf, 9, M41ST85Y_WR,
						      M41ST85Y_INVALID) >= 0)
					return 0;
			}
			return -EIO;
		}
	case RTC_IRQP_READ:	/* Read the periodic IRQ rate.  */
		{
			DPRINTK("RTC_IRQP_READ\n");
			return put_user(m41st85y.irqp,
					(unsigned long __user *)arg);
		}
	case RTC_IRQP_SET:	/* Set periodic IRQ rate.       */
		{
			DPRINTK("RTC_IRQP_SET\n");
			wbuf[0] = 0x13;
			switch (arg) {
			case 0:
				wbuf[1] = 0x00;
				break;
			case 1:
				wbuf[1] = 0xF0;
				break;
			case 2:
				wbuf[1] = 0xE0;
				break;
			case 4:
				wbuf[1] = 0xD0;
				break;
			case 8:
				wbuf[1] = 0xC0;
				break;
			case 16:
				wbuf[1] = 0xB0;
				break;
			case 32:
				wbuf[1] = 0xA0;
				break;
			case 64:
				wbuf[1] = 0x90;
				break;
			case 128:
				wbuf[1] = 0x80;
				break;
			case 256:
				wbuf[1] = 0x70;
				break;
			case 512:
				wbuf[1] = 0x60;
				break;
			case 1024:
				wbuf[1] = 0x50;
				break;
			case 2048:
				wbuf[1] = 0x40;
				break;
			case 4096:
				wbuf[1] = 0x30;
				break;
			case 8192:
				wbuf[1] = 0x20;
				break;
			case 32768:
				wbuf[1] = 0x10;
				break;
			default:
				return -1;
			}

			if (m41st85y_transfer(&m41st85y,
					      wbuf, 2, M41ST85Y_WR,
					      M41ST85Y_INVALID) >= 0)
				return 0;
			return -EIO;
		}
	case RTC_EPOCH_READ:	/* Read the epoch.      */
		{
			DPRINTK("RTC_EPOCH_READ\n");
			return put_user(m41st85y.epoch,
					(unsigned long __user *)arg);
		}
	case RTC_EPOCH_SET:	/* Set the epoch.       */
		{
			DPRINTK("RTC_EPOCH_SET\n");
			copy_from_user(&m41st85y.epoch, (void *)arg,
				       sizeof(long));
			return 0;
		}
	default:
		return -ENOTTY;
	}
}

static int m41st85y_open(struct inode *inode, struct file *file)
{
	int lerrno = -EBUSY;

	spin_lock_irq(&m41st85y.lock);
	if (m41st85y.status & M41ST85Y_ISOPEN) {
		spin_unlock_irq(&m41st85y.lock);
		return lerrno;
	}

	m41st85y.status |= M41ST85Y_ISOPEN;
	m41st85y.data_queue = 0;
	spin_unlock_irq(&m41st85y.lock);
	return 0;
}

static int m41st85y_close(struct inode *inode, struct file *file)
{
	spin_lock_irq(&m41st85y.lock);
	m41st85y.status &= ~M41ST85Y_ISOPEN;
	spin_unlock_irq(&m41st85y.lock);
	return 0;
}

static ssize_t m41st85y_read(struct file *filp, char __user * buff,
			     size_t count, loff_t * offp)
{
	wait_event(m41st85y.wait_queue, m41st85y.data_queue != 0);
	m41st85y.data_queue = 0;

	if ((m41st85y.cmd == RTC_AIE_ON) || (m41st85y.cmd == RTC_UIE_ON)) {
		while (1) {
			m41st85y_transfer(&m41st85y,
					  rbuf, 1, M41ST85Y_RD, 0x0F);

			DPRINTK("AF 0x%x\n", rbuf[0]);
			if ((rbuf[0] & 0x40) == 0x00)
				break;
		}
	}
	copy_to_user(buff, &m41st85y.data_queue, sizeof(m41st85y.data_queue));
	return sizeof(m41st85y.data_queue);
}

/*
 * exported stuffs
 */

EXPORT_SYMBOL(rtc_register);
EXPORT_SYMBOL(rtc_unregister);
EXPORT_SYMBOL(rtc_control);

int rtc_register(rtc_task_t * task)
{
	if (task == NULL || task->func == NULL)
		return -EINVAL;
	spin_lock_irq(&m41st85y.lock);
	if (m41st85y.status & M41ST85Y_ISOPEN) {
		spin_unlock_irq(&m41st85y.lock);
		return -EBUSY;
	}
	spin_lock(&m41st85y.task_lock);
	if (m41st85y.task_callback) {
		spin_unlock(&m41st85y.task_lock);
		spin_unlock_irq(&m41st85y.lock);
		return -EBUSY;
	}

	m41st85y.status |= M41ST85Y_ISOPEN;
	m41st85y.task_callback = task;
	spin_unlock(&m41st85y.task_lock);
	spin_unlock_irq(&m41st85y.lock);
	return 0;
}

int rtc_control(rtc_task_t * task, unsigned int cmd, unsigned long arg)
{
	spin_lock_irq(&m41st85y.task_lock);
	if (m41st85y.task_callback != task) {
		spin_unlock_irq(&m41st85y.task_lock);
		return -ENXIO;
	}
	spin_unlock_irq(&m41st85y.task_lock);
	return m41st85y_ioctl(NULL, NULL, cmd, arg);
}

int rtc_unregister(rtc_task_t * task)
{
	spin_lock_irq(&m41st85y.lock);
	spin_lock(&m41st85y.task_lock);

	if (m41st85y.task_callback != task) {
		spin_unlock(&m41st85y.task_lock);
		spin_unlock_irq(&m41st85y.lock);
		return -ENXIO;
	}
	m41st85y.task_callback = NULL;

	/* diasbilng the RTC's AIE, UIE and PIE control */
	if (m41st85y_transfer(&m41st85y, rbuf, 1, M41ST85Y_RD, 0x0A) >= 0) {
		wbuf[0] = 0x0A;
		wbuf[1] = rbuf[0] & ~0xC0;
		if (m41st85y_transfer(&m41st85y,
				      wbuf, 2, M41ST85Y_WR,
				      M41ST85Y_INVALID) >= 0) {
			m41st85y.status &= ~M41ST85Y_ISOPEN;
			spin_unlock(&m41st85y.task_lock);
			spin_unlock_irq(&m41st85y.lock);
			return 0;
		}
	}

	spin_unlock(&m41st85y.task_lock);
	spin_unlock_irq(&m41st85y.lock);
	return -EIO;
}

void rtc_get_rtc_time(struct rtc_time *ltime)
{
	if (ltime == NULL)
		return;

	memset(ltime, 0, sizeof(struct rtc_time));

	if (m41st85y_transfer(&m41st85y, rbuf, 9, M41ST85Y_RD, 0x00) >= 0) {
		ltime->tm_sec = BCD2BIN(rbuf[1] & 0x7f);
		ltime->tm_min = BCD2BIN(rbuf[2] & 0x7f);
		ltime->tm_hour = BCD2BIN(rbuf[3] & 0x3f);
		ltime->tm_wday = BCD2BIN(rbuf[4] & 0x07);
		ltime->tm_mday = BCD2BIN(rbuf[5] & 0x3f);
		ltime->tm_mon = BCD2BIN(rbuf[6] & 0x1f);
		ltime->tm_year = BCD2BIN(rbuf[7]);
	}
}

/*
 * The various file operations we support.
 */

static struct file_operations m41st85y_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = m41st85y_read,
	.poll = NULL /* ??? m41st85y_poll */ ,
	.ioctl = m41st85y_ioctl,
	.open = m41st85y_open,
	.release = m41st85y_close,
	.fasync = NULL /* ??? m41st85y_fasync */ ,
};

static struct miscdevice m41st85y_dev = {
	RTC_MINOR,
	M41ST85Y_NAME,
	&m41st85y_fops
};

static int __init m41st85y_init(void)
{
	int i = M41ST85Y_NOBUS;
	struct i2c_msg msg[1];

	if ((irqpio != M41ST85Y_INVALID) && (sqwpio != M41ST85Y_INVALID)) {
		msg[0].addr = M41ST85Y_ADDR;
		msg[0].flags = M41ST85Y_WR;
		msg[0].len = 0;
		msg[0].buf = NULL;
		if (busid < M41ST85Y_NOBUS) {
			DPRINTK("busid[%d]\n", busid);
			m41st85y.adapter = i2c_get_adapter(busid);
			if (i2c_transfer(m41st85y.adapter, msg, 1) >= 0)
				i = busid;
		} else {
			/* searching in which bus is the RTC */
			for (i = 0; i < M41ST85Y_NOBUS; i++) {
				int res;

				m41st85y.adapter = i2c_get_adapter(i);
				if ((res =
				     i2c_transfer(m41st85y.adapter, msg,
						  1)) >= 0)
					break;
				DPRINTK("RTC device isn't on I2C-%d=%d\n", i,
					res);
			}
		}
		if (i != M41ST85Y_NOBUS) {
			DPRINTK("RTC device is on I2C-%d=yes\n", i);

			if (m41st85y_power_up() >= 0) {
				if (misc_register(&m41st85y_dev) == 0) {
					spin_lock_init(&m41st85y.lock);
					spin_lock_init(&m41st85y.task_lock);
					init_waitqueue_head(&
							    (m41st85y.
							     wait_queue));
					m41st85y.cmd = 0;	/* none */
					m41st85y.epoch = 1900;	/* default value on Linux */
					printk(KERN_INFO
					       "RTC-IRQ line plugged on PIO[%d,%d]\n",
					       m41st85y_get_pioport(irqpio),
					       m41st85y_get_piopin(irqpio));
					printk(KERN_INFO
					       "RTC-SQW line plugged on PIO[%d,%d]\n",
					       m41st85y_get_pioport(sqwpio),
					       m41st85y_get_piopin(sqwpio));

					if ((m41st85y_irqpio =
					     stpio_request_pin
					     (m41st85y_get_pioport(irqpio),
					      m41st85y_get_piopin(irqpio),
					      M41ST85Y_NAME,
					      STPIO_BIDIR_Z1)) != NULL) {
						if ((m41st85y_sqwpio =
						     stpio_request_pin
						     (m41st85y_get_pioport
						      (sqwpio),
						      m41st85y_get_piopin
						      (sqwpio), M41ST85Y_NAME,
						      STPIO_IN)) != NULL) {
							stpio_request_irq
							    (m41st85y_irqpio,
							     M41ST85Y_IRQ_LEVEL,
							     m41st85y_handler,
							     (void *)&m41st85y);
							stpio_request_irq
							    (m41st85y_sqwpio,
							     M41ST85Y_SQW_LEVEL,
							     m41st85y_handler,
							     (void *)&m41st85y);

							printk(KERN_INFO
							       "STMicroelectronics M41ST85Y RTC Driver up I2C-%d initialized\n",
							       i);
							return 0;
						} else
							stpio_free_pin
							    (m41st85y_irqpio);
					}
					printk(KERN_ERR
					       "RTC request irq failure.\n");
					misc_deregister(&m41st85y_dev);
				} else
					printk(KERN_ERR
					       "RTC driver registration failure.\n");
			}
		} else
			printk(KERN_ERR
			       "I2C adapter layer initialization failure.\n");
	} else
		printk(KERN_ERR
		       "PIOs input values are required. irqpio=0Xxx sqwpio=0Xxx\n");

	printk(KERN_ERR
	       "STMicroelectronics M41ST85Y RTC initialization failure.\n");
	return -ENODEV;
}

static void __exit m41st85y_exit(void)
{
	stpio_free_irq(m41st85y_irqpio);
	stpio_free_pin(m41st85y_irqpio);
	stpio_free_irq(m41st85y_sqwpio);
	stpio_free_pin(m41st85y_sqwpio);

	misc_deregister(&m41st85y_dev);
	printk(KERN_INFO "STMicroelectronics M41ST85Y RTC Driver released.\n");
}

module_param(busid, uint, 0644);
module_param(irqpio, uint, 0644);
module_param(sqwpio, uint, 0644);
module_init(m41st85y_init);
module_exit(m41st85y_exit);
MODULE_PARM_DESC(busid, "I2C bus ID");
MODULE_PARM_DESC(irqpio, "PIO port/pin for RTC-IRQ line");
MODULE_PARM_DESC(busid, "PIO port/pin for RTC-SWQ line");
MODULE_DESCRIPTION("External RTC upon I2C");
MODULE_LICENSE("GPL");
