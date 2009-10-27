#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7105_emi_bank_configured[EMI_BANKS];



/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7105_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7105_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7105_pata_resources),
	.resource	= stx7105_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7105_configure_pata(struct stx7105_pata_config *config)
{
	unsigned long bank_base;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7105_emi_bank_configured[config->emi_bank]);
	stx7105_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7105_pata_resources[0].start += bank_base;
	stx7105_pata_resources[0].end += bank_base;
	stx7105_pata_resources[1].start += bank_base;
	stx7105_pata_resources[1].end += bank_base;
	stx7105_pata_resources[2].start = config->irq;
	stx7105_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7105_pata_device);
}



/* NAND Resources --------------------------------------------------------- */

static struct stm_pad_config stx7105_nand_flex_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("NANDRnotB"),
	},
};

static struct platform_device stx7105_nand_flex_device = {
	.name = "stm-nand-flex",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xFE701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x14a0), -1),
	},
	.dev.platform_data = &(struct stm_plat_nand_flex_data){
		.pad_config	= &stx7105_nand_flex_pad_config,
	},
};

void __init stx7105_configure_nand_flex(int nr_banks,
					struct stm_nand_bank_data *banks,
					int rbn_connected)
{
	struct stm_plat_nand_flex_data *data;

	data = stx7105_nand_flex_device.dev.platform_data;
	data->nr_banks = nr_banks;
	data->banks = banks;
	data->flex_rbn_connected = rbn_connected;

	platform_device_register(&stx7105_nand_flex_device);
}

/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include "fdma_firmware_7200.h"

static struct stm_plat_fdma_hw stx7105_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.periph_regs = {
		.sync_reg = 0x8000 + (0xfe2 << 2), /* 0xbf88 */
		.cmd_sta  = 0x8000 + (0xff0 << 2), /* 0xbfc0 */
		.cmd_set  = 0x8000 + (0xff1 << 2), /* 0xbfc4 */
		.cmd_clr  = 0x8000 + (0xff2 << 2), /* 0xbfc8 */
		.cmd_mask = 0x8000 + (0xff3 << 2), /* 0xbfcc */
		.int_sta  = 0x8000 + (0xff4 << 2), /* 0xbfd0 */
		.int_set  = 0x8000 + (0xff5 << 2), /* 0xbfd4 */
		.int_clr  = 0x8000 + (0xff6 << 2), /* 0xbfd8 */
		.int_mask = 0x8000 + (0xff7 << 2), /* 0xbfdc */
	},
	.dmem_offset = 0x8000,
	.dmem_size   = 0x800 << 2, /* 2048 * 4 = 8192 */
	.imem_offset = 0xc000,
	.imem_size   = 0x1000 << 2, /* 4096 * 4 = 16384 */
};

static struct stm_plat_fdma_data stx7105_fdma_0_platform_data = {
	.hw = &stx7105_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_data stx7105_fdma_1_platform_data = {
	.hw = &stx7105_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7105_fdma_0_platform_data_addr &stx7105_fdma_0_platform_data
#define stx7105_fdma_1_platform_data_addr &stx7105_fdma_1_platform_data

#else

#define stx7105_fdma_0_platform_data_addr NULL
#define stx7105_fdma_1_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7105_fdma_0_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1380), -1),
	},
	.dev.platform_data = stx7105_fdma_0_platform_data_addr,
};

static struct platform_device stx7105_fdma_1_device = {
	.name		= "stm-fdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x13a0), -1),
	},
	.dev.platform_data = stx7105_fdma_1_platform_data_addr,
};

static struct platform_device stx7105_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7105_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7105_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

/* Internal temperature sensor resources ---------------------------------- */

static struct platform_device stx7105_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.name = "STx7105 chip temperature",
		.pdn = { SYS_CFG, 41, 4, 4 },
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
	},
};

/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7105_pio_devices[] = {
	/* COMMS PIO blocks */
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xc00), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(0),
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xc80), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(1),
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xd00), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(2),
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1060), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(3),
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1040), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(4),
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1020), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(5),
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd026000, 0x100),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0x1000), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(6),
	},

	/* Standalone PIO block */
	[7] = {
		.name = "stm-pio10",
		.id = -1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe010000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(evt2irq(0xb40), -1),
		},
		.dev.platform_data = &(struct stm_plat_pio10_data) {
			.start_pio = 7,
			.num_pio = 10,
			.port_data = {
				{ STM_PLAT_PIO_DATA_LABELS(7) },
				{ STM_PLAT_PIO_DATA_LABELS(8) },
				{ STM_PLAT_PIO_DATA_LABELS(9) },
				{ STM_PLAT_PIO_DATA_LABELS(10) },
				{ STM_PLAT_PIO_DATA_LABELS(11) },
				{ STM_PLAT_PIO_DATA_LABELS(12) },
				{ STM_PLAT_PIO_DATA_LABELS(13) },
				{ STM_PLAT_PIO_DATA_LABELS(14) },
				{ STM_PLAT_PIO_DATA_LABELS(15) },
				{ STM_PLAT_PIO_DATA_LABELS(16) },
			},
		},
	},
};

static void __init stx7105_pio_late_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7105_pio_devices); i++)
		if (stx7105_pio_devices[i].name) /* No PIO0 */
			platform_device_register(&stx7105_pio_devices[i]);
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7105_sysconf_device = {
	.name		= "stm-sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe001000, 0x1e0),
	},
	.dev.platform_data = &(struct stm_plat_sysconf_data) {
		.groups_num = 3,
		.groups = (struct stm_plat_sysconf_group []) {
			PLAT_SYSCONF_GROUP(SYS_DEV, 0x000),
			PLAT_SYSCONF_GROUP(SYS_STA, 0x008),
			PLAT_SYSCONF_GROUP(SYS_CFG, 0x100),
		},
	},
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stx7105_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7105_sysconf_device, 1);
	stm_gpio_early_init(stx7105_pio_devices,
			ARRAY_SIZE(stx7105_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7105 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stx7105_postcore_setup(void)
{
	emi_init(0, 0xfe700000);

	return 0;
}
postcore_initcall(stx7105_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static int __init stx7105_subsys_setup(void)
{
	/* we need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts
	 * are available. */
	stx7105_pio_late_setup();

	return 0;
}
subsys_initcall(stx7105_subsys_setup);

static struct platform_device *stx7105_devices[] __initdata = {
	&stx7105_fdma_0_device,
	&stx7105_fdma_1_device,
	&stx7105_fdma_xbar_device,
	&stx7105_sysconf_device,
	&stx7105_rng_hwrandom_device,
	&stx7105_rng_devrandom_device,
	&stx7105_temp_device,
};

static int __init stx7105_devices_setup(void)
{
	return platform_add_devices(stx7105_devices,
			ARRAY_SIZE(stx7105_devices));
}
device_initcall(stx7105_devices_setup);
