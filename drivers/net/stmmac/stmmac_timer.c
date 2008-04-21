/* 
 * drivers/net/stmmac/stmmac_timer.c
 *
 * Real time clock device generates an interrupt at regular 
 * intervals in order to notify the Ethernet driver about frame
 * receptions.
 *
 * Copyright (C) 2007 by STMicroelectronics
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
*/

#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/rtc.h>

struct rtc_device *stmmac_rtc;
rtc_task_t stmmac_task;

extern void stmmac_schedule_rx(struct net_device *dev);

int stmmac_timer_close(void)
{
	rtc_irq_unregister(stmmac_rtc, &stmmac_task);
	rtc_class_close(stmmac_rtc);
	return 0;
}
int stmmac_timer_start(unsigned int freq)
{
	rtc_irq_set_freq(stmmac_rtc, &stmmac_task, freq);
	rtc_irq_set_state(stmmac_rtc, &stmmac_task, 1);
	return 0;
}

int stmmac_timer_stop(void)
{
	rtc_irq_set_state(stmmac_rtc, &stmmac_task, 0);
	return 0;
}

/*
 * Use periodic interrupt for handling the reception process
 */
static void stmmac_rtc_handler(void *priv)
{
	struct net_device *dev = (struct net_device *)priv;

	stmmac_schedule_rx(dev);

	return;
}

int stmmac_timer_open(struct net_device *dev, unsigned int freq)
{
	stmmac_task.private_data = dev;
	stmmac_task.func = stmmac_rtc_handler;

	stmmac_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (stmmac_rtc == NULL){
		printk(KERN_ERR "open rtc device failed\n");
		return -ENODEV;
	}

	rtc_irq_register(stmmac_rtc, &stmmac_task);

	/* Periodic mode is not supported */
	if ((rtc_irq_set_freq(stmmac_rtc, &stmmac_task, freq) < 0)) {
		printk(KERN_ERR "set periodic failed\n");
		rtc_irq_unregister(stmmac_rtc, &stmmac_task);
		rtc_class_close(stmmac_rtc);
		return -1;
	}

	printk(KERN_INFO "stmmac_timer enabled - %s (freq %dHz)\n",
	       CONFIG_RTC_HCTOSYS_DEVICE, freq);

	return 0;
}
