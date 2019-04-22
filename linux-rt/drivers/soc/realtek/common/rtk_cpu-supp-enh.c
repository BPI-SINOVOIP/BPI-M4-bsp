/*
 * Realtek CPU supply enhancer
 *
 * Copyright (c) 2019, Realtek Semiconductor Corporation
 *
 * Author:
 *        Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

struct rtk_cse_data {
	struct device *dev;
	struct notifier_block nb;
	struct regulator *supply;
};

static int rtk_cse_cb(struct notifier_block *nb, unsigned long action, void *p)
{
	struct rtk_cse_data *data = container_of(nb, struct rtk_cse_data, nb);
	struct cpufreq_freqs *freqs = p;
	int ret = 0;

	if (action != CPUFREQ_PRECHANGE)
		return NOTIFY_DONE;

	ret = regulator_set_mode(data->supply, freqs->new >= 1000000 ?
		REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL);
	if (ret)
		dev_err(data->dev, "failed to set mode: %d\n", ret);
	return NOTIFY_OK;
}

static int rtk_cse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *dvfs_dev;
	struct rtk_cse_data *data;
	int ret;

	dvfs_dev = bus_find_device_by_name(&platform_bus_type, NULL, "cpu-dvfs");
	if (!dvfs_dev) {
		dev_err(dev, "no cpu-dvfs device\n");
		return -ENODEV;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	data->supply = regulator_get_optional(dev, "cpudvs");
	if (IS_ERR(data->supply)) {
		ret = PTR_ERR(data->supply);
		if (ret == -EPROBE_DEFER || ret == -ENODEV) {
			dev_dbg(dev, "cpudvs: not ready, returns -EPROBE_DEFER\n");
			return -EPROBE_DEFER;
		} else
			dev_err(dev, "regulator_get() returns %d\n", ret);
		return ret;
	}

	data->nb.notifier_call = rtk_cse_cb;
	cpufreq_register_notifier(&data->nb, CPUFREQ_TRANSITION_NOTIFIER);
	dev_info(dev, "initialized\n");
	return 0;
}

static struct platform_driver rtk_cse_driver = {
	.probe = rtk_cse_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "rtk-cse",
	},
};

static int __init rtk_cse_init(void)
{
	int ret;
	struct platform_device *pdev;

	ret = platform_driver_register(&rtk_cse_driver);
	if (ret) {
		pr_err("%s: platform_driver_register() returns %d\n", __func__, ret);
		return ret;
	}

	pdev = platform_device_register_simple("rtk-cse", 0, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		pr_err("%s: platform_driver_register() returns %d\n", __func__, ret);
		platform_driver_unregister(&rtk_cse_driver);
		return ret;
	}
	return 0;
}
module_init(rtk_cse_init);
