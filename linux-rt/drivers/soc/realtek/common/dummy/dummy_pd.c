#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include "pwrctrl-pd.h"

struct dummy_pd dummy_pd0 = {
	.core = {
		.pd.name = "dummy_pd_0",
		.pc.name = "pctrl_dummy_pd_0",
		.pc.ops = &dummy_ops,
	},
};

struct dummy_pd dummy_pd1 = {
	.core = {
		.pd.name = "dummy_pd_1",
		.pc.name = "pctrl_dummy_pd_1",
		.pc.ops = &dummy_ops,
	},
};

static struct pwrctrl_pd *dummy_pds[2] = {
	[0] = &dummy_pd0.core,
	[1] = &dummy_pd1.core,
};

static struct power_controller_data dummy_pd_data = {
	.ppds = dummy_pds,
	.num_ppds = ARRAY_SIZE(dummy_pds),
};

static const struct of_device_id dummy_pd_ids[] = {
	{ .compatible = "realtek,dummy-power-controller" },
	{}
};

static int dummy_pd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);
	dev_set_drvdata(dev, &dummy_pd_data);
        power_controller_init_pds(dev, &dummy_pd_data);

	return 0;
}

static struct platform_driver dummy_pd_driver = {
	.probe = dummy_pd_probe,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "dummy-pd",
		.of_match_table = dummy_pd_ids,
	},
};
module_platform_driver(dummy_pd_driver);

