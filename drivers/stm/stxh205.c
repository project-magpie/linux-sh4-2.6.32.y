/*
 * (c) 2011 STMicroelectronics Limited
 *
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/stm/emi.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stxh205.h>
#include <asm/irq-ilc.h>
#include "pio-control.h"

/* PIO ports resources ---------------------------------------------------- */

#define STXH205_PIO_ENTRY(_num, _base, _irq)				\
	[_num] = {							\
		.name = "stm-gpio",					\
		.id = _num,						\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_base, 0x100),		\
			STM_PLAT_RESOURCE_IRQ(_irq, -1),		\
		},							\
	}

static struct platform_device stxh205_pio_devices[16] = {
	/* PIO_SBC: 0-3 */
	STXH205_PIO_ENTRY(0,  0xfe610000, 129),
	STXH205_PIO_ENTRY(1,  0xfe611000, 130),
	STXH205_PIO_ENTRY(2,  0xfe612000, 131),
	STXH205_PIO_ENTRY(3,  0xfe613000, 132),

	/* PIO_BANK_1_0: 4-8 */
	STXH205_PIO_ENTRY(4,  0xfda60000, 133),
	STXH205_PIO_ENTRY(5,  0xfda61000, 134),
	STXH205_PIO_ENTRY(6,  0xfda62000, 135),
	STXH205_PIO_ENTRY(7,  0xfda63000, 136),
	STXH205_PIO_ENTRY(8,  0xfda64000, 137),

	/* PIO_BANK_1_1: 9-12 */
	STXH205_PIO_ENTRY(9,  0xfda70000, 138),
	STXH205_PIO_ENTRY(10, 0xfda71000, 139),
	STXH205_PIO_ENTRY(11, 0xfda72000, 140),
	STXH205_PIO_ENTRY(12, 0xfda73000, 141),

	/* PIO_BANK_2: 13-15 */
	STXH205_PIO_ENTRY(13, 0xfde20000, 142),
	STXH205_PIO_ENTRY(14, 0xfde21000, 143),
	STXH205_PIO_ENTRY(15, 0xfde22000, 144),
};

#define STXH205_PIO_ENTRY_CONTROL(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _lsb, _msb,			\
		_rt)							\
	[_num] = {							\
		.alt = { SYSCONF(_alt_num) },				\
		.oe = { SYSCONF(_oe_num), _lsb, _msb },			\
		.pu = { SYSCONF(_pu_num), _lsb, _msb },			\
		.od = { SYSCONF(_od_num), _lsb, _msb },			\
		.retiming = {						\
			{ SYSCONF(_rt) },				\
			{ SYSCONF(_rt+1) },				\
		},							\
	}

#define STXH205_PIO_ENTRY_CONTROL4(_num, _alt_num,		\
		_oe_num, _pu_num, _od_num, _rt)			\
	STXH205_PIO_ENTRY_CONTROL(_num,   _alt_num,		\
		_oe_num, _pu_num, _od_num,  0,  7,		\
		_rt),						\
	STXH205_PIO_ENTRY_CONTROL(_num+1, _alt_num+1,		\
		_oe_num, _pu_num, _od_num,  8, 15,		\
		_rt+2),						\
	STXH205_PIO_ENTRY_CONTROL(_num+2, _alt_num+2,		\
		_oe_num, _pu_num, _od_num, 16, 23,		\
		_rt+4),						\
	STXH205_PIO_ENTRY_CONTROL(_num+3, _alt_num+3,		\
		_oe_num, _pu_num, _od_num, 24, 31,		\
		_rt+6)

static const struct stm_pio_control_config stxh205_pio_control_configs[16] = {
	/*                        pio, alt,  oe,  pu,  od,lsb,msb, rt */
	/* 0-3: SBC */
	STXH205_PIO_ENTRY_CONTROL4(0,    0,   4,   5,   6,           7),
	/* 4-12: BANK 1 */
	STXH205_PIO_ENTRY_CONTROL4(4,  100, 109, 112, 115,         118),
	STXH205_PIO_ENTRY_CONTROL4(8,  104, 110, 113, 116,         126),
	STXH205_PIO_ENTRY_CONTROL(12,  108, 111, 114, 117,  0,  7, 134),
	/* 13-15: BANK 2 */
	STXH205_PIO_ENTRY_CONTROL(13,  200, 203, 204, 205,  0,  7, 206),
	STXH205_PIO_ENTRY_CONTROL(14,  201, 203, 204, 205,  8, 15, 208),
	STXH205_PIO_ENTRY_CONTROL(15,  202, 203, 204, 205, 16, 23, 210),
};

static struct stm_pio_control stxh205_pio_controls[16];

static int stxh205_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stxh205_pio_config *config = priv;

	BUG_ON(port > ARRAY_SIZE(stxh205_pio_devices));
	BUG_ON(function < 0 || function > 5);

	if (function == 0) {
		switch (direction) {
		case stm_pad_gpio_direction_in:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
			break;
		case stm_pad_gpio_direction_out:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_OUT);
			break;
		case stm_pad_gpio_direction_bidir:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_BIDIR);
			break;
		default:
			BUG();
			break;
		}
	} else {
		stm_pio_control_config_direction(port, pin, direction,
				config ? config->mode : NULL);
	}

	stm_pio_control_config_function(port, pin, function);

	if (config && config->retime)
		stm_pio_control_config_retime(port, pin, config->retime);

	return 0;
}

static const struct stm_pio_control_retime_offset stxh205_pio_retime_offset = {
	.clk1notclk0_offset 	= 0,
	.delay_lsb_offset	= 2,
	.delay_msb_offset	= 3,
	.invertclk_offset	= 4,
	.retime_offset		= 5,
	.clknotdata_offset	= 6,
	.double_edge_offset	= 7,
};

static void __init stxh205_pio_init(void)
{
	stm_pio_control_init(stxh205_pio_control_configs, stxh205_pio_controls,
			     ARRAY_SIZE(stxh205_pio_control_configs),
			     &stxh205_pio_retime_offset);
}

/* sysconf resources ------------------------------------------------------ */

void stxh205_sysconf_reg_name(char *name, int size, int group, int num)
{
	if (group >= 3)
		group++;
	snprintf(name, size, "SYSCONF%d", (group * 100) + num);
}

static struct platform_device stxh205_sysconf_devices[] = {
	{
		/* SBC system configuration bank 0 registers */
		/* SYSCFG_BANK0 (aka SYSCFG_SBC, Sapphire): 0-42 */
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe600000, 0xac),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 0,
					.offset = 0,
					.name = "SYSCFG_SBC",
					.reg_name = stxh205_sysconf_reg_name,
				}
			},
		}
	}, {
		/* SYSCFG_BANK1 (aka Coral): 100-176 */
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfda50000, 0x134),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 1,
					.offset = 0,
					.name = "SYSCFG_BANK1",
					.reg_name = stxh205_sysconf_reg_name,
				}
			},
		}
	}, {
		/* SYSCFG_BANK2 (aka Perl): 200-243 */
		.name		= "sysconf",
		.id		= 2,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd500000, 0xb0),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 2,
					.offset = 0,
					.name = "SYSCFG_BANK2",
					.reg_name = stxh205_sysconf_reg_name,
				}
			},
		}
	}, {
		/* SYSCFG_BANK3 (aka Opal): 400-510 */
		.name		= "sysconf",
		.id		= 3,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd541000, 0x1bc),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 3,
					.offset = 0,
					.name = "SYSCFG_BANK3",
					.reg_name = stxh205_sysconf_reg_name,
				}
			},
		}
	}, {
		/* LPM Configuration registers */
		.name		= "sysconf",
		.id		= 4,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe4b5100, 0x54),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 4,
					.offset = 0,
					.name = "LPM_CFG",
				}
			},
		}
	},
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stxh205_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;
	unsigned long chip_revision;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(stxh205_sysconf_devices,
			ARRAY_SIZE(stxh205_sysconf_devices));
	stxh205_pio_init();
	stm_gpio_early_init(stxh205_pio_devices,
			ARRAY_SIZE(stxh205_pio_devices),
			ILC_FIRST_IRQ + ILC_NR_IRQS);
	stm_pad_init(ARRAY_SIZE(stxh205_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, stxh205_pio_config);

	sc = sysconf_claim(SYSCONF(41), 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_revision = (devid >> 28) + 1;
	boot_cpu_data.cut_major = chip_revision;

	printk(KERN_INFO "STxH205/7 version %ld.x\n", chip_revision);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stxh205_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stxh205_pio_devices); i++)
		platform_device_register(&stxh205_pio_devices[i]);

	return 0;
}
postcore_initcall(stxh205_postcore_setup);

/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stxh205_devices[] __initdata = {
	&stxh205_sysconf_devices[0],
	&stxh205_sysconf_devices[1],
	&stxh205_sysconf_devices[2],
	&stxh205_sysconf_devices[3],
	&stxh205_sysconf_devices[4],
};

static int __init stxh205_devices_setup(void)
{
	return platform_add_devices(stxh205_devices,
			ARRAY_SIZE(stxh205_devices));
}
device_initcall(stxh205_devices_setup);
