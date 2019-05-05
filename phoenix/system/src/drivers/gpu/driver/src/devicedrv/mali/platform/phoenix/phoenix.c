/*
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mali/mali_utgard.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include "mali_osk_mali.h"
#include "mali_kernel_linux.h"
#include "mali_kernel_common.h"

struct gpu_wrapper_priv {
	struct device *dev;
	struct reset_control *rstc;
	struct clk *clk;
	int suspended;
};

static struct gpu_wrapper_priv *priv;

static int gpu_wrapper_runtime_suspend(struct device *dev)
{
	struct mali_device *mdev __maybe_unused = dev_get_drvdata(dev);
	int (*cb)(struct device *__dev);
	int ret;

	dev_dbg(dev, "[GPU] Enter %s\n", __func__);

	if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_suspend;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	ret = cb ? cb(dev) : 0;

	if (!priv->suspended) {
#if defined(CONFIG_MALI_DEVFREQ)
		clk_disable_unprepare(mdev->clock);
#endif
	}
	priv->suspended = 1;

	dev_dbg(dev, "[GPU] Exit %s\n", __func__);

	return ret;
}

static int gpu_wrapper_runtime_resume(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct mali_device *mdev __maybe_unused= dev_get_drvdata(dev);

	dev_dbg(dev, "[GPU] Enter %s\n", __func__);

	if (priv->suspended) {
#if defined(CONFIG_MALI_DEVFREQ)
		clk_prepare_enable(mdev->clock);
#endif
	}
	priv->suspended = 0;

	if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_resume;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	dev_dbg(dev, "[GPU] Exit %s\n", __func__);

	return cb ? cb(dev) : 0;
}

static const struct dev_pm_ops gpu_wrapper_pm_ops = {
	.runtime_suspend = gpu_wrapper_runtime_suspend,
	.runtime_resume = gpu_wrapper_runtime_resume,
};

__maybe_unused static struct device_type gpu_device_type = {
	.name = "gpu",
	.pm = &gpu_wrapper_pm_ops,
};

static struct mali_gpu_device_data mali_gpu_data = {
	.max_job_runtime = 60000,
	.shared_mem_size = 512 * 1024 * 1024,
	.fb_start = 0x0,
	.fb_size = 0xFFFFF000,
};

int mali_platform_device_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	//struct device_node *np = dev->of_node;
	int ret;

	dev_info(dev, "[GPU] %s\n", __func__);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	if (dev->type)
		dev_err(dev, "dev->type existed: %s\n", dev->type->name);
	else
		dev->type = &gpu_device_type;

	priv->clk = clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		dev_err(dev, "clk_get() returns %d\n", ret);
		return ret;
	}
	clk_prepare_enable(priv->clk);
	priv->suspended = 0;

	priv->rstc = reset_control_get(dev, NULL);
	if (IS_ERR(priv->rstc)) {
		ret = PTR_ERR(priv->rstc);
		dev_warn(dev, "reset_control_get() failed: %d\n", ret);
		priv->rstc = NULL;
	} else {
		reset_control_deassert(priv->rstc);
	}


	ret = platform_device_add_data(pdev, &mali_gpu_data, sizeof(mali_gpu_data));
	if (!ret) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(dev, 500);
		pm_runtime_use_autosuspend(dev);
#endif
		pm_runtime_enable(dev);
#endif
	}

	return 0;
}


int mali_platform_device_deinit(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "[GPU] %s\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(dev);
#endif

	if (priv->rstc);
		reset_control_put(priv->rstc);
	kfree(priv);
	priv = NULL;

	return 0;
}


