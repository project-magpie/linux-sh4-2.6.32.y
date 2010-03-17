/*
 * arch/sh/boards/mach-iptv7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics IPTV PLUGGIN board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7105.h>
#include <linux/stm/nand.h>
#include <linux/stm/pci-synopsys.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>

#define IPTV7105_PIO_PHY_RESET stm_gpio(15, 5)
#define IPTV7105_PIO_PCI_IDSEL stm_gpio(10, 2)
#define IPTV7105_PIO_PCI_SERR  stm_gpio(15, 4)
#define IPTV7105_PIO_PCI_RESET stm_gpio(15, 7)
#define IPTV7105_PIO_FLASH_VPP stm_gpio(6, 5)
#define IPTV7105_PIO_SPI_BOOT_CLK   stm_gpio(15, 0)
#define IPTV7105_PIO_SPI_BOOT_DOUT  stm_gpio(15, 1)
#define IPTV7105_PIO_SPI_BOOT_NOTCS stm_gpio(15, 2)
#define IPTV7105_PIO_SPI_BOOT_DIN   stm_gpio(15, 3)

/*
 * Comment out this line to use NAND through the EMI bit-banging driver
 * instead of the Flex driver.
 */
#define NAND_USES_FLEX

static void __init iptv7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7105 IPTV board initialisation\n");

	stx7105_early_device_init();

	/* CN5, which is usually connected to CN6 on the debug board and
	 * eventually becomes female DB9 socket CN8 there... */
	stx7105_configure_asc(0, &(struct stx7105_asc_config) {
			.routing.asc2 = stx7105_asc2_pio4,
			.hw_flow_control = 1,
			.is_console = 1, });

	/* CN17, which is usually connected to CN18 on the debug board and
	 * eventually becomes female DB9 socket CN7 there... */
	stx7105_configure_asc(3, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });

	/* CN4 */
	stx7105_configure_asc(2, &(struct stx7105_asc_config) {
			.hw_flow_control = 1,
			.is_console = 0, });
}

static int iptv7105_phy_reset(void *bus)
{
	gpio_set_value(IPTV7105_PIO_PHY_RESET, 0);
	udelay(100);
	gpio_set_value(IPTV7105_PIO_PHY_RESET, 1);
	udelay(1);

	return 1;
}

static struct plat_stmmacphy_data iptv7105_phy_private_data = {
	/* Micrel KSZ8041FTL */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &iptv7105_phy_reset,
};

static struct platform_device iptv7105_phy_device = {
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
		.platform_data = &iptv7105_phy_private_data,
	}
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

#if 0
static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p32",
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias	= "m25p80",
		.bus_num	= 0,
		.chip_select	= stm_gpio(2, 4),
		.max_speed_hz	= 5000000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};
#endif

/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
	{
		.name   = "Boot firmware",
		.offset = 0,
		.size   = 0x00100000
	}, {
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

struct stm_nand_bank_data nand_bank_data = {
	.csn		= 1,
	.nr_partitions	= ARRAY_SIZE(nand_parts),
	.partitions	= nand_parts,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.timing_data		= &(struct stm_nand_timing_data) {
		.sig_setup      = 50,	   /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay     = 50,	   /* in us */
	},
};

#ifndef NAND_USES_FLEX
static struct platform_device nand_device = {
	.name		= "stm-nand-emi",
	.dev.platform_data = &(struct stm_plat_nand_emi_data){
		.nr_banks	= 1,
		.banks		= &nand_bank_data,
		.emi_rbn_gpio	= -1,
	},
};
#endif

static struct platform_device *iptv7105_devices[] __initdata = {
	&iptv7105_phy_device,
#ifndef NAND_USES_FLEX
	&nand_device,
#endif
};

/* PCI configuration */

static struct stm_plat_pci_config iptv7105_pci_config = {
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
	.pci_reset_gpio = IPTV7105_PIO_PCI_RESET,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx7105_pcibios_map_platform_irq(&iptv7105_pci_config, pin);
}

static int __init iptv7105_devices_init(void)
{
	/* The IDSEL line is connected to PIO10.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(IPTV7105_PIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(IPTV7105_PIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "iptv7105: Failed to claim PCI_IDSEL PIO!\n");
	/* Setup the PCI_SERR# PIO */
	if (gpio_request(IPTV7105_PIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(IPTV7105_PIO_PCI_SERR);
		iptv7105_pci_config.serr_irq =
				gpio_to_irq(IPTV7105_PIO_PCI_SERR);
		set_irq_type(iptv7105_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_ERR "iptv7105: Failed to claim PCI SERR PIO!\n");
	}
	/* And finally! */
	stx7105_configure_pci(&iptv7105_pci_config);

	stx7105_configure_sata(0);

	/* Configure the SPI Boot as input */
	if (gpio_request(IPTV7105_PIO_SPI_BOOT_CLK, "SPI Boot CLK") == 0)
		gpio_direction_input(IPTV7105_PIO_SPI_BOOT_CLK);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot CLK!\n");

	if (gpio_request(IPTV7105_PIO_SPI_BOOT_DOUT, "SPI Boot DOUT") == 0)
		gpio_direction_input(IPTV7105_PIO_SPI_BOOT_DOUT);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot DOUT!\n");

	if (gpio_request(IPTV7105_PIO_SPI_BOOT_NOTCS, "SPI Boot NOTCS") == 0)
		gpio_direction_input(IPTV7105_PIO_SPI_BOOT_NOTCS);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot NOTCS!\n");

	if (gpio_request(IPTV7105_PIO_SPI_BOOT_DIN, "SPI Boot DIN") == 0)
		gpio_direction_input(IPTV7105_PIO_SPI_BOOT_DIN);
	else
		printk(KERN_ERR "iptv7105: Failed to claim SPI Boot DIN!\n");
	stx7105_configure_ssc_spi(1, &(struct stx7105_ssc_config) {
			.routing.ssc1.sclk = stx7105_ssc1_sclk_pio2_5,
			.routing.ssc1.mtsr = stx7105_ssc1_mtsr_pio2_6,
			.routing.ssc1.mrst = stx7105_ssc1_mrst_pio2_7, });

	stx7105_configure_ssc_i2c(2, &(struct stx7105_ssc_config) {
			.routing.ssc2.sclk = stx7105_ssc2_sclk_pio3_4,
			.routing.ssc2.mtsr = stx7105_ssc2_mtsr_pio3_5, });
	stx7105_configure_ssc_i2c(3, &(struct stx7105_ssc_config) {
			.routing.ssc3.sclk = stx7105_ssc3_sclk_pio3_6,
			.routing.ssc3.mtsr = stx7105_ssc3_mtsr_pio3_7, });

	stx7105_configure_usb(0, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_low,
			.pwr_enabled = 1,
			.routing.usb0.ovrcur = stx7105_usb0_ovrcur_pio4_4,
			.routing.usb0.pwr = stx7105_usb0_pwr_pio4_5, });
	stx7105_configure_usb(1, &(struct stx7105_usb_config) {
			.ovrcur_mode = stx7105_usb_ovrcur_active_low,
			.pwr_enabled = 1,
			.routing.usb1.ovrcur = stx7105_usb1_ovrcur_pio4_6,
			.routing.usb1.pwr = stx7105_usb1_pwr_pio4_7, });

	gpio_request(IPTV7105_PIO_PHY_RESET, "eth_phy_reset");
	gpio_direction_output(IPTV7105_PIO_PHY_RESET, 1);

	stx7105_configure_ethernet(0, &(struct stx7105_ethernet_config) {
			.mode = stx7105_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0, });

	stx7105_configure_lirc(&(struct stx7105_lirc_config) {
			.rx_mode = stx7105_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 1, });

	stx7105_configure_audio(&(struct stx7105_audio_config) {
			.pcm_player_0_output =
					stx7105_pcm_player_0_output_2_channels,
			});

	/* just permanetly enable the flash*/
	gpio_request(IPTV7105_PIO_FLASH_VPP, "eth_phy_reset");
	gpio_direction_output(IPTV7105_PIO_FLASH_VPP, 1);
#ifdef NAND_USES_FLEX
	stx7105_configure_nand_flex(1, &nand_bank_data, 1);
#endif

#if 0
	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));
#endif

	return platform_add_devices(iptv7105_devices,
				    ARRAY_SIZE(iptv7105_devices));
}
arch_initcall(iptv7105_devices_init);

static void __iomem *iptv7105_ioport_map(unsigned long port,
					   unsigned int size)
{
	/*
	 * If we have PCI then this should never be called because we
	 * are using the generic iomap implementation. If we don't
	 * have PCI then there are no IO mapped devices, so it still
	 * shouldn't be called.
	 */
	BUG();
	return (void __iomem *)CCN_PVR;
}

struct sh_machine_vector mv_iptv7105 __initmv = {
	.mv_name	= "iptv7105",
	.mv_setup	= iptv7105_setup,
	.mv_nr_irqs	= NR_IRQS,
	.mv_ioport_map	= iptv7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
