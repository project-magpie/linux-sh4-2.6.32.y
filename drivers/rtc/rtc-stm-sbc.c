/*
   * This driver implements a RTC for the Low Power Mode
   * in some STMicroelectronics devices.
   *
   * Copyright (C) 2012 STMicroelectronics Limited
   * Author: Satbir Singh <satbir.singh@st.com>
   *
   * May be copied or modified under the terms of the GNU General Public
   * License.  See linux/COPYING for more information.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/stm/lpm.h>

#define DRV_NAME "stm-rtc-sbc"

struct stm_sbc_rtc {
	struct rtc_device *rtc_dev;
	struct rtc_wkalrm alarm;
	struct rtc_time tm_cur;
};

static unsigned long get_time_in_sec(void)
{
	return jiffies_to_msecs(jiffies)/1000;
}

static int stm_sbc_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long lpt;
	unsigned long lpt_cur;
	struct stm_sbc_rtc *rtc = dev_get_drvdata(dev);

	lpt = get_time_in_sec();
	rtc_tm_to_time(&rtc->tm_cur, &lpt_cur);
	rtc_time_to_tm(lpt, tm);

	if (lpt < lpt_cur)
		lpt = lpt - lpt_cur;
	rtc_time_to_tm(lpt, &rtc->tm_cur);

	return 0;
}

static int stm_sbc_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct stm_sbc_rtc *rtc = dev_get_drvdata(dev);

	memcpy(&rtc->tm_cur, tm, sizeof(rtc->tm_cur));

	return 0;
}

static int stm_sbc_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct stm_sbc_rtc *rtc = dev_get_drvdata(dev);

	memcpy(wkalrm, &rtc->alarm, sizeof(struct rtc_wkalrm));

	return 0;
}

static int stm_sbc_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	int ret = 0;
	unsigned long lpt;
	struct stm_sbc_rtc *rtc = dev_get_drvdata(dev);

	rtc_tm_to_time(&t->time, &lpt);
	lpt = lpt - get_time_in_sec();
	rtc_time_to_tm(lpt, &t->time);

	memcpy(&rtc->alarm, t, sizeof(struct rtc_wkalrm));

	ret = stm_lpm_set_wakeup_time(lpt);
	if (ret < 0)
		return ret;

	device_set_wakeup_enable(dev, true);

	return 0;
}

static struct rtc_class_ops stm_sbc_rtc_ops = {
	.read_time = stm_sbc_rtc_read_time,
	.set_time = stm_sbc_rtc_set_time,
	.read_alarm = stm_sbc_rtc_read_alarm,
	.set_alarm = stm_sbc_rtc_set_alarm,
};


static int __devinit stm_sbc_rtc_probe(struct platform_device *pdev)
{
	struct stm_sbc_rtc *rtc;
	int ret = 0;

	rtc = devm_kzalloc(&pdev->dev,
		sizeof(struct stm_sbc_rtc), GFP_KERNEL);
	if (unlikely(!rtc))
		return -ENOMEM;

	rtc_time_to_tm(get_time_in_sec(), &rtc->tm_cur);

	device_set_wakeup_capable(&pdev->dev, 1);
	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = rtc_device_register(DRV_NAME, &pdev->dev,
		&stm_sbc_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		ret = PTR_ERR(rtc->rtc_dev);
		goto err_badreg;
	}

	return ret;

err_badreg:
	device_set_wakeup_capable(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __devexit stm_sbc_rtc_remove(struct platform_device *pdev)
{
	struct stm_sbc_rtc *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc_dev);
	device_set_wakeup_capable(&pdev->dev, 0);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int stm_sbc_rtc_freeze(struct device *dev)
{
	return 0;
}
static int stm_sbc_rtc_restore(struct device *dev)
{
	struct stm_sbc_rtc *rtc = dev_get_drvdata(dev);

	rtc_alarm_irq_enable(rtc->rtc_dev, 0);
	device_set_wakeup_enable(dev, false);

	return 0;
}

static const struct dev_pm_ops stm_sbc_rtc_pm_ops = {
	.freeze = stm_sbc_rtc_freeze,
	.restore = stm_sbc_rtc_restore,
};
#define STM_SBC_RTC_PM_OPS  (&stm_sbc_rtc_pm_ops)
#else
#define STM_SBC_RTC_PM_OPS  NULL
#endif

static struct platform_driver stm_rtc_platform_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = STM_SBC_RTC_PM_OPS,
	},
	.probe = stm_sbc_rtc_probe,
	.remove = __devexit_p(stm_sbc_rtc_remove),
};

static int __init stm_sbc_rtc_init(void)
{
	return platform_driver_register(&stm_rtc_platform_driver);
}

static void __exit stm_sbc_rtc_exit(void)
{
	platform_driver_unregister(&stm_rtc_platform_driver);
}

module_init(stm_sbc_rtc_init);
module_exit(stm_sbc_rtc_exit);

MODULE_DESCRIPTION("STMicroelectronics LPM RTC driver");
MODULE_AUTHOR("satbir.singh@st.com");
MODULE_LICENSE("GPL");
