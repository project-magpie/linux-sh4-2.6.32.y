#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7200.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */

static int __initdata stx7200_emi_bank_configured[EMI_BANKS];



/* PATA resources --------------------------------------------------------- */

/* EMI A20 = CS1 (active low)
 * EMI A21 = CS0 (active low)
 * EMI A19 = DA2
 * EMI A18 = DA1
 * EMI A17 = DA0 */
static struct resource stx7200_pata_resources[] = {
	/* I/O base: CS1=N, CS0=A */
	[0] = STM_PLAT_RESOURCE_MEM(1 << 20, 8 << 17),
	/* CTL base: CS1=A, CS0=N, DA2=A, DA1=A, DA0=N */
	[1] = STM_PLAT_RESOURCE_MEM((1 << 21) + (6 << 17), 4),
	/* IRQ */
	[2] = STM_PLAT_RESOURCE_IRQ(-1, -1),
};

static struct platform_device stx7200_pata_device = {
	.name		= "pata_platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stx7200_pata_resources),
	.resource	= stx7200_pata_resources,
	.dev.platform_data = &(struct pata_platform_info) {
		.ioport_shift	= 17,
	},
};

void __init stx7200_configure_pata(struct stx7200_pata_config *config)
{
	unsigned long bank_base;

	if (!config) {
		BUG();
		return;
	}

	BUG_ON(config->emi_bank < 0 || config->emi_bank >= EMI_BANKS);
	BUG_ON(stx7200_emi_bank_configured[config->emi_bank]);
	stx7200_emi_bank_configured[config->emi_bank] = 1;

	bank_base = emi_bank_base(config->emi_bank);

	stx7200_pata_resources[0].start += bank_base;
	stx7200_pata_resources[0].end += bank_base;
	stx7200_pata_resources[1].start += bank_base;
	stx7200_pata_resources[1].end += bank_base;
	stx7200_pata_resources[2].start = config->irq;
	stx7200_pata_resources[2].end = config->irq;

	emi_config_pata(config->emi_bank, config->pc_mode);

	platform_device_register(&stx7200_pata_device);
}



/* NAND Resources --------------------------------------------------------- */

static struct stm_pad_config stx7200_nand_flex_pad_config = {
	.labels_num = 1,
	.labels = (struct stm_pad_label []) {
		STM_PAD_LABEL("NANDRnotB"),
	},
};

static struct platform_device stx7200_nand_flex_device = {
	.name = "stm-nand-flex",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xFEF01000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(123), -1),
	},
	.dev.platform_data = &(struct stm_plat_nand_flex_data){
		.pad_config	= &stx7200_nand_flex_pad_config,
	},
};

void __init stx7200_configure_nand_flex(int nr_banks,
					struct stm_nand_bank_data *banks,
					int rbn_connected)
{
	struct stm_plat_nand_flex_data *data;

	data = stx7200_nand_flex_device.dev.platform_data;
	data->nr_banks = nr_banks;
	data->banks = banks;
	data->flex_rbn_connected = rbn_connected;

	platform_device_register(&stx7200_nand_flex_device);
}

/* FDMA resources --------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include "fdma_firmware_7200.h"

static struct stm_plat_fdma_hw stx7200_fdma_hw = {
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

static struct stm_plat_fdma_data stx7200_fdma_0_platform_data = {
	.hw = &stx7200_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

static struct stm_plat_fdma_data stx7200_fdma_1_platform_data = {
	.hw = &stx7200_fdma_hw,
	.fw = &stm_fdma_firmware_7200,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
};

#define stx7200_fdma_0_platform_data_addr &stx7200_fdma_0_platform_data
#define stx7200_fdma_1_platform_data_addr &stx7200_fdma_1_platform_data

#else

#define stx7200_fdma_0_platform_data_addr NULL
#define stx7200_fdma_1_platform_data_addr NULL

#endif /* CONFIG_STM_DMA */

static struct platform_device stx7200_fdma_0_device = {
	.name		= "stm-fdma",
	.id		= 0,
	.num_resources	= 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd810000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(13), -1),
	},
	.dev.platform_data = stx7200_fdma_0_platform_data_addr,
};

static struct platform_device stx7200_fdma_1_device = {
	.name		= "stm-fdma",
	.id		= 1,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		STM_PLAT_RESOURCE_MEM(0xfd820000, 0x10000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(15), -1),
	},
	.dev.platform_data = stx7200_fdma_1_platform_data_addr,
};

static struct platform_device stx7200_fdma_xbar_device = {
	.name		= "stm-fdma-xbar",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd830000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device stx7200_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfdb70000, 0x1000),
	}
};

static struct platform_device stx7200_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfdb70000, 0x1000),
	}
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device stx7200_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd020000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(96), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(0),
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd021000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(97), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(1),
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd022000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(98), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(2),
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd023000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(99), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(3),
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd024000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(100), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(4),
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd025000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(101), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(5),
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd026000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(102), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(6),
	},
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd027000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(103), -1),
		},
		.dev.platform_data = &STM_PLAT_PIO_DATA_LABELS_ONLY(7),
	},
};

static void __init stx7200_pio_late_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stx7200_pio_devices); i++)
		platform_device_register(&stx7200_pio_devices[i]);
}



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stx7200_sysconf_device = {
	.name		= "stm-sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd704000, 0x1d4),
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
void __init stx7200_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&stx7200_sysconf_device, 1);
	stm_gpio_early_init(stx7200_pio_devices,
			ARRAY_SIZE(stx7200_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STx7200 version %ld.x\n", chip_revision);

	/* ClockgenB powers up with all the frequency synths bypassed.
	 * Enable them all here. Without this, USB 1.1 doesn't work,
	 * as it needs a 48MHz clock which is separate from the USB 2
	 * clock which is derived from the SATA clock. */
	ctrl_outl(0, 0xFD701048);

	/* Configure the ST40 RTC to source its clock from clockgenB.
	 * In theory this should be board specific, but so far nobody
	 * has ever done this. */
	sc = sysconf_claim(SYS_CFG, 8, 1, 1, "rtc");
	sysconf_write(sc, 1);

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
		STM_PLAT_RESOURCE_MEM(0xfdf00000, 0x874),
	},
};

static int __init stx7200_postcore_setup(void)
{
	return platform_device_register(&emi);
}
postcore_initcall(stx7200_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stx7200_devices[] __initdata = {
	&stx7200_fdma_0_device,
#if 0
	&stx7200_fdma_1_device,
#endif
	&stx7200_fdma_xbar_device,
	&stx7200_sysconf_device,
	&stx7200_rng_hwrandom_device,
	&stx7200_rng_devrandom_device,
};

static int __init stx7200_devices_setup(void)
{
	stx7200_pio_late_setup();

	return platform_add_devices(stx7200_devices,
			ARRAY_SIZE(stx7200_devices));
}
device_initcall(stx7200_devices_setup);
