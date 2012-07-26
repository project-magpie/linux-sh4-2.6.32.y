/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include <linux/device.h>
#include <linux/platform_device.h>

int platform_allow_pm_sysconf(struct device *dev,
	int reg_nr, int freezing)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pdev->id != 4)
		return 1;
	if (reg_nr < (2 * sizeof(long)))
		return 1;
	return 0;
}
