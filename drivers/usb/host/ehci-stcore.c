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
#include <linux/stm/soc.h>
#include <linux/stm/pm.h>

/* The transaction opcode is programmed in this register */
#define AHB2STBUS_STBUS_OPC_OFFSET      0x00    /* From PROTOCOL_BASE */
#define AHB2STBUS_STBUS_OPC_4BIT        0x00
#define AHB2STBUS_STBUS_OPC_8BIT        0x01
#define AHB2STBUS_STBUS_OPC_16BIT       0x02
#define AHB2STBUS_STBUS_OPC_32BIT       0x03
#define AHB2STBUS_STBUS_OPC_64BIT       0x04

/* The message length in number of packets is programmed in this register. */
#define AHB2STBUS_MSGSIZE_OFFSET        0x04    /* From PROTOCOL_BASE */
#define AHB2STBUS_MSGSIZE_DISABLE       0x0
#define AHB2STBUS_MSGSIZE_2             0x1
#define AHB2STBUS_MSGSIZE_4             0x2
#define AHB2STBUS_MSGSIZE_8             0x3
#define AHB2STBUS_MSGSIZE_16            0x4
#define AHB2STBUS_MSGSIZE_32            0x5
#define AHB2STBUS_MSGSIZE_64            0x6

/* The chunk size in number of packets is programmed in this register */
#define AHB2STBUS_CHUNKSIZE_OFFSET      0x08    /* From PROTOCOL_BASE */
#define AHB2STBUS_CHUNKSIZE_DISABLE     0x0
#define AHB2STBUS_CHUNKSIZE_2           0x1
#define AHB2STBUS_CHUNKSIZE_4           0x2
#define AHB2STBUS_CHUNKSIZE_8           0x3
#define AHB2STBUS_CHUNKSIZE_16          0x4
#define AHB2STBUS_CHUNKSIZE_32          0x5
#define AHB2STBUS_CHUNKSIZE_64          0x6

#define AHB2STBUS_TIMEOUT		0x0c

#define AHB2STBUS_SW_RESET		0x10

/* Wrapper Glue registers */

#define AHB2STBUS_STRAP_OFFSET          0x14    /* From WRAPPER_GLUE_BASE */
#define AHB2STBUS_STRAP_PLL             0x08    /* undocumented */
#define AHB2STBUS_STRAP_8_BIT           0x00    /* ss_word_if */
#define AHB2STBUS_STRAP_16_BIT          0x04    /* ss_word_if */


/* Extensions to the standard USB register set */

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET       (0x10 + 0x84) /* From EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD       0x00800080

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

static int ehci_st40_reset(struct usb_hcd *hcd)
{
	writel(AHB2STBUS_INOUT_THRESHOLD,
	       hcd->regs + AHB2STBUS_INSREG01_OFFSET);
	return ehci_init(hcd);
}

#ifdef CONFIG_PM
static int
stm_ehci_bus_suspend(struct usb_hcd *hcd)
{
	ehci_bus_suspend(hcd);
/*
 * force the root hub to be resetted on resume!
 * re-enumerates everything during a standby, mem and hibernation...
 */
	usb_root_hub_lost_power(hcd->self.root_hub);
	return 0;
}
#else
#define stm_ehci_bus_suspend		NULL
#endif

static const struct hc_driver ehci_stm_hc_driver = {
	.description = hcd_name,
	.product_desc = "st-ehci",
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
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

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
/*
 * The ehci_bus_suspend suspends all the root hub ports but
 * it leaves all the interrupts enabled on insert/remove devices
 */
	.bus_suspend = stm_ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
};

static void ehci_hcd_st40_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

static int ehci_hcd_stm_probe(struct platform_device *dev)
{
	int retval = 0;
	struct usb_hcd *hcd;
        struct ehci_hcd *ehci;
	struct plat_usb_data *pdata = dev->dev.platform_data;
	struct resource *res;

	hcd = usb_create_hcd(&ehci_stm_hc_driver, &dev->dev, dev->dev.bus_id);
	if (!hcd) {
		retval = -ENOMEM;
		goto err0;
	}

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;

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

	res = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	retval = usb_add_hcd(hcd, res->start, 0);
	if (retval == 0) {
		pdata->ehci_hcd = hcd;
#ifdef CONFIG_PM
		hcd->self.root_hub->do_remote_wakeup = 0;
		hcd->self.root_hub->persist_enabled = 0;
		hcd->self.root_hub->autosuspend_disabled = 1;
		hcd->self.root_hub->autoresume_disabled = 1;
#endif
		return retval;
	}
	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

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

int ohci_hcd_stm_probe(struct platform_device *pdev);

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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res->start) {
		ehci_hcd_stm_probe(pdev); /* is it EHCI able ? */
		pdata->ehci_hcd = pdev->dev.driver_data;
	}

#ifdef CONFIG_USB_OHCI_HCD
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res->start) {
		ohci_hcd_stm_probe(pdev); /* is it OHCI able ? */
		pdata->ohci_hcd = pdev->dev.driver_data;
	}
#endif
	return 0;
}

static void st_usb_shutdown(struct platform_device *pdev)
{
	struct plat_usb_data *pdata = pdev->dev.platform_data;
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

static struct platform_driver ehci_hcd_stm_driver = {
	.driver.name = "stm-ehci",
};

static struct platform_driver st_usb_driver = {
	.driver.name = "st-usb",
	.probe = st_usb_probe,
	.shutdown = st_usb_shutdown,
	.suspend = st_usb_suspend,
	.resume = st_usb_resume,
};

static int __init st_usb_init(void)
{
	dgb_print("\n");
	platform_driver_register(&st_usb_driver);
	return 0;
}
module_init(st_usb_init);
