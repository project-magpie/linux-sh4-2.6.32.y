#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7141.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7141_emi_bank_configured[EMI_BANKS];



/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7141_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7141_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7141_pata_resources),
	.resource	= stx7141_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift = 17,
	},
};

void __init stx7141_configure_pata(struct stx7141_pata_config *config)
{
	static int configured;
	unsigned long bank_base;

	BUG_ON(configured);
	configured = 1;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7141_emi_bank_configured[config->emi_bank]);
	stx7141_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7141_pata_resources[0].start += bank_base;
	stx7141_pata_resources[0].end += bank_base;
	stx7141_pata_resources[1].start += bank_base;
	stx7141_pata_resources[1].end += bank_base;
	stx7141_pata_resources[2].start = config->irq;
	stx7141_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7141_pata_device);
}



/* NAND Resources --------------------------------------------------------- */

static struct stm_pad_config stx7141_nand_flex_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("NANDRnotB"),
	},
};

static struct platform_device stx7141_nand_flex_device = {
	.name = "stm-nand-flex",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xFE701000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(39), -1),
	},
	.dev.platform_data = &(struct stm_plat_nand_flex_data){
		.pad_config     = &stx7141_nand_flex_pad_config,
	},
};

/* stx7141_configure_nand - Configures NAND support for the STx7141
 *
 * Requires generic platform NAND driver (CONFIG_MTD_NAND_PLATFORM).
 * Uses 'gen_nand.x' as ID for specifying MTD partitions on the kernel
 * command line. */
void __init stx7141_configure_nand_flex(int nr_banks,
					struct stm_nand_bank_data *banks,
					int rbn_connected)
{
	struct stm_plat_nand_flex_data *data;

	data = stx7141_nand_flex_device.dev.platform_data;
	data->nr_banks = nr_banks;
	data->banks = banks;
	data->flex_rbn_connected = rbn_connected;

	platform_device_register(&stx7141_nand_flex_device);
}



/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7141 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw stx7141_fdma_hw = {
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

static struct stm_plat_fdma_data stx7141_fdma_0_platform_data = {
	.hw = &stx7141_fdma_hw,
	.fw = &stm_fdma_firmware_7141,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_data stx7141_fdma_1_platform_data = {
	.hw = &stx7141_fdma_hw,
	.fw = &stm_fdma_firmware_7141,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7141_fdma_0_platform_data_addr &stx7141_fdma_0_platform_data
#define stx7141_fdma_1_platform_data_addr &stx7141_fdma_1_platform_data

#else

#define stx7141_fdma_0_platform_data_addr NULL
#define stx7141_fdma_1_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7141_fdma_0_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe220000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(44), -1),
	},
	.dev.platform_data = stx7141_fdma_0_platform_data_addr,
};

static struct platform_device stx7141_fdma_1_device = {
	.name		= "stm-fdma",
	.id		= 1,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xfe410000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(45), -1),
	},
	.dev.platform_data = stx7141_fdma_1_platform_data_addr,
};

static struct platform_device stx7141_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe420000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7141_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

static struct platform_device stx7141_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe250000, 0x1000),
	}
};

/* Internal temperature sensor resources ---------------------------------- */

static unsigned long stx7141_temp1_get_data(void *priv)
{
	/* Some "bright sparkle" decided to split the data field
	 * between SYS_STA12 & SYS_STA13 registers, having 11 (!!!)
	 * bits unused in the SYS_STA13... WHY OH WHY?!?!?! */
	static struct sysconf_field *data1_0_3, *data1_4_6;

	if (!data1_0_3)
		data1_0_3 = sysconf_claim(SYS_STA, 12, 28, 31, "stm-temp.1");
	if (!data1_4_6)
		data1_4_6 = sysconf_claim(SYS_STA, 13, 0, 2, "stm-temp.1");
	if (!data1_0_3 || !data1_4_6)
		return 0;

	return (sysconf_read(data1_4_6) << 4) | sysconf_read(data1_0_3);
}

static struct platform_device stx7141_temp_devices[] = {
	{
		.name			= "stm-temp",
		.id			= 0,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.name = "STx7141 chip temperature 0",
			.pdn = { SYS_CFG, 41, 4, 4 },
			.dcorrect = { SYS_CFG, 41, 5, 9 },
			.overflow = { SYS_STA, 12, 8, 8 },
			.data = { SYS_STA, 12, 10, 16 },
		},
	}, {
		.name			= "stm-temp",
		.id			= 1,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.name = "STx7141 chip temperature 1",
			.pdn = { SYS_CFG, 41, 14, 14 },
			.dcorrect = { SYS_CFG, 41, 15, 19 },
			.overflow = { SYS_STA, 12, 26, 26 },
			.custom_get_data = stx7141_temp1_get_data,
		},
	}, {
		.name			= "stm-temp",
		.id			= 2,
		.dev.platform_data	= &(struct plat_stm_temp_data) {
			.name = "STx7141 chip temperature 2",
			.pdn = { SYS_CFG, 41, 24, 24 },
			.dcorrect = { SYS_CFG, 41, 25, 29 },
			.overflow = { SYS_STA, 13, 12, 12 },
			.data = { SYS_STA, 13, 14, 20 },
		},
	}
};

/* PIO ports resources ---------------------------------------------------- */

#define STX7141_PIO_PAD_CONFIG(_port_no, _pin_no, _sys_cfg, _lsb, _msb) \
	{ \
		.labels_num = 1, \
		.labels = (struct stm_pad_label []) { \
			STM_PAD_LABEL("PIO" #_port_no "." #_pin_no), \
		}, \
		.sysconf_values_num = 1, \
		.sysconf_values = (struct stm_pad_sysconf_value []) { \
			STM_PAD_SYS_CFG(_sys_cfg, _lsb, _msb, 0), \
		}, \
	}

static struct stm_plat_pio_data stx7141_pio_platform_data[] = {
	/* no [0], see below */
	[1] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(1, 0, 19, 0, 1),
			[1] = STX7141_PIO_PAD_CONFIG(1, 1, 19, 2, 3),
			[2] = STX7141_PIO_PAD_CONFIG(1, 2, 19, 4, 5),
			[3] = STX7141_PIO_PAD_CONFIG(1, 3, 19, 6, 7),
			[4] = STX7141_PIO_PAD_CONFIG(1, 4, 19, 8, 8),
			[5] = STX7141_PIO_PAD_CONFIG(1, 5, 19, 9, 9),
			[6] = STX7141_PIO_PAD_CONFIG(1, 6, 19, 10, 10),
			[7] = STX7141_PIO_PAD_CONFIG(1, 7, 19, 11, 11),
		},
	},
	[2] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(2, 0, 19, 12, 12),
			[1] = STX7141_PIO_PAD_CONFIG(2, 1, 19, 13, 13),
			[2] = STX7141_PIO_PAD_CONFIG(2, 2, 19, 14, 14),
			[3] = STX7141_PIO_PAD_CONFIG(2, 3, 19, 15, 15),
			[4] = STX7141_PIO_PAD_CONFIG(2, 4, 19, 16, 16),
			[5] = STX7141_PIO_PAD_CONFIG(2, 5, 19, 17, 17),
			[6] = STX7141_PIO_PAD_CONFIG(2, 6, 19, 18, 18),
			[7] = STX7141_PIO_PAD_CONFIG(2, 7, 19, 19, 19),
		},
	},
	[3] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(3, 0, 19, 20, 20),
			[1] = STX7141_PIO_PAD_CONFIG(3, 1, 19, 21, 21),
			[2] = STX7141_PIO_PAD_CONFIG(3, 2, 19, 22, 23),
			[3] = STX7141_PIO_PAD_CONFIG(3, 3, 19, 24, 25),
			[4] = STX7141_PIO_PAD_CONFIG(3, 4, 19, 26, 27),
			[5] = STX7141_PIO_PAD_CONFIG(3, 5, 19, 28, 29),
			[6] = STX7141_PIO_PAD_CONFIG(3, 6, 19, 30, 31),
			[7] = STX7141_PIO_PAD_CONFIG(3, 7, 20, 0, 0),
		},
	},
	[4] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(4, 0, 20, 1, 2),
			[1] = STX7141_PIO_PAD_CONFIG(4, 1, 20, 3, 4),
			[2] = STX7141_PIO_PAD_CONFIG(4, 2, 20, 5, 6),
			[3] = STX7141_PIO_PAD_CONFIG(4, 3, 20, 7, 8),
			[4] = STX7141_PIO_PAD_CONFIG(4, 4, 20, 9, 10),
			[5] = STX7141_PIO_PAD_CONFIG(4, 5, 20, 11, 12),
			[6] = STX7141_PIO_PAD_CONFIG(4, 6, 20, 13, 13),
			[7] = STX7141_PIO_PAD_CONFIG(4, 7, 20, 14, 14),
		},
	},
	[5] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(5, 0, 20, 15, 16),
			[1] = STX7141_PIO_PAD_CONFIG(5, 1, 20, 17, 18),
			[2] = STX7141_PIO_PAD_CONFIG(5, 2, 20, 19, 19),
			[3] = STX7141_PIO_PAD_CONFIG(5, 3, 20, 20, 20),
			[4] = STX7141_PIO_PAD_CONFIG(5, 4, 20, 21, 21),
			[5] = STX7141_PIO_PAD_CONFIG(5, 5, 20, 22, 23),
			[6] = STX7141_PIO_PAD_CONFIG(5, 6, 20, 24, 24),
			[7] = STX7141_PIO_PAD_CONFIG(5, 7, 20, 25, 26),
		},
	},
	[6] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(6, 0, 20, 27, 28),
			[1] = STX7141_PIO_PAD_CONFIG(6, 1, 25, 29, 30),
			[2] = STX7141_PIO_PAD_CONFIG(6, 2, 25, 0, 1),
			[3] = STX7141_PIO_PAD_CONFIG(6, 3, 25, 2, 3),
			[4] = STX7141_PIO_PAD_CONFIG(6, 4, 25, 4, 5),
			[5] = STX7141_PIO_PAD_CONFIG(6, 5, 25, 6, 7),
			[6] = STX7141_PIO_PAD_CONFIG(6, 6, 25, 8, 9),
			[7] = STX7141_PIO_PAD_CONFIG(6, 7, 25, 10, 11),
		},
	},
	[7] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(7, 0, 25, 12, 13),
			[1] = STX7141_PIO_PAD_CONFIG(7, 1, 25, 14, 15),
			[2] = STX7141_PIO_PAD_CONFIG(7, 2, 25, 16, 17),
			[3] = STX7141_PIO_PAD_CONFIG(7, 3, 25, 18, 19),
			[4] = STX7141_PIO_PAD_CONFIG(7, 4, 25, 20, 21),
			[5] = STX7141_PIO_PAD_CONFIG(7, 5, 25, 22, 23),
			[6] = STX7141_PIO_PAD_CONFIG(7, 6, 25, 24, 25),
			[7] = STX7141_PIO_PAD_CONFIG(7, 7, 25, 26, 27),
		},
	},
	[8] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(8, 0, 25, 28, 30),
			[1] = STX7141_PIO_PAD_CONFIG(8, 1, 35, 0, 2),
			[2] = STX7141_PIO_PAD_CONFIG(8, 2, 35, 3, 5),
			[3] = STX7141_PIO_PAD_CONFIG(8, 3, 35, 6, 8),
			[4] = STX7141_PIO_PAD_CONFIG(8, 4, 35, 9, 11),
			[5] = STX7141_PIO_PAD_CONFIG(8, 5, 35, 12, 14),
			[6] = STX7141_PIO_PAD_CONFIG(8, 6, 35, 15, 17),
			[7] = STX7141_PIO_PAD_CONFIG(8, 7, 35, 18, 20),
		},
	},
	[9] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(9, 0, 35, 21, 22),
			[1] = STX7141_PIO_PAD_CONFIG(9, 1, 35, 23, 24),
			[2] = STX7141_PIO_PAD_CONFIG(9, 2, 35, 25, 26),
			[3] = STX7141_PIO_PAD_CONFIG(9, 3, 35, 27, 28),
			[4] = STX7141_PIO_PAD_CONFIG(9, 4, 35, 29, 30),
			[5] = STX7141_PIO_PAD_CONFIG(9, 5, 46, 0, 1),
			[6] = STX7141_PIO_PAD_CONFIG(9, 6, 46, 2, 3),
			[7] = STX7141_PIO_PAD_CONFIG(9, 7, 46, 4, 5),
		},
	},
	[10] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(10, 0, 46, 6, 7),
			[1] = STX7141_PIO_PAD_CONFIG(10, 1, 46, 8, 9),
			[2] = STX7141_PIO_PAD_CONFIG(10, 2, 46, 10, 11),
			[3] = STX7141_PIO_PAD_CONFIG(10, 3, 46, 12, 13),
			[4] = STX7141_PIO_PAD_CONFIG(10, 4, 46, 14, 15),
			[5] = STX7141_PIO_PAD_CONFIG(10, 5, 46, 16, 17),
			[6] = STX7141_PIO_PAD_CONFIG(10, 6, 46, 18, 19),
			[7] = STX7141_PIO_PAD_CONFIG(10, 7, 46, 20, 21),
		},
	},
	[11] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(11, 0, 46, 22, 23),
			[1] = STX7141_PIO_PAD_CONFIG(11, 1, 46, 24, 26),
			[2] = STX7141_PIO_PAD_CONFIG(11, 2, 46, 27, 29),
			[3] = STX7141_PIO_PAD_CONFIG(11, 3, 47, 0, 2),
			[4] = STX7141_PIO_PAD_CONFIG(11, 4, 47, 3, 5),
			[5] = STX7141_PIO_PAD_CONFIG(11, 5, 47, 6, 8),
			[6] = STX7141_PIO_PAD_CONFIG(11, 6, 47, 9, 11),
			[7] = STX7141_PIO_PAD_CONFIG(11, 7, 47, 12, 14),
		},
	},
	[12] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(12, 0, 47, 15, 17),
			[1] = STX7141_PIO_PAD_CONFIG(12, 1, 47, 18, 20),
			[2] = STX7141_PIO_PAD_CONFIG(12, 2, 47, 21, 23),
			[3] = STX7141_PIO_PAD_CONFIG(12, 3, 47, 24, 25),
			[4] = STX7141_PIO_PAD_CONFIG(12, 4, 47, 26, 27),
			[5] = STX7141_PIO_PAD_CONFIG(12, 5, 47, 28, 29),
			[6] = STX7141_PIO_PAD_CONFIG(12, 6, 48, 0, 2),
			[7] = STX7141_PIO_PAD_CONFIG(12, 7, 48, 3, 5),
		},
	},
	[13] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(13, 0, 48, 6, 8),
			[1] = STX7141_PIO_PAD_CONFIG(13, 1, 48, 9, 11),
			[2] = STX7141_PIO_PAD_CONFIG(13, 2, 48, 12, 14),
			[3] = STX7141_PIO_PAD_CONFIG(13, 3, 48, 15, 17),
			[4] = STX7141_PIO_PAD_CONFIG(13, 4, 48, 18, 20),
			[5] = STX7141_PIO_PAD_CONFIG(13, 5, 48, 21, 23),
			[6] = STX7141_PIO_PAD_CONFIG(13, 6, 48, 24, 25),
			[7] = STX7141_PIO_PAD_CONFIG(13, 7, 48, 26, 27),
		},
	},
	[14] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(14, 0, 48, 28, 30),
			[1] = STX7141_PIO_PAD_CONFIG(14, 1, 49, 0, 2),
			[2] = STX7141_PIO_PAD_CONFIG(14, 2, 49, 3, 5),
			[3] = STX7141_PIO_PAD_CONFIG(14, 3, 49, 6, 8),
			[4] = STX7141_PIO_PAD_CONFIG(14, 4, 49, 9, 11),
			[5] = STX7141_PIO_PAD_CONFIG(14, 5, 49, 12, 14),
			[6] = STX7141_PIO_PAD_CONFIG(14, 6, 49, 15, 17),
			[7] = STX7141_PIO_PAD_CONFIG(14, 7, 49, 18, 19),
		},
	},
	[15] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(15, 0, 49, 20, 21),
			[1] = STX7141_PIO_PAD_CONFIG(15, 1, 49, 22, 23),
			[2] = STX7141_PIO_PAD_CONFIG(15, 2, 49, 24, 25),
			[3] = STX7141_PIO_PAD_CONFIG(15, 3, 49, 26, 27),
			[4] = STX7141_PIO_PAD_CONFIG(15, 4, 49, 28, 28),
			[5] = STX7141_PIO_PAD_CONFIG(15, 5, 49, 29, 29),
			[6] = STX7141_PIO_PAD_CONFIG(15, 6, 49, 30, 30),
			[7] = STX7141_PIO_PAD_CONFIG(15, 7, 50, 0, 1),
		},
	},
	[16] = {
		.pad_configs = (struct stm_pad_config []) {
			[0] = STX7141_PIO_PAD_CONFIG(16, 0, 50, 2, 3),
			[1] = STX7141_PIO_PAD_CONFIG(16, 1, 50, 4, 5),
			[2] = STX7141_PIO_PAD_CONFIG(16, 2, 50, 6, 7),
			[3] = STX7141_PIO_PAD_CONFIG(16, 3, 50, 8, 8),
			[4] = STX7141_PIO_PAD_CONFIG(16, 4, 50, 9, 9),
			[5] = STX7141_PIO_PAD_CONFIG(16, 5, 50, 10, 10),
			[6] = STX7141_PIO_PAD_CONFIG(16, 6, 50, 11, 11),
			[7] = STX7141_PIO_PAD_CONFIG(16, 7, 50, 12, 12),
		},
	},
};

static struct platform_device stx7141_pio_devices[] = {
	/* Some eee... individual named the first PIO block PIO1,
	 * so there is no PIO0 here... */

	/* COMMS PIO blocks */
	[0] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(49), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[1],
	},
	[1] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(50), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[2],
	},
	[2] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(51), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[3],
	},
	[3] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(52), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[4],
	},
	[4] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(53), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[5],
	},
	[5] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(54), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[6],
	},
	[6] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd026000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(55), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[7],
	},

	/* Standalone PIO blocks */
	[7] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe010000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(59), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[8],
	},
	[8] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe011000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(60), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[9],
	},
	[9] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe012000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(61), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[10],
	},
	[10] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe013000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(62), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[11],
	},
	[11] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe014000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(63), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[12],
	},
	[12] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe015000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(64), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[13],
	},
	[13] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe016000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(65), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[14],
	},
	[14] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe017000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(66), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[15],
	},
	[15] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe018000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(67), -1),
		},
		.dev.platform_data = &stx7141_pio_platform_data[16],
	},
};

static void __init stx7141_pio_late_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7141_pio_devices); i++)
		platform_device_register(&stx7141_pio_devices[i]);
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7141_sysconf_device = {
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
void __init stx7141_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7141_sysconf_device, 1);
	stm_gpio_early_init(stx7141_pio_devices,
			ARRAY_SIZE(stx7141_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7141 version %ld.x\n", chip_revision);

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

static int __init stx7141_postcore_setup(void)
{
	return platform_device_register(&emi);
}
postcore_initcall(stx7141_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static int __init stx7141_subsys_setup(void)
{
	/* we need to do PIO setup before module init, because some
	 * drivers (eg gpio-keys) require that the interrupts
	 * are available. */
	stx7141_pio_late_setup();

	return 0;
}
subsys_initcall(stx7141_subsys_setup);

static struct platform_device *stx7141_devices[] __initdata = {
	&stx7141_fdma_0_device,
	&stx7141_fdma_1_device,
	&stx7141_fdma_xbar_device,
	&stx7141_sysconf_device,
	&stx7141_rng_hwrandom_device,
	&stx7141_rng_devrandom_device,
	&stx7141_temp_devices[0],
	&stx7141_temp_devices[1],
	&stx7141_temp_devices[2],
};

static int __init stx7141_devices_setup(void)
{
	return platform_add_devices(stx7141_devices,
			ARRAY_SIZE(stx7141_devices));
}
device_initcall(stx7141_devices_setup);
