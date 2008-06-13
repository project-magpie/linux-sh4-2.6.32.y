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
#include <linux/mutex.h>
#include <asm/io.h>
#include "stb7100-common.h"

#define RESOURCE_NAME "USB wrapper"

static DEFINE_MUTEX(wraper_mutex);

/*
 * Set up the USB hardware wrapper
 */
int ST40_start_host_control(struct platform_device *pdev)
{
	struct plat_usb_data *usb_wrapper = pdev->dev.platform_data;
	unsigned long ahb2stbus_wrapper_glue_base =
		usb_wrapper->ahb2stbus_wrapper_glue_base;
	unsigned long ahb2stbus_protocol_base =
		usb_wrapper->ahb2stbus_protocol_base;
	unsigned long reg, req_reg;
	int retval;
	void *wrapper_base;
	void *protocol_base;

	mutex_lock(&wraper_mutex);

	if (usb_wrapper->initialised)
		goto success;

	retval = -EBUSY;

	if (!request_mem_region(ahb2stbus_wrapper_glue_base, 0x100,
				RESOURCE_NAME))
		goto err1;

	if (!request_mem_region(ahb2stbus_protocol_base, 0x100,
				RESOURCE_NAME))
		goto err2;

	retval = -ENOMEM;

	wrapper_base = ioremap(ahb2stbus_wrapper_glue_base, 0x100);
	if (!wrapper_base)
		goto err3;

	protocol_base = ioremap(ahb2stbus_protocol_base, 0x100);
	if (!protocol_base)
		goto err4;

	if (usb_wrapper->flags &
	    (USB_FLAGS_STRAP_8BIT | USB_FLAGS_STRAP_16BIT)) {
		/* Set strap mode */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		if (usb_wrapper->flags & USB_FLAGS_STRAP_16BIT)
			reg |= AHB2STBUS_STRAP_16_BIT;
		else
			reg &= ~AHB2STBUS_STRAP_16_BIT;
		writel(reg, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	if (usb_wrapper->flags & USB_FLAGS_STRAP_PLL) {
		/* Start PLL */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
		       wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
		writel(reg & (~AHB2STBUS_STRAP_PLL),
		       wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
	}

	if (usb_wrapper->flags & USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE) {
		/* Set the STBus Opcode Config for load/store 32 */
		writel(AHB2STBUS_STBUS_OPC_32BIT,
		       protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);

		/* Set the Message Size Config to n packets per message */
		writel(AHB2STBUS_MSGSIZE_4,
		       protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

		/* Set the chunksize to n packets */
		writel(AHB2STBUS_CHUNKSIZE_4,
		       protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	}

	if (usb_wrapper->flags &
	    (USB_FLAGS_STBUS_CONFIG_THRESHOLD128 |
	     USB_FLAGS_STBUS_CONFIG_THRESHOLD256)) {

		if (usb_wrapper->flags & USB_FLAGS_STBUS_CONFIG_THRESHOLD128)
			req_reg =
				(1<<21) |  /* Turn on read-ahead */
				(5<<16) |  /* Opcode is store/load 32 */
				(0<<15) |  /* Turn off write posting */
				(1<<14) |  /* Enable threshold */
				(3<<9)  |  /* 2**3 Packets in a chunk */
				(0<<4)  |  /* No messages */
				7;         /* Threshold is 128 */
		else
			req_reg =
				(1<<21) |  /* Turn on read-ahead */
				(5<<16) |  /* Opcode is store/load 32 */
				(0<<15) |  /* Turn off write posting */
				(1<<14) |  /* Enable threshold */
				(3<<9)  |  /* 2**3 Packets in a chunk */
				(0<<4)  |  /* No messages */
				(8<<0);    /* Threshold is 256 */

		do {
			writel(req_reg, protocol_base + AHB2STBUS_STBUS_CONFIG);
			reg = readl(protocol_base + AHB2STBUS_STBUS_CONFIG);
		} while ((reg & 0x7FFFFFFF) != req_reg);
	}

	usb_wrapper->initialised = 1;

success:
	mutex_unlock(&wraper_mutex);
	return 0;

err4:
	iounmap(wrapper_base);
err3:
	release_mem_region(ahb2stbus_protocol_base, 0x100);
err2:
	release_mem_region(ahb2stbus_wrapper_glue_base, 0x100);
err1:
	mutex_unlock(&wraper_mutex);
	return retval;
}
EXPORT_SYMBOL(ST40_start_host_control);

MODULE_DESCRIPTION ("STM USB Host Controller wrapper driver");
MODULE_AUTHOR ("Mark Glaisher <mark.glaisher@st.com>");
MODULE_LICENSE ("GPL");
