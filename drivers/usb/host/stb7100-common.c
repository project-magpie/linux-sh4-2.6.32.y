/*
 * STb7100 common OHCI/EHCI controller functions.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * This file is licenced under the GPL.
 */

#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include "stb7100-common.h"

#ifdef CONFIG_CPU_SUBTYPE_STX7200
#define STRAP_MODE	0 /* 8 bit */
#define MSGSIZE		AHB2STBUS_MSGSIZE_4
#define CHUNKSIZE	AHB2STBUS_CHUNKSIZE_4
#else
#define STRAP_MODE	AHB2STBUS_STRAP_16_BIT
#define MSGSIZE		AHB2STBUS_MSGSIZE_64
#define CHUNKSIZE	AHB2STBUS_CHUNKSIZE_64
#endif


/*
 * Set up the USB hardware wrapper
 */
void ST40_start_host_control(struct platform_device *pdev)
{
	struct plat_usb_data *usb_wrapper = pdev->dev.platform_data;
	unsigned long ahb2stbus_wrapper_glue_base =
		usb_wrapper->ahb2stbus_wrapper_glue_base;
	unsigned long ahb2stbus_protocol_base =
		usb_wrapper->ahb2stbus_protocol_base;
	unsigned long reg;

	if (xchg(&usb_wrapper->initialised, 1))
		return;

	/* Set strap mode */
	reg = readl(ahb2stbus_wrapper_glue_base + AHB2STBUS_STRAP_OFFSET);
	reg &= ~AHB2STBUS_STRAP_16_BIT;
	reg |= STRAP_MODE;
	writel(reg, ahb2stbus_wrapper_glue_base + AHB2STBUS_STRAP_OFFSET);

	/* Start PLL */
	reg = readl(ahb2stbus_wrapper_glue_base + AHB2STBUS_STRAP_OFFSET);
	writel(reg | AHB2STBUS_STRAP_PLL,
	       ahb2stbus_wrapper_glue_base + AHB2STBUS_STRAP_OFFSET);
	mdelay(100);
	writel(reg & (~AHB2STBUS_STRAP_PLL),
	       ahb2stbus_wrapper_glue_base + AHB2STBUS_STRAP_OFFSET);
	mdelay(100);

	/* Set the STBus Opcode Config for load/store 32 */
	writel(AHB2STBUS_STBUS_OPC_32BIT,
	       ahb2stbus_protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);

	/* Set the Message Size Config to n packets per message */
	writel(MSGSIZE,
	       ahb2stbus_protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

	writel(CHUNKSIZE,
	       ahb2stbus_protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);

	usb_wrapper->power_up(pdev);
}
