/*
 * Copyright (c) 2011 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_STXH205_H
#define __LINUX_STM_STXH205_H

#include <linux/device.h>
#include <linux/stm/platform.h>

#define SYSCONFG_GROUP(x) \
	(((x) < 400) ? ((x) / 100) : 3)
#define SYSCONF_OFFSET(x) \
	(((x) < 400) ? ((x) % 100) : ((x) - 400))

#define SYSCONF(x) \
	SYSCONFG_GROUP(x), SYSCONF_OFFSET(x)

#define LPM_SYSCONF_BANK	(4)

struct stxh205_pio_config {
	struct stm_pio_control_mode_config *mode;
	struct stm_pio_control_retime_config *retime;
};


void stxh205_early_device_init(void);

#define STXH205_ASC(x) (((x) < 10) ? (x) : ((x)-7))

struct stxh205_asc_config {
	int hw_flow_control;
	int is_console;
};
void stxh205_configure_asc(int asc, struct stxh205_asc_config *config);

struct stxh205_ethernet_config {
	enum {
		stxh205_ethernet_mode_mii,
		stxh205_ethernet_mode_rmii,
		stxh205_ethernet_mode_reverse_mii
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stxh205_configure_ethernet(struct stxh205_ethernet_config *config);

void stxh205_configure_usb(int port);

#endif
