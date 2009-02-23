/*
 * HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2009 STMicroelectronics Limited
 * Author: Francesco Virlinzi
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/pm.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include "../core/hcd.h"
#include "./hcd-stm.h"

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

static int st_usb_boot(struct platform_device *dev)
{
	struct plat_usb_data *usb_wrapper = dev->dev.platform_data;
	unsigned long reg, req_reg;
	void *wrapper_base = usb_wrapper->ahb2stbus_wrapper_glue_base;
	void *protocol_base = usb_wrapper->ahb2stbus_protocol_base;

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

		req_reg = (1<<21) |  /* Turn on read-ahead */
			  (5<<16) |  /* Opcode is store/load 32 */
			  (0<<15) |  /* Turn off write posting */
			  (1<<14) |  /* Enable threshold */
			  (3<<9)  |  /* 2**3 Packets in a chunk */
			  (0<<4)  ;  /* No messages */
		reg |= ((usb_wrapper->flags &
			USB_FLAGS_STBUS_CONFIG_THRESHOLD128) ? 7 /* 128 */ :
				(8<<0));/* 256 */
		do {
			writel(req_reg, protocol_base +
				AHB2STBUS_MSGSIZE_OFFSET);
			reg = readl(protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
		} while ((reg & 0x7FFFFFFF) != req_reg);
	}
	return 0;
}

static int st_usb_probe(struct platform_device *pdev)
{
	struct plat_usb_data *pdata = pdev->dev.platform_data;
	unsigned long ahb2stbus_wrapper_glue_base =
		pdata->ahb2stbus_wrapper_glue_base;
	unsigned long ahb2stbus_protocol_base =
		pdata->ahb2stbus_protocol_base;
	struct resource *res;

	dgb_print("\n");
	/* Power on */
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 0);
	/* Wait the ack */
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 0);

	if (!request_mem_region(ahb2stbus_wrapper_glue_base, 0x100,
			pdev->name))
		return -1;

	if (!request_mem_region(ahb2stbus_protocol_base, 0x100,
			pdev->name))
		return -1;

	pdata->ahb2stbus_wrapper_glue_base
		= ioremap(ahb2stbus_wrapper_glue_base, 0x100);
	if (!pdata->ahb2stbus_wrapper_glue_base)
		return -1;

	pdata->ahb2stbus_protocol_base =
		ioremap(ahb2stbus_protocol_base, 0x100);
	if (!pdata->ahb2stbus_protocol_base)
		return -1;

	st_usb_boot(pdev);

	return 0;
}

static void st_usb_shutdown(struct platform_device *pdev)
{
	dgb_print("\n");
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 1);
}

#ifdef CONFIG_PM
static int st_usb_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct plat_usb_data *pdata = pdev->dev.platform_data;
	unsigned long wrapper_base = pdata->ahb2stbus_wrapper_glue_base;
	void *protocol_base = pdata->ahb2stbus_protocol_base;
	struct usb_hcd *hcd = pdata->ehci_hcd;
	long reg;
	dgb_print("\n");

	if (pdata->flags & USB_FLAGS_STRAP_PLL) {
		/* PLL turned off */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	writel(0, hcd->regs + AHB2STBUS_INSREG01_OFFSET);
	writel(0, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	writel(0, protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

	writel(1, protocol_base + AHB2STBUS_SW_RESET);
	mdelay(10);
	writel(0, protocol_base + AHB2STBUS_SW_RESET);

	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 1);
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 1);
	return 0;
}
static int st_usb_resume(struct platform_device *pdev)
{
	dgb_print("\n");
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 0);
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 0);
	st_usb_boot(pdev);
	return 0;
}
#else
#define st_usb_suspend	NULL
#define st_usb_resume	NULL
#endif

static struct platform_driver st_usb_driver = {
	.driver.name = "st-usb",
	.driver.owner = THIS_MODULE,
	.probe = st_usb_probe,
	.shutdown = st_usb_shutdown,
	.suspend = st_usb_suspend,
	.resume = st_usb_resume,
};

struct hcd_usb_data {
	int is_ohci;
	int (*fnt)(struct platform_device *pdev);
};

static int check_capability(struct device *dev, void *data)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct plat_usb_data *pdata = pdev->dev.platform_data;
	struct hcd_usb_data *hdata =
		(struct usb_data *)data;
	int id = (hdata->is_ohci == 0 ? 0 : 1);
	struct resource *res;
	dgb_print("\n");
	dgb_print(">>>\n");
/*
 * Check if the st-usb has xHCI capability
 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, id);
	if (res->start) {
		hdata->fnt(pdev);
		if (hdata->is_ohci)
			pdata->ohci_hcd = pdev->dev.driver_data;
		else
			pdata->ehci_hcd = pdev->dev.driver_data;
	}
}

static void __init st_usb_init(void)
{
	dgb_print("\n");
	platform_driver_register(&st_usb_driver);
}

static void __exit st_usb_exit(void)
{
	dgb_print("\n");
	platform_driver_unregister(&st_usb_driver);
}

int st_usb_register_hcd(int is_ohci, int (*fnt)(struct platform_device *pdev))
{
	struct hcd_usb_data data = {
		.is_ohci = is_ohci,
		.fnt = fnt,
	};
	dgb_print("\n");
	driver_for_each_device(&st_usb_driver.driver, NULL,
		&data, check_capability);
	return 0;
}
EXPORT_SYMBOL_GPL(st_usb_register_hcd);

MODULE_LICENSE("GPL");

arch_initcall(st_usb_init);
module_exit(st_usb_exit);
