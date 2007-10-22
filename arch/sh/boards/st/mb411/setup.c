/*
 * arch/sh/boards/st/mb411/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 MBoard board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <asm/io.h>
#include <asm/mb411/harp.h>

static int ascs[2] __initdata = { 2, 3 };

void __init mb411_setup(char** cmdline_p)
{
	unsigned char epldver;
	unsigned char pod_devid;

	printk("STMicroelectronics STb7100 MBoard board initialisation\n");

	epldver = ctrl_inb(EPLD_EPLDVER),
	printk("EPLD v%dr%d, PCB ver %X\n",
	       epldver >> 4, epldver & 0xf,
	       ctrl_inb(EPLD_PCBVER));

	pod_devid = ctrl_inb(EPLD_POD_DEVID);
	printk("POD EPLD version: %d, DevID: MB411(%d) Rev.%c\n",
	       ctrl_inb(EPLD_POD_REVID),
	       pod_devid >> 4, 'A'-1+(pod_devid & 0xf));

        stx7100_early_device_init();
        stb7100_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0 | PLAT_STM_PWM_OUT1,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		(SSC_I2C_CAPABILITY << (0*2)) |
		((SSC_SPI_CAPABILITY | SSC_I2C_CAPABILITY) << (1*2)) |
		(SSC_I2C_CAPABILITY << (2*2)),
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0xa3e00300,
		.end	= 0xa3e00300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 7,
		.end	= 7,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

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

static void stb7100_mtd_set_vpp(struct map_info *map, int vpp)
{
	if (vpp) {
		harp_set_vpp_on();
	} else {
		harp_set_vpp_off();
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= stb7100_mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct resource physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
	.num_resources	= 1,
	.resource	= &physmap_flash_resource,
};

static struct plat_stmmacphy_data phy_private_data = {
        .bus_id = 0,
        .phy_addr = 0,
        .phy_mask = 0,
        .interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device mb411_phy_device = {
        .name           = "stmmacphy",
        .id             = 0,
        .num_resources  = 1,
        .resource       = (struct resource[]) {
                {
                        .name   = "phyirq",
                        .start  = 0,
                        .end    = 0,
                        .flags  = IORESOURCE_IRQ,
                },
        },
        .dev = {
                .platform_data = &phy_private_data,
         }
};

static struct platform_device *mb411_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
	&mb411_phy_device,
};

static int __init device_init(void)
{
	stx7100_configure_sata();
	stx7100_configure_pwm(&pwm_private_info);
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_alsa();
	stx7100_configure_ethernet(0, 0, 0);

	return platform_add_devices(mb411_devices,
                                    ARRAY_SIZE(mb411_devices));
}

device_initcall(device_init);
