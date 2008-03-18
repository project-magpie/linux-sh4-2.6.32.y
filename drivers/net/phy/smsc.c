/*
 * drivers/net/phy/smsc.c
 *
 * Driver for SMSC PHYs
 *
 * Author: Herbert Valerio Riedel
 *
 * Copyright (c) 2006 Herbert Valerio Riedel <hvr@gnu.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Changelog:
 *	March 2008
 *	- Added LAN8700 PHY (Giuseppe Cavallaro <peppe.cavallaro@st.com>)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>

#define MII_SMSC_ISF 29 /* Interrupt Source Flags */
#define MII_SMSC_IM  30 /* Interrupt Mask */

#define MII_SMSC_ISF_INT1 (1<<1) /* Auto-Negotiation Page Received */
#define MII_SMSC_ISF_INT2 (1<<2) /* Parallel Detection Fault */
#define MII_SMSC_ISF_INT3 (1<<3) /* Auto-Negotiation LP Ack */
#define MII_SMSC_ISF_INT4 (1<<4) /* Link Down */
#define MII_SMSC_ISF_INT5 (1<<5) /* Remote Fault Detected */
#define MII_SMSC_ISF_INT6 (1<<6) /* Auto-Negotiation complete */
#define MII_SMSC_ISF_INT7 (1<<7) /* ENERGYON */

#define MII_SMSC_ISF_INT_ALL (0x0e)

#define MII_SMSC_ISF_INT_PHYLIB_EVENTS \
	(MII_SMSC_ISF_INT6 | MII_SMSC_ISF_INT4)

/* LAN8700 phy identifier values */
#define LAN8700_PHY_ID		0x0007c0c0
#define LAN8700_PHY_LO_ID_REVA 	0xc0c1
#define LAN8700_PHY_LO_ID_REVB 	0xc0c2
#define LAN8700_PHY_LO_ID_REVC 	0xc0c3
#define LAN8700_PHY_LO_ID_REVD 	0xc0c4
#define LAN8700_PHY_LO_ID_REVE 	0xc0c5
#define LAN8700_PHY_LO_ID_REVF 	0xc0c6
#define LAN8700_PHY_LO_ID_REVG 	0xc0c7

static int smsc_config_intr(struct phy_device *phydev)
{
	int rc = phy_write (phydev, MII_SMSC_IM,
			((PHY_INTERRUPT_ENABLED == phydev->interrupts)
			? MII_SMSC_ISF_INT_PHYLIB_EVENTS
			: 0));

	return rc < 0 ? rc : 0;
}

static int smsc_ack_interrupt(struct phy_device *phydev)
{
	int rc = phy_read (phydev, MII_SMSC_ISF);

	return rc < 0 ? rc : 0;
}

static int smsc_config_init(struct phy_device *phydev)
{
	return smsc_ack_interrupt (phydev);
}


static struct phy_driver lan83c185_driver = {
	.phy_id		= 0x0007c0a0, /* OUI=0x00800f, Model#=0x0a */
	.phy_id_mask	= 0xfffffff0,
	.name		= "SMSC LAN83C185",

	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_INTERRUPT | PHY_HAS_MAGICANEG,

	/* basic functions */
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.config_init	= smsc_config_init,

	/* IRQ related */
	.ack_interrupt	= smsc_ack_interrupt,
	.config_intr	= smsc_config_intr,

	.driver		= { .owner = THIS_MODULE, }
};

static struct phy_driver lan8700_driver = {
        .phy_id         = LAN8700_PHY_ID,
        .phy_id_mask    = 0xfffffff0,
        .name           = "SMSC LAN8700",
        .features       = PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause,
        .flags          = PHY_HAS_INTERRUPT,
	.config_init    = smsc_config_init,
        .config_aneg    = genphy_config_aneg,
        .read_status    = genphy_read_status,
        .ack_interrupt  = smsc_ack_interrupt,
        .config_intr    = smsc_config_intr,
	.suspend 	= genphy_suspend,
	.resume  	= genphy_resume,
	.driver         = { .owner = THIS_MODULE, }
};


static int __init smsc_init(void)
{
	int retval;

	retval = phy_driver_register (&lan83c185_driver);

	if (retval < 0)
		return retval;

	return phy_driver_register(&lan8700_driver);
}

static void __exit smsc_exit(void)
{
	phy_driver_unregister (&lan83c185_driver);
	phy_driver_unregister (&lan8700_driver);
}

MODULE_DESCRIPTION("SMSC PHY driver");
MODULE_AUTHOR("Herbert Valerio Riedel");
MODULE_LICENSE("GPL");

module_init(smsc_init);
module_exit(smsc_exit);
