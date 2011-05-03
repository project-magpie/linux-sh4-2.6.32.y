/*
 * arch/sh/boards/mach-fldb/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics Freeman Lite Development Board support.
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



#define FLDB_PIO_RESET_OUTN stm_gpio(11, 6)
#define FLDB_PIO_PCI_IDSEL stm_gpio(16, 2)
#define FLDB_PIO_PCI_RESET stm_gpio(16, 5)



static void __init fldb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman Lite Development Board "
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



static struct platform_device fldb_led_df1 = {
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



static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_mask = 0,
};

static struct platform_device *fldb_devices[] __initdata = {
	&fldb_led_df1,
};



static struct spi_board_info fldb_serial_flash =  {
	.modalias = "m25p80",
	.bus_num = 0,
	.chip_select = stm_gpio(17, 4),
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



static struct stm_plat_pci_config fldb_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = FLDB_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return fli7510_pcibios_map_platform_irq(&fldb_pci_config, pin);
}



static int __init fldb_device_init(void)
{
	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FLDB_PIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FLDB_PIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FLDB_PIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fldb: Failed to claim RESET_OUTN PIO!\n");
	}

	/* The IDSEL line is connected to PIO16.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(FLDB_PIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(FLDB_PIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "fldb: Failed to claim PCI_IDSEL PIO!\n");

	/* And finally! */
	fli7510_configure_pci(&fldb_pci_config);

	fli7510_configure_pwm(&(struct fli7510_pwm_config) {
			.out0_enabled = 1,
#if 0
			/* Connected to DF1 LED, currently used as a
			 * GPIO-controlled one (see above) */
			.out1_enabled = 1,
#endif
#if 0
			/* PWM driver doesn't support these yet... */
			.out2_enabled = 1,
			.out3_enabled = 1,
#endif
			});

	/* CNB2 ("I2C1" connector), CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(0);
	/* CNB3 ("I2C2" connector), UB2 (EEPROM), UH4 (STM8 uC),
	 * CNJ2 ("FE Board" connector) */
	fli7510_configure_ssc_i2c(1);
	/* CNB5 ("I2C3" connector), CNF3 ("LVDS Out C and D" connector),
	 * CNL1 ("Extension Board" connector) */
	fli7510_configure_ssc_i2c(2);
	/* CNK4 ("VGA In" connector), UK1 (EEPROM) */
	fli7510_configure_ssc_i2c(3);
	/* UD13 (SPI Flash) */
	fli7510_configure_ssc_spi(4, NULL);

	spi_register_board_info(&fldb_serial_flash, 1);

	fli7510_configure_usb(0, &(struct fli7510_usb_config) {
			.ovrcur_mode = fli7510_usb_ovrcur_active_low, });

	fli7510_configure_ethernet(&(struct fli7510_ethernet_config) {
			.mode = fli7510_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = 1,
			.mdio_bus_data = &stmmac_mdio_bus,
		});

	fli7510_configure_lirc();

	/*
	 * To use the MMC/SD card with the external
	 * CNG6 connector, the CNG4 has to be connected to CNG3.
	 */
	fli7510_configure_mmc();

	return platform_add_devices(fldb_devices,
			ARRAY_SIZE(fldb_devices));
}
arch_initcall(fldb_device_init);



static void __iomem *fldb_ioport_map(unsigned long port, unsigned int size)
{
	/* If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called. */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_fldb __initmv = {
	.mv_name	= "fldb",
	.mv_setup	= fldb_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= fldb_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
