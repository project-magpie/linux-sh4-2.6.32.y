/*
 * STb7100 common OHCI/EHCI controller functions.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * This file is licenced under the GPL.
 */

#include <linux/device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include "stb7100-common.h"

/*
 * Set up the USB hardware wrapper
 */
void ST40_start_host_control(struct platform_device *dev)
{
	unsigned long reg;
	static int initialised = 0;

	printk("%s\n", __FUNCTION__);

	if (xchg(&initialised, 1))
		return;

	printk("%s proceeding\n", __FUNCTION__);

	/* Make sure PLL is on */
	reg = readl(SYS_CFG2);
	if (reg & SYS_CFG2_PLL_POWER_DOWN_BIT) {
		writel(reg & (~SYS_CFG2_PLL_POWER_DOWN_BIT), SYS_CFG2);
		mdelay(100);
	}

	/* Set 16 bit strap mode */
	reg = readl(AHB2STBUS_STRAP);
	writel(reg | AHB2STBUS_STRAP_16_BIT, AHB2STBUS_STRAP);

	/* Start PLL */
	reg = readl(AHB2STBUS_STRAP);
	writel(reg | AHB2STBUS_STRAP_PLL, AHB2STBUS_STRAP);
	mdelay(100);
	writel(reg & (~AHB2STBUS_STRAP_PLL), AHB2STBUS_STRAP);
	mdelay(100);

	/* Set the STBus Opcode Config for 32-bit access */
	writel(AHB2STBUS_STBUS_OPC_32BIT, AHB2STBUS_STBUS_OPC);

	/* Set the Message Size Config to 64 packets per message */
	writel(AHB2STBUS_MSGSIZE_64, AHB2STBUS_MSGSIZE);

	/* Set the Chunk Size Config to 64 packets per chunk */
	writel(AHB2STBUS_CHUNKSIZE_64, AHB2STBUS_CHUNKSIZE);
}
