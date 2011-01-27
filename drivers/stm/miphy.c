/*
 * STMicroelectronics MiPHY driver
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <asm/processor.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stm/platform.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/miphy.h>

#define MIPHY_RESET			0x00
#define RST_RX				(1<<4)

#define MIPHY_STATUS			0x01
#define MIPHY_CONTROL			0x02
#define DIS_LINK_RST			(1<<4)

#define MIPHY_INT_STATUS		0x04
#define BITUNLOCK_INT			(1<<1)
#define SYMBUNLOCK_INT			(1<<2)
#define FIFOOVERLAP_INT			(1<<3)

#define MIPHY_BOUNDARY_1		0x10
#define POWERSEL_SEL			(1<<2)
#define SPDSEL_SEL			(1<<0)

#define MIPHY_BOUNDARY_3		0x12
#define RX_LSPD				(1<<5)

#define MIPHY_COMPENS_CONTROL_1		0x40

#define MIPHY_IDLL_TEST			0x72
#define START_CLK_HF			(1<<6)
#define STOP_CLK_HF			(1<<7)

#define MIPHY_DES_BITLOCK_CFG		0x85
#define CLEAR_BIT_UNLOCK_FLAG		(1<<0)
#define UPDATE_TRANS_DENSITY		(1<<1)

#define MIPHY_DES_BITLOCK		0x86

#define MIPHY_DES_BITLOCK_STATUS	0x88
#define BIT_LOCK			(1<<0)
#define BIT_LOCK_FAILED			(1<<1)
#define BIT_UNLOCK			(1<<2)
#define TRANS_DENSITY_UPDATED		(1<<3)


static struct miphy_device *miphy_dev;


/* Start functions for miphy port
 * Ideally all the start functions should be identical, however
 * they tend to be different in physical arrangement.
 * The physicall (IP) arrangement lead to different start functions.
 */

/*
 * MiPhy Port 0 Start function for SATA
 */

static void tap_miphy_start_port0(struct miphy_if_ops *ops)
{
	int timeout;
	void (*reg_write)(int port, u8 addr, u8 data) 	= ops->reg_write;
	u8 (*reg_read)(int port, u8 addr)		= ops->reg_read;

	/* TODO: Get rid of this */
	if (cpu_data->type == CPU_STX7108) {
		/*Force SATA port 1 in Slumber Mode */
		reg_write(1, 0x11, 0x8);
		/*Force Power Mode selection from MiPHY soft register 0x11 */
		reg_write(1, 0x10, 0x4);
	}

	/* Force Macro1 in reset and request PLL calibration reset */

	/* Force PLL calibration reset, PLL reset and assert
	 * Deserializer Reset */
	reg_write(0, 0x00, 0x16);
	reg_write(0, 0x11, 0x0);
	/* Force macro1 to use rx_lspd, tx_lspd (by default rx_lspd
	 * and tx_lspd set for Gen1)  */
	reg_write(0, 0x10, 0x1);

	/* Force Recovered clock on first I-DLL phase & all
	 * Deserializers in HP mode */

	/* Force Rx_Clock on first I-DLL phase on macro1 */
	reg_write(0, 0x72, 0x40);
	/* Force Des in HP mode on macro1 */
	reg_write(0, 0x12, 0x00);

	/* Wait for HFC_READY = 0 */
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (reg_read(0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	/* Restart properly Process compensation & PLL Calibration */

	/* Set properly comsr definition for 30 MHz ref clock */
	reg_write(0, 0x41, 0x1E);
	/* comsr compensation reference */
	reg_write(0, 0x42, 0x28);
	/* Set properly comsr definition for 30 MHz ref clock */
	reg_write(0, 0x41, 0x1E);
	/* comsr cal gives more suitable results in fast PVT for comsr
	   used by TX buffer to build slopes making TX rise/fall fall
	   times. */
	reg_write(0, 0x42, 0x33);
	/* Force VCO current to value defined by address 0x5A */
	reg_write(0, 0x51, 0x2);
	/* Force VCO current to value defined by address 0x5A */
	reg_write(0, 0x5A, 0xF);
	/* Enable auto load compensation for pll_i_bias */
	reg_write(0, 0x47, 0x2A);
	/* Force restart compensation and enable auto load for
	 * Comzc_Tx, Comzc_Rx & Comsr on macro1 */
	reg_write(0, 0x40, 0x13);

	/* Wait for comzc & comsr done */
	while ((reg_read(0, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/* Recommended settings for swing & slew rate FOR SATA GEN 1
	 * from CPG */
	reg_write(0, 0x20, 0x00);
	/* (Tx Swing target 500-550mV peak-to-peak diff) */
	reg_write(0, 0x21, 0x2);
	/* (Tx Slew target120-140 ps rising/falling time) */
	reg_write(0, 0x22, 0x4);

	/* Force Macro1 in partial mode & release pll cal reset */
	reg_write(0, 0x00, 0x10);
	udelay(10);

#if 0
	/* SSC Settings. SSC will be enabled through Link */
	reg_write(0, 0x53, 0x00); /* pll_offset */
	reg_write(0, 0x54, 0x00); /* pll_offset */
	reg_write(0, 0x55, 0x00); /* pll_offset */
	reg_write(0, 0x56, 0x04); /* SSC Ampl=0.48% */
	reg_write(0, 0x57, 0x11); /* SSC Ampl=0.48% */
	reg_write(0, 0x58, 0x00); /* SSC Freq=31KHz */
	reg_write(0, 0x59, 0xF1); /* SSC Freq=31KHz */
	/*SSC Settings complete*/
#endif

	reg_write(0, 0x50, 0x8D);
	reg_write(0, 0x50, 0x8D);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13) */

	while ((reg_read(0, 0x01) & 0x03) != 0x03)
		cpu_relax();

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	reg_write(0, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	reg_write(0, 0x72, 0x00);

	/* Deassert deserializer reset */
	reg_write(0, 0x00, 0x00);
	/* des_bit_lock_en is set */
	reg_write(0, 0x02, 0x08);

	/* bit lock detection strength */
	reg_write(0, 0x86, 0x61);
}

/*
 * MiPhy Port 1 Start function for SATA
 */
static void tap_miphy_start_port1(struct miphy_if_ops *ops)
{
	int timeout;
	void (*reg_write)(int port, u8 addr, u8 data) 	= ops->reg_write;
	u8 (*reg_read)(int port, u8 addr)		= ops->reg_read;

	/* Force PLL calibration reset, PLL reset and assert Deserializer
	 * Reset */
	reg_write(1, 0x00, 0x2);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro2 */
	reg_write(1, 0x40, 0x13);

	/* Force PLL reset  */
	reg_write(0, 0x00, 0x2);
	/* Set properly comsr definition for 30 MHz ref clock */
	reg_write(0, 0x41, 0x1E);
	/* to get more optimum result on comsr calibration giving faster
	 * rise/fall time in SATA spec Gen1 useful for some corner case.*/
	reg_write(0, 0x42, 0x33);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro1 */
	reg_write(0, 0x40, 0x13);

	/*Wait for HFC_READY = 0*/
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (reg_read(0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	reg_write(1, 0x11, 0x0);
	/* Force macro2 to use rx_lspd, tx_lspd  (by default rx_lspd and
	 * tx_lspd set for Gen1) */
	reg_write(1, 0x10, 0x1);
	/* Force Rx_Clock on first I-DLL phase on macro2*/
	reg_write(1, 0x72, 0x40);
	/* Force Des in HP mode on macro2 */
	reg_write(1, 0x12, 0x00);

	while ((reg_read(1, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/*RECOMMENDED SETTINGS for Swing & slew rate FOR SATA GEN 1 from CPG*/
	reg_write(1, 0x20, 0x00);
	/*(Tx Swing target 500-550mV peak-to-peak diff) */
	reg_write(1, 0x21, 0x2);
	/*(Tx Slew target120-140 ps rising/falling time) */
	reg_write(1, 0x22, 0x4);
	/*Force Macr21 in partial mode & release pll cal reset */
	reg_write(1, 0x00, 0x10);
	udelay(10);
	/* Release PLL reset  */
	reg_write(0, 0x00, 0x0);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13)*/
	while ((reg_read(1, 0x01) & 0x03) != 0x03)
		cpu_relax();

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	reg_write(1, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	reg_write(1, 0x72, 0x00);

	/* Deassert deserializer reset */
	reg_write(1, 0x00, 0x00);
	/*des_bit_lock_en is set */
	reg_write(1, 0x02, 0x08);

	/*bit lock detection strength */
	reg_write(1, 0x86, 0x61);
}

static int tap_miphy_sata_start(int port, struct miphy_if *iface)
{
	int rval = 0;
	switch (port) {
	case 0:
		tap_miphy_start_port0(iface->ops);
		break;
	case 1:
		tap_miphy_start_port1(iface->ops);
		break;
	default:
		rval = -EINVAL;
	}
	return rval;
}

static int tap_miphy_pcie_start(int port, struct miphy_if *iface)
{
	/* TODO */
	return -1;
}

/*
 * MiPhy Port 0 & 1 Start function for SATA
 * only for 7108 CUT2
 */

static int mp_miphy_sata_start(int port, struct miphy_if *iface)
{
	unsigned int regvalue;
	int timeout;
	void (*reg_write)(int port, u8 addr, u8 data) 	= iface->ops->reg_write;
	u8 (*reg_read)(int port, u8 addr)		= iface->ops->reg_read;

	if (port < 0 || port > 1)
		return -EINVAL;

	/* Force PLL calibration reset, PLL reset
	 * and assert Deserializer Reset */
	reg_write(port, 0x00, 0x16);
	reg_write(port, 0x11, 0x0);
	/* Force macro1 to use rx_lspd, tx_lspd
	 * (by default rx_lspd and tx_lspd set for Gen1) */
	reg_write(port, 0x10, 0x1);
	/* Force Rx_Clock on first I-DLL phase on macro1 */
	reg_write(port, 0x72, 0x40);
	/* Force Des in HP mode on macro1 */
	reg_write(port, 0x12, 0x00);

	/*Wait for HFC_READY = 0*/
	timeout = 50;
	while (timeout-- && (reg_read(port, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	/*Set properly comsr definition for 30 MHz ref clock */
	reg_write(port, 0x41, 0x1E);
	 /*Set properly comsr definition for 30 MHz ref clock */
	reg_write(port, 0x42, 0x33);
	/* Force VCO current to value defined by address 0x5A
	 * and disable PCIe100Mref bit */
	reg_write(port, 0x51, 0x2);
	/* Enable auto load compensation for pll_i_bias */
	reg_write(port, 0x47, 0x2A);

	/* Force restart compensation and enable auto load for
	 * Comzc_Tx, Comzc_Rx & Comsr on macro1 */
	reg_write(port, 0x40, 0x13);
	while ((reg_read(port, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/* STOS_SemaphoreWait(MiPHY_Int);  Wait for Compensation
	 * Completion Interrupt : Not using MiPHY Interrupt for now */
	/* Recommended Settings for Swing & slew rate FOR SATA GEN 1 from CCI
	 * conf gen sel = 00b to program Gen1 banked registers &
	 * VDDT filter ON */
	reg_write(port, 0x20, 0x10);
	/*(Tx Swing target 500-550mV peak-to-peak diff) */
	reg_write(port, 0x21, 0x3);
	/*(Tx Slew target120-140 ps rising/falling time) */
	reg_write(port, 0x22, 0x4);
	/*Force Macro1 in partial mode & release pll cal reset */
	reg_write(port, 0x00, 0x10);
	udelay(100);
	/* SSC Settings. SSC will be enabled through Link */
	/*  pll_offset */
	reg_write(port, 0x53, 0x00);
	/*  pll_offset */
	reg_write(port, 0x54, 0x00);
	/*  pll_offset */
	reg_write(port, 0x55, 0x00);
	/*  SSC Ampl.=0.4%  */
	reg_write(port, 0x56, 0x03);
	/*  SSC Ampl.=0.4% */
	reg_write(port, 0x57, 0x63);
	/*  SSC Freq=31KHz */
	reg_write(port, 0x58, 0x00);
	/*  SSC Freq=31KHz   */
	reg_write(port, 0x59, 0xF1);
	/*SSC Settings complete*/
	reg_write(port, 0x50, 0x8D);
	/*MIPHY PLL ratio */
	reg_read(port, 0x52);
	/*  Wait for phy_ready */
	/* When phy is in ready state ( register 0x01 reads 0x13)*/
	regvalue = reg_read(port, 0x01);
	timeout = 50;
	while (timeout-- && ((regvalue & 0x03) != 0x03)) {
		regvalue = reg_read(port, 0x01);
		udelay(2000);
	}
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);
	if ((regvalue & 0x03) == 0x03) {
		/* Enable macro1 to use rx_lspd  &
		 * tx_lspd from link interface */
		reg_write(port, 0x10, 0x00);
		/* Release Rx_Clock on first I-DLL phase on macro1 */
		reg_write(port, 0x72, 0x00);
		/* Assert deserializer reset */
		reg_write(port, 0x00, 0x10);
		/* des_bit_lock_en is set */
		reg_write(port, 0x02, 0x08);
		/* bit lock detection strength */
		reg_write(port, 0x86, 0x61);
		/* Deassert deserializer reset */
		reg_write(port, 0x00, 0x00);
	}
	return 0;
}

/*
 * MiPhy Port 0 & 1 Start function for PCIE
 * only for 7108 CUT2
 */
static int mp_miphy_pcie_start(int port, struct miphy_if *iface)
{
	/* TODO */
	return -1;
}


/****************************************************************************
 * 	MiPHY Generic functions available for other drivers
 */
static void stm_miphy_write(struct stm_miphy *miphy, u8 addr, u8 data)
{
	struct miphy_device *dev = miphy->dev;
	int type = miphy->interface;
	int port = miphy->port;
	struct miphy_if *iface;

	list_for_each_entry(iface, &dev->ifaces, list)
		if (iface->type == type) {
			down(&dev->mutex);
			iface->ops->reg_write(port, addr, data);
			up(&dev->mutex);
		}
}

static u8 stm_miphy_read(struct stm_miphy *miphy, u8 addr)
{
	struct miphy_device *dev = miphy->dev;
	int type = miphy->interface;
	int port = miphy->port;
	int rval = 0;
	struct miphy_if *iface;

	list_for_each_entry(iface, &dev->ifaces, list)
		if (iface->type == type) {
			down(&dev->mutex);
			rval = iface->ops->reg_read(port, addr);
			up(&dev->mutex);
		}
	return rval;
}

#ifdef DEBUG
/* Pretty-ish print of Miphy registers, helpful for debugging */
void stm_miphy_dump_registers(struct stm_miphy *miphy)
{
       printk(KERN_INFO "MIPHY_RESET (0x0): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_RESET));
       printk(KERN_INFO "MIPHY_STATUS (0x1): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_STATUS));
       printk(KERN_INFO "MIPHY_CONTROL (0x1): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_CONTROL));
       printk(KERN_INFO "MIPHY_INT_STATUS (0x4): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_INT_STATUS));
       printk(KERN_INFO "MIPHY_BOUNDARY_1 (0x10): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_BOUNDARY_1));
       printk(KERN_INFO "MIPHY_BOUNDARY_3 (0x12): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_BOUNDARY_3));
       printk(KERN_INFO "MIPHY_COMPENS_CONTROL_1 (0x40): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_COMPENS_CONTROL_1));
       printk(KERN_INFO "MIPHY_IDLL_TEST (0x72): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_IDLL_TEST));
       printk(KERN_INFO "MIPHY_DES_BITLOCK_CFG (0x85): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_DES_BITLOCK_CFG));
       printk(KERN_INFO "MIPHY_DES_BITLOCK (0x86): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_DES_BITLOCK));
       printk(KERN_INFO "MIPHY_DES_BITLOCK_STATUS (0x88): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_DES_BITLOCK_STATUS));
}
#endif /* DEBUG */

static int stm_miphy_start(struct stm_miphy *miphy)
{
	struct miphy_device *dev = miphy->dev;
	int type = miphy->interface;
	int port = miphy->port;
	int rval = -ENODEV;
	struct miphy_if *iface;

	list_for_each_entry(iface, &dev->ifaces, list)
		if (iface->type == type) {
			down(&dev->mutex);
			if (miphy->mode == SATA_MODE)
				rval = iface->start_sata(port, iface);
			else if (miphy->mode == PCIE_MODE)
				rval = iface->start_pcie(port, iface);
			up(&dev->mutex);
		}

	/* Clear the contents of interrupt control register,
	   excluding fifooverlap_int */
	stm_miphy_write(miphy, MIPHY_INT_STATUS, 0x77);

	return rval;
}

static struct miphy_device *miphy_device_new(void)
{
	/* Singleton device */
	if (!miphy_dev)	{
		miphy_dev = kzalloc(sizeof(struct miphy_device), GFP_KERNEL);
		miphy_dev->user_count = 0;
		init_MUTEX(&miphy_dev->mutex);
		INIT_LIST_HEAD(&miphy_dev->ifaces);
	}
	return miphy_dev;
}

struct miphy_device *miphy_if_register(enum miphy_if_type type, void *if_data,
					struct miphy_if_ops *ops)
{
	struct miphy_if *iface;
	struct miphy_device *dev;
	if (!ops)
		return NULL;

	dev =  miphy_device_new();

	iface = kzalloc(sizeof(struct miphy_if), GFP_KERNEL);
	iface->ops = ops;
	switch (type) {
	case  TAP_IF:
		iface->start_sata = tap_miphy_sata_start;
		iface->start_pcie = tap_miphy_pcie_start;
		break;
	case  UPORT_IF:
		iface->start_sata = mp_miphy_sata_start;
		iface->start_pcie = mp_miphy_pcie_start;
		break;
	default:
		iface->start_sata = NULL;
		iface->start_pcie = NULL;
	}
	iface->data = if_data;
	iface->type = type;
	list_add(&iface->list, &dev->ifaces);
	++dev->user_count;

	return dev;
}

int miphy_if_unregister(struct miphy_device *dev, enum miphy_if_type type)
{
	struct miphy_if *iface;
	list_for_each_entry(iface, &dev->ifaces, list) {
		if (iface->type == type) {
			list_del(&iface->list);
			--dev->user_count;
			kfree(iface);
			return 0;
		}
	}
	return -ENODEV;
}



int stm_miphy_init(struct stm_miphy *miphy)
{

	if (!miphy_dev) {
		printk(KERN_ERR"No MiPHY Instance created or "
			"No register r/w interfaces registered with MiPHY\n");
		BUG();
	}

	miphy->dev 			= miphy_dev;
	miphy->start 			= stm_miphy_start;
	stm_miphy_start(miphy);

	return 0;
}

void stm_miphy_exit(struct stm_miphy *miphy)
{
	if (!miphy->dev->user_count)
		kfree(miphy_dev);
}
