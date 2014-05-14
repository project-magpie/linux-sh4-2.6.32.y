/*
 * arch/sh/boards/mach-sat7111/setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: Jon Frosdick (jon.frosdick@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics sat7111 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/stm/platform.h>
#include <linux/stm/stx7111.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <sound/stm.h>
#include <linux/bpa2.h>

#define SAT7111_PHY_RESET stm_gpio(5, 3)

const char *LMI_IO_partalias[] = { "v4l2-coded-video-buffers", "BPA2_Region1", "v4l2-video-buffers" ,
                                    "coredisplay-video", "gfx-memory", "BPA2_Region0", "LMI_VID", NULL };

/*
0x40000000 - 0x403FFFFF - cocpu 1 ram (4mb)
0x40400000 - 0x407FFFFF - cocpu 2 ram (4mb)
0x40800000 - 0x49FFFFFF - linux   (159mb)
0x4A000000 - 0x4B1FFFFF - bigphys ( 18mb)
0x4B200000 - 0x4FFFFFFF - lmi_io  ( 78mb)
*/
static struct bpa2_partition_desc bpa2_parts_table[] = {
    {
 	    .name  = "bigphysarea",
 	    .start = 0x4A000000,
 	    .size  = 0x01200000, /* 18 Mb */
 	    .flags = 0,
 	    .aka   = NULL
    },
    {
 	    .name  = "LMI_IO",
 	    .start = 0x4B200000,
 	    .size  = 0x04E00000, /* 78 Mb */
 	    .flags = 0,
 	    .aka   = LMI_IO_partalias
    },
 };

static void __init sat7111_setup(char** cmdline_p)
{
	printk(KERN_INFO "Spark-7111 board initialization\n");

	stx7111_early_device_init();

	stx7111_configure_asc(2, &(struct stx7111_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });

  	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct platform_device sat7111_nor_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0, 8*1024*1024),
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width		= 2,
		.nr_parts	= 3,
		.parts		=  (struct mtd_partition []) {
			{
				.name   = "Boot firmware", 	//  (512K)
				.size   = 0x00080000,
				.offset = 0x00000000,
			},
			{
				.name   = "Kernel",		//  (7MB)
				.size   = 0x00700000,
				.offset = 0x00080000,
			},
			{
				.name   = "Reserve",		//  (512K)
				.size   = MTDPART_SIZ_FULL,
				.offset = 0x00780000,
			},
		},
	},
};

struct stm_nand_bank_data sat7111_nand_data = {
	.csn		= 1,
	.options	= NAND_NO_AUTOINCR | NAND_USE_FLASH_BBT,
	.nr_partitions	= 4,
	.partitions	= (struct mtd_partition []) {
	     {
		 .name = "Spark Kernel",
		 .size = 0x0800000,
		 .offset = 0
	     }, {
		 .name = "Spark Rootfs",
		 .size = 0x17800000,
		 .offset = 0x800000
	     }, {
		 .name = "E2 Kernel",
		 .size = 0x800000,
		 .offset = 0x18000000
	     }, {
		 .name = "E2 RootFs",
		 .size = 0x5000000,
		 .offset = 0x18800000
	     },
	},
	.timing_data	= &(struct stm_nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
};

static struct platform_device sat7111_nand_flash = {
	.name		= "stm-nand-emi",
	.dev.platform_data = &(struct stm_plat_nand_emi_data){
		.nr_banks	= 1,
		.banks		= &sat7111_nand_data,
		.emi_rbn_gpio	= -1,
	},
};

static int sat7111_phy_reset(void *bus)
{
	gpio_set_value(SAT7111_PHY_RESET, 1);
	udelay(1);
	gpio_set_value(SAT7111_PHY_RESET, 0);
	udelay(1);
	gpio_set_value(SAT7111_PHY_RESET, 1);
	return 1;
};

static struct stmmac_mdio_bus_data stmmac_mdio_bus = {
	.bus_id = 0,
	.phy_reset = &sat7111_phy_reset,
	.phy_mask = 0,
};

static struct platform_device *sat7111_devices[] __initdata = {
	&sat7111_nor_flash,
	&sat7111_nand_flash,
};

static int __init sat7111_devices_init(void)
{
	stx7111_configure_ssc_i2c(0, NULL);
	stx7111_configure_ssc_i2c(1, NULL);
	stx7111_configure_ssc_i2c(2, NULL);
	stx7111_configure_ssc_i2c(3, NULL);

	stx7111_configure_usb(&(struct stx7111_usb_config) {
			.invert_ovrcur = 1, });

	stx7111_configure_ethernet(&(struct stx7111_ethernet_config) {
			.mode = stx7111_ethernet_mode_mii,
			.ext_clk = 0,
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac_mdio_bus,});

	stx7111_configure_lirc(&(struct stx7111_lirc_config) {
			.rx_mode = stx7111_lirc_rx_mode_ir,
			.tx_enabled = 1,
			.tx_od_enabled = 0, });

	gpio_request(SAT7111_PHY_RESET, "PHY");

	gpio_direction_output(SAT7111_PHY_RESET, 0);

	stx7111_configure_nand(&(struct stm_nand_config) {
			.driver = stm_nand_flex,
			.nr_banks = 1,
			.banks = &sat7111_nand_flash,
			.rbn.flex_connected = 1,});

	return platform_add_devices(sat7111_devices,
				    ARRAY_SIZE(sat7111_devices));
}
arch_initcall(sat7111_devices_init);

static void __iomem *sat7111_ioport_map(unsigned long port, unsigned int size)
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

static void __init sat7111_init_irq(void)
{

}

struct sh_machine_vector mv_sat7111 __initmv = {
	.mv_name		= "sat7111",
	.mv_setup		= sat7111_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= sat7111_init_irq,
	.mv_ioport_map		= sat7111_ioport_map,
};
