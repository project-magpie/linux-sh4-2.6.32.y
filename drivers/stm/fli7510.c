/*
 * (c) 2010-2011 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/emi.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/device.h>
#include <linux/stm/fli7510.h>
#include <asm/irq-ilc.h>



/* EMI resources ---------------------------------------------------------- */
static void fli7510_emi_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, "EMI_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "EMI_ACK")
			== value)
			break;
		mdelay(10);
	}

	return;
}

static struct platform_device fli7510_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 128 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM(0xfd100000, 0x874),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(CFG_PWR_DWN_CTL,
				0, 0, "EMI_PWR"),
			STM_DEVICE_SYSCONF(CFG_EMI_ROPC_STATUS,
				16, 16, "EMI_ACK"),
		},
		.power = fli7510_emi_power,

	}
};


/* NAND Resources --------------------------------------------------------- */

static struct platform_device fli7510_nand_emi_device = {
	.name			= "stm-nand-emi",
	.dev.platform_data	= &(struct stm_plat_nand_emi_data) {
	},
};

static struct platform_device fli7510_nand_flex_device = {
	.num_resources		= 2,
	.resource		= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xFD101000, 0x1000),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(35), -1),
	},
	.dev.platform_data	= &(struct stm_plat_nand_flex_data) {
	},
};

void __init fli7510_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Configure platform device for stm-nand-emi driver */
		emi_data = fli7510_nand_emi_device.dev.platform_data;
		emi_data->nr_banks = config->nr_banks;
		emi_data->banks = config->banks;
		emi_data->emi_rbn_gpio = config->rbn.emi_gpio;
		platform_device_register(&fli7510_nand_emi_device);
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure platform device for stm-nand-flex/afm driver */
		flex_data = fli7510_nand_flex_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		fli7510_nand_flex_device.name =
			(config->driver == stm_nand_flex) ?
			"stm-nand-flex" : "stm-nand-afm";
		platform_device_register(&fli7510_nand_flex_device);
		break;
	}
}

/* SPI FSM setup ---------------------------------------------------------- */

static struct platform_device fli7510_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd102000, 0x1000),
	},
};

void __init fli7510_configure_spifsm(struct stm_plat_spifsm_data *data)
{
	fli7510_spifsm_device.dev.platform_data = data;

	platform_device_register(&fli7510_spifsm_device);
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stm_fdma_firmware_7510 = {
	.rev_id    = 0x8000 + (0x000 << 2), /* 0x8000 */
	.cmd_statn = 0x8000 + (0x450 << 2), /* 0x9140 */
	.req_ctln  = 0x8000 + (0x460 << 2), /* 0x9180 */
	.ptrn      = 0x8000 + (0x560 << 2), /* 0x9580 */
	.cntn      = 0x8000 + (0x562 << 2), /* 0x9588 */
	.saddrn    = 0x8000 + (0x563 << 2), /* 0x958c */
	.daddrn    = 0x8000 + (0x564 << 2), /* 0x9590 */
};

static struct stm_plat_fdma_hw fli7510_fdma_hw = {
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

static struct stm_plat_fdma_data fli7510_fdma_platform_data = {
	.hw = &fli7510_fdma_hw,
	.fw = &stm_fdma_firmware_7510,
};

/*
 * Normally device 0 would be the real-time fdma and device 1 would be the
 * non-real-time fdma. Here they are swapped as output pins 0-31 on fdma-xbar
 * are routed to the non-real-time fdma and output pins 32-63 are routed to the
 * real-time fdma. You must ensure that that firmware to load is named correctly
 */
static struct platform_device fli7510_fdma_devices[] = {
	{
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd910000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(38), -1),
		},
		.dev.platform_data = &fli7510_fdma_platform_data,
	}, {
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd660000, 0x10000),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(36), -1),
		},
		.dev.platform_data = &fli7510_fdma_platform_data,
	}
};

static struct platform_device fli7510_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd980000, 0x1000),
	},
};



/* Hardware RNG resources ------------------------------------------------- */

static struct platform_device fli7510_rng_hwrandom_device = {
	.name = "stm-hwrandom",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd3e0000, 0x1000),
	}
};

static struct platform_device fli7510_rng_devrandom_device = {
	.name = "stm-rng",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd3e0000, 0x1000),
	}
};



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device fli7510_pio_devices[] = {
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c0000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(75), -1),
		},
	},
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c4000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(76), -1),
		},
	},
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c8000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(77), -1),
		},
	},
	[3] = {
		.name = "stm-gpio",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5cc000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(78), -1),
		},
	},
	[4] = {
		.name = "stm-gpio",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5d0000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(79), -1),
		},
	},
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5d4000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(80), -1),
		},
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5d8000, 0x100),
		},
	},
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5dc000, 0x100),
		},
	},
	[8] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5e0000, 0x100),
		},
	},
	[9] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5e4000, 0x100),
		},
	},
	[10] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd984000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(125), -1),
		},
	},
	[11] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd988000, 0x100),
		},
	},
	[12] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd98c000, 0x100),
		},
	},
	[13] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd990000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(2), -1),
		},
	},
	[14] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd994000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(3), -1),
		},
	},
	[15] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd998000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(81), -1),
		},
	},
	[16] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd99c000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(82), -1),
		},
	},
	[17] = {
		.name = "stm-gpio",
		.id = 17,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a0000, 0x100),
		},
	},
	[18] = {
		.name = "stm-gpio",
		.id = 18,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a4000, 0x100),
		},
	},
	[19] = {
		.name = "stm-gpio",
		.id = 19,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a8000, 0x100),
		},
	},
	[20] = {
		.name = "stm-gpio",
		.id = 20,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9ac000, 0x100),
		},
	},
	[21] = {
		.name = "stm-gpio",
		.id = 21,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b0000, 0x100),
		},
	},
	[22] = {
		.name = "stm-gpio",
		.id = 22,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b4000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(83), -1),
		},
	},
	[23] = {
		.name = "stm-gpio",
		.id = 23,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b8000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(84), -1),
		},
	},
	[24] = {
		.name = "stm-gpio",
		.id = 24,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9bc000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(85), -1),
		},
	},
	[25] = {
		.name = "stm-gpio",
		.id = 25,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c0000, 0x100),
		},
	},
	[26] = {
		.name = "stm-gpio",
		.id = 26,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c4000, 0x100),
		},
	},
	[27] = {
		.name = "stm-gpio",
		.id = 27,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c8000, 0x100),
		},
	},
};

static struct platform_device fli7520_pio_devices[] = {
	/* Warning, no PIO ports 0 to 4... */
	[5] = {
		.name = "stm-gpio",
		.id = 5,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c0000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(75), -1),
		},
	},
	[6] = {
		.name = "stm-gpio",
		.id = 6,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c4000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(76), -1),
		},
	},
	[7] = {
		.name = "stm-gpio",
		.id = 7,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5c8000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(77), -1),
		},
	},
	[8] = {
		.name = "stm-gpio",
		.id = 8,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5cc000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(78), -1),
		},
	},
	[9] = {
		.name = "stm-gpio",
		.id = 9,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd5d0000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(79), -1),
		},
	},
	[10] = {
		.name = "stm-gpio",
		.id = 10,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd984000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(125), -1),
		},
	},
	[11] = {
		.name = "stm-gpio",
		.id = 11,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd988000, 0x100),
		},
	},
	[12] = {
		.name = "stm-gpio",
		.id = 12,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd98c000, 0x100),
		},
	},
	[13] = {
		.name = "stm-gpio",
		.id = 13,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd990000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(2), -1),
		},
	},
	[14] = {
		.name = "stm-gpio",
		.id = 14,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd994000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(3), -1),
		},
	},
	[15] = {
		.name = "stm-gpio",
		.id = 15,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd998000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(81), -1),
		},
	},
	[16] = {
		.name = "stm-gpio",
		.id = 16,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd99c000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(82), -1),
		},
	},
	[17] = {
		.name = "stm-gpio",
		.id = 17,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a0000, 0x100),
		},
	},
	[18] = {
		.name = "stm-gpio",
		.id = 18,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a4000, 0x100),
		},
	},
	[19] = {
		.name = "stm-gpio",
		.id = 19,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9a8000, 0x100),
		},
	},
	[20] = {
		.name = "stm-gpio",
		.id = 20,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9ac000, 0x100),
		},
	},
	[21] = {
		.name = "stm-gpio",
		.id = 21,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b0000, 0x100),
		},
	},
	[22] = {
		.name = "stm-gpio",
		.id = 22,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b4000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(83), -1),
		},
	},
	[23] = {
		.name = "stm-gpio",
		.id = 23,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9b8000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(84), -1),
		},
	},
	[24] = {
		.name = "stm-gpio",
		.id = 24,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9bc000, 0x100),
			STM_PLAT_RESOURCE_IRQ(ILC_IRQ(85), -1),
		},
	},
	[25] = {
		.name = "stm-gpio",
		.id = 25,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c0000, 0x100),
		},
	},
	[26] = {
		.name = "stm-gpio",
		.id = 26,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c4000, 0x100),
		},
	},
	[27] = {
		.name = "stm-gpio",
		.id = 27,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9c8000, 0x100),
		},
	},
	[28] = {
		.name = "stm-gpio",
		.id = 28,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9cc000, 0x100),
		},
	},
	[29] = {
		.name = "stm-gpio",
		.id = 29,
		.num_resources = 1,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd9d0000, 0x100),
		},
	},
};

static int fli7510_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	switch (direction) {
	case stm_pad_gpio_direction_in:
		BUG_ON(function != -1);
		stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
		break;
	case stm_pad_gpio_direction_out:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_OUT :
				STM_GPIO_DIRECTION_OUT);
		break;
	case stm_pad_gpio_direction_bidir:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_BIDIR :
				STM_GPIO_DIRECTION_BIDIR);
		break;
	default:
		BUG();
		break;
	}

	return 0;
}

/* MMC/SD resources ------------------------------------------------------ */

static struct stm_pad_config fli7510_mmc_pad_config = {
	.gpios_num = 15,
	.gpios = (struct stm_pad_gpio []) {
		STM_PAD_PIO_OUT_NAMED(23, 2, 1, "MMCCLK"),/* MMC clock */
		STM_PAD_PIO_OUT(23, 3, 1),	/* MMC command */
		STM_PAD_PIO_IN(23, 4, -1),	/* Card Detect */
		STM_PAD_PIO_IN(23, 5, -1),	/* Over Current */
		STM_PAD_PIO_IN(23, 6, -1),	/* MMC Write Protection */
		STM_PAD_PIO_OUT(23, 7, 1),	/* PWR*/
		STM_PAD_PIO_OUT(27, 0, 1),	/* LED*/
		STM_PAD_PIO_BIDIR(24, 0, 1),	/* MMC/SD Data 0*/
		STM_PAD_PIO_BIDIR(24, 1, 1),	/* MMC/SD Data 1*/
		STM_PAD_PIO_BIDIR(24, 2, 1),	/* MMC/SD Data 2*/
		STM_PAD_PIO_BIDIR(24, 3, 1),	/* MMC/SD Data 3*/
		STM_PAD_PIO_BIDIR(24, 4, 1),	/* MMC Data 4*/
		STM_PAD_PIO_BIDIR(24, 5, 1),	/* MMC Data 5*/
		STM_PAD_PIO_BIDIR(24, 6, 1),	/* MMC Data 6*/
		STM_PAD_PIO_BIDIR(24, 7, 1),	/* MMC Data 7*/
		STM_PAD_PIO_OUT(20, 5, 1),	/* Open drain mode
						 * (for external card) */
	},
};

static int mmc_pad_resources(struct sdhci_host *sdhci)
{
	if (!devm_stm_pad_claim(sdhci->mmc->parent, &fli7510_mmc_pad_config,
				dev_name(sdhci->mmc->parent)))
		return -ENODEV;

	return 0;
}

static struct sdhci_pltfm_data fli7510_mmc_platform_data = {
		.init = mmc_pad_resources,
		.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
};

static struct platform_device fli7510_mmc_device = {
		.name = "sdhci",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xFD9F0000, 0x400),
			STM_PLAT_RESOURCE_IRQ_NAMED("mmcirq", ILC_IRQ(109), -1),
		},
		.dev = {
			.platform_data = &fli7510_mmc_platform_data,
		}
};

void __init fli7510_configure_mmc(void)
{
	struct sysconf_field *sc;

	/* Selects the polarity of HSMMC_CARD_DET as input signal inverted*/
	sc = sysconf_claim(TRS_PU_CFG_0, 0, 2, 2, "mmc");
	sysconf_write(sc, 1);

	/* Selects the mode for PIO24, bits 17:18 MMC when "1" "0"  */
	sc = sysconf_claim(TRS_PU_CFG_0, 0, 17, 18, "mmc");
	sysconf_write(sc, 1);

	platform_device_register(&fli7510_mmc_device);
}

/* sysconf resources ------------------------------------------------------ */

#ifdef CONFIG_DEBUG_FS

#define SYSCONF_REG(field) _SYSCONF_REG(#field, field)
#define _SYSCONF_REG(name, group, num) case num: return name

static const char *fli7510_sysconf_PRB_PU_CFG_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_RESET_CTL);
	SYSCONF_REG(CFG_BOOT_CTL);
	SYSCONF_REG(CFG_SYS1);
	SYSCONF_REG(CFG_MPX_CTL);
	SYSCONF_REG(CFG_PWR_DWN_CTL);
	SYSCONF_REG(CFG_SYS2);
	SYSCONF_REG(CFG_MODE_PIN_STATUS);
	SYSCONF_REG(CFG_PCI_ROPC_STATUS);
	}

	return "???";
}

static const char *fli7510_sysconf_PRB_PU_CFG_2(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_ST40_HOST_BOOT_ADDR);
	SYSCONF_REG(CFG_ST40_CTL_BOOT_ADDR);
	SYSCONF_REG(CFG_SYS10);
	SYSCONF_REG(CFG_RNG_BIST_CTL);
	SYSCONF_REG(CFG_SYS12);
	SYSCONF_REG(CFG_SYS13);
	SYSCONF_REG(CFG_SYS14);
	SYSCONF_REG(CFG_EMI_ROPC_STATUS);
	}

	return "???";
}

static const char *fli7510_sysconf_TRS_SPARE_REGS_0(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_COMMS_CONFIG_1);
	SYSCONF_REG(CFG_TRS_CONFIG);
	SYSCONF_REG(CFG_COMMS_CONFIG_2);
	SYSCONF_REG(CFG_USB_SOFT_JTAG);
	SYSCONF_REG(CFG_TRS_SPARE_REG5_NOTUSED_0);
	SYSCONF_REG(CFG_TRS_CONFIG_2);
	SYSCONF_REG(CFG_COMMS_TRS_STATUS);
	SYSCONF_REG(CFG_EXTRA_ID1_LSB);
	}

	return "???";
}

static const char *fli7510_sysconf_TRS_SPARE_REGS_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_SPARE_1);
	SYSCONF_REG(CFG_SPARE_2);
	SYSCONF_REG(CFG_SPARE_3);
	SYSCONF_REG(CFG_TRS_SPARE_REG4_NOTUSED);
	SYSCONF_REG(CFG_TRS_SPARE_REG5_NOTUSED_1);
	SYSCONF_REG(CFG_TRS_SPARE_REG6_NOTUSED);
	SYSCONF_REG(CFG_DEVICE_ID);
	SYSCONF_REG(CFG_EXTRA_ID1_MSB);
	}

	return "???";
}

static const char *fli7510_sysconf_VDEC_PU_CFG_0(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_TOP_SPARE_REG1);
	SYSCONF_REG(CFG_TOP_SPARE_REG2);
	SYSCONF_REG(CFG_TOP_SPARE_REG3);
	SYSCONF_REG(CFG_ST231_DRA2_DEBUG);
	SYSCONF_REG(CFG_ST231_AUD1_DEBUG);
	SYSCONF_REG(CFG_ST231_AUD2_DEBUG);
	SYSCONF_REG(CFG_REG7_0);
	SYSCONF_REG(CFG_INTERRUPT);
	}

	return "???";
}

static const char *fli7510_sysconf_VDEC_PU_CFG_1(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_ST231_DRA2_PERIPH_REG1);
	SYSCONF_REG(CFG_ST231_DRA2_BOOT_REG2);
	SYSCONF_REG(CFG_ST231_AUD1_PERIPH_REG3);
	SYSCONF_REG(CFG_ST231_AUD1_BOOT_REG4);
	SYSCONF_REG(CFG_ST231_AUD2_PERIPH_REG5);
	SYSCONF_REG(CFG_ST231_AUD2_BOOT_REG6);
	SYSCONF_REG(CFG_REG7_1);
	SYSCONF_REG(CFG_INTERRUPT_REG8);
	}

	return "???";
}

static const char *fli7510_sysconf_VOUT_SPARE_REGS(int num)
{
	switch (num) {
	SYSCONF_REG(CFG_REG1_VOUT_PIO_ALT_SEL);
	SYSCONF_REG(CFG_REG2_VOUT_PIO_ALT_SEL);
	SYSCONF_REG(CFG_VOUT_SPARE_REG3);
	SYSCONF_REG(CFG_REG4_DAC_CTRL);
	SYSCONF_REG(CFG_REG5_VOUT_DEBUG_PAD_CTL);
	SYSCONF_REG(CFG_REG6_TVOUT_DEBUG_CTL);
	SYSCONF_REG(CFG_REG7_UNUSED);
	}

	return "???";
}

static const char *fli7510_sysconf_CKG_DDR(int num)
{
	switch (num) {
	SYSCONF_REG(CKG_DDR_CTL_PLL_DDR_FREQ);
	SYSCONF_REG(CKG_DDR_STATUS_PLL_DDR);
	}

	return "???";
}
#endif

#ifdef CONFIG_DEBUG_FS
#define FLI7510_REG_NAME_FUNC(name) name
#else
#define FLI7510_REG_NAME_FUNC(name) NULL
#endif

#define FLI7510_SYSCONF_ENTRY(_id, _name, _start)			\
	{								\
		.name = "stm-sysconf",					\
		.id = _id,						\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_start, 0x20),		\
		},							\
		.dev.platform_data = &(struct stm_plat_sysconf_data) {	\
			.groups_num = 1,				\
			.groups = (struct stm_plat_sysconf_group []) {	\
				{					\
					.group = _name,			\
					.offset = 0,			\
					.name = #_name,			\
					.reg_name = 			\
			 FLI7510_REG_NAME_FUNC(fli7510_sysconf_##_name),\
				},					\
			},						\
		},							\
	}


static struct platform_device fli7510_sysconf_devices[] = {
	FLI7510_SYSCONF_ENTRY(0, PRB_PU_CFG_1, 0xfd220000),
	FLI7510_SYSCONF_ENTRY(1, PRB_PU_CFG_2, 0xfd228000),
	FLI7510_SYSCONF_ENTRY(2, TRS_SPARE_REGS_0, 0xfd9ec000),
	FLI7510_SYSCONF_ENTRY(3, TRS_SPARE_REGS_1, 0xfd9f4000),
	FLI7510_SYSCONF_ENTRY(4, VDEC_PU_CFG_0, 0xfd7a0000),
	FLI7510_SYSCONF_ENTRY(5, VDEC_PU_CFG_1, 0xfd7c0000),
	/* Addresss probed in fli7510_sysconf_setup() as different for ultra */
	FLI7510_SYSCONF_ENTRY(6, VOUT_SPARE_REGS, 0xfd5e8000),
	FLI7510_SYSCONF_ENTRY(7, CKG_DDR, 0xfde80000),
};

static void fli7510_sysconf_setup(void)
{
	struct resource *mem_res = &fli7510_sysconf_devices[6].resource[0];

	if (cpu_data->type != CPU_FLI7510) {
		mem_res->start = 0xfd5d4000;
		mem_res->end = mem_res->start + 0x20 - 1;
	}
}



/* Early initialisation-- --------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init fli7510_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long verid;
	char *chip_variant;
	unsigned long devid;
	unsigned long chip_revision;
	int gpios_num;

	verid = *((unsigned *)0xfd9e9078) >> 16;

	if (cpu_data->type == CPU_FLI7510) {
		if (verid != 0x1d56)
			printk(KERN_WARNING "Wrong chip variant data, "
					"assuming FLI7510!\n");
		chip_variant = "510";
	} else {
		/* CPU should be detected as 520 so far... */
		WARN_ON(!CPU_FLI7520);

		switch (verid) {
		case 0x1d60:
			cpu_data->type = CPU_FLI7520;
			chip_variant = "520";
			break;
		case 0x1d6a:
			cpu_data->type = CPU_FLI7530;
			chip_variant = "530";
			break;
		case 0x1d74:
			cpu_data->type = CPU_FLI7540;
			chip_variant = "540";
			break;
		default:
			printk(KERN_WARNING "Wrong chip variant data, "
					"assuming FLI7540!\n");
			cpu_data->type = CPU_FLI7540;
			chip_variant = "520/530/540";
			break;
		}
	}


	/* Initialise PIO and sysconf drivers */

	fli7510_sysconf_setup();
	sysconf_early_init(fli7510_sysconf_devices,
			ARRAY_SIZE(fli7510_sysconf_devices));

	if (cpu_data->type == CPU_FLI7510) {
		gpios_num = ARRAY_SIZE(fli7510_pio_devices);
		stm_gpio_early_init(fli7510_pio_devices, gpios_num,
				ILC_FIRST_IRQ + ILC_NR_IRQS);
	} else {
		gpios_num = ARRAY_SIZE(fli7520_pio_devices);
		stm_gpio_early_init(fli7520_pio_devices, gpios_num,
				ILC_FIRST_IRQ + ILC_NR_IRQS);
	}
	stm_pad_init(gpios_num * STM_GPIO_PINS_PER_PORT,
		     -1, 0, fli7510_pio_config);

	sc = sysconf_claim(CFG_DEVICE_ID, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28);
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "Freeman %s version %ld.x, ST40%s core\n",
			chip_variant, chip_revision,
			FLI7510_ST40HOST_CORE ? "HOST" : "RT");

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init fli7510_postcore_setup(void)
{
	int result;
	int i;

	result = platform_device_register(&fli7510_emi);

	if (cpu_data->type == CPU_FLI7510) {
		for (i = 0; i < ARRAY_SIZE(fli7510_pio_devices) &&
				result == 0; i++) {
			result = platform_device_register(
					&fli7510_pio_devices[i]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(fli7520_pio_devices) &&
				result == 0; i++) {
			/* Skip non-existing ports... */
			if (fli7520_pio_devices[i].name)
				result = platform_device_register(
						&fli7520_pio_devices[i]);
		}
	}

	return result;
}
postcore_initcall(fli7510_postcore_setup);



/* Late initialisation ---------------------------------------------------- */

static struct platform_device *fli7510_devices[] __initdata = {
	&fli7510_fdma_devices[0],
	&fli7510_fdma_devices[1],
	&fli7510_fdma_xbar_device,
	&fli7510_sysconf_devices[0],
	&fli7510_sysconf_devices[1],
	&fli7510_sysconf_devices[2],
	&fli7510_sysconf_devices[3],
	&fli7510_sysconf_devices[4],
	&fli7510_sysconf_devices[5],
	&fli7510_sysconf_devices[6],
	&fli7510_rng_hwrandom_device,
	&fli7510_rng_devrandom_device,
};

static int __init fli7510_devices_setup(void)
{
	return platform_add_devices(fli7510_devices,
			ARRAY_SIZE(fli7510_devices));
}
device_initcall(fli7510_devices_setup);
