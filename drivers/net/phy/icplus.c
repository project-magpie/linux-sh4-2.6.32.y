/*
 * Driver for ICPlus PHYs
 *
 * Copyright (c) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

MODULE_DESCRIPTION("ICPlus IP175C/IP101A/IC1001 PHY drivers");
MODULE_AUTHOR("Michael Barkowski");
MODULE_LICENSE("GPL");

/* IP101A/IP1001 */
#define IP10XX_SPEC_CTRL_STATUS		16  /* Spec. Control Register */
#define IP1001_SPEC_CTRL_STATUS_2	20  /* IP1001 Spec. Control Reg 2 */
#define IP1001_PHASE_SEL_MASK		3 /* IP1001 RX/TXPHASE_SEL */
#define IP1001_APS_ON			11  /* IP1001 APS Mode  bit */
#define IP101A_APS_ON			2   /* IP101A APS Mode bit */
#define IP101A_IRQ_CONF_STATUS		0x11	/* Conf Info IRQ & Status Reg */
#define	IP101A_IRQ_PIN_USED		(1<<15)
#define	IP101A_IRQ_DEFAULT		IP101A_IRQ_PIN_USED
#define IP101A_G_WOL_CTRL		0x10	/* WoL+ Control Register */
#define IP101A_G_WOL_MAC_ADDR		0x10	/* WoL+ MAC addr Register */
#define IP101A_G_WOL_STATUS		0x11	/* WoL+ Status Register */

/* WOL PLUS register definitions */
#define IP101A_G_WOL_ENABLE		(1 << 15)
#define IP101A_G_WOL_MASTER		(1 << 14)
#define IP101A_G_WOL_INTH		(1 << 13)
#define IP101A_G_WOL_MAGIC_PKT		(1 << 11)
#define IP101A_G_WOL_ANY_PKT 		(1 << 10)
#define IP101A_G_WOL_LINK_CHANGE	(1 << 9)
#define IP101A_G_WOL_DOWN_SPEED		(1 << 8)
#define IP101A_G_WOL_TIMER_SEL_30S	(0 << 6)
#define IP101A_G_WOL_TIMER_SEL_3M	(1 << 6)
#define IP101A_G_WOL_TIMER_SEL_5M	(2 << 6)
#define IP101A_G_WOL_TIMER_SEL_10M	(3 << 6)
#define IP101A_G_WOL_MANUAL_SET		(1 << 5)
#define IP101A_G_WOL_TIMER_SEL_30SEC	0xff3f	/* 30 sec. */
#define IP101A_G_WOL_TIMER_SEL_3MIN	0x0040	/* 3 min. */
#define IP101A_G_WOL_TIMER_SEL_5MIN	0x0080	/* 5 min. */
#define IP101A_G_WOL_TIMER_SEL_10MIN	0x00c0	/* 10 min. */

/* After 3min the PHY can enter in WoL+ mode and the link is 10M
 * it resumes by default as soon as the link change or there is
 * traffic on the wire.
 */
#define	IP101A_G_DEFAULT_WOL	(IP101A_G_WOL_ENABLE | IP101A_G_WOL_MASTER | \
				 IP101A_G_WOL_ANY_PKT	|	\
				 IP101A_G_WOL_LINK_CHANGE |	\
				 IP101A_G_WOL_DOWN_SPEED |	\
				 IP101A_G_WOL_TIMER_SEL_3MIN)
#undef ICPLUS_DEBUG
/*#define ICPLUS_DEBUG*/
#ifdef ICPLUS_DEBUG
#define DBG(fmt, args...)  printk(fmt, ## args)
#else
#define DBG(fmt, args...)  do { } while (0)
#endif

static int ip175c_config_init(struct phy_device *phydev)
{
	int err, i;
	static int full_reset_performed = 0;

	if (full_reset_performed == 0) {

		/* master reset */
		err = phydev->bus->write(phydev->bus, 30, 0, 0x175c);
		if (err < 0)
			return err;

		/* ensure no bus delays overlap reset period */
		err = phydev->bus->read(phydev->bus, 30, 0);

		/* data sheet specifies reset period is 2 msec */
		mdelay(2);

		/* enable IP175C mode */
		err = phydev->bus->write(phydev->bus, 29, 31, 0x175c);
		if (err < 0)
			return err;

		/* Set MII0 speed and duplex (in PHY mode) */
		err = phydev->bus->write(phydev->bus, 29, 22, 0x420);
		if (err < 0)
			return err;

		/* reset switch ports */
		for (i = 0; i < 5; i++) {
			err = phydev->bus->write(phydev->bus, i,
						 MII_BMCR, BMCR_RESET);
			if (err < 0)
				return err;
		}

		for (i = 0; i < 5; i++)
			err = phydev->bus->read(phydev->bus, i, MII_BMCR);

		mdelay(2);

		full_reset_performed = 1;
	}

	if (phydev->addr != 4) {
		phydev->state = PHY_RUNNING;
		phydev->speed = SPEED_100;
		phydev->duplex = DUPLEX_FULL;
		phydev->link = 1;
		netif_carrier_on(phydev->attached_dev);
	}

	return 0;
}

static int ip1xx_reset(struct phy_device *phydev)
{
	int bmcr;

	/* Software Reset PHY */
	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	bmcr |= BMCR_RESET;
	bmcr = phy_write(phydev, MII_BMCR, bmcr);
	if (bmcr < 0)
		return bmcr;

	do {
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;
	} while (bmcr & BMCR_RESET);

	return 0;
}

static int ip1001_config_init(struct phy_device *phydev)
{
	int c;

	c = ip1xx_reset(phydev);
	if (c < 0)
		return c;

	/* Enable Auto Power Saving mode */
	c = phy_read(phydev, IP1001_SPEC_CTRL_STATUS_2);
	if (c < 0)
		return c;
	c |= IP1001_APS_ON;
	c = phy_write(phydev, IP1001_SPEC_CTRL_STATUS_2, c);
	if (c < 0)
		return c;

	return 0;
}

static int ip101a_g_down_speed(struct phy_device *phydev, int down_speed)
{
	int reg = phy_read_page(phydev, IP101A_G_WOL_CTRL, 4);
	if (reg < 0)
		return reg;

	if (down_speed)
		reg |= IP101A_G_WOL_DOWN_SPEED;
	else
		reg &= ~IP101A_G_WOL_DOWN_SPEED;

	return phy_write_page(phydev, IP101A_G_WOL_CTRL, 4, reg);
}

static int ip101a_g_set_mode(struct phy_device *phydev, int mode)
{
	int reg = phy_read_page(phydev, IP101A_G_WOL_CTRL, 4);
	if (reg < 0)
		return reg;

	DBG("IC+101A/G set %s mode timer\n", mode ? "master" : "slave");
	if (mode)
		reg |= IP101A_G_WOL_MASTER;
	else
		reg &= ~IP101A_G_WOL_MASTER;

	reg &= IP101A_G_WOL_TIMER_SEL_30SEC;

	return phy_write_page(phydev, IP101A_G_WOL_CTRL, 4, reg);
}

static int ip101a_g_set_macaddr(struct phy_device *phydev)
{
	struct net_device *netdev = phydev->attached_dev;
	int old = phy_read(phydev, 20);

	if (!netdev)
		return -ENODEV;

	/* Supposing at this stage the parent has a valid dev_addr;
	 * so do not perform any extra check.
	 */
	if (!is_valid_ether_addr(netdev->dev_addr))
		return -EINVAL;

	phy_write(phydev, 20, 5);
	phy_write(phydev, IP101A_G_WOL_MAC_ADDR,
		  netdev->dev_addr[0] << 8 | netdev->dev_addr[1]);
	phy_write(phydev, IP101A_G_WOL_MAC_ADDR,
		  netdev->dev_addr[2] << 8 | netdev->dev_addr[3]);
	phy_write(phydev, IP101A_G_WOL_MAC_ADDR,
		  netdev->dev_addr[4] << 8 | netdev->dev_addr[5]);

	phy_write(phydev, 20, old);
	return 0;
}

static int ip101a_g_set_wol(struct phy_device *phydev)
{
	int wol_mode = phydev->wol;
	int value;

	value = phy_read_page(phydev, IP101A_G_WOL_CTRL, 4);
	if (value < 0)
		return value;

	value &= ~(IP101A_G_WOL_MAGIC_PKT | IP101A_G_WOL_ANY_PKT |
		 IP101A_G_WOL_LINK_CHANGE);

	if (wol_mode & WAKE_MAGIC) {
		int ret;

		ret = ip101a_g_set_macaddr(phydev);
		if (ret)
			return ret;

		value |= IP101A_G_WOL_MAGIC_PKT;
	}

	if (wol_mode & WAKE_UCAST)
		value |= IP101A_G_WOL_ANY_PKT;

	value |= IP101A_G_WOL_LINK_CHANGE | IP101A_G_WOL_ENABLE;

	phy_write_page(phydev, IP101A_G_WOL_CTRL, 4, value);

	DBG("IC+101A/G WoL+ crtl register 0x%x\n",
	    phy_read_page(phydev, IP101A_G_WOL_CTRL, 4));

	pr_info("IC+101a/g: wait for entering in WoL+ mode...\n");
	mdelay(30000);

	return 0;
}

int ip101a_g_suspend(struct phy_device *phydev)
{
	if (device_may_wakeup(&phydev->dev)) {
		int ret;

		/* Set Master mode and disable speed down feature (not usable
		 * when suspend).
		 */
		ret = ip101a_g_down_speed(phydev, 1);
		if (ret)
			return ret;
		ret = ip101a_g_set_mode(phydev, 1);
		if (ret)
			return ret;

		return ip101a_g_set_wol(phydev);
	} else
		return genphy_suspend(phydev);
}

int ip101a_g_resume(struct phy_device *phydev)
{
	if (device_may_wakeup(&phydev->dev)) {
		/* Restore the default WoL+ settings */
		phy_write_page(phydev, IP101A_G_WOL_CTRL, 4,
			       IP101A_G_DEFAULT_WOL);
		return 0;
	}
	return genphy_resume(phydev);
}

static int ip101a_config_init(struct phy_device *phydev)
{
	int c;

	c = ip1xx_reset(phydev);
	if (c < 0)
		return c;

	c = phy_write(phydev, IP101A_IRQ_CONF_STATUS, IP101A_IRQ_DEFAULT);
	if (c < 0)
		return c;

	/* Set the WoL+ with the default configuration */
	phy_write_page(phydev, IP101A_G_WOL_CTRL, 4, IP101A_G_DEFAULT_WOL);
	DBG("IC+101A/G WoL+ crtl register 0x%x\n",
	    phy_read_page(phydev, IP101A_G_WOL_CTRL, 4));

	c = phy_read(phydev, IP10XX_SPEC_CTRL_STATUS);
	if (c < 0)
		return c;

	/* Enable Auto Power Saving mode */
	c |= IP101A_APS_ON;
	c = phy_write(phydev, IP10XX_SPEC_CTRL_STATUS, c);
	if (c < 0)
		return c;

	return 0;
}

static int ip175c_read_status(struct phy_device *phydev)
{
	if (phydev->addr == 4) /* WAN port */
		genphy_read_status(phydev);
	else
		/* Don't need to read status for switch ports */
		phydev->irq = PHY_IGNORE_INTERRUPT;

	return 0;
}

static int ip175c_config_aneg(struct phy_device *phydev)
{
	if (phydev->addr == 4) /* WAN port */
		genphy_config_aneg(phydev);

	return 0;
}

static int ip101a_ack_interrupt(struct phy_device *phydev)
{
	int ret;

	ret = phy_read(phydev, IP101A_IRQ_CONF_STATUS);
	if (ret < 0)
		return ret;

	ret = phy_read_page(phydev, IP101A_G_WOL_STATUS, 17);
	if (ret < 0)
		return ret;
	DBG("WOL status register 0x%x\n", ret);

	return 0;
}

static struct phy_driver ip175c_driver = {
	.phy_id		= 0x02430d80,
	.name		= "ICPlus IP175C",
	.phy_id_mask	= 0x0ffffff0,
	.features	= PHY_BASIC_FEATURES,
	.config_init	= &ip175c_config_init,
	.config_aneg	= &ip175c_config_aneg,
	.read_status	= &ip175c_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
};

static struct phy_driver ip1001_driver = {
	.phy_id		= 0x02430d90,
	.name		= "ICPlus IP1001",
	.phy_id_mask	= 0x0ffffff0,
	.features	= PHY_GBIT_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
	.config_init	= &ip1001_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
};

static struct phy_driver ip101a_driver = {
	.phy_id		= 0x02430c54,
	.name		= "ICPlus IP101A",
	.phy_id_mask	= 0x0ffffff0,
	.features	= PHY_BASIC_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
	.config_init	= &ip101a_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.flags		= PHY_HAS_INTERRUPT,
	.wol_supported	= WAKE_PHY | WAKE_UCAST | WAKE_MAGIC,
	.ack_interrupt	= ip101a_ack_interrupt,
	.suspend	= ip101a_g_suspend,
	.resume		= ip101a_g_resume,
	.driver		= { .owner = THIS_MODULE,},
};

static int __init icplus_init(void)
{
	int ret = 0;

	ret = phy_driver_register(&ip1001_driver);
	if (ret < 0)
		return -ENODEV;

	ret = phy_driver_register(&ip101a_driver);
	if (ret < 0)
		return -ENODEV;

	return phy_driver_register(&ip175c_driver);
}

static void __exit icplus_exit(void)
{
	phy_driver_unregister(&ip1001_driver);
	phy_driver_unregister(&ip101a_driver);
	phy_driver_unregister(&ip175c_driver);
}

module_init(icplus_init);
module_exit(icplus_exit);
