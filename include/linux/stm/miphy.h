/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_MIPHY_H
#define __LINUX_STM_MIPHY_H

struct stm_miphy {
	int ports_num;
	int (*jtag_tick)(int tms, int tdi, void *priv);
	void *jtag_priv;
};

struct stm_miphy_sysconf_soft_jtag {
	struct sysconf_field *tms;
	struct sysconf_field *tck;
	struct sysconf_field *tdi;
	struct sysconf_field *tdo;
};

#ifdef CONFIG_STM_MIPHY

void stm_miphy_init(struct stm_miphy *miphy, int port);
int stm_miphy_sysconf_jtag_tick(int tms, int tdi, void *priv);

#else

static inline void stm_miphy_init(struct stm_miphy *miphy, int port)
{
}

static inline int stm_miphy_sysconf_jtag_tick(int tms, int tdi, void *priv)
{
	return -1;
}

#endif

#endif
