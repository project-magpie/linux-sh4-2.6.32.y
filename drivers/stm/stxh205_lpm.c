/* ---------------------------------------------------------------------------
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.cm>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * ----------------------------------------------------------------------------
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/platform.h>

/*
 * Actual Lille Settings
 */
#define SBC_ADDRESS   0xFE400000
#define SBC_SIZE 0xB4400
#define MAILBOX_IRQ_NUM   (176+64)

static struct platform_device stm_lpm_device = {
	.name = "stlpm",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.start = 0xFE400000,
			.end   = 0xFE400000 + SBC_SIZE,
			.flags = IORESOURCE_MEM,
		},
		{	.start = (176 + 64),
			.end   = (176 + 64),
			.flags = IORESOURCE_IRQ
		}
	}
};

const char *lpm_get_cpu_type(void)
{
	return get_cpu_subtype(&current_cpu_data);
}

static int __init stxh205_lpm_init(void)
{
	return platform_device_register(&stm_lpm_device);
}

module_init(stxh205_lpm_init);
