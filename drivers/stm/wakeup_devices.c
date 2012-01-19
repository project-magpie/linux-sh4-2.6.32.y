/*
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 */

#include <linux/stm/wakeup_devices.h>
#include <linux/device.h>
#include <linux/platform_device.h>

static int wokenup_by;

int stm_get_wakeup_reason(void)
{
	return wokenup_by;
}

void __stm_set_wakeup_reason(int irq)
{
	wokenup_by = irq;
}

#ifdef CONFIG_STM_LPM

#include <linux/stm/lpm.h>

int stm_notify_wakeup_devices(struct stm_wakeup_devices *wkd)
{
	unsigned char c_wk_device = 0;

	c_wk_device |= wkd->lirc_can_wakeup ? STM_LPM_WAKEUP_IR : 0;
	c_wk_device |= wkd->hdmi_cec ? STM_LPM_WAKEUP_CEC : 0;
	c_wk_device |= wkd->kscan ? STM_LPM_WAKEUP_FRP : 0;
	c_wk_device |= wkd->eth_phy_can_wakeup ? STM_LPM_WAKEUP_WOL : 0;
	c_wk_device |= wkd->eth1_phy_can_wakeup ? STM_LPM_WAKEUP_WOL : 0;
	c_wk_device |= wkd->asc ? STM_LPM_WAKEUP_ASC : 0;
	c_wk_device |= wkd->rtc ? STM_LPM_WAKEUP_RTC : 0;

	return stm_lpm_set_wakeup_device(c_wk_device);
}

/*
 * It isn't so easy a generic code to translate
 * the bitmap coming from the SBC to a generic interrupt number.
 * It's better a generic remapping based on a chip array
 *
 * Now each platform, where the SBC is, has to declare
 * a specific platform_lpm_to_irq[] array to translate
 * the bitmap to the Linux irq numer on the chip
 */
int __initdata platform_lpm_to_irq[8] __attribute__ ((weak)) = {
	0, 0, 0, 0, 0, 0, 0, 0
};

int stm_retrieve_wakeup_reason(void)
{
	unsigned long c_wk_device = 0;

	stm_lpm_get_wakeup_device((enum stm_lpm_wakeup_devices *)&c_wk_device);

	if (!c_wk_device) {
		pr_err("ERROR: %s: Wakeup reason Not found\n"
			"ERROR: %s: Check if the platform has a specific \n"
			"       platform_lpm_to_irq array implementation\n",
			__func__, __func__);
		return -1;
	}
	return platform_lpm_to_irq[ffs(c_wk_device) - 1];
}
#else
int stm_notify_wakeup_devices(struct stm_wakeup_devices *wkd)
{
	return 0;
}

int stm_retrieve_wakeup_reason(void)
{
	return 0;
}
#endif

static void stm_wake_init(struct stm_wakeup_devices *wkd)
{
	memset(wkd, 0, sizeof(*wkd));
}

static int __check_wakeup_device(struct device *dev, void *data)
{
	struct stm_wakeup_devices *wkd = (struct stm_wakeup_devices *)data;

	if (device_may_wakeup(dev)) {
		pr_info("[STM][PM] -> device %s can wakeup\n", dev_name(dev));
		if (!strcmp(dev_name(dev), "lirc"))
			wkd->lirc_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "hdmi"))
			wkd->hdmi_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth"))
			wkd->eth_phy_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth.0"))
			wkd->eth_phy_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stmmaceth.1"))
			wkd->eth1_phy_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stm-hdmi-cec"))
			wkd->hdmi_cec = 1;
		else if (!strcmp(dev_name(dev), "stm-hdmi-hot"))
			wkd->hdmi_hotplug = 1;
		else if (!strcmp(dev_name(dev), "stm-kscan"))
			wkd->kscan = 1;
		else if (!strcmp(dev_name(dev), "stm-rtc"))
			wkd->rtc = 1;
		else if (!strcmp(dev_name(dev), "stm-asc"))
			wkd->asc = 1;

	}
	return 0;
}

int stm_check_wakeup_devices(struct stm_wakeup_devices *wkd)
{
	stm_wake_init(wkd);
	bus_for_each_dev(&platform_bus_type, NULL, wkd, __check_wakeup_device);
	return 0;
}

EXPORT_SYMBOL(stm_check_wakeup_devices);

