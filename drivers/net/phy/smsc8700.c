/* ============================================================================
 * This is a driver for the SMSC LAN8700 PHY controller
 *
 * Copyright (C) 2006 ST Microelectronics (R&D) Ltd.
 *
 * ----------------------------------------------------------------------------
 * Changelog:
 *   Sep 2006
 *	Converted PHY driver to new 2.6.17 PHY device
 *	Carl Shaw <carl.shaw@st.com>
 *	Added contributions by Steve Glendinning <Steve.Glendinning@smsc.com>
 *   July 2006
 *	Copied from the STe101p PHY driver (originally written by
 *	Giuseppe Cavallaro <peppe.cavallaro@st.com>) by
 *	Nigel Hathaway <nigel.hathaway@st.com>
 * ===========================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/delay.h>

#undef PHYDEBUG
#define DEFAULT_PHY_ID  0
#define RESOURCE_NAME	"lan8700"


/* LAN8700 vendor-specific registers */
#define MII_REV		0x10	/* Silicon revision register */
#define MII_MCSR	0x11	/* Mode control/status register */
#define MII_SMR		0x12	/* Special modes register */
#define MII_SCSIR	0x1b	/* Special control/status indications register */
#define MII_SITCR	0x1c	/* Special internal testability controls register */
#define MII_ISR		0x1d	/* Interrupt source register */
#define MII_IMR		0x1e	/* Interrupt mask register */
#define MII_PSCSR	0x1f	/* PHY Special control/status register */

#define MII_SCSIR_AMDIXCTRL	0x8000	/* Auto-MDIX control (1 to disable AMDIX) */
#define MII_SCSIR_CH_SELECT	0x2000	/* MDIX control (1 for AMDIX) */

/* LAN8700 Interrupt Source Register values */
#define MII_ISR_ANPR	0x0002	/* Auto-Negotiation page received */
#define MII_ISR_PDF	0x0004	/* Parallel detection fault */
#define MII_ISR_ANLA	0x0008	/* Auto-Negotiation LP Acknowledge */
#define MII_ISR_LINK	0x0010	/* Link down (status negated) */
#define MII_ISR_RFLT	0x0020	/* Remote fault detected */
#define MII_ISR_ANC	0x0040	/* Auto-Negotiation completed */
#define MII_ISR_EOG	0x0080	/* ENERGYON generated */

/* LAN8700 PHY Special control/status register values */
#define MII_PSCSR_ANC		0x1000	/* Auto-Negotiation done */
#define MII_PSCSR_SPD_MASK	0x001C	/* HCDSPEED value */
#define MII_PSCSR_SPD_10	0x0004	/* 10Mbps */
#define MII_PSCSR_SPD_100	0x0008	/* 100Mbps */
#define MII_PSCSR_SPD_FDPLX	0x0010	/* Full Duplex */

#define MII_IMR_INTSRC_ENERGYON             0x0080
#define MII_IMR_INTSRC_AUTONEGCOM           0x0040
#define MII_IMR_INTSRC_REMFLT               0x0020
#define MII_IMR_INTSRC_LNKDN                0x0010
#define MII_IMR_INTSRC_AUTONEGLP            0x0008
#define MII_IMR_INTSRC_PARFLT               0x0004
#define MII_IMR_INTSRC_AUTONEGPR            0x0002

#define MII_IMR_DEFAULT_MASK (MII_IMR_INTSRC_AUTONEGCOM | MII_IMR_INTSRC_REMFLT | MII_IMR_INTSRC_LNKDN)


/* LAN8700 phy identifier values */
#define LAN8700_PHY_ID		0x0007c0c0
#define LAN8700_PHY_LO_ID_REVA 	0xc0c1
#define LAN8700_PHY_LO_ID_REVB 	0xc0c2
#define LAN8700_PHY_LO_ID_REVC 	0xc0c3
#define LAN8700_PHY_LO_ID_REVD 	0xc0c4
#define LAN8700_PHY_LO_ID_REVE 	0xc0c5
#define LAN8700_PHY_LO_ID_REVF 	0xc0c6
#define LAN8700_PHY_LO_ID_REVG 	0xc0c7

static int lan8700_config_init(struct phy_device *phydev)
{
	int value, err;
	int timeout = 1000;

	/* Enable the MODE pins to enable autoneg */
	value = phy_read(phydev, MII_SMR);
	if (value < 0)
                return value;
	value |= 0x0E0;
	err = phy_write(phydev, MII_SMR, value);
	if (err < 0)
		return err;
	/* Software Reset PHY */
	value = phy_read(phydev, MII_BMCR);
	if (value < 0)
		return value;

	value |= BMCR_RESET;
	err = phy_write(phydev, MII_BMCR, value);
	if (err < 0)
		return err;

	do{
		mdelay(1);
		value = phy_read(phydev, MII_BMCR);
	} while ((value & BMCR_RESET) && (--timeout));

	if (!timeout){
		printk(KERN_ERR "LAN8700 PHY timed out during reset!\n");
		return -1;
	}

	return 0;
}

static int lan8700_config_intr(struct phy_device *phydev)
{
        int err, value;

        if(phydev->interrupts == PHY_INTERRUPT_ENABLED){
		/* Enable interrupts */
		err = phy_write(phydev, MII_IMR, MII_IMR_DEFAULT_MASK);
		/* clear any pending interrupts */
		if (err == 0){
			value = phy_read(phydev, MII_ISR);
			if (value <0){
				err = value;
			}
		}
	} else
		err = phy_write(phydev, MII_IMR, 0);

        return err;
}

static int lan8700_ack_interrupt(struct phy_device *phydev)
{
        int err = phy_read(phydev, MII_ISR);
        if (err < 0)
                return err;

        return 0;
}

static struct phy_driver lan8700_pdriver = {
        .phy_id         = LAN8700_PHY_ID,
        .phy_id_mask    = 0xfffffff0,
        .name           = "LAN8700",
        .features       = PHY_BASIC_FEATURES | SUPPORTED_Pause,
        .flags          = PHY_HAS_INTERRUPT,
	.config_init    = lan8700_config_init,
        .config_aneg    = genphy_config_aneg,
        .read_status    = genphy_read_status,
        .ack_interrupt  = lan8700_ack_interrupt,
        .config_intr    = lan8700_config_intr,
        .driver         = { .owner = THIS_MODULE,},
};

static int __init lan8700_init(void)
{
	int retval;

	return phy_driver_register(&lan8700_pdriver);
}

static void __exit lan8700_exit(void)
{
	phy_driver_unregister(&lan8700_pdriver);
}

module_init(lan8700_init);
module_exit(lan8700_exit);

MODULE_DESCRIPTION("SMSC LAN8700 PHY driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
