/*
 * arch/sh/boards/st/hms1/setup.c
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * HMS1 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <asm/io.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

/*
 * Initialize the board
 */
void __init hms1_setup(char** cmdline_p)
{
	unsigned long sysconf;
	unsigned long chip_revision, chip_7109;

	printk("HMS1 board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((sysconf >> 12) & 0x3ff) == 0x02c);
	chip_revision = (sysconf >> 28) +1;

	if (chip_7109)
		printk("STb7109 version %ld.x\n", chip_revision);
	else
		printk("STb7100 version %ld.x\n", chip_revision);

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));

	/* SCIF_PIO_OUT_EN=0 */
	/* Route UART2 and PWM to PIO4 instead of SCIF */
	sysconf &= ~(1<<0);

	/* Set SSC2_MUX_SEL = 0 */
	/* Treat SSC2 as I2C instead of SSC */
	sysconf &= ~(1<<3);

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Permanently enable Flash VPP */
	{
		static struct stpio_pin *pin;
		pin = stpio_request_pin(2,5, "VPP", STPIO_OUT);
		stpio_set_pin(pin, 1);
	}

	/* The ST40RTC sources its clock from clock */
	/* generator B */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(8));
	ctrl_outl(sysconf | 0x2, SYSCONF_SYS_CFG(8));

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ( ( chip_7109 && (chip_revision < 2)) ||
	     (!chip_7109 && (chip_revision < 3)) ) {
		static struct stpio_pin *pin;
		pin = stpio_request_pin(5,6, "USBOC", STPIO_OUT);
		stpio_set_pin(pin, 0);
	}

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();


	/* Configure the pio pins for LIRC */
	stpio_request_pin(3, 3, "IR", STPIO_IN);
	stpio_request_pin(3, 4, "IR", STPIO_IN);
	stpio_request_pin(3, 5, "IR", STPIO_ALT_OUT);
	stpio_request_pin(3, 6, "IR", STPIO_ALT_OUT);

#ifdef CONFIG_STM_PWM
	stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
#endif
}


/* Need a way to set:
static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

also need a way to set LIRC platform parms.
*/

#if 0
/* No board devives until smsc 911x is platformised */
static struct platform_device *hms1_devices[] __initdata = {
	&lirc_device,
};

static int __init device_init(void)
{
	return platform_add_devices(hms1_devices, ARRAY_SIZE(hms1_devices));
}

subsys_initcall(device_init);
#endif
