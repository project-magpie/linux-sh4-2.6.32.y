/*
 * arch/sh/boards/st/mb680/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7105 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include <mach/common.h>

static int ascs[2] __initdata = { 2, 3 };

static void __init mb680_setup(char** cmdline_p)
{
	printk("STMicroelectronics STx7105 Mboard initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
	.routing	= PWM_OUT0_PIO13_0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY),
	.routing =
		SSC3_SCLK_PIO3_6 | SSC3_MTSR_PIO3_7 | SSC3_MRST_PIO3_7,
};

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 1,
		.oc_actlow = 0,
		.oc_pinsel = USB0_OC_PIO4_4,
		.pwr_en = 1,
		.pwr_pinsel = USB0_PWR_PIO4_5,
	}, {
		.oc_en = 1,
		.oc_actlow = 0,
		.oc_pinsel = USB1_OC_PIO4_6,
		.pwr_en = 1,
		.pwr_pinsel = USB1_PWR_PIO4_7,
	}
};

static struct platform_device mb680_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "LD5",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(2, 4),
			},
			{
				.name = "LD6",
				.gpio = stpio_to_gpio(2, 3),
			},
		},
	},
};

/*
 * mb680 rev C added software control of the PHY reset, and buffers which
 * allow isolation of the MII pins so that their use as MODE pins is not
 * compromised by the PHY.
 */
static struct stpio_pin *phy_reset, *switch_en;

static int mb680_phy_reset(void* bus)
{
	stpio_set_pin(phy_reset, 1);
	stpio_set_pin(switch_en, 1);
	udelay(1);
	stpio_set_pin(phy_reset, 0);
	udelay(100);
	stpio_set_pin(phy_reset, 1);
	udelay(1);
	stpio_set_pin(switch_en, 0);

	return 0;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* National Semiconductor DP83865 (rev A/B) or SMSC 8700 (rev C) */
	.bus_id = 0,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &mb680_phy_reset,
};

static struct platform_device mb680_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME, should be ILC_EXT_IRQ(6), */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct platform_device *mb680_devices[] __initdata = {
	&mb680_leds,
	&mb680_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.codelen = 0x1e,
	.alt_codelen = 0,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};



/* PCI configuration */

static struct pci_config_data  pci_config = {
	.pci_irq = {PCI_PIN_DEFAULT, PCI_PIN_DEFAULT, PCI_PIN_DEFAULT, PCI_PIN_DEFAULT},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {PCI_PIN_DEFAULT, PCI_PIN_UNUSED, PCI_PIN_UNUSED, PCI_PIN_UNUSED},
	.pci_clk = 33333333
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return  stx7105_pcibios_map_platform_irq(&pci_config, pin);
}

static int __init device_init(void)
{
	stx7105_configure_pci(&pci_config);
	stx7200_configure_sata(0);
	stx7200_configure_sata(1);
	stx7105_configure_pwm(&pwm_private_info);
	stx7105_configure_ssc(&ssc_private_info);

	/*
	 * Note that USB port configuration depends on jumper
	 * settings:
	 *		  PORT 0  SW		PORT 1	SW
	 *		+----------------------------------------
	 * OC	normal	|  4[4]	J5A 2-3		 4[6]	J10A 2-3
	 *	alt	| 12[5]	J5A 1-2		14[6]	J10A 1-2
	 * PWR	normal	|  4[5]	J5B 2-3		 4[7]	J10B 2-3
	 *	alt	| 12[6]	J5B 1-2		14[7]	J10B 1-2
	 */

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_reset = stpio_request_pin(5, 5, "ResetMII", STPIO_OUT);
	switch_en = stpio_request_pin(11, 2, "MIIBusSwitch", STPIO_OUT);
	stx7105_configure_ethernet(0, 0, 0, 0, 1, 0);

	stx7105_configure_lirc(&lirc_scd);

	return platform_add_devices(mb680_devices, ARRAY_SIZE(mb680_devices));
}
arch_initcall(device_init);


#ifndef CONFIG_GENERIC_IOMAP
static void __iomem *mb680_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}
#endif

static void __init mb680_init_irq(void)
{
#ifndef CONFIG_SH_ST_MB705
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
#endif
}

struct sh_machine_vector mv_mb680 __initmv = {
	STM_MACHINE_VEC(mb680)
};
