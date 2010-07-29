/*
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * include/linux/mmc/sdhci-stm.h
 *
 * platform data for the sdhci stm driver.
 *
 * Copyright (C) 2010  STMicroelectronics Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 */

#ifndef __SDHCI_STM_PLAT_H__
#define __SDHCI_STM_PLAT_H__

#include <linux/stm/platform.h>
#include <linux/stm/pad.h>

struct sdhci_platform_data {
	struct stm_pad_config *pad_config;
};

/* SDHCI_STM Resource configuration */
static inline int sdhci_claim_resource(struct platform_device *pdev)
{
	int ret = 0;
	struct sdhci_platform_data *plat_dat = pdev->dev.platform_data;

	/* Pad routing setup required on STM platforms */
	if (!devm_stm_pad_claim(&pdev->dev, plat_dat->pad_config,
				dev_name(&pdev->dev))) {
		pr_err("%s: Failed to request pads!\n", __func__);
		ret = -ENODEV;
	}
	return ret;
}
#endif /* __SDHCI_STM_PLAT_H__ */
