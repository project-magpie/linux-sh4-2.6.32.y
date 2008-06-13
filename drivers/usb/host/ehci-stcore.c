/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include "stb7100-common.h"

#ifdef	CONFIG_PM
static int ehci_st40_suspend(struct usb_hcd *hcd, pm_message_t message)
{
	/* Needs implementation! Look at ehci-pci as guide */
	return 0;
}

static int ehci_st40_resume(struct usb_hcd *hcd)
{
	/* Needs implementation! Look at ehci-pci as guide */
	return 0;
}
#endif

static int ehci_st40_reset(struct usb_hcd *hcd)
{
	writel(AHB2STBUS_INOUT_THRESHOLD,
	       hcd->regs + AHB2STBUS_INSREG01_OFFSET);
	return ehci_init(hcd);
}

static const struct hc_driver ehci_st40_hc_driver = {
	.description = hcd_name,
	.product_desc = "STM EHCI Host Controller",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset = ehci_st40_reset,
	.start = ehci_run,
#ifdef	CONFIG_PM
	.suspend = ehci_st40_suspend,
	.resume = ehci_st40_resume,
#endif
	.stop = ehci_stop,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
};

static void ehci_hcd_st40_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

static int ehci_hcd_st40_probe(const struct hc_driver *driver,
			       struct usb_hcd **hcd_out,
			       struct platform_device *dev)
{
	int retval = 0;
	struct usb_hcd *hcd;
        struct ehci_hcd *ehci;

	retval = ST40_start_host_control(dev);
	if (retval)
		return retval;

	hcd = usb_create_hcd(driver, &dev->dev, dev->dev.bus_id);
	if (!hcd) {
		retval = -ENOMEM;
		goto err0;
	}

	hcd->rsrc_start = dev->resource[0].start;
	hcd->rsrc_len = dev->resource[0].end - dev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed");
		retval = -ENOMEM;
		goto err2;
	}

       ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs + HC_LENGTH(readl(&ehci->caps->hc_capbase));

	/* cache this readonly data; minimize device reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	retval = usb_add_hcd(hcd, dev->resource[1].start, 0);
	if (retval == 0)
		return retval;

	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

static int ehci_hcd_st40_driver_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	ret = ehci_hcd_st40_probe(&ehci_st40_hc_driver, &hcd, pdev);
	return ret;
}

static int ehci_hcd_st40_driver_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	ehci_hcd_st40_remove(hcd, pdev);
	return 0;
}

static struct platform_driver ehci_hcd_st40_driver = {
	.probe = ehci_hcd_st40_driver_probe,
	.remove = ehci_hcd_st40_driver_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "stm-ehci",
		.bus = &platform_bus_type
	}
};
