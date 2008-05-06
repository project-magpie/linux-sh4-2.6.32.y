/*
 * drivers/net/phy/national.c
 *
 * Driver for National Semiconductor PHYs
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Copyright (c) 2008 STMicroelectronics Limited
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Changelog:
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>

/* DP83865 phy identifier values */
#define DP83865_PHY_ID		0x20005c7a

#define NS_EXP_MEM_CTL	0x16
#define NS_EXP_MEM_DATA	0x1d
#define NS_EXP_MEM_ADD	0x1e

static u8 ns_exp_read(struct phy_device *phydev, u16 reg)
{
	phy_write(phydev, NS_EXP_MEM_ADD, reg);
	return phy_read(phydev, NS_EXP_MEM_DATA);
}

static void ns_exp_write(struct phy_device *phydev, u16 reg, u8 data)
{
	phy_write(phydev, NS_EXP_MEM_ADD, reg);
	phy_write(phydev, NS_EXP_MEM_DATA, data);
}

static int ns_config_intr(struct phy_device *phydev)
{
#if 0
	int rc = phy_write (phydev, MII_SMSC_IM,
			((PHY_INTERRUPT_ENABLED == phydev->interrupts)
			? MII_SMSC_ISF_INT_PHYLIB_EVENTS
			: 0));

	return rc < 0 ? rc : 0;
#endif
	return 0;
}

static int ns_ack_interrupt(struct phy_device *phydev)
{
#if 0
	int rc = phy_read (phydev, MII_SMSC_ISF);

	return rc < 0 ? rc : 0;
#endif
	return 0;
}

static int ns_config_init(struct phy_device *phydev)
{
	/* Enable 8 bit expended memory read/write (no auto increment) */
	phy_write(phydev, NS_EXP_MEM_CTL, 0);

	/* Check 10Mb loopback mode */
	printk("%s: 10BASE-T Half duplex loopback %d\n", __FUNCTION__,
	       ns_exp_read(phydev, 0x1c0));

//	ns_exp_write(phydev, 0x1c0, ns_exp_read(phydev, 0x1c0) | 1);

	printk("%s: 10BASE-T Half duplex loopback %d\n", __FUNCTION__,
	       ns_exp_read(phydev, 0x1c0));

	return ns_ack_interrupt (phydev);
}

static struct phy_driver dp83865_driver = {
        .phy_id         = DP83865_PHY_ID,
        .phy_id_mask    = 0xfffffff0,
        .name           = "NatSemi DP83865",
        .features       = PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause,
        .flags          = PHY_HAS_INTERRUPT,
	.config_init    = ns_config_init,
        .config_aneg    = genphy_config_aneg,
        .read_status    = genphy_read_status,
        .ack_interrupt  = ns_ack_interrupt,
        .config_intr    = ns_config_intr,
	.suspend 	= genphy_suspend,
	.resume  	= genphy_resume,
	.driver         = { .owner = THIS_MODULE, }
};


static int __init ns_init(void)
{
	int retval;

	return phy_driver_register(&dp83865_driver);
}

static void __exit ns_exit(void)
{
	phy_driver_unregister (&dp83865_driver);
}

MODULE_DESCRIPTION("NatSemi PHY driver");
MODULE_AUTHOR("Herbert Valerio Riedel");
MODULE_LICENSE("GPL");

module_init(ns_init);
module_exit(ns_exit);
