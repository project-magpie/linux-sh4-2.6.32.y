/* linux/drivers/mmc/host/sdhci-stm.c
 *
 * Copyright (C) 2010 STMicroelectronics Ltd
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * SDHCI support for STM platforms
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include <linux/io.h>

#include <linux/mmc/sdhci-stm.h>

#include "sdhci.h"

static struct sdhci_ops sdhci_pltfm_ops = {
	/* Nothing to do for now. */
};

struct sdhci_stm {
	struct sdhci_host *host;
	struct platform_device *pdev;
	struct resource *ioarea;
	struct sdhci_platform_data *pdata;
};

static int __devinit sdhci_probe(struct platform_device *pdev)
{
	struct sdhci_platform_data *pdata;
	struct device *dev = &pdev->dev;
	struct sdhci_host *host;
	struct sdhci_stm *sc;
	struct resource *res;
	int ret, irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no memory specified\n");
		return -ENOENT;
	}

	irq = platform_get_irq_byname(pdev, "mmcirq");
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return irq;
	}

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(dev, "no device data specified\n");
		return -ENOENT;
	}

	host = sdhci_alloc_host(&pdev->dev, 0);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		dev_dbg(&pdev->dev, "error allocating host\n");
		return PTR_ERR(host);
	}

	sc = sdhci_priv(host);
	sc->host = host;
	sc->pdev = pdev;
	sc->pdata = pdata;

	platform_set_drvdata(pdev, host);

	sc->ioarea = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (!sc->ioarea) {
		dev_err(dev, "failed to reserve register area\n");
		ret = -ENXIO;
		goto out;
	}

	ret = sdhci_claim_resource(pdev);
	if (ret < 0)
		goto out;

	host->hw_name = "sdhci-stm";
	host->ops = &sdhci_pltfm_ops;
	host->irq = irq;
	host->quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC;

	host->ioaddr = ioremap(res->start, resource_size(res));
	if (!host->ioaddr) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = sdhci_add_host(host);
	if (ret) {
		dev_dbg(&pdev->dev, "error adding host\n");
		goto out;
	}

	return 0;

out:
	if (host->ioaddr)
		iounmap(host->ioaddr);

	if (sc->ioarea) {
		release_resource(sc->ioarea);
		kfree(sc->ioarea);
	}

	sdhci_free_host(host);

	return ret;
}

static int __devexit sdhci_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_stm *sc = sdhci_priv(host);

	sdhci_remove_host(host, 1);

	iounmap(host->ioaddr);
	release_resource(sc->ioarea);
	kfree(sc->ioarea);

	sdhci_free_host(host);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int sdhci_stm_suspend(struct platform_device *dev, pm_message_t pm)
{
	struct sdhci_host *host = platform_get_drvdata(dev);

	sdhci_suspend_host(host, pm);
	return 0;
}

static int sdhci_stm_resume(struct platform_device *dev)
{
	struct sdhci_host *host = platform_get_drvdata(dev);

	sdhci_resume_host(host);
	return 0;
}
#endif

static struct platform_driver sdhci_driver = {
	.driver = {
		   .name = "sdhci-stm",
		   .owner = THIS_MODULE,
		   },
	.probe = sdhci_probe,
	.remove = __devexit_p(sdhci_remove),
#ifdef CONFIG_PM
	.suspend = sdhci_stm_suspend,
	.resume = sdhci_stm_resume,
#endif
};

static int __init sdhci_stm_init(void)
{
	return platform_driver_register(&sdhci_driver);
}

module_init(sdhci_stm_init);

static void __exit sdhci_stm_exit(void)
{
	platform_driver_unregister(&sdhci_driver);
}

module_exit(sdhci_stm_exit);

MODULE_DESCRIPTION("STM Secure Digital Host Controller Interface driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL v2");
