/*
 * STx710x Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/io.h>
#include <linux/stm/soc.h>
#include <asm/sci.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID        (SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)      (SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)      (SYSCONF_BASE + 0x100 + ((n) * 4))

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xffe00000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 26, 27, 28, 29 },
	}, {
		.mapbase	= 0xffe80000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 43, 44, 45, 46 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct resource wdt_resource[] = {
	/* Watchdog timer only needs a register address */
	[0] = {
		.start = 0xFFC00008,
		.end = 0xFFC00010,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device wdt_device = {
	.name = "wdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(wdt_resource),
	.resource = wdt_resource,
};

static struct resource rtc_resource[]= {
	[0] = {
		.start = 0xffc80000,
		.end   = 0xffc80000 + 0x40,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = 20,/* Alarm IRQ   */
		.flags = IORESOURCE_IRQ
	},
	[2] = {
		.start = 21,/* Periodic IRQ*/
		.flags = IORESOURCE_IRQ
	},
};
static struct platform_device rtc_device = {
	.name		= "rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resource),
	.resource	= rtc_resource,
};

static struct resource st40_ohci_resources[] = {
	/*this lot for the ohci block*/
	[0] = {
		.start = 0xb9100000 + 0xffc00,
		.end  =  0xb9100000 +0xffcff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
			.start = 168,
			.end   = 168,
			.flags = IORESOURCE_IRQ,
	}
};
static struct resource st40_ehci_resources[] = {
	/*now this for the ehci*/
	[0] =  {
			.start = 0xb9100000 + 0xffe00,
			.end = 0xb9100000 + 0xffeff,
			.flags = IORESOURCE_MEM,
	},
	[1] = {
			.start = 169,
			.end   = 169,
			.flags = IORESOURCE_IRQ,
	},
};

static u64 st40_dma_mask = 0xfffffff;

static struct platform_device  st40_ohci_devices = {
	.name = "ST40-ohci",
	.id=1,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
	},
	.num_resources = ARRAY_SIZE(st40_ohci_resources),
	.resource = st40_ohci_resources,
};

static struct platform_device  st40_ehci_devices = {
	.name = "ST40-ehci",
	.id=2,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
	},
	.num_resources = ARRAY_SIZE(st40_ehci_resources),
	.resource = st40_ehci_resources,
};

static struct platform_device *stx710x_devices[] __initdata = {
	&sci_device,
	&wdt_device,
	&rtc_device,
	&st40_ohci_devices,
	&st40_ehci_devices,
};

static int __init stx710x_devices_setup(void)
{
	return platform_add_devices(stx710x_devices,
				    ARRAY_SIZE(stx710x_devices));
}
device_initcall(stx710x_devices_setup);

/*
 * INTC style interrupts
 */
static struct ipr_data ipr_map[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0, 12, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 19, 0,  4, 2 }, /* TMU2 TIPCI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 20, 0,  0, 2 }, /* RTC ATI (alarm) */
	{ 21, 0,  0, 2 }, /* RTC PRI (period) */
	{ 22, 0,  0, 2 }, /* RTC CUI (carry) */
	{ 23, 1,  4, 3 }, /* SCI ERI */
	{ 24, 1,  4, 3 }, /* SCI RXI */
	{ 25, 1,  4, 3 }, /* SCI TXI */
	{ 40, 2,  4, 3 }, /* SCIF ERI */
	{ 41, 2,  4, 3 }, /* SCIF RXI */
	{ 42, 2,  4, 3 }, /* SCIF BRI */
	{ 43, 2,  4, 3 }, /* SCIF TXI */
	{ 34, 2,  8, 7 }, /* DMAC DMTE0 */
	{ 35, 2,  8, 7 }, /* DMAC DMTE1 */
	{ 36, 2,  8, 7 }, /* DMAC DMTE2 */
	{ 37, 2,  8, 7 }, /* DMAC DMTE3 */
	{ 28, 2,  8, 7 }, /* DMAC DMAE */
};

/*
 * INTC2-Style interrupts, vectors IRQ 112-175 INTEVT 0x1000-0x17e0
 */
static struct intc2_data intc2_irq_table[] = {
	/* IRQ, IPR index, IPR shift, mask index, mask shift, prio */
	{113, 4,  0, 4,  1, 13},	/* Group0:  pio5 */
	{114, 4,  0, 4,  2, 13},	/*          pio4 */
	{115, 4,  0, 4,  3, 13},	/*          pio3 */

	{117, 4,  4, 4,  5, 13},	/* Group1:  ssc2 */
	{118, 4,  4, 4,  6, 13} ,	/*          ssc1 */
	{119, 4,  4, 4,  7, 13},	/*          ssc0 */

	{120, 4,  8, 4,  8, 13},	/* Group2:  uart3 */
	{121, 4,  8, 4,  9, 13},	/*          uart2 */
	{122, 4,  8, 4, 10, 13},	/*          uart1 */
	{123, 4,  8, 4, 11, 13},	/*          uart0 */

	{124, 4, 12, 4, 12, 13},	/* Group3:  irb_wakeup */
	{125, 4, 12, 4, 13, 13},	/*          irb */
	{126, 4, 12, 4, 14, 13},	/*          pwm */
	{127, 4, 12, 4, 15, 13},	/*          mafe */

	{129, 4, 16, 4, 17, 13},	/* Group4:  disqec */
	{130, 4, 16, 4, 18, 13},	/*          daa */
	{131, 4, 16, 4, 19, 13},	/*          ttxt */

	{135, 4, 20, 4, 23, 13},	/* Group5:  sbatm */

	{136, 4, 24, 4, 24, 13},	/* Group6:  lx_delphi */
	{137, 4, 24, 4, 25, 13},	/*          lx_aud */
	{138, 4, 24, 4, 26, 13},	/*          dcxo */

	{140, 4, 28, 4, 28, 13},	/* Group7:  fdma_mbox */
	{141, 4, 28, 4, 29, 13},	/*          fdma_gp0 */
	{142, 4, 28, 4, 30, 13},	/*          i2s2spdif */
	{143, 4, 28, 4, 31, 13},	/*          cpxm */

	{144, 8,  0, 8,  0, 13},	/* Group8:  pcmplyr0 */
	{145, 8,  0, 8,  1, 13},	/*          pcmplyr1 */
	{146, 8,  0, 8,  2, 13},	/*          pcmrdr */
	{147, 8,  0, 8,  3, 13},	/*          spdifplyr */

	{148, 8,  4, 8,  4, 13},	/* Group9:  glh */
	{149, 8,  4, 8,  5, 13},	/*          delphi_pre0 */
	{150, 8,  4, 8,  6, 13},	/*          delphi_pre1 */
	{151, 8,  4, 8,  7, 13},	/*          delphi_mbe */

	{153, 8,  8, 8,  9, 13},	/* Group10:  lmu */
	{154, 8,  8, 8, 10, 13},	/*           vtg1 */
	{155, 8,  8, 8, 11, 13},	/*           vtg2 */

	{156, 8, 12, 8, 12, 13},	/* Group11:  blt */
	{157, 8, 12, 8, 13, 13},	/*           dvp */
	{158, 8, 12, 8, 14, 13},	/*           hdmi */
	{159, 8, 12, 8, 15, 13},	/*           hdcp */

	{160, 8, 16, 8, 16, 13},	/* Group12:  pti */
	{162, 8, 16, 8, 18, 13},	/*           pdes */

	{164, 8, 20, 8, 20, 13},	/* Group13:  sig_chk */
	{165, 8, 20, 8, 21, 13},	/*           dma_fin */
	{166, 8, 20, 8, 22, 13},	/*           sec_cp */

	{168, 8, 24, 8, 24, 13},	/* Group14:  ohci */
	{169, 8, 24, 8, 25, 13},	/*           ehci */
	{170, 8, 24, 8, 26, 13},	/*           sata */
};

static struct intc2_desc intc2_irq_desc __read_mostly = {
	.prio_base	= 0xb9001300,
	.msk_base	= 0xb9001340,
	.mskclr_base	= 0xb9001360,

	.intc2_data	= intc2_irq_table,
	.nr_irqs	= ARRAY_SIZE(intc2_irq_table),

	.chip = {
		.name	= "INTC2-stx710x",
	},
};

static struct ipr_data ipr_irq_table[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0,  8, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 32, 2,  0, 7 }, /* HUDI */
/* these here are only valid if INTC_ICR bit 7 is set to 1!
#if 1
	{  2, 3, 12, 3 }, /* IRL0 */
	{  5, 3,  8, 3 }, /* IRL1 */
	{  8, 3,  4, 3 }, /* IRL2 */
	{ 11, 3,  0, 3 }, /* IRL3 */
#endif
};

static unsigned long ipr_offsets[] = {
	0xffd00004UL,	/* 0: IPRA */
	0xffd00008UL,	/* 1: IPRB */
	0xffd0000cUL,	/* 2: IPRC */
	0xffd00010UL,	/* 3: IPRD */
