/*
 * OHCI HCD (Host Controller Driver) for USB
 *
 * (C) copyright STMicroelectronics 2005
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * STMicroelectronics on-chip USB host controller Bus Glue.
 * Based on the StrongArm ohci-sa1111.c file
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/platform_device.h>
#include "ohci-stcore.h"

extern int usb_disabled(void);

#define DEVICE_NAME "STB7100 OHCI"
#include "stb7100-common.h"

static irqreturn_t ohci_st40_irq(struct usb_hcd *hcd)
{
	irqreturn_t retval;

	usb_hcd_st40_wait_irq();
	retval = ohci_irq(hcd);

	return retval;
}

static void
ohci_hcd_st40_remove(struct usb_hcd *hcd, struct platform_device *dev)
{
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

static int
ohci_st40_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret = 0;

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		err("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static int
ohci_st40_suspend(struct usb_hcd *hcd, pm_message_t message)
{
	return 0;
}

static int
ohci_st40_resume(struct usb_hcd *hcd)
{
	return 0;
}

static const struct hc_driver ohci_st40_hc_driver = {
	.description =		hcd_name,
	.product_desc =		DEVICE_NAME,
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/* generic hardware linkage */
	.irq =			ohci_st40_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/* basic lifecycle operations */
	.start =		ohci_st40_start,
#ifdef	CONFIG_PM
	.suspend =		ohci_st40_suspend,
	.resume =		ohci_st40_resume,
#endif
	.stop =			ohci_stop,

	/* managing i/o requests and associated device resources */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/* scheduling support */
	.get_frame_number =	ohci_get_frame,

	/* root hub support */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_USB_SUSPEND
/* note we don't export these funcs for our ohci*/
/*	.hub_suspend =		ohci_hub_suspend,*/
/*	.hub_resume =		ohci_hub_resume,*/
#endif
	.start_port_reset =	ohci_start_port_reset,
};

static int ohci_hcd_st40_probe(const struct hc_driver *driver,
			       struct usb_hcd **hcd_out,
			       struct platform_device *dev)
{
	struct usb_hcd *hcd;
	int retval;

	ST40_start_host_control(dev);

	hcd = usb_create_hcd(driver, &dev->dev, DEVICE_NAME);
	if (!hcd) {
		pr_debug("hcd_create_hcd failed");
		retval = -ENOMEM;
		goto err0;
	}

	hcd->rsrc_start = dev->resource[0].start;
	hcd->rsrc_len = dev->resource[0].end - dev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,	hcd_name)) {
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

	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = usb_add_hcd(hcd, dev->resource[1].start, SA_INTERRUPT);
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

static int ohci_hcd_st40_drv_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	ret = ohci_hcd_st40_probe(&ohci_st40_hc_driver, &hcd, pdev);
	return ret;
}

static int ohci_hcd_st40_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	ohci_hcd_st40_remove(hcd, pdev);
	return 0;
}

static struct platform_driver ohci_hcd_st40_driver = {
	.probe = ohci_hcd_st40_drv_probe,
	.remove = ohci_hcd_st40_drv_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "ST40-ohci",
		.owner = THIS_MODULE,
	}
};

static int __init ohci_hcd_st40_init(void)
{
	printk(DRIVER_INFO " (ST40)\n");
	return platform_driver_register(&ohci_hcd_st40_driver);
}

static void __exit ohci_hcd_st40_cleanup(void)
{
	platform_driver_unregister(&ohci_hcd_st40_driver);
}

module_init(ohci_hcd_st40_init);
module_exit(ohci_hcd_st40_cleanup);
