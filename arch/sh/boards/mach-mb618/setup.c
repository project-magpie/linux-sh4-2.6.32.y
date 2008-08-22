/*
 * arch/sh/boards/st/mb618/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7111 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include <mach/common.h>

/* Whether the hardware supports NOR or NAND Flash depends on J34.
 * In position 1-2 CSA selects NAND, in position 2-3 is selects NOR.
 * Note that J30A must be in position 2-3 to select the on board Flash
 * (both NOR and NAND).
 */
#define FLASH_NOR

static int ascs[2] __initdata = { 2, 3 };

static void __init mb618_setup(char** cmdline_p)
{
	printk("STMicroelectronics STx7111 Mboard initialisation\n");

	stx7111_early_device_init();
	stx7111_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_SPI_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY),
};

static struct platform_device mb618_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB green",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(6, 0),
			},
			{
				.name = "HB red",
				.gpio = stpio_to_gpio(6, 1),
			},
		},
	},
};

static struct gpio_keys_button mb618_buttons[] = {
	{
		.code = BTN_0,
		.gpio = stpio_to_gpio(6, 2),
		.desc = "SW2",
	},
	{
		.code = BTN_1,
		.gpio = stpio_to_gpio(6, 3),
		.desc = "SW3",
	},
	{
		.code = BTN_2,
		.gpio = stpio_to_gpio(6, 4),
		.desc = "SW4",
	},
	{
		.code = BTN_3,
		.gpio = stpio_to_gpio(6, 5),
		.desc = "SW5",
	},
};

static struct gpio_keys_platform_data mb618_button_data = {
	.buttons = mb618_buttons,
	.nbuttons = ARRAY_SIZE(mb618_buttons),
};

static struct platform_device mb618_button_device = {
	.name = "gpio-keys",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &mb618_button_data,
	}
};

/* J34 must be in the 2-3 position to enable NOR Flash */
static struct stpio_pin *vpp_pio;

static void set_vpp(struct map_info * info, int enable)
{
	stpio_set_pin(vpp_pio, enable);
}

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

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
		.platform_data	= &physmap_flash_data,
	},
};

static int mb618_phy_reset05(void *bus)
{
	/* Bring the PHY out of reset in MII mode */
	epld_write(0x4 | 0, 0);
	epld_write(0x4 | 1, 0);

	return 1;
}

static int mb618_phy_reset06(void *bus)
{
	epld_write(1, 0);	/* bank = Ctrl */

	/* Bring the PHY out of reset in MII mode */
	epld_write(0x4 | 0, 4);
	epld_write(0x4 | 1, 4);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* SMSC LAN 8700 */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device mb618_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME*/
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x06000000,
			/* Minimum size to ensure mapped by PMB */
			.end	= 0x06000000+(8*1024*1024)-1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};

/* J34 must be in the 1-2 position to enable NOR Flash */
static struct mtd_partition nand_partitions[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct nand_config_data mb618_nand_config = {
	.emi_bank		= 0,
	.emi_withinbankoffset	= 0,

	/* Timings for NAND512W3A */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	= 40,		 /* times in ns */
		.rd_oee_start	= 0,
		.rd_oee_end	= 10,
		.rd_latchpoint	= 10,
		.busreleasetime = 0,

		.wr_cycle_time	= 40,
		.wr_oee_start	= 0,
		.wr_oee_end	= 10,
	},

	.chip_delay		= 40,		/* time in us */
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
};

static struct platform_device *mb618_devices[] __initdata = {
	&mb618_leds,
	&epld_device,
#ifdef FLASH_NOR
	&physmap_flash,
#endif
	&mb618_phy_device,
	&mb618_button_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.codelen = 0x1e,
	.alt_codelen = 0,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

#ifdef CONFIG_SND
/* SCART switch simple control */

/* Enable CVBS output to both (TV & VCR) SCART outputs */
static int mb618_scart_audio_init(struct i2c_client *client, void *priv)
{
	const char cmd[] = { 0x2, 0x11 };
	int cmd_len = sizeof(cmd);

	return i2c_master_send(client, cmd, cmd_len) != cmd_len;
}

/* Audio on SCART outputs control */
static struct i2c_board_info mb618_scart_audio __initdata = {
	I2C_BOARD_INFO("snd_conv_i2c", 0x4b),
	.type = "STV6417",
	.platform_data = &(struct snd_stm_conv_i2c_info) {
		.group = "Analog Output",
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
		.init = mb618_scart_audio_init,
		.enable_supported = 1,
		.enable_cmd = (char []){ 0x01, 0x09 },
		.enable_cmd_len = 2,
		.disable_cmd = (char []){ 0x01, 0x00 },
		.disable_cmd_len = 2,
	},
};
#endif

static int __init device_init(void)
{
	stx7111_configure_pwm(&pwm_private_info);
	stx7111_configure_ssc(&ssc_private_info);
	stx7111_configure_usb(1); /* Enable inverter */
	stx7111_configure_ethernet(1, 0, 0, 0);
        stx7111_configure_lirc(&lirc_scd);

	vpp_pio = stpio_request_pin(3,4, "VPP", STPIO_OUT);

#ifdef CONFIG_SND
	i2c_register_board_info(1, &mb618_scart_audio, 1);
#endif

#ifndef FLASH_NOR
	stx7111_configure_nand(&mb618_nand_config);
	/* The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it.
	 */
	stpio_set_pin(vpp_pio, 1);
#endif

	return platform_add_devices(mb618_devices, ARRAY_SIZE(mb618_devices));
}
arch_initcall(device_init);

static void __iomem *mb618_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

/*
 * We have several EPLD versions to cope with, with slightly different memory
 * maps and features:
 *
 * version 04:
 * off  read        reset
 *  0   Status      undef  (unused)
 *  4   Ctrl        20     (unused)
 *  8   Test        33
 *  c   Ident       0      (should be 1 but broken)
 * (note writes are broken)
 *
 * version 05:
 * off  read     write       reset
 *  0   Ident    Ctrl        45
 *  4   Test     Test        55
 *  8   IntStat  IntMaskSet  -
 *  c   IntMask  IntMaskClr  0
 *
 * version 06:
 * off        read       write         reset
 *  0         Ident      Bank          46 (Bank register defaults to 0)
 *  4 bank=0  Test       Test          55
 *  4 bank=1  Ctrl       Ctrl          0e
 *  4 bank=2  IntPri0    IntPri0  f9
 *  4 bank=3  IntPri1    IntPri1  f0
 *  8         IntStat    IntMaskSet    -
 *  c         IntMask    IntMaskClr    00
 *
 * Ctrl register bits:
 *  0 = Ethernet Phy notReset
 *  1 = RMIInotMIISelect
 *  2 = Mode Select_7111 (ModeSelect when D0 == 1)
 *  3 = Mode Select_8700 (ModeSelect when D0 == 0)
 */

static void __init mb618_init_irq(void)
{
	unsigned char epld_reg;
	int test_offset = -1;
	int version_offset = -1;
	int version = -1;

	epld_early_init(&epld_device);

	epld_reg = epld_read(0x4);
	switch (epld_reg) {
	case 0x20:
		/*
		 * Probably the Ctrl reg of a 04 EPLD. Look for the default
		 * value in the test reg (we can't do a test as it is broken).
		 */
		epld_reg = epld_read(0x8);
		if (epld_reg == 0x33)
			version = 4;
		break;
	case 0x55:
		/* Probably the Test reg of the 05 or later EPLD */
		test_offset = 4;
		version_offset = 0;
		break;
	}

	if (test_offset > 0) {
		epld_write(0x63, test_offset);
		epld_reg = epld_read(test_offset);
		if (epld_reg != (unsigned char)(~0x63)) {
			printk(KERN_WARNING
			       "Failed mb618 EPLD test (off %02x, res %02x)\n",
			       test_offset, epld_reg);
			return;
		}

		/* Assume we can trust the version register */
		version = epld_read(version_offset) & 0xf;
	}

	if (version < 0) {
		printk(KERN_WARNING "Unable to determine mb618 EPLD version\n");
		return;
	}

	printk(KERN_INFO "mb618 EPLD version %02d\n", version);

	switch (version) {
	case 0 ... 4:
		/* EPLD is unusable */
		break;
	case 5:
		/* We need to control the PHY reset in software */
		phy_private_data.phy_reset = &mb618_phy_reset05;

		/* Quick hack to test STEM Ethernet card */

		/* The version 05 EPLD contains the harp style encoded
		 * interrupt controller, which as usual gives us problems.
		 * Enable the STEM INTR0 signal, and ignore the others.
		 * This results in SYSITRQ[3..0] = 0100 when active,
		 * SYSITRQ[3..0] = 0100 when inactive, so we can treat
		 * SYSITRQ[2] as an active high interrupt pin.
		 */
		epld_write(1<<4, 8);

		break;
	default:
		/* Assume v6 and above EPLDs are compatible */

		/* We need to control the PHY reset in software */
		phy_private_data.phy_reset = &mb618_phy_reset06;

		/*
		 * We have the nice new shiny interrupt system at last.
		 * For the moment just replicate the functionality to
		 * route the STEM interrupt through.
		 */

		/* Route STEM Int0 (EPLD int 4) to output 2 */
		epld_write(3, 0);	/* bank = IntPri1 */
		epld_reg = epld_read(4);
		epld_reg &= 0xfc;
		epld_reg |= 2;
		epld_write(epld_reg, 4);

		/* Enable it */
		epld_write(1<<4, 8);
	}
}

struct sh_machine_vector mv_mb618 __initmv = {
	.mv_name		= "mb618",
	.mv_setup		= mb618_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb618_init_irq,
	.mv_ioport_map		= mb618_ioport_map,
};
