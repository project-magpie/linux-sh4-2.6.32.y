/*
 * Platform PM Capability - STx5197
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/stm/pm.h>
#ifdef CONFIG_PM
static int
usb_pwr_ack(struct platform_device *dev, int host_phy, int on)
{
	static struct sysconf_field *sc;

	if (!sc)
		sc = sysconf_claim(SYS_STA, CFG_MONITOR_E, 30, 30,
			"usb pwd ack");

	while (sysconf_read(sc) != on);

	return 0;
}

static int
usb_pwr_dwn(struct platform_device *dev, int host_phy, int pwd)
{
	static struct sysconf_field *sc;

	/* Power on USB */
	if (!sc) {
		sc = sysconf_claim(SYS_CFG, CFG_CONTROL_H, 8, 8, "usb pwd req");
	}

	sysconf_write(sc, (pwd ? 1 : 0));

	return 0;
}
#if 0
static int
usb_sw_reset(struct platform_device *dev, int host_phy)
{
	static struct sysconf_field *sc;

	/* Reset USB */
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 4, 4, 4, "USB_RST");
	sysconf_write(sc, 0);
	mdelay(10);
	sysconf_write(sc, 1);
	mdelay(10);

	return 0;
}
#endif

static int
emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG, CFG_CONTROL_I, 31, 31,
			"emi pwd req");

	sysconf_write(sc, (dwn ? 1 : 0));
	return 0;
}

static int
emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_DEV, CFG_MONITOR_J, 20, 20,
			"emi pwr ack");
/*	while (sysconf_read(sc) != ack);*/
	mdelay(10);
	return 0;
}


static struct platform_device_pm stx5197_pm_devices[] = {
pm_plat_name("emi", NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&st_usb, NULL, usb_pwr_dwn, usb_pwr_ack, NULL),
};
#endif
