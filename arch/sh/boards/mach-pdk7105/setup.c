/*
 * arch/sh/boards/mach-pdk7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics PDK7105-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>

/*
 * Flash setup depends on whether system is configured as boot-from-NOR
 * (default) or boot-from-NAND.
 *
 * Jumper settings (board v1.2-011):
 *
 * boot-from-      |   NOR                     NAND
 * ---------------------------------------------------------------
 * JE2 (CS routing) |  0 (EMIA->NOR_CS)        1 (EMIA->NAND_CS)
 *                  |    (EMIB->NOR_CS)          (EMIB->NOR_CS)
 *                  |    (EMIC->NAND_CS)         (EMIC->NOR_CS)
 * JE3 (data width) |  0 (16bit)               1 (8bit)
 * JE5 (mode 15)    |  0 (boot NOR)            1 (boot NAND)
 * ---------------------------------------------------------------
 *
 */

static int ascs[2] __initdata = { 2, 3 };

static void __init pdk7105_setup(char** cmdline_p)
{
	printk("STMicroelectronics PDK7105-SDK board initialisation\n");

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

static struct platform_device pdk7105_leds = {
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

static struct stpio_pin *phy_reset_pin;

static int pdk7105_phy_reset(void* bus)
{
	stpio_set_pin(phy_reset_pin, 0);
	udelay(100);
	stpio_set_pin(phy_reset_pin, 1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &pdk7105_phy_reset,
};

static struct platform_device pdk7105_phy_device = {
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

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data pdk7105_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device pdk7105_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 128*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &pdk7105_physmap_flash_data,
	},
};

/* Configuration for Serial Flash */
static struct mtd_partition serialflash_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p64",
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num        = 8,
		.chip_select    = spi_set_cs(15, 2),
		.max_speed_hz   = 500000,
		.platform_data  = &serialflash_data,
		.mode           = SPI_MODE_3,
	},
};

static struct platform_device spi_pio_device[] = {
	{
		.name           = "spi_st_pio",
		.id             = 8,
		.num_resources  = 0,
		.dev            = {
			.platform_data =
				&(struct ssc_pio_t) {
					.pio = {{15, 0}, {15, 1}, {15, 3} },
				},
		},
	},
};
/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
	{
		.name   = "NAND root",
		.offset = 0,
		.size   = 0x00800000
	}, {
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data nand_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset   = 0,
	.rbn_port               = -1,
	.rbn_pin                = -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup      = 50,           /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 40,
		.rd_on          = 10,
		.rd_off         = 40,
		.chip_delay     = 50,           /* in us */
	},
	.flex_rbn_connected     = 0,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device nand_device =
STM_NAND_DEVICE("stm-nand-flex", 2, &nand_config,
		nand_parts, ARRAY_SIZE(nand_parts), NAND_USE_FLASH_BBT);



static struct platform_device *pdk7105_devices[] __initdata = {
	&pdk7105_physmap_flash,
	&pdk7105_leds,
	&pdk7105_phy_device,
	&spi_pio_device[0],
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
	.pci_irq = { PCI_PIN_DEFAULT, PCI_PIN_DEFAULT,
			PCI_PIN_UNUSED, PCI_PIN_UNUSED },
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = { PCI_PIN_DEFAULT, PCI_PIN_UNUSED,
			PCI_PIN_UNUSED, PCI_PIN_UNUSED },
	.pci_clk = 33333333,
	.pci_reset_pio = stpio_to_gpio(15, 7)
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return  stx7105_pcibios_map_platform_irq(&pci_config, pin);
}

static int __init device_init(void)
{
	u32 bank1_start;
	u32 bank2_start;
	struct sysconf_field *sc;
	u32 boot_mode;

	bank1_start = emi_bank_base(1);
	bank2_start = emi_bank_base(2);

	/* Configure FLASH according to boot device mode pins */
	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_mode");
	boot_mode = sysconf_read(sc);
	if (boot_mode == 0x0)
		/* Default configuration */
		pr_info("Configuring FLASH for boot-from-NOR\n");
	else if (boot_mode == 0x1) {
		/* Swap NOR/NAND banks */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		pdk7105_physmap_flash.resource[0].start = bank1_start;
		pdk7105_physmap_flash.resource[0].end = bank2_start - 1;
		nand_device.id = 0;
	}

	stx7105_configure_pci(&pci_config);
	stx7105_configure_sata();
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

	phy_reset_pin = stpio_request_set_pin(15, 5, "eth_phy_reset",
					      STPIO_OUT, 1);
	stx7105_configure_ethernet(0, 0, 0, 0, 0, 0);
	stx7105_configure_lirc(&lirc_scd);
	stx7105_configure_audio_pins(3, 1, 1);

	/*
	 * FLASH_WP is shared by NOR and NAND.  However, since MTD NAND has no
	 * concept of WP/VPP, we must permanently enable it
	 */
	stpio_request_set_pin(6, 4, "FLASH_WP", STPIO_OUT, 1);

	stx7105_configure_nand(&nand_device);
	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(pdk7105_devices, ARRAY_SIZE(pdk7105_devices));
}
arch_initcall(device_init);

static void __iomem *pdk7105_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init pdk7105_init_irq(void)
{
#ifndef CONFIG_SH_ST_MB705
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
#endif
}

struct sh_machine_vector mv_pdk7105 __initmv = {
	.mv_name		= "pdk7105",
	.mv_setup		= pdk7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= pdk7105_init_irq,
	.mv_ioport_map		= pdk7105_ioport_map,
};

