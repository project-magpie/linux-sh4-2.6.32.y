/*
 * arch/sh/boards/st/mb519/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7200 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <asm/irq-ilc.h>
#include <asm/io.h>
#include <asm/mach/harp.h>
#include "../common/epld.h"

static int ascs[2] __initdata = { 2, 3 };

static void __init mb519_setup(char** cmdline_p)
{
	printk("STMicroelectronics STx7200 Mboard initialisation\n");

	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  = (
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_SPI_CAPABILITY) |
		ssc4_has(SSC_I2C_CAPABILITY)),
};

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

static struct plat_stmmacphy_data phy_private_data[2] = {
{
	/* MAC0: STE101P */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
}, {
	/* MAC1: SMSC LAN 8700 */
	.bus_id = 1,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
} };

static struct platform_device mb519_phy_devices[2] = {
{
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			/* This should be:
			 * .start = ILC_IRQ(93),
			 * .end = ILC_IRQ(93),
			 * but because the mb519 uses the MII0_MDINT line
			 * as MODE4, and the STE101P MDINT pin is O/C,
			 * there may or maynot be a pull-up resistor
			 * depending on switch SW1-4. Most of the time there
			 * isn't, so disable the interrupt.
			 */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[0],
	 }
}, {
	.name		= "stmmacphy",
	.id		= 1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= ILC_IRQ(95),
			.end	= ILC_IRQ(95),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[1],
	}
} };

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= EPLD_BASE,
			.end	= EPLD_BASE + EPLD_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 16,
	},
};

static struct mtd_partition nand_partitions[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct nand_config_data mb519_nand_config = {
	.emi_bank		= 1,
	.emi_withinbankoffset	= 0,

	/* Timing data for STEM Module MB588A (ST-NAND512W3A2C) */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	= 40,		 /* times in ns */
		.rd_oee_start	= 0,
		.rd_oee_end	= 10,
		.rd_latchpoint	= 10,
		.busreleasetime = 10,

		.wr_cycle_time	= 40,
		.wr_oee_start	= 0,
		.wr_oee_end	= 10,
	},

	.chip_delay		= 20,
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
	.rbn_port		= -1,
	.rbn_pin		= -1,
};


static struct platform_device *mb519_devices[] __initdata = {
	&epld_device,
	&physmap_flash,
	&mb519_phy_devices[0],
	&mb519_phy_devices[1],
};

static int __init device_init(void)
{
	unsigned int epld_rev;
	unsigned int pcb_rev;

	epld_rev = epld_read(EPLD_EPLDVER);
	pcb_rev = epld_read(EPLD_PCBVER);
	printk("mb519 PCB rev %X EPLD rev %dr%d\n",
	       pcb_rev,
	       epld_rev >> 4, epld_rev & 0xf);

	stx7200_configure_pwm(&pwm_private_info);
	stx7200_configure_ssc(&ssc_private_info);
	stx7200_configure_usb();
	stx7200_configure_ethernet(0, 0, 1, 0);
	// stx7200_configure_ethernet(1, 0, 1, 1);
	stx7200_configure_lirc();
	stx7200_configure_nand(&mb519_nand_config);

	return platform_add_devices(mb519_devices, ARRAY_SIZE(mb519_devices));
}
arch_initcall(device_init);

static void __iomem *mb519_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb519_init_irq(void)
{
	epld_early_init(&epld_device);

#if 0
	/* The off chip interrupts on the mb519 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active.
	 *
	 * Note that this changed between EPLD rev 1r2 and 1r3. This is correct
	 * for 1r3 which should be the most common now.
	 */
	ctrl_outw(1<<4, EPLD_IntMask0Set); /* IntPriority(4) <= not STEM_notINTR0 */
#endif
}

struct sh_machine_vector mv_mb519 __initmv = {
	.mv_name		= "mb519",
	.mv_setup		= mb519_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb519_init_irq,
	.mv_ioport_map		= mb519_ioport_map,
};
