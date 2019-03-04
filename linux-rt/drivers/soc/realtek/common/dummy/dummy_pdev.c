#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

static int dummy_runtime_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int dummy_runtime_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int dummy_resume(struct device *dev)
{
	struct device_node *np = dev->of_node;

	dev_info(dev, "%s\n", __func__);

	if (of_get_property(np, "runtime-pm-resumed", NULL))
		pm_runtime_get_sync(dev);
	return 0;
}

static int dummy_suspend(struct device *dev)
{
	struct device_node *np = dev->of_node;


	dev_info(dev, "%s\n", __func__);

	if (of_get_property(np, "runtime-pm-resumed", NULL))
		pm_runtime_put_sync(dev);
	return 0;
}

static const struct dev_pm_ops dummy_pm_ops = {
	SET_RUNTIME_PM_OPS(dummy_runtime_suspend, dummy_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(dummy_suspend, dummy_resume)
};

static const struct of_device_id dummy_ids[] = {
	{ .compatible = "realtek,dummy" },
	{}
};

static int dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	dev_info(dev, "%s\n", __func__);
	if (of_get_property(np, "runtime-pm-enabled", NULL)) {
		dev_info(dev, "%s: %s\n", __func__, "runtime-pm-enabled");
		pm_runtime_set_suspended(dev);
		pm_runtime_enable(dev);

		if (of_get_property(np, "runtime-pm-resumed", NULL)) {
			dev_info(dev, "%s: %s\n", __func__, "runtime-pm-resumed");
			pm_runtime_get_sync(dev);
		}
	}

	return 0;
}

static struct platform_driver dummy_driver = {
	.probe = dummy_probe,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "dummy",
		.of_match_table = dummy_ids,
		.pm             = &dummy_pm_ops,
	},
};
module_platform_driver(dummy_driver);

