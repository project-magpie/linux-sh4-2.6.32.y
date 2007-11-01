/*
 * drivers/net/stmmac/stmmac_mdio.c
 *
 * STMMAC Ethernet Driver -- MDIO bus implementation
 * Provides Bus interface for MII registers
 *
 * Author: Carl Shaw <carl.shaw@st.com>
 * Maintainer: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * Copyright (c) 2006-2007 STMicroelectronics
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/phy.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "stmmac.h"

/**
 * stmmac_mdio_read
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * Description: it reads data from the MII register from within the phy device.
 */
int stmmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct eth_driver_local *lp = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = lp->mac->hw.mii.addr;
	unsigned int mii_busy = lp->mac->hw.mii.addr_busy;
	unsigned int mii_data = lp->mac->hw.mii.data;

	int data;
	u16 regValue = (((phyaddr << 11) & (0x0000F800)) |
			((phyreg << 6) & (0x000007C0)));

	while (((readl(ioaddr + mii_address)) & mii_busy) == 1) {
	}

	writel(regValue, ioaddr + mii_address);

	while (((readl(ioaddr + mii_address)) & mii_busy) == 1) {
	}

	/* Read the data from the MII data register */
	data = (int)readl(ioaddr + mii_data);
	return data;
}

/**
 * stmmac_mdio_write
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * @phydata: phy data
 * Description: it writes the data intto the MII register from within the device.
 */
int stmmac_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg, u16 phydata)
{
	struct net_device *ndev = bus->priv;
	struct eth_driver_local *lp = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = lp->mac->hw.mii.addr;
	unsigned int mii_busy = lp->mac->hw.mii.addr_busy;
	unsigned int mii_write = lp->mac->hw.mii.addr_write;
	unsigned int mii_data = lp->mac->hw.mii.data;

	u16 value =
	    (((phyaddr << 11) & (0x0000F800)) | ((phyreg << 6) & (0x000007C0)))
	    | mii_write;

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + mii_address)) & mii_busy) == 1) {
	}

	/* Set the MII address register to write */
	writel(phydata, ioaddr + mii_data);
	writel(value, ioaddr + mii_address);

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + mii_address)) & mii_busy) == 1) {
	}

	/* NOTE: we need to perform this "extra" read in order to fix an error
	 * during the write operation */
	stmmac_mdio_read(bus, phyaddr, phyreg);
	return 0;
}

/* Resets the MII bus */
int stmmac_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct eth_driver_local *lp = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = lp->mac->hw.mii.addr;

	printk(KERN_DEBUG "stmmac_mdio_reset: called!\n");

	if (lp->phy_reset) {
		printk("stmmac_mdio_reset: calling phy_reset\n");
		return lp->phy_reset(lp->bsp_priv);
	}

	/* This is a workaround for problems with the STE101P PHY.
	 * It doesn't complete its reset until at least one clock cycle
	 * on MDC, so perform a dummy mdio read.
	 */
	writel(0, ioaddr + mii_address);

	return 0;
}

/**
 * stmmac_mdio_register
 * @ndev: net device structure
 * Description: it registers the MII bus
 */
int stmmac_mdio_register(struct net_device *ndev)
{
	int err = 0;
	struct mii_bus *new_bus = kzalloc(sizeof(struct mii_bus), GFP_KERNEL);
	int *irqlist = kzalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	struct eth_driver_local *lp = netdev_priv(ndev);

	if (new_bus == NULL)
		return -ENOMEM;

	/* Assign IRQ to phy at address phy_addr */
	irqlist[lp->phy_addr] = lp->phy_irq;

	new_bus->name = "STMMAC MII Bus",
	    new_bus->read = &stmmac_mdio_read,
	    new_bus->write = &stmmac_mdio_write,
	    new_bus->reset = &stmmac_mdio_reset, new_bus->id = (int)lp->bus_id;
	new_bus->priv = ndev;
	new_bus->irq = irqlist;
	new_bus->phy_mask = lp->phy_mask;
	new_bus->dev = 0;	/* FIXME */
	printk(KERN_DEBUG "calling mdiobus_register\n");
	err = mdiobus_register(new_bus);
	printk(KERN_DEBUG "calling mdiobus_register done\n");
	if (err != 0) {
		printk(KERN_ERR "%s: Cannot register as MDIO bus\n",
		       new_bus->name);
		goto bus_register_fail;
	}

	lp->mii = new_bus;
	return 0;

      bus_register_fail:
	kfree(new_bus);
	return err;
}

/**
 * stmmac_mdio_unregister
 * @ndev: net device structure
 * Description: it unregisters the MII bus
 */
int stmmac_mdio_unregister(struct net_device *ndev)
{
	struct eth_driver_local *lp = netdev_priv(ndev);

	mdiobus_unregister(lp->mii);
	lp->mii->priv = NULL;
	kfree(lp->mii);

	return 0;
}
