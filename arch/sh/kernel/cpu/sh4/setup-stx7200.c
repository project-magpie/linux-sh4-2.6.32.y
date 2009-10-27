/*
 * STx7200 SH-4 Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/sysconf.h>
#include <asm/irq-ilc.h>



/* SH4-only resources ----------------------------------------------------- */

static struct resource rtc_resource[]= {
        [0] = {
		.start = 0xffc80000,
		.end   = 0xffc80000 + 0x3c,
	        .flags = IORESOURCE_IO
	},
	[1] = { /* periodic irq */
		.start = 21,
		.end   = 21,
	        .flags = IORESOURCE_IRQ
	},
	[2] = { /* carry irq */
		.start = 22,
		.end   = 22,
	        .flags = IORESOURCE_IRQ
	},
	[3] = { /* alarm irq */
		.start = 20,
		.end   = 20,
	        .flags = IORESOURCE_IRQ
	},
};

static struct platform_device rtc_device = {
	.name           = "sh-rtc",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(rtc_resource),
	.resource       = rtc_resource,
};

static struct platform_device ilc3_device = {
	.name		= "ilc3",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0xfd804000,
			.end	= 0xfd804000 + 0x900,
			.flags	= IORESOURCE_MEM
		}
	},
};

static struct platform_device *stx7200_sh4_devices[] __initdata = {
	&rtc_device,
	&ilc3_device,
};

static int __init stx7200_sh4_devices_setup(void)
{
	return platform_add_devices(stx7200_sh4_devices,
			ARRAY_SIZE(stx7200_sh4_devices));
}
device_initcall(stx7200_sh4_devices_setup);



/* Interrupt initialisation ----------------------------------------------- */

enum {
	UNUSED = 0,

	/* interrupt sources */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	RTC_ATI, RTC_PRI, RTC_CUI,
	SCIF_ERI, SCIF_RXI, SCIF_BRI, SCIF_TXI,
	WDT,
	HUDI,

	/* interrupt groups */
	TMU2, RTC, SCIF,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(RTC_ATI, 0x480), INTC_VECT(RTC_PRI, 0x4a0),
	INTC_VECT(RTC_CUI, 0x4c0),
	INTC_VECT(SCIF_ERI, 0x4e0), INTC_VECT(SCIF_RXI, 0x500),
	INTC_VECT(SCIF_BRI, 0x520), INTC_VECT(SCIF_TXI, 0x540),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),
	INTC_GROUP(RTC, RTC_ATI, RTC_PRI, RTC_CUI),
	INTC_GROUP(SCIF, SCIF_ERI, SCIF_RXI, SCIF_BRI, SCIF_TXI),
};

static struct intc_prio_reg prio_registers[] = {
					/*  15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */ { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */ {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */ {    0,    0,    0,  HUDI } },
};

static DECLARE_INTC_DESC(intc_desc, "stx7200", vectors, groups,
			 NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	int irq;
	struct sysconf_field *sc;

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);

	for (irq=0; irq<16; irq++) {
		set_irq_chip(irq, &dummy_irq_chip);
		set_irq_chained_handler(irq, ilc_irq_demux);
	}

	ilc_early_init(&ilc3_device);
	ilc_demux_init();
}
