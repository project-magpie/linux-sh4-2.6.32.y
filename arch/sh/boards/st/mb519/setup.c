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
#include <asm/led.h>

#define SYSCONF_BASE 0xfd704000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

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

#define EPLD_Flash		(EPLD_BASE + 0x400000)
#define EPLD_Stem		(EPLD_BASE + 0x500000)
#define EPLD_StemSet		(EPLD_BASE + 0x600000)
#define EPLD_StemClr		(EPLD_BASE + 0x700000)
#define EPLD_DACSPMux		(EPLD_BASE + 0xD00000)

/*
 * Initialize the board
 */
void __init mb519_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision;
	unsigned short epld_rev = ctrl_inw(EPLD_ver);
	unsigned short pcb_rev = ctrl_inw(EPLD_cpcbver);

	printk("STMicroelectronics STx7200 Mboard initialisation\n");
	printk("mb519 PCB rev %X EPLD rev %dr%d\n",
	       pcb_rev,
	       epld_rev >> 4, epld_rev & 0xf);

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

	/* Permanently enable Flash VPP */
	ctrl_outw(3, EPLD_Flash);

	/* ClockgenB powers up with all the frequency synths bypassed.
	 * Enable them all here.  Without this, USB 1.1 doesn't work,
	 * as it needs a 48MHz clock which is separate from the USB 2
	 * clock which is derived from the SATA clock. */
	ctrl_outl(0, 0xFD701048);

	stx7200eth_hw_setup(0, 0, 1);
	stx7200eth_hw_setup(1, 0, 1);
}

#if 0 // def CONFIG_MTD_PHYSMAP
static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	},
	{
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	},
	{
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,      /* will expand to the end of the flash */
		.offset = 0x00140000,
	}
};

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		ctrl_outw(3, EPLD_Flash);
	} else {
		ctrl_outw(2, EPLD_Flash);
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};
#define physmap_flash_data_addr &physmap_flash_data
#else
#define physmap_flash_data_addr NULL
#endif

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= physmap_flash_data_addr,
	},
};

static struct platform_device *mb519_devices[] __initdata = {
	&physmap_flash,
};

static int __init device_init(void)
{
	return platform_add_devices(mb519_devices, ARRAY_SIZE(mb519_devices));
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
	 *
	 * Note that this changed between EPLD rev 1r2 and 1r3. This is correct
	 * for 1r3 which should be the most common now.
	 */
	ctrl_outw(1<<4, EPLD_IntMask0Set); /* IntPriority(4) <= not STEM_notINTR0 */
}

/* Flash the heartbeat LED (LD12T) on the mb520 */
void mach_led(int position, int value)
{
	static struct stpio_pin *led = NULL;
	if (led == NULL)
		led = stpio_request_pin(4, 7, "LED", STPIO_OUT);

	stpio_set_pin(led, !value);
}

struct sh_machine_vector mv_stx7200mboard __initmv = {
	.mv_name		= "STx7200 Reference board";
	.mv_setup		= mb442_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= stx7200mboard_init_irq,
	.mv_ioport_map		= stx7200mboard_ioport_map,
#ifdef CONFIG_HEARTBEAT
	.mv_heartbeat		= heartbeat_heart,
#endif
};
ALIAS_MV(stx7200mboard)
