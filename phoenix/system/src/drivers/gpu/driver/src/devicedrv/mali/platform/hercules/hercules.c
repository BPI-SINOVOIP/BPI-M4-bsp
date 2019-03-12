/*
 * Copyright (C) 2017 Realtek Semiconductor Corporation
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

#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#endif

struct gpu_wrapper_priv {
	struct device *dev;
	void __iomem *base;
	struct reset_control *rstc;
	struct clk *clk;
	int first_resume;
#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	struct thermal_zone_device *tz;
#endif
};

static void enable_l2_bisr(struct gpu_wrapper_priv *priv);
static void gpu_sram_power_on(struct gpu_wrapper_priv *priv);

static struct gpu_wrapper_priv *priv;

#define SECURE_MODE_CONTROL_HANDLER  0x981FE0E0

void *secure_mode_mapped_addr = NULL;

static int hercules_gpu_reset_and_secure_mode_enable(void)
{
	u32 phys_offset    = SECURE_MODE_CONTROL_HANDLER & 0x00001FFF;

	iowrite32(1, ((u8 *)secure_mode_mapped_addr) + phys_offset);

	if (1 == (u32)ioread32(((u8 *)secure_mode_mapped_addr) + phys_offset)) {
		MALI_DEBUG_PRINT(3, ("Mali reset GPU and enable secured mode successfully! \n"));
		return 0;
	}

	MALI_PRINT_ERROR(("Failed to reset GPU and enable Mali secured mode !!! \n"));

	return -1;

}

static int hercules_gpu_reset_and_secure_mode_disable(void)
{
	u32 phys_offset    = SECURE_MODE_CONTROL_HANDLER & 0x00001FFF;

	iowrite32(0, ((u8 *)secure_mode_mapped_addr) + phys_offset);

	if (0 == (u32)ioread32(((u8 *)secure_mode_mapped_addr) + phys_offset)) {
		MALI_DEBUG_PRINT(3, ("Mali reset GPU and disable secured mode successfully! \n"));
		return 0;
	}

	MALI_PRINT_ERROR(("Failed to reset GPU and disable mali secured mode !!! \n"));
	return -1;
}

static int hercules_secure_mode_init(void)
{
	u32 phys_addr_page = SECURE_MODE_CONTROL_HANDLER & 0xFFFFE000;
	u32 phys_offset    = SECURE_MODE_CONTROL_HANDLER & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);

	secure_mode_mapped_addr = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != secure_mode_mapped_addr) {
		return hercules_gpu_reset_and_secure_mode_disable();
	}
	MALI_DEBUG_PRINT(2, ("Failed to ioremap for Mali secured mode! \n"));
	return -1;
}

static void hercules_secure_mode_deinit(void)
{
	if (NULL != secure_mode_mapped_addr) {
		hercules_gpu_reset_and_secure_mode_disable();
		iounmap(secure_mode_mapped_addr);
		secure_mode_mapped_addr = NULL;
	}
}

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

	clk_disable_unprepare(priv->clk);
#if defined(CONFIG_MALI_DEVFREQ)
	clk_disable_unprepare(mdev->clock);
#endif
	dev_dbg(dev, "[GPU] Exit %s\n", __func__);

	return ret;
}

static int gpu_wrapper_runtime_resume(struct device *dev)
{
	int (*cb)(struct device *__dev);
	struct mali_device *mdev __maybe_unused= dev_get_drvdata(dev);

	dev_dbg(dev, "[GPU] Enter %s\n", __func__);

	if (!priv->first_resume) {
#if defined(CONFIG_MALI_DEVFREQ)
		clk_prepare_enable(mdev->clock);
#endif
		clk_prepare_enable(priv->clk);
	}
	priv->first_resume = 0;

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

static void enable_l2_bisr(struct gpu_wrapper_priv *priv)
{
	int retry = 200;
	void __iomem *base = priv->base + 0x20000; /* 0x981F0000 */

	/* unnecessary to enable L2 BISR on FPGA */
	if (IS_ENABLED(CONFIG_RTK_PLATFORM_FPGA))
		return;

	dev_info(priv->dev, "%s\n", __func__);

	writel(0x00000030, base + 0xE000);
	writel(0x00000021, base + 0xE074);
	writel(0x00000063, base + 0xE074);
	while (--retry > 0) {
		if ((readl(base + 0xE014) & 0x30) == 0x30)
			break;
		mdelay(10);
	}
	if (retry == 0)
		dev_warn(priv->dev, "L2 BISR timeout\n");

	if ((readl(base + 0xE028) & 0x7) != 0x00)
		dev_warn(priv->dev, "L2 BISR failed\n");

	writel(0x000001ef, base + 0xE074);
	writel(0x00000000, base + 0xE000);
}

static void gpu_sram_power_on(struct gpu_wrapper_priv *priv)
{
	void __iomem *base;
	unsigned int val;

	if (IS_ENABLED(CONFIG_RTK_PLATFORM_FPGA) ||
		IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS))
		return;

	dev_info(priv->dev, "%s\n", __func__);

	base = ioremap(0x98007000, 0x1000);

	writel(0x00000700, base + 0xb70);
	udelay(500);

	val = readl(base + 0xb74);
	if (val != 0x4)
		dev_warn(priv->dev, "%s: 0x98007b74 != 0x4(0x%08x)\n", __func__, val);

	clk_disable(priv->clk);
	reset_control_reset(priv->rstc);
	clk_enable(priv->clk);

	val = readl(base + 0xfd0);
	val &= ~BIT(3);
	val |= BIT(19);
	writel(val, base + 0xfd0);

	iounmap(base);
}

__maybe_unused static void gpu_sram_power_off(struct gpu_wrapper_priv *priv)
{
	void __iomem *base;
	unsigned int val;

	if (IS_ENABLED(CONFIG_RTK_PLATFORM_FPGA) ||
		IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS))
		return;

	dev_info(priv->dev, "%s\n", __func__);

	base = ioremap(0x98007000, 0x1000);

	val = readl(base + 0xfd0);
	val |= BIT(3);
	writel(val, base + 0xfd0);

	writel(0x00000701, base + 0xb70);
	udelay(500);

	val = readl(base + 0xb74);
	if (val != 0x4)
		dev_err(priv->dev, "%s: 0x98007b74 != 0x4(0x%08x)\n", __func__, val);
	iounmap(base);
}

static void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
}

#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)

#define FALLBACK_STATIC_TEMPERATURE 55000

/* Calculate gpu static power example for reference */
static unsigned long arm_model_static_power(unsigned long voltage)
{
	int temperature, temp;
	int temp_squared, temp_cubed, temp_scaling_factor;
	const unsigned long coefficient = (113UL << 20) / (729000000UL >> 10);
	const unsigned long voltage_cubed = (voltage * voltage * voltage) >> 10;
	unsigned long static_power;

	if (priv->tz) {
		int ret;

		ret = priv->tz->ops->get_temp(priv->tz, &temperature);
		if (ret) {
			MALI_DEBUG_PRINT(2, ("Error reading temperature for gpu thermal zone: %d\n", ret));
			temperature = FALLBACK_STATIC_TEMPERATURE;
		}
	} else {
		temperature = FALLBACK_STATIC_TEMPERATURE;
	}

	/* Calculate the temperature scaling factor. To be applied to the
	 * voltage scaled power.
	 */
	temp = temperature / 1000;
	temp_squared = temp * temp;
	temp_cubed = temp_squared * temp;
	temp_scaling_factor =
		(2 * temp_cubed)
		- (80 * temp_squared)
		+ (4700 * temp)
		+ 32000;

	static_power = (((coefficient * voltage_cubed) >> 20)
			* temp_scaling_factor)
		       / 1000000;

	return static_power;
}

/* Calculate gpu dynamic power example for reference */
static unsigned long arm_model_dynamic_power(unsigned long freq,
		unsigned long voltage)
{
	/* The inputs: freq (f) is in Hz, and voltage (v) in mV.
	 * The coefficient (c) is in mW/(MHz mV mV).
	 *
	 * This function calculates the dynamic power after this formula:
	 * Pdyn (mW) = c (mW/(MHz*mV*mV)) * v (mV) * v (mV) * f (MHz)
	 */
	const unsigned long v2 = (voltage * voltage) / 1000; /* m*(V*V) */
	const unsigned long f_mhz = freq / 1000000; /* MHz */
	const unsigned long coefficient = 1000; /* mW/(MHz*mV*mV) */
	unsigned long dynamic_power;

	dynamic_power = (coefficient * v2 * f_mhz) / 1000000; /* mW */

	return dynamic_power;
}

struct devfreq_cooling_power arm_cooling_ops = {
	.get_static_power = arm_model_static_power,
	.get_dynamic_power = arm_model_dynamic_power,
};
#endif

static struct mali_gpu_device_data mali_gpu_data = {
	.max_job_runtime = 1000,
	.shared_mem_size = 1024 * 1024 * 1024,
	.utilization_callback = mali_gpu_utilization_callback,
	.fb_start = 0x0,
	.fb_size = 0xFFFFF000,

	.secure_mode_init = hercules_secure_mode_init,
	.secure_mode_deinit = hercules_secure_mode_deinit,
	.gpu_reset_and_secure_mode_enable = hercules_gpu_reset_and_secure_mode_enable,
	.gpu_reset_and_secure_mode_disable = hercules_gpu_reset_and_secure_mode_disable,
#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	.gpu_cooling_ops = &arm_cooling_ops,
#endif
};

int mali_platform_device_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	dev_info(dev, "[GPU] %s\n", __func__);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->base = of_iomap(np, 0);
	if (!priv->base) {
		dev_err(dev, "of_iomap() failed\n");
		kfree(priv);
		return -ENOMEM;
	}

	if (dev->type)
		dev_err(dev, "dev->type existed: %s\n", dev->type->name);
	else
		dev->type = &gpu_device_type;

	priv->rstc = reset_control_get(dev, NULL);
	if (IS_ERR(priv->rstc)) {
		ret = PTR_ERR(priv->rstc);
		dev_warn(dev, "reset_control_get() failed: %d\n", ret);
		priv->rstc = NULL;
	} else {
		reset_control_deassert(priv->rstc);
	}

	priv->clk = clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		dev_warn(dev, "clk_get() failed: %d\n", ret);
		priv->clk = NULL;
	}

	clk_prepare_enable(priv->clk);
	gpu_sram_power_on(priv);
	enable_l2_bisr(priv);

	/*
	 * Runtime pm will start with runtime resume.
	 * When clk is enable/disable in runtime pm, the first
	 * resume should clk_enable should be skipped to keep
	 * symmetry enable/disable.
	 */
	priv->first_resume = 1;

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

#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	priv->tz = thermal_zone_get_zone_by_name("cpu-thermal");
	if (IS_ERR(priv->tz)) {
		dev_warn(dev, "failed to get thermal_zone: %ld\n",
			PTR_ERR(priv->tz));
		priv->tz = NULL;
	}
#endif

	return 0;
}


int mali_platform_device_deinit(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "[GPU] %s\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(dev);
#endif

	clk_disable_unprepare(priv->clk);
	clk_put(priv->clk);
	if (priv->rstc);
		reset_control_put(priv->rstc);
	iounmap(priv->base);
	kfree(priv);

	return 0;
}


