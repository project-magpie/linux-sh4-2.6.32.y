/*
 * STx5197 SH-4 Setup
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/irq-ilc.h>

/* SH4-only resources ----------------------------------------------------- */

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd100000,
			.end	= 0xfd100000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

static struct platform_device *stx5197_sh4_devices[] __initdata = {
	&ilc3_device,
};

static int __init stx5197_sh4_devices_setup(void)
{
	return platform_add_devices(stx5197_sh4_devices,
				    ARRAY_SIZE(stx5197_sh4_devices));
}
device_initcall(stx5197_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	WDT,
	HUDI,

	/* interrupt groups */
	TMU2, RTC,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,       } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0,    0,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
};

static DECLARE_INTC_DESC(intc_desc, "stx5197", vectors, groups,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int i;

	register_intc_controller(&intc_desc);

	for (i = 0; i < 16; i++) {
		/*
		 * This is a hack to allow for the fact that we don't
		 * register a chip type for the IRL lines. Without
		 * this the interrupt type is "no_irq_chip" which
		 * causes problems when trying to register the chained
		 * handler.
		 */
		set_irq_chip(i, &dummy_irq_chip);

		set_irq_chained_handler(i, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
