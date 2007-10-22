/*
 * arch/sh/boards/st/mb519/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7200 Mboard support.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <asm/io.h>

#define SYSCONF_BASE 0xfd704000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

/*
 * Initialize the board
 */
void __init mb519_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision;

	printk("STMicroelectronics STx7200 Mboard initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_revision = (sysconf >> 28) +1;

	printk("STx7200 version %ld.x\n", chip_revision);

	/* Serial port set up */
	/* Route UART2&3 or SCI inputs instead of DVP to pins: conf_pad_dvp = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(40));
	sysconf &= ~(1<<16);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(40));

	/* Route UART2&3/SCI outputs instead of DVP to pins: conf_pad_pio[1]=0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<25);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* No idea, more routing: conf_pad_pio[0] = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<24);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Route UART2 (inputs and outputs) instead of SCI to pins: ssc2_mux_sel = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<2);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* conf_pad_pio[4] = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<28);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Route UART3 (inputs and outputs) instead of SCI to pins: ssc3_mux_sel = 0 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~(1<<3);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* conf_pad_clkobs = 1 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf |= (1<<14);
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* I2C and USB related routing */
	/* bit4: ssc4_mux_sel = 0 (treat SSC4 as I2C) */
	/* bit26: conf_pad_pio[2] = 0 route USB etc instead of DVO */
	/* bit27: conf_pad_pio[3] = 0 DVO output selection (probably ignored) */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf &= ~((1<<27)|(1<<26)|(1<<4));
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Enable SOFT_JTAG mode.
	 * Taken from OS21, but is this correct?
	 */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(33));
	sysconf |= (1<<6);
	sysconf &= ~((1<<0)|(1<<1)|(1<<2)|(1<<3));
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(33));

	/* Route Ethernet pins to output */
	/* bit26-16: conf_pad_eth(10:0) */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(41));
	/* MII0: conf_pad_eth(0) = 0 (ethernet) */
	sysconf &= ~(1<<16);
	/* MII1: conf_pad_eth(2) = 0, (3)=0, (4)=0, (9)=0, (10)=0 (ethernet)
	 * MII1: conf_pad_eth(6) = 0 (MII1TXD[0] = output) */
	sysconf &= ~( (1<<(16+2)) | (1<<(16+3)) | (1<<(16+4)) | (1<<(16+6)) |
		      (1<<(16+9)) | (1<<(16+10)));
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(41));

	stx7200eth_hw_setup(0);
	stx7200eth_hw_setup(1);
}

static int __init device_init(void)
{
}
subsys_initcall(device_init);

static void __iomem *stx7200mboard_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init stx7200mboard_init_irq(void)
{
	/* The off chip interrupts on the mb519 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active.
	 */
#define EPLD_BASE 0xa5000000
#define EPLD_ver		(EPLD_BASE + 0x000000)
#define EPLD_cpcbver		(EPLD_BASE + 0x020000)
#define EPLD_stem		(EPLD_BASE + 0x040000)
#define EPLD_driver		(EPLD_BASE + 0x060000)
#define EPLD_reset		(EPLD_BASE + 0x080000)
#define EPLD_IntStat0		(EPLD_BASE + 0x0A0000)
#define EPLD_IntStat1		(EPLD_BASE + 0x0C0000)
#define EPLD_IntMask0		(EPLD_BASE + 0x0E0000)
#define EPLD_IntMask0Set	(EPLD_BASE + 0x100000)
#define EPLD_IntMask0Clear	(EPLD_BASE + 0x120000)
#define EPLD_IntMask1		(EPLD_BASE + 0x140000)
#define EPLD_IntMask1Set	(EPLD_BASE + 0x160000)
#define EPLD_IntMask1Clear	(EPLD_BASE + 0x180000)
#define EPLD_LedStdAddr		(EPLD_BASE + 0x1A0000)

	printk("mb519 PCB rev %d EPLD rev %d\n",
	       ctrl_inw(EPLD_cpcbver), ctrl_inw(EPLD_ver));
	printk("intstat %02x %02x at %08x %08x\n",
	       ctrl_inw(EPLD_IntStat0), ctrl_inw(EPLD_IntStat1),
	       EPLD_IntStat0, EPLD_IntStat1);

	ctrl_outw(1<<3, EPLD_IntMask0Set); /* IntPriority(3) <= not STEM_notINTR0 */
}

struct sh_machine_vector mv_stx7200mboard __initmv = {
	.mv_name		= "STx7200 Reference board";
	.mv_setup		= mb442_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= stx7200mboard_init_irq,
	.mv_ioport_map		= stx7200mboard_ioport_map,
};
ALIAS_MV(stx7200mboard)
