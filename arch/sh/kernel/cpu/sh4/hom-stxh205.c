/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2011  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/stxh205.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/gpio.h>

#include <asm/irq-ilc.h>

#include "stm_hom.h"
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>
#include <linux/stm/wakeup_devices.h>


#define DDR3SS_REG		0xfde50000

#define PCLK	100000000
#define BAUDRATE_VAL_M1(bps)    	\
	((((bps * (1 << 14)) + (1 << 13)) / (PCLK / (1 << 6))))

#define SBC_MBX				0xfe4b4000
#define SBC_MBX_WRITE_STATUS(x)		(SBC_MBX + 0x4 + 0x4 * (x))

#ifdef CONFIG_STM_LPM
/*
 * This vercor is used to translate the sbc_wake_reason_bitmap
 * coming from the SBC to the interrupt number Linux uses
 */
int platform_lpm_to_irq[8] = {
	ILC_IRQ(5),	/* IRB */
	ILC_IRQ(31),	/* HDMI_CEC */
	ILC_IRQ(49),	/* Kscan */
	ILC_IRQ(22),	/* Wol */
	ILC_IRQ(6),	/* RTC */
	ILC_IRQ(43),	/* ASC */
	ILC_IRQ(107),	/* HDMI_HotPlug */
	0,		/* NMI (-2)?... */
};
#endif

static unsigned long stxh205_hom_table[] __cacheline_aligned = {
synopsys_ddr32_in_hom(DDR3SS_REG),

/*
 * Send message 'ENTER_PASSIVE' (0x5)
 */
POKE32(SBC_MBX_WRITE_STATUS(0), 0x5),
/* END. */
END_MARKER,

};

static void __iomem *early_console_base;
static struct stm_wakeup_devices stxh205_wkd;

static int stxh205_hom_begin(void)
{
	pr_info("[STM][PM] Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&stxh205_wkd);

	return 0;
}

static void stxh205_hom_early_console(void)
{

	writel(0x1189 & ~0x80, early_console_base + 0x0c); /* ctrl */
	writel(BAUDRATE_VAL_M1(115200), early_console_base); /* baud */
	writel(20, early_console_base + 0x1c);  /* timeout */
	writel(1, early_console_base + 0x10); /* int */
	writel(0x1189, early_console_base + 0x0c); /* ctrl */

	mdelay(100);

	pr_info("Early console ready\n");
}

static char hom_dirty_dtu_buf[16 * 4];

static int stxh205_hom_prepare(void)
{
	stm_freeze_board(NULL);

	/* Notify the wakeup device to the external micro */
	stm_notify_wakeup_devices(&stxh205_wkd);

	memcpy(hom_dirty_dtu_buf, 0x80004080, 16 * 4);

	return 0;
}

static int stxh205_hom_complete(void)
{
	memcpy(0x80004080, hom_dirty_dtu_buf, 16 * 4);

	/* Enable the INTC2 */
	writel(7, 0xfda30000 + 0x00);	/* INTPRI00 */
	writel(1, 0xfda30000 + 0x60);	/* INTMSKCLR00 */

	stm_defrost_board(NULL);

	__stm_set_wakeup_reason(stm_retrieve_wakeup_reason());
	return 0;
}

static struct stm_mem_hibernation stxh205_hom = {

	.tbl_addr = (unsigned long)stxh205_hom_table,
	.tbl_size = DIV_ROUND_UP(ARRAY_SIZE(stxh205_hom_table) *
			sizeof(long), L1_CACHE_BYTES),

	.ops.begin = stxh205_hom_begin,
	.ops.prepare = stxh205_hom_prepare,
	.ops.complete = stxh205_hom_complete,
};

static int __init hom_stxh205_setup(void)
{
	int ret;

	ret = stm_hom_register(&stxh205_hom);
	if (!ret) {
		early_console_base = (void *)
		ioremap(stm_asc_configured_devices[stm_asc_console_device]
			->resource[0].start, 0x1000);
		pr_info("[STM]: [PM]: [HoM]: Early console @ %p\n",
			early_console_base);
		stxh205_hom.early_console = stxh205_hom_early_console;
	}
	return ret;
}

module_init(hom_stxh205_setup);
