/*
 * aotom_main.c
 *
 * (c) 2010 Spider-Team
 * (c) 2011 oSaoYa
 * (c) 2012-1012 Stefan Seyfried
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/*
 * fulan front panel driver.
 *
 * Devices:
 *	- /dev/vfd (vfd ioctls and read/write function)
 *
 */


#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/termbits.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
/* for rtc / reboot_notifier hooks */
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

#include "aotom_main.h"
#include "utf.h"

static short paramDebug = 0;
#define TAGDEBUG "[aotom] "

#define dprintk(level, x...) do { \
if ((paramDebug) && (paramDebug > level)) printk(TAGDEBUG x); \
} while (0)

#define DISPLAYWIDTH_MAX  8

#define INVALID_KEY      -1

#define NO_KEY_PRESS     -1
#define KEY_PRESS_DOWN    1
#define KEY_PRESS_UP      0

static char *gmt = "+0000";

#define NAME_NODE "display_type"
#define NAME_DIR "aotom"

struct proc_dir_entry *own_proc_dir;
struct proc_dir_entry *own_proc_node;

typedef struct {
	int minor;
	int open_count;
} tFrontPanelOpen;

#define FRONTPANEL_MINOR_VFD    0
#define FRONTPANEL_MINOR_RC     1
#define LASTMINOR               2

static tFrontPanelOpen FrontPanelOpen [LASTMINOR];

typedef struct {
	int state;
	int period;
	int stop;
	struct task_struct *led_task;
	struct semaphore led_sem;
} tLedState;

static tLedState led_state[LASTLED];

#define BUFFERSIZE              256    //must be 2 ^ n

struct receive_s {
	int len;
	unsigned char buffer[BUFFERSIZE];
};

#define cMaxReceiveQueue	100
static wait_queue_head_t   wq;

static struct receive_s receive[cMaxReceiveQueue];
static int receiveCount = 0;

static struct semaphore 	   write_sem;
static struct semaphore 	   receive_sem;
static struct semaphore 	   draw_thread_sem;

#define RTC_NAME "aotom-rtc"
static struct platform_device *rtc_pdev;

unsigned char ASCII[48][2] =
{
	{0xF1, 0x38},	//A
	{0x74, 0x72},	//B
	{0x01, 0x68},	//C
	{0x54, 0x72},	//D
	{0xE1, 0x68},	//E
	{0xE1, 0x28},	//F
	{0x71, 0x68},	//G
	{0xF1, 0x18},	//H
	{0x44, 0x62},	//I
	{0x45, 0x22},	//J
	{0xC3, 0x0C},	//K
	{0x01, 0x48},	//L
	{0x51, 0x1D},	//M
	{0x53, 0x19},	//N
	{0x11, 0x78},	//O
	{0xE1, 0x38},	//P
	{0x13, 0x78},	//Q
	{0xE3, 0x38},	//R
	{0xF0, 0x68},	//S
	{0x44, 0x22},	//T
	{0x11, 0x58},	//U
	{0x49, 0x0C},	//V
	{0x5B, 0x18},	//W
	{0x4A, 0x05},	//X
	{0x44, 0x05},	//Y
	{0x48, 0x64},	//Z
	/* A--Z  */

	{0x01, 0x68},
	{0x42, 0x01},
	{0x10, 0x70},	//
	{0x43, 0x09},	//
	{0xE0, 0x00},	//
	{0xEE, 0x07},	//
	{0xE4, 0x02},	//
	{0x50, 0x00},	//
	{0xE0, 0x00},	//
	{0x05, 0x00},	//
	{0x48, 0x04},	//

	{0x11, 0x78},	//
	{0x44, 0x02},	//
	{0xE1, 0x70},	//
	{0xF0, 0x70},	//
	{0xF0, 0x18},	//
	{0xF0, 0x68},	//
	{0xF1, 0x68},	//
	{0x10, 0x30},	//
	{0xF1, 0x78},	//
	{0xF0, 0x78},	//
	/* 0--9  */
	{0x0, 0x0}
};

static int VFD_Show_Time(u8 hh, u8 mm)
{
	if( (hh > 24) || (mm > 59))
	{
		dprintk(2, "%s bad parameter!\n", __func__);
		return -1;
	}

	return YWPANEL_FP_SetTime(hh*3600 + mm*60);
}

static int VFD_Show_Ico(LogNum_T log_num, int log_stat)
{
	return YWPANEL_VFD_ShowIco(log_num, log_stat);
}

static struct task_struct *draw_task = 0;
static int draw_thread_stop  = 1;

int aotomSetIcon(int which, int on);

void clear_display(void)
{
	YWPANEL_VFD_ShowString("        ");
}

static void VFD_set_all_icons(void)
{
	int i;

	for(i=1; i <= 45; i++)
		aotomSetIcon(i, 1);
}

static void VFD_clear_all_icons(void)
{
	int i;

	for(i=1; i <= 45; i++)
		aotomSetIcon(i, 0);
}

static void VFD_clr(void)
{
	YWPANEL_VFD_ShowTimeOff();
	clear_display();
	VFD_clear_all_icons();
}

static int draw_thread(void *arg)
{
	struct vfd_ioctl_data *data = (struct vfd_ioctl_data *) arg;
	char buf[sizeof(data->data) + 0 * 2 * DISPLAYWIDTH_MAX];
	int len = data->length;
	int off = 0;
	int doton3 = 0;
	
	if (YWPANEL_width == 4 && len == 5 && data->data[2] == '.')
		doton3 = 1;

	if (0 && len > YWPANEL_width + doton3) {
 		memset(buf, ' ', sizeof(buf));
		off = YWPANEL_width - 1;
		memcpy(buf + off, data->data, len);
		len += off;
		buf[len + YWPANEL_width] = 0;
	} else {
		memcpy(buf, data->data, len);
		buf[len] = 0;
	}

	draw_thread_stop = 0;

	if(len > YWPANEL_width + doton3) {
		int pos;
		int i;
		YWPANEL_VFD_ShowString(buf);
		// sleep ~500 ms before scrolling
		for (i = 0; i < 12; i++) {
			if(kthread_should_stop()) {
				draw_thread_stop = 1;
				return 0;
			}
			msleep(40);
		}
		for(pos = 1; pos <= len - YWPANEL_width; pos++) {
			if(kthread_should_stop()) {
				draw_thread_stop = 1;
				return 0;
			}
			YWPANEL_VFD_ShowString(buf + pos);
		// sleep 200 ms
			for (i = 0; i < 5; i++) {
				if(kthread_should_stop()) {
					draw_thread_stop = 1;
					return 0;
				}
				msleep(40);
			}
		}
		// sleep ~500 ms after scrolling
		for (i = 0; i < 12; i++) {
			if(kthread_should_stop()) {
				draw_thread_stop = 1;
				return 0;
			}
			msleep(40);
		}
	}

	if(len > 0)
		YWPANEL_VFD_ShowString(buf + off);
	else
		clear_display();

	draw_thread_stop = 1;
	return 0;
}

static int led_thread(void *arg)
{
	int led = (int) arg;
	// toggle LED status for a given time period

	led_state[led].stop = 0;

	while(!kthread_should_stop()) {
		if (!down_interruptible(&led_state[led].led_sem)) {
			if (kthread_should_stop())
				break;
			while (!down_trylock(&led_state[led].led_sem)); // make sure semaphore is at 0
			YWPANEL_VFD_SetLed(led, led_state[led].state ? LOG_OFF : LOG_ON);
			while ((led_state[led].period > 0) && !kthread_should_stop()) {
				msleep(10);
				led_state[led].period -= 10;
			}
			// switch LED back to manually set state
			YWPANEL_VFD_SetLed(led, led_state[led].state);
		}
	}
	led_state[led].stop = 1;
		led_state[led].led_task = 0;
	return 0;
}

static struct vfd_ioctl_data last_draw_data;

int run_draw_thread(struct vfd_ioctl_data *draw_data)
{
	if(down_interruptible (&draw_thread_sem))
		return -ERESTARTSYS;

	// return if there's already a draw task running for the same text
	if(!draw_thread_stop && draw_task && (last_draw_data.length == draw_data->length)
	&& !memcmp(&last_draw_data.data, draw_data->data, draw_data->length)) {
		up(&draw_thread_sem);
	return 0;
	}

	memcpy(&last_draw_data, draw_data, sizeof(struct vfd_ioctl_data));

	// stop existing thread, if any
	if(!draw_thread_stop && draw_task) {
	kthread_stop(draw_task);
	while(!draw_thread_stop)
		msleep(1);
	draw_task = 0;
	}

	if (draw_data->length < YWPANEL_width) {
	char buf[DISPLAYWIDTH_MAX];
	memset(buf, ' ', sizeof(buf));
	if (draw_data->length)
		memcpy(buf, draw_data->data, draw_data->length);
	YWPANEL_VFD_ShowString(buf);
	} else {
	draw_thread_stop = 2;
	draw_task = kthread_run(draw_thread,draw_data,"draw thread");

	//wait until thread has copied the argument
	while(draw_thread_stop == 2)
		msleep(1);
	}
	up(&draw_thread_sem);
	return 0;
}

static int AOTOMfp_Get_Key_Value(void)
{
	int ret, key_val;

	ret =  YWPANEL_VFD_GetKeyValue();

	switch(ret) {
	case 105:
		key_val = KEY_LEFT;
		break;
	case 103:
		key_val = KEY_UP;
		break;
	case 28:
		key_val = KEY_OK;
		break;
	case 106:
		key_val = KEY_RIGHT;
		break;
	case 108:
		key_val = KEY_DOWN;
		break;
	case 88:
		key_val = KEY_POWER;
		break;
	case 102:
		key_val = KEY_MENU;
		break;
	case 48:
		key_val = KEY_EXIT;
		break;
		default:
		key_val = INVALID_KEY;
		break;
	}

	return key_val;
}

int aotomSetTime(char* time)
{
	int res = 0;

	dprintk(5, "%s >\n", __func__);
	dprintk(5, "%s time: %02d:%02d\n", __func__, time[2], time[3]);

	res = VFD_Show_Time(time[2], time[3]);
#if defined(SPARK) || defined(SPARK7162)
	YWPANEL_FP_ControlTimer(true);
#endif
	dprintk(5, "%s <\n", __func__);
	return res;
}

int vfd_init_func(void)
{
	dprintk(5, "%s >\n", __func__);
	printk("Fulan VFD module initializing\n");
	return 0;
}

int aotomSetIcon(int which, int on)
{
	int  res = 0;

	dprintk(5, "%s > %d, %d\n", __func__, which, on);
	if (which < 1 || which > 45)
	{
		printk("VFD/AOTOM icon number out of range %d\n", which);
		return -EINVAL;
	}

	which-=1;
	res = VFD_Show_Ico(((which/15)+11)*16+(which%15)+1, on);

	dprintk(10, "%s <\n", __func__);

	return res;
}

/* export for later use in e2_proc */
EXPORT_SYMBOL(aotomSetIcon);

static ssize_t AOTOMdev_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char* kernel_buf;
	int res = 0;

	struct vfd_ioctl_data data;

	dprintk(5, "%s > (len %d, offs %d)\n", __func__, len, (int) *off);

	if (((tFrontPanelOpen *)(filp->private_data))->minor != FRONTPANEL_MINOR_VFD) {
		printk("Error Bad Minor\n");
		return -EOPNOTSUPP;
	}

	kernel_buf = kmalloc(len, GFP_KERNEL);

	if (kernel_buf == NULL) {
		printk("%s return no mem<\n", __func__);
		return -ENOMEM;
	}
	copy_from_user(kernel_buf, buff, len);

	if (len > sizeof(data.data))
		data.length = sizeof(data.data);
	else
		data.length = len;

	while ((data.length > 0) && (kernel_buf[data.length - 1 ] == '\n'))
	  data.length--;

	if (data.length > sizeof(data.data))
		len = data.length = sizeof(data.data);

	memcpy(data.data, kernel_buf, data.length);
	res = run_draw_thread(&data);

	kfree(kernel_buf);

	dprintk(10, "%s < res %d len %d\n", __func__, res, len);

	if (res < 0)
		return res;
	return len;
}

static void flashLED(int led, int ms) {
	if (!led_state[led].led_task || ms < 1)
		return;
	led_state[led].period = ms;
	up(&led_state[led].led_sem);
}

static ssize_t AOTOMdev_read(struct file *filp, char __user *buff, size_t len, loff_t *off)
{
	dprintk(5, "%s > (len %d, offs %d)\n", __func__, len, (int) *off);

	if (((tFrontPanelOpen*)(filp->private_data))->minor == FRONTPANEL_MINOR_RC)
	{
		while (receiveCount == 0)
		{
			if (wait_event_interruptible(wq, receiveCount > 0))
				return -ERESTARTSYS;
		}
		flashLED(LED_GREEN, 100);

		/* 0. claim semaphore */
		if (down_interruptible(&receive_sem))
			return -ERESTARTSYS;

		/* 1. copy data to user */
		copy_to_user(buff, receive[0].buffer, receive[0].len);

		/* 2. copy all entries to start and decrease receiveCount */
		receiveCount--;
		memmove(&receive[0], &receive[1], (cMaxReceiveQueue - 1) * sizeof(struct receive_s));

		/* 3. free semaphore */
		up(&receive_sem);

		return receive[0].len;
	}
	return -EOPNOTSUPP;
}

static int AOTOMdev_open(struct inode *inode, struct file *filp)
{
	int minor;
	dprintk(5, "%s >\n", __func__);
	minor = MINOR(inode->i_rdev);
	dprintk(1, "open minor %d\n", minor);

	if (minor == FRONTPANEL_MINOR_RC && FrontPanelOpen[minor].open_count) {
		printk("EUSER\n");
		return -EUSERS;
	}

	FrontPanelOpen[minor].open_count++;
	filp->private_data = &FrontPanelOpen[minor];

	dprintk(5, "%s <\n", __func__);
	return 0;
}

static int AOTOMdev_close(struct inode *inode, struct file *filp)
{
	int minor;
	dprintk(5, "%s >\n", __func__);
	minor = MINOR(inode->i_rdev);
	dprintk(1, "close minor %d\n", minor);

	if (FrontPanelOpen[minor].open_count > 0)
		FrontPanelOpen[minor].open_count--;

	dprintk(5, "%s <\n", __func__);
	return 0;
}

static struct aotom_ioctl_data aotom_data;
static struct vfd_ioctl_data vfd_data;

static int AOTOMdev_ioctl(struct inode *Inode, struct file *File, unsigned int cmd, unsigned long arg)
{
	int icon_nr = 0;
	static int mode = 0;
	int res = -EINVAL;
	dprintk(5, "%s > 0x%.8x\n", __func__, cmd);

	if(down_interruptible (&write_sem))
		return -ERESTARTSYS;

	switch(cmd) {
	case VFDSETMODE:
	case VFDSETLED:
	case VFDICONDISPLAYONOFF:
	case VFDSETTIME:
	case VFDBRIGHTNESS:
		if (copy_from_user(&aotom_data, (void *) arg, sizeof(aotom_data)))
			return -EFAULT;
	}

	switch(cmd) {
	case VFDSETMODE:
		mode = aotom_data.u.mode.compat;
		break;
	case VFDSETLED:
#if defined(SPARK) || defined(SPARK7162)
		if (aotom_data.u.led.led_nr > -1 && aotom_data.u.led.led_nr < LED_MAX) {
			switch (aotom_data.u.led.on) {
			case LOG_OFF:
			case LOG_ON:
				res = YWPANEL_VFD_SetLed(aotom_data.u.led.led_nr, aotom_data.u.led.on);
				led_state[aotom_data.u.led.led_nr].state = aotom_data.u.led.on;
				break;
			default: // toggle (for aotom_data.u.led.on * 10) ms
				flashLED(aotom_data.u.led.led_nr, aotom_data.u.led.on * 10);
			}
		}
#endif
		break;
	case VFDBRIGHTNESS:
		if (aotom_data.u.brightness.level < 0)
			aotom_data.u.brightness.level = 0;
		else if (aotom_data.u.brightness.level > 7)
			aotom_data.u.brightness.level = 7;
		res = YWPANEL_VFD_SetBrightness(aotom_data.u.brightness.level);
		break;
	case VFDICONDISPLAYONOFF:
	{
#if defined(SPARK)
		switch (aotom_data.u.icon.icon_nr) {
		case 0:
			res = YWPANEL_VFD_SetLed(LED_RED, aotom_data.u.icon.on);
			led_state[LED_RED].state = aotom_data.u.icon.on;
			break;
		case 35:
			res = YWPANEL_VFD_SetLed(LED_GREEN, aotom_data.u.icon.on);
			led_state[LED_GREEN].state = aotom_data.u.icon.on;
			break;
		default:
			break;
		}
#endif
#if defined(SPARK7162)
		icon_nr = aotom_data.u.icon.icon_nr;
		//e2 icons workarround
		//printk("icon_nr = %d\n", icon_nr);
		if (icon_nr >= 256) {
			icon_nr = icon_nr / 256;
			switch (icon_nr) {
			case 0x11:
				icon_nr = 0x0E; //widescreen
				break;
			case 0x13:
				icon_nr = 0x0B; //CA
				break;
			case 0x15:
				icon_nr = 0x19; //mp3
				break;
			case 0x17:
				icon_nr = 0x1A; //ac3
				break;
			case 0x1A:
				icon_nr = 0x03; //play
				break;
			case 0x1e:
				icon_nr = 0x07; //record
				break;
			case 38:
				break; //cd part1
			case 39:
				break; //cd part2
			case 40:
				break; //cd part3
			case 41:
				break; //cd part4
			default:
				icon_nr = 0; //no additional symbols at the moment
				break;
			}
		}	  
		if (aotom_data.u.icon.on != 0)
			aotom_data.u.icon.on = 1;
		if (icon_nr > 0 && icon_nr <= 45 )
			res = aotomSetIcon(icon_nr, aotom_data.u.icon.on);
		if (icon_nr == 46){
			switch (aotom_data.u.icon.on){
			case 1:
				VFD_set_all_icons();
				res = 0;
				break;
			case 0:
				VFD_clear_all_icons();
				res = 0;
				break;
			default:
				break;
			}
		}
#endif		
		mode = 0;
		break;
	}
	case VFDSTANDBY:
	{
#if defined(SPARK) || defined(SPARK7162)
		u32 uTime = 0;
		//u32 uStandByKey = 0;
		//u32 uPowerOnTime = 0;
		get_user(uTime, (int *) arg);
		//printk("uTime = %d\n", uTime);

		//uPowerOnTime = YWPANEL_FP_GetPowerOnTime();
		//printk("1uPowerOnTime = %d\n", uPowerOnTime);

		YWPANEL_FP_SetPowerOnTime(uTime);

		//uPowerOnTime = YWPANEL_FP_GetPowerOnTime();
		//printk("2uPowerOnTime = %d\n", uPowerOnTime);
		#if 0
		uStandByKey = YWPANEL_FP_GetStandByKey(0);
		printk("uStandByKey = %d\n", uStandByKey);
		uStandByKey = YWPANEL_FP_GetStandByKey(1);
		printk("uStandByKey = %d\n", uStandByKey);
		uStandByKey = YWPANEL_FP_GetStandByKey(2);
		printk("uStandByKey = %d\n", uStandByKey);
		uStandByKey = YWPANEL_FP_GetStandByKey(3);
		printk("uStandByKey = %d\n", uStandByKey);
		uStandByKey = YWPANEL_FP_GetStandByKey(4);
		printk("uStandByKey = %d\n", uStandByKey);
		#endif
		clear_display();
		YWPANEL_FP_ControlTimer(true);
		YWPANEL_FP_SetCpuStatus(YWPANEL_CPUSTATE_STANDBY);
		res = 0;
#endif
		break;
	}
	case VFDSETTIME2:
	{
		u32 uTime = 0;
		res = get_user(uTime, (int *)arg);
		if (! res)
		{
			res = YWPANEL_FP_SetTime(uTime);
			YWPANEL_FP_ControlTimer(true);
		}
		break;
	}
	case VFDSETTIME:
		res = aotomSetTime(aotom_data.u.time.time);
		break;
	case VFDGETTIME:
	{
#if defined(SPARK) || defined(SPARK7162)
		u32 uTime = 0;
		uTime = YWPANEL_FP_GetTime();
		//printk("uTime = %d\n", uTime);
		res = put_user(uTime, (int *) arg);
#endif
		break;
	}
	case VFDGETWAKEUPMODE:
		break;
	case VFDDISPLAYCHARS:
		if (mode == 0)
		{
			if (copy_from_user(&vfd_data, (void *) arg, sizeof(vfd_data)))
				return -EFAULT;
			if (vfd_data.length > sizeof(vfd_data.data))
				vfd_data.length = sizeof(vfd_data.data);
			while ((vfd_data.length > 0) && (vfd_data.data[vfd_data.length - 1 ] == '\n'))
				vfd_data.length--;
				res = run_draw_thread(&vfd_data);
		} else
			mode = 0;
		break;
	case VFDDISPLAYWRITEONOFF:
		break;
	case VFDDISPLAYCLR:
		vfd_data.length = 0;
		res = run_draw_thread(&vfd_data);
		break;
#if defined(SPARK)
	case 0x5305:
		res = 0;
		break;
#endif
	case 0x5401:
		res = 0;
		break;
	case VFDGETSTARTUPSTATE:
	{
		YWPANEL_STARTUPSTATE_t State;
		if (YWPANEL_FP_GetStartUpState(&State))
			res = put_user(State, (int *) arg);
		break;
	}
	case VFDGETVERSION:
	{
		YWPANEL_Version_t panel_version;
		memset(&panel_version, 0, sizeof(YWPANEL_Version_t));
		if (YWPANEL_FP_GetVersion(&panel_version))
			res = put_user (panel_version.DisplayInfo, (int *)arg);
		break;
	}

	default:
		printk("VFD/AOTOM: unknown IOCTL 0x%x\n", cmd);
		mode = 0;
		break;
	}

	up(&write_sem);

	dprintk(5, "%s <\n", __func__);
	return res;
}

static unsigned int AOTOMdev_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &wq, wait);

	if(receiveCount > 0)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations vfd_fops =
{
	.owner = THIS_MODULE,
	.ioctl = AOTOMdev_ioctl,
	.write = AOTOMdev_write,
	.read  = AOTOMdev_read,
	.poll  = AOTOMdev_poll,
	.open  = AOTOMdev_open,
	.release  = AOTOMdev_close
};

/*----- Button driver -------*/

static char *button_driver_name = "fulan front panel buttons";
static struct input_dev *button_dev;
static int bad_polling = 1;
static struct workqueue_struct *fpwq;

static void button_bad_polling(struct work_struct *work)
{
	int btn_pressed = 0;

	int report_key = 0;

	while(bad_polling == 1)
	{
		int button_value;
		msleep(50);
		button_value = AOTOMfp_Get_Key_Value();
		if (button_value != INVALID_KEY) {
			dprintk(5, "got button: %X\n", button_value);
			flashLED(LED_GREEN, 100);
			//VFD_Show_Ico(DOT2,LOG_ON);
			//YWPANEL_VFD_SetLed(1, LOG_ON);
			if (1 == btn_pressed) {
				if (report_key == button_value)
					continue;
				input_report_key(button_dev, report_key, 0);
				input_sync(button_dev);
			}
			report_key = button_value;
			btn_pressed = 1;
			switch(button_value) {
				case KEY_LEFT:
				case KEY_RIGHT:
				case KEY_UP:
				case KEY_DOWN:
				case KEY_OK:
				case KEY_MENU:
				case KEY_EXIT:
				case KEY_POWER:
					input_report_key(button_dev, button_value, 1);
					input_sync(button_dev);
					break;
				default:
					dprintk(5, "[BTN] unknown button_value %d\n", button_value);
			}
		}
		else {
			if(btn_pressed) {
				btn_pressed = 0;
				//msleep(80);
				//VFD_Show_Ico(DOT2,LOG_OFF);
				//YWPANEL_VFD_SetLed(1, LOG_OFF);
				input_report_key(button_dev, report_key, 0);
				input_sync(button_dev);
			}
		}
	}
	bad_polling = 2;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
static DECLARE_WORK(button_obj, button_bad_polling);
#else
static DECLARE_WORK(button_obj, button_bad_polling, NULL);
#endif
static int button_input_open(struct input_dev *dev)
{
	fpwq = create_workqueue("button");
	if(queue_work(fpwq, &button_obj)) {
		dprintk(5, "[BTN] queue_work successful ...\n");
		return 0;
	}
	dprintk(5, "[BTN] queue_work not successful, exiting ...\n");
	return 1;
}

static void button_input_close(struct input_dev *dev)
{
	bad_polling = 0;
	while (bad_polling != 2)
		msleep(1);
	bad_polling = 1;

	if (fpwq) {
		destroy_workqueue(fpwq);
		dprintk(5, "[BTN] workqueue destroyed\n");
	}
}

int button_dev_init(void)
{
	int error;

	dprintk(5, "[BTN] allocating and registering button device\n");

	button_dev = input_allocate_device();
	if (!button_dev)
		return -ENOMEM;

	button_dev->name = button_driver_name;
	button_dev->open = button_input_open;
	button_dev->close= button_input_close;

	set_bit(EV_KEY    , button_dev->evbit );
	set_bit(KEY_UP    , button_dev->keybit);
	set_bit(KEY_DOWN  , button_dev->keybit);
	set_bit(KEY_LEFT  , button_dev->keybit);
	set_bit(KEY_RIGHT , button_dev->keybit);
	set_bit(KEY_POWER , button_dev->keybit);
	set_bit(KEY_MENU  , button_dev->keybit);
	set_bit(KEY_OK    , button_dev->keybit);
	set_bit(KEY_EXIT  , button_dev->keybit);

	error = input_register_device(button_dev);
	if (error)
		input_free_device(button_dev);

	return error;
}

void button_dev_exit(void)
{
	dprintk(5, "[BTN] unregistering button device\n");
	input_unregister_device(button_dev);
}

static int aotom_reboot_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	switch(event) {
		case SYS_POWER_OFF:
			YWPANEL_VFD_ShowString("POWEROFF");
			break;
		case SYS_HALT:
			YWPANEL_VFD_ShowString("HALT");
			break;
		default:
			YWPANEL_VFD_ShowString("REBOOT");
			return NOTIFY_DONE;
	}
	msleep(1000);
	clear_display();
	YWPANEL_FP_ControlTimer(true);
	YWPANEL_FP_SetCpuStatus(YWPANEL_CPUSTATE_STANDBY);
	return NOTIFY_DONE;
};

static struct notifier_block aotom_reboot_block = {
	.notifier_call = aotom_reboot_event,
	.priority = INT_MAX,
};

static int aotom_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u32 uTime = 0;
	//printk("%s\n", __func__);
	uTime = YWPANEL_FP_GetTime();
	rtc_time_to_tm(uTime, tm);
	return 0;
}

static int tm2time(struct rtc_time *tm)
{
	return mktime(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static int aotom_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	int res = 0;
	//printk("%s\n", __func__);
	u32 uTime = tm2time(tm);
	res = YWPANEL_FP_SetTime(uTime);
	YWPANEL_FP_ControlTimer(true);
	return res;
}

static int aotom_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *al)
{
	return 0;
}

static int aotom_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *al)
{
	u32 a_time = 0;
	if (al->enabled)
		a_time = tm2time(&al->time);
	printk(KERN_INFO "%s enabled:%d time: %d\n", __func__, al->enabled, a_time);
	YWPANEL_FP_SetPowerOnTime(a_time);
	return 0;
}

static const struct rtc_class_ops aotom_rtc_ops = {
	.read_time	= aotom_rtc_read_time,
	.set_time	= aotom_rtc_set_time,
	.read_alarm	= aotom_rtc_read_alarm,
	.set_alarm	= aotom_rtc_set_alarm,
};

static int __devinit aotom_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	/* I have no idea where the pdev comes from, but it needs the can_wakeup = 1
	 * otherwise we don't get the wakealarm sysfs attribute... :-) */
	pdev->dev.power.can_wakeup = 1;
	rtc = rtc_device_register("aotom", &pdev->dev, &aotom_rtc_ops, THIS_MODULE);
	printk(KERN_DEBUG "%s %p\n", __func__, rtc);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);
	printk(KERN_DEBUG "%s 2\n", __func__);
	platform_set_drvdata(pdev, rtc);
	return 0;
}

static int __devexit aotom_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device *rtc = platform_get_drvdata(pdev);
	printk(KERN_DEBUG "%s %p\n", __func__, rtc);
	rtc_device_unregister(rtc);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver aotom_rtc_driver = {
	.probe = aotom_rtc_probe,
	.remove = __devexit_p(aotom_rtc_remove),
	.driver = {
		.name	= RTC_NAME,
		.owner	= THIS_MODULE,
	},
};


ssize_t proc_node_read(char *buffer, char **start, off_t off, int count, int *eof, void *data)
{
    int len;
   *eof = 1;
   len=sprintf(buffer, "%d\n", YWPANEL_width);
   return len;
}


#define DEVICE_NAME "vfd"
static struct cdev   aotom_cdev;
static struct class *aotom_class = 0;
static int __init aotom_init_module(void)
{
	int i,result;
	printk("[aotom] Fulan front panel driver\n");

	if(YWPANEL_VFD_Init()) {
		printk("[aotom] unable to init module\n");
		return -1;
	}

	VFD_clr();

	if(button_dev_init() != 0)
		return -1;

	result = register_chrdev_region(MKDEV(VFD_MAJOR, 0), 1, DEVICE_NAME);
	if (result < 0) {
		printk( KERN_ALERT "[aotom] Fulan front panel driver cannot register chrdev region (%d)\n", result);
		return result;
	}

	if(NULL == (aotom_class = class_create(THIS_MODULE, DEVICE_NAME))) {
		printk("[aotom] Fulan front panel driver couldn't create class '%s' driver\n", DEVICE_NAME);
		unregister_chrdev_region(MKDEV(VFD_MAJOR, 0), 1);
		return -1;
	}

	if(NULL == device_create(aotom_class, NULL, MKDEV(VFD_MAJOR, 0), NULL, "vfd")) {
		printk("[aotom] Fulan front panel driver couldn't create device '%s' driver\n", DEVICE_NAME);
		class_destroy(aotom_class);
		unregister_chrdev_region(MKDEV(VFD_MAJOR, 0), 1);
		return -1;
	}

	cdev_init(&aotom_cdev, &vfd_fops);
	if (cdev_add(&aotom_cdev, MKDEV(VFD_MAJOR, 0), 1) < 0)
	{
		printk("[aotom] Fulan front panel driver couldn't register '%s' driver\n", DEVICE_NAME);
		cdev_del(&aotom_cdev);
		device_destroy(aotom_class, MKDEV(VFD_MAJOR, 0));
		class_destroy(aotom_class);
		unregister_chrdev_region(MKDEV(VFD_MAJOR, 0), 1);
		return -1;
	}

	sema_init(&write_sem, 1);
	sema_init(&receive_sem, 1);
	sema_init(&draw_thread_sem, 1);

	for (i = 0; i < LASTMINOR; i++) {
		FrontPanelOpen[i].open_count = 0;
		FrontPanelOpen[i].minor = i;
	}

	for (i = 0; i < LASTLED; i++) {
		led_state[i].state = LOG_OFF;
		led_state[i].period = 0;
		led_state[i].stop = 1;
		sema_init(&led_state[i].led_sem, 0);
		led_state[i].led_task = kthread_run(led_thread, (void *) i, "led thread");
	}

	register_reboot_notifier(&aotom_reboot_block);
	i = platform_driver_register(&aotom_rtc_driver);
	if (i)
		printk(KERN_ERR "[aotom] %s platform_driver_register failed: %d\n", __func__, i);
	else
		rtc_pdev = platform_device_register_simple(RTC_NAME, -1, NULL, 0);

	if (IS_ERR(rtc_pdev))
		printk(KERN_ERR "[aotom] %s platform_device_register_simple failed: %ld\n",
				__func__, PTR_ERR(rtc_pdev));

	/* proc file stuff */
	own_proc_dir = create_proc_entry(NAME_DIR, S_IFDIR | S_IRWXUGO, NULL);
	if (own_proc_dir == NULL ) {
	    printk(KERN_ERR "[aotom] can't create /proc/%s\n", NAME_DIR);
	    return -ENOMEM;
	}
	dprintk(5, "[aotom] Create /proc/%s\n", NAME_DIR);
	own_proc_dir->gid =0 ;

	own_proc_node=create_proc_entry(NAME_NODE, S_IFREG | S_IRUGO | S_IWUGO, own_proc_dir);
	if (own_proc_node == NULL ) {
	    printk(KERN_ERR "[aotom] can't create /proc/%s/%s\n", NAME_DIR, NAME_NODE);
	    return -ENOMEM;
	}
	dprintk(5, "[aotom] Create /proc/%s/%s\n", NAME_DIR, NAME_NODE);
	own_proc_node->uid = 0;
	own_proc_node->gid =0 ;

	own_proc_node->read_proc=proc_node_read;

	if ( YWPANEL_width == 4 ) {
		YWPANEL_VFD_ShowString("init");
	} else {
		YWPANEL_VFD_ShowString("  init  ");
	}

	dprintk(5, "[aotom] %s <\n", __func__);

	return 0;
}

static int led_thread_active(void) {
	int i;

	for (i = 0; i < LASTLED; i++)
		if(!led_state[i].stop && led_state[i].led_task)
			return -1;
	return 0;
}

static void __exit aotom_cleanup_module(void)
{
	int i;

	/* proc file stuff */
	remove_proc_entry(NAME_NODE, own_proc_dir);
	dprintk(5, "[aotom] Remove .../proc/%s\n", NAME_DIR);
	remove_proc_entry(NAME_DIR, NULL);
	dprintk(5, "[aotom] Remove .../proc/%s/%s\n", NAME_DIR, NAME_NODE);


	unregister_reboot_notifier(&aotom_reboot_block);
	platform_driver_unregister(&aotom_rtc_driver);
	platform_set_drvdata(rtc_pdev, NULL);
	platform_device_unregister(rtc_pdev);

	if(!draw_thread_stop && draw_task)
		kthread_stop(draw_task);

	for (i = 0; i < LASTLED; i++)
		if(!led_state[i].stop && led_state[i].led_task) {
			up(&led_state[i].led_sem);
			kthread_stop(led_state[i].led_task);
		}

	while(!draw_thread_stop && !led_thread_active())
		msleep(1);

	dprintk(5, "[aotom] [BTN] unloading ...\n");
	button_dev_exit();

	cdev_del(&aotom_cdev);
	device_destroy(aotom_class, MKDEV(VFD_MAJOR, 0));
	class_destroy(aotom_class);
	unregister_chrdev_region(MKDEV(VFD_MAJOR, 0), 1);

	YWPANEL_VFD_Term();
	printk("[aotom] Fulan front panel module unloading\n");
}

module_init(aotom_init_module);
module_exit(aotom_cleanup_module);

module_param(paramDebug, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(paramDebug, "Debug Output 0=disabled >0=enabled(debuglevel)");

module_param(gmt,charp,0);
MODULE_PARM_DESC(gmt, "gmt offset (default +0000");

MODULE_DESCRIPTION("VFD module for fulan boxes");
MODULE_AUTHOR("Spider-Team, oSaoYa");
MODULE_LICENSE("GPL");
