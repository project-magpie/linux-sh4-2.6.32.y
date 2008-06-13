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

#include "stb7100-common.h"

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

#ifdef	CONFIG_PM
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
#endif

static const struct hc_driver ohci_st40_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"STM OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/* generic hardware linkage */
	.irq =			ohci_irq,
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

static int ohci_hcd_stm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	const struct hc_driver *driver = &ohci_st40_hc_driver;
	int retval;

	if (usb_disabled())
		return -ENODEV;

	retval = ST40_start_host_control(pdev);
	if (retval)
		return retval;

	hcd = usb_create_hcd(driver, &pdev->dev, pdev->dev.bus_id);
	if (!hcd) {
		pr_debug("hcd_create_hcd failed");
		retval = -ENOMEM;
		goto err0;
	}

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;

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

	retval = usb_add_hcd(hcd, pdev->resource[1].start, 0);
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

static int ohci_hcd_stm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	return 0;
}

static struct platform_driver ohci_hcd_stm_driver = {
	.driver = {
		.name = "stm-ohci",
                .bus = &platform_bus_type
	},
	.probe = ohci_hcd_stm_probe,
	.remove = ohci_hcd_stm_remove,
        .shutdown = usb_hcd_platform_shutdown,
};
