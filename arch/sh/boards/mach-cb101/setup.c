/*
 * arch/sh/boards/st/cb101/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics cb101 board support.
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
#include <linux/delay.h>
#include <sound/stm.h>

static int ascs[2] __initdata = { 2, 3 };

void __init cb101_setup(char** cmdline_p)
{
	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 1);
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_SPI_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY) |
		ssc4_has(SSC_SPI_CAPABILITY) |
		ssc5_has(SSC_I2C_CAPABILITY) ,
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

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
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

static int phy_reset(void* bus)
{
	static struct stpio_pin *ethreset = NULL;

	if (ethreset == NULL) {
		ethreset = stpio_request_set_pin(4, 7, "STE101P_RST", STPIO_OUT, 1);
	}

	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);
	udelay(1000);
	stpio_set_pin(ethreset, 1);

	return 0;
}


static struct plat_stmmacphy_data phy_private_data = {
	/* MAC0: STE101P */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = phy_reset,
};

static struct platform_device cb101_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			/* See mb519 for why we disable interrupts here */
			.start	= -1,
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	 }
};

static struct mtd_partition nand1_parts[] = {
	{
		.name	= "NAND1 root",
		.offset	= 0,
		.size 	= 0x00800000,
	}, {
		.name	= "NAND1 home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct mtd_partition nand2_parts[] = {
	{
		.name	= "NAND2 data",
		.offset	= 0,
		.size	= MTDPART_SIZ_FULL
	},
};

/* Timing data for onboard NAND */
static struct emi_timing_data nand_timing_data = {
	.rd_cycle_time	 = 40,		 /* times in ns */
	.rd_oee_start	 = 0,
	.rd_oee_end	 = 10,
	.rd_latchpoint	 = 10,

	.busreleasetime  = 10,
	.wr_cycle_time	 = 40,
	.wr_oee_start	 = 0,
	.wr_oee_end	 = 10,

	.wait_active_low = 0,

};

static struct nand_config_data cb101_nand_config[] = {
{
	.emi_bank		= 1,
	.emi_withinbankoffset	= 0,

	.emi_timing_data	= &nand_timing_data,

	.chip_delay		= 25,
	.mtd_parts		= nand1_parts,
	.nr_parts		= ARRAY_SIZE(nand1_parts),
	.rbn_port		= 2,
	.rbn_pin		= 7,
}, {
	.emi_bank		= 2,
	.emi_withinbankoffset	= 0,

	.emi_timing_data	= &nand_timing_data,

	.chip_delay		= 25,
	.mtd_parts		= nand2_parts,
	.nr_parts		= ARRAY_SIZE(nand2_parts),
	.rbn_port		= 2,
	.rbn_pin		= 7,
}
};

#ifdef CONFIG_SND
/* ALSA dummy converter for PCM input, to configure required
 * Left Justified mode */
static struct platform_device cb101_snd_input = {
	.name = "snd_conv_dummy",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "SPDIF/Analog Input",
		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};
#endif

static struct platform_device *cb101_devices[] __initdata = {
	&physmap_flash,
	&cb101_phy_device,
#ifdef CONFIG_SND
	&cb101_snd_input,
#endif
};

static int __init device_init(void)
{
	stx7200_configure_ssc(&ssc_private_info);
	stx7200_configure_usb(0);
	stx7200_configure_usb(1);
	stx7200_configure_usb(2);
	stx7200_configure_ethernet(0, 0, 0, 0);
	stx7200_configure_lirc(NULL);
	stx7200_configure_nand(&cb101_nand_config[0]);
	stx7200_configure_nand(&cb101_nand_config[1]);

	return platform_add_devices(cb101_devices, ARRAY_SIZE(cb101_devices));
}
arch_initcall(device_init);

static void __iomem *cb101_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init cb101_init_irq(void)
{
}

struct sh_machine_vector mv_cb101 __initmv = {
	.mv_name		= "cb101",
	.mv_setup		= cb101_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= cb101_init_irq,
	.mv_ioport_map		= cb101_ioport_map,
};
