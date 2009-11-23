#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7111.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7111_emi_bank_configured[EMI_BANKS];



/* NAND Resources --------------------------------------------------------- */

static struct stm_pad_config stx7111_nand_flex_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("NANDRnotB"),
	},
};

static struct platform_device stx7111_nand_flex_device = {
	.name = "stm-nand-flex",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xFE701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x14c0), -1),
	},
	.dev.platform_data = &(struct stm_plat_nand_flex_data){
		.pad_config     = &stx7111_nand_flex_pad_config,
	},
};

void __init stx7111_configure_nand_flex(int nr_banks,
					struct stm_nand_bank_data *banks,
					int rbn_connected)
{
	struct stm_plat_nand_flex_data *data;

	data = stx7111_nand_flex_device.dev.platform_data;
	data->nr_banks = nr_banks;
	data->banks = banks;
	data->flex_rbn_connected = rbn_connected;

	platform_device_register(&stx7111_nand_flex_device);
}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7111 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7111_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset = 0x8000,
		.size   = 0x800 << 2, /* 2048 * 4 = 8192 */
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
	.imem = {
		.offset = 0xc000,
		.size   = 0x1000 << 2, /* 4096 * 4 = 16384 */
	},
};

static struct stm_plat_fdma_data stx7111_fdma_0_platform_data = {
	.hw = &stx7111_fdma_hw,
	.fw = &stm_fdma_firmware_7111,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_data stx7111_fdma_1_platform_data = {
	.hw = &stx7111_fdma_hw,
	.fw = &stm_fdma_firmware_7111,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7111_fdma_0_platform_data_addr &stx7111_fdma_0_platform_data
#define stx7111_fdma_1_platform_data_addr &stx7111_fdma_1_platform_data

#else

#define stx7111_fdma_0_platform_data_addr NULL
#define stx7111_fdma_1_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7111_fdma_0_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x1380), -1),
	},
	.dev.platform_data = stx7111_fdma_0_platform_data_addr,
};

static struct platform_device stx7111_fdma_1_device = {
	.name		= "stm-fdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(evt2irq(0x13a0), -1),
	},
	.dev.platform_data = stx7111_fdma_1_platform_data_addr,
};

static struct platform_device stx7111_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7111_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7111_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

/* Internal temperature sensor resources ---------------------------------- */

static struct platform_device stx7111_temp_device = {
	.name			= "stm-temp",
	.id			= -1,
	.dev.platform_data	= &(struct plat_stm_temp_data) {
		.name = "STx7111 chip temperature",
		.pdn = { SYS_CFG, 41, 4, 4 },
		.dcorrect = { SYS_CFG, 41, 5, 9 },
		.overflow = { SYS_STA, 12, 8, 8 },
		.data = { SYS_STA, 12, 10, 16 },
	},
};

/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7111_pio_devices[] = {
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
};

static void __init stx7111_pio_late_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7111_pio_devices); i++)
		platform_device_register(&stx7111_pio_devices[i]);
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7111_sysconf_device = {
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
void __init stx7111_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7111_sysconf_device, 1);
	stm_gpio_early_init(stx7111_pio_devices,
			ARRAY_SIZE(stx7111_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7111 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static struct platform_device emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128*1024*1024),
		STM_PLAT_RESOURCE_MEM(0xfe700000, 0x874),
	},
};

static int __init stx7111_postcore_setup(void)
{
	return platform_device_register(&emi);
}
postcore_initcall(stx7111_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7111_devices[] __initdata = {
	&stx7111_fdma_0_device,
	&stx7111_fdma_1_device,
	&stx7111_fdma_xbar_device,
	&stx7111_sysconf_device,
	&stx7111_rng_hwrandom_device,
	&stx7111_rng_devrandom_device,
	&stx7111_temp_device,
};

static int __init stx7111_devices_setup(void)
{
	stx7111_pio_late_setup();

	return platform_add_devices(stx7111_devices,
			ARRAY_SIZE(stx7111_devices));
}
device_initcall(stx7111_devices_setup);
