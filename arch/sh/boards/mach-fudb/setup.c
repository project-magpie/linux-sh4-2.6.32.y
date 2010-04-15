/*
 * arch/sh/boards/mach-fudb/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics Freeman Ultra Development Board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/platform.h>
#include <linux/stm/fli7510.h>
#include <linux/stm/pci-synopsys.h>
#include <asm/irq-ilc.h>



#define FUDB_PIO_RESET_OUTN stm_gpio(11, 5)
#define FUDB_PIO_SPI_WPN stm_gpio(18, 2)



static void __init fudb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman Ultra Development Board "
			"initialisation\n");

	fli7510_early_device_init();

	/* CNB1 ("UART 2" connector) */
	fli7510_configure_asc(1, &(struct fli7510_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });

	/* CNB4 ("UART 3" connector) */
	fli7510_configure_asc(2, &(struct fli7510_asc_config) {
			.hw_flow_control = 0,
			.is_console = 0, });
}



static struct platform_device fudb_led_df1 = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "DF1 orange",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(8, 5),
			},
		},
	},
};



static struct platform_device fudb_phy_device = {
	.name = "stmmacphy",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.name = "phyirq",
			.start = -1, /* FIXME */
			.end = -1,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev.platform_data = &(struct plat_stmmacphy_data) {
		.bus_id = 0,
		.phy_addr = 1,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_MII,
	},
};



static struct platform_device *fudb_devices[] __initdata = {
	&fudb_led_df1,
	&fudb_phy_device,
};



static struct spi_board_info fudb_serial_flash =  {
	.modalias = "m25p80",
	.bus_num = 0,
	.chip_select = stm_gpio(20, 2),
	.max_speed_hz = 7000000,
	.mode = SPI_MODE_3,
	.platform_data = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25px64",
		.nr_parts = 2,
		.parts = (struct mtd_partition []) {
			{
				.name = "SerialFlash_1",
				.size = 0x00080000,
				.offset = 0,
			}, {
				.name = "SerialFlash_2",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			},
		},
	},
};



static int __init fudb_device_init(void)
{
	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FUDB_PIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FUDB_PIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FUDB_PIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fudb: Failed to claim RESET_OUTN PIO!\n");
	}

	fli7510_configure_pwm(&(struct fli7510_pwm_config) {
#if 0
			/* PWM driver doesn't support these yet... */
			.out2_enabled = 1,
			.out3_enabled = 1,
#endif
			});

	/* CNB2 ("I2C1" connector), CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(0);
	/* CNB3 ("I2C2" connector), UB4 (EEPROM), UH4 (STM8 uC) */
	fli7510_configure_ssc_i2c(1);
	/* CNB5 ("I2C3" connector), CND1 ("Mini PCI Express" connector),
	 * CNF3 ("LVDS Out" connector), CNL1 ("Extension Board" connector) */
	fli7510_configure_ssc_i2c(2);
	/* CNK4 ("VGA In" connector), UK1 (EEPROM) */
	fli7510_configure_ssc_i2c(3);
	/* UD3 (SPI Flash) */
	fli7510_configure_ssc_spi(4, NULL);

	spi_register_board_info(&fudb_serial_flash, 1);

	fli7510_configure_usb(0, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });
	fli7510_configure_usb(1, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });

	fli7510_configure_ethernet(&(struct fli7510_ethernet_config) {
			.mode = fli7510_ethernet_mode_rmii,
			.ext_clk = 0,
			.phy_bus = 0, });

	fli7510_configure_lirc();

	return platform_add_devices(fudb_devices,
			ARRAY_SIZE(fudb_devices));
}
arch_initcall(fudb_device_init);



static void __iomem *fudb_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_fudb __initmv = {
	.mv_name	= "fudb",
	.mv_setup	= fudb_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= fudb_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
