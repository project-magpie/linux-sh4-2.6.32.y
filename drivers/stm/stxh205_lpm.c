/*
 * <root>/drivers/stm/stxh205_lpm.c
 *
 * This define resources for internal SBC
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */

#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <asm/irq-ilc.h>

/* SBC DMEM and IMEM area */
#define SBC_ADDRESS	0xFE400000
#define SBC_SIZE	0xA0000


/* mail box address */
#define SBC_MB_ADDRESS	0xFE4B4000
#define SBC_MB_SIZE	0x400

/* configuration register address */
#define SBC_CF_ADDRESS	0xFE4B5100
#define SBC_CF_SIZE	0x200


static struct platform_device stm_lpm_device = {
	.name = "stm-lpm",
	.id = 0,
	.num_resources = 4,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(SBC_ADDRESS, SBC_SIZE),
		STM_PLAT_RESOURCE_MEM(SBC_MB_ADDRESS, SBC_MB_SIZE),
		STM_PLAT_RESOURCE_MEM(SBC_CF_ADDRESS, SBC_CF_SIZE),
		STM_PLAT_RESOURCE_IRQ(ILC_IRQ(64), -1),
	}
};


static int __init stxh205_lpm_init(void)
{
	return platform_device_register(&stm_lpm_device);
}

module_init(stxh205_lpm_init);
