/*
 * arch/sh/boards/st/cb101/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * cb101 board support.
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
void __init cb101_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision;

	printk("cb101 board initialisation\n");

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

	/* ClockgenB powers up with all the frequency synths bypassed.
	 * Enable them all here.  Without this, USB 1.1 doesn't work,
	 * as it needs a 48MHz clock which is separate from the USB 2
	 * clock which is derived from the SATA clock. */
	ctrl_outl(0, 0xFD701048);

	stx7200eth_hw_setup(0, 0, 0);
}

static void phy_reset(void* bus)
{
	static struct stpio_pin *ethreset = NULL;

	if (ethreset == NULL) {
		ethreset = stpio_request_pin(4, 7, "STE101P_RST", STPIO_OUT);
	}

	stpio_set_pin(ethreset, 1);
	udelay(1);
	stpio_set_pin(ethreset, 0);
	udelay(1000);
	stpio_set_pin(ethreset, 1);
}

static struct plat_stmmacenet_data stmmaceth_private_data = {
	.bus_id = 0,
	.phy_addr = 14,
	.phy_mask = 0,
	.phy_name = "ste101p",
	.pbl = 32,
	.fix_mac_speed = fix_mac_speed,
	.phy_reset = phy_reset,
};

static int __init device_init(void)
{
	// return platform_add_devices(cb101_devices, ARRAY_SIZE(cb101_devices));
}
device_initcall(device_init);

static void __iomem *cb101_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init cb101_init_irq(void)
{
}

struct sh_machine_vector mv_cb101 __initmv = {
	.mv_name		= "cb101";
	.mv_setup		= cb101_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= cb101_ioport_map,
};
ALIAS_MV(cb101)
