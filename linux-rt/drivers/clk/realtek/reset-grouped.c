/*
 * reset-grouped.c - Realtek Grouped Reset Controller
 *
 * Copyright (C) 2019 Realtek Semiconductor Corporation
 *
 * Author:
 *	Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/realtek/rtk_mmio.h>
#include <dt-bindings/reset/rtk,reset.h>
#include <soc/realtek/rtk_chip.h>

#include "common.h"

#define GRC_DESC_ID(_id)           ((_id >> 8) - 1)
#define GRC_RESET_ID(_id)          ((_id) & 0xff)
#define GRC_RESET_BIT_MASK(_id)    BIT((_id) & 0xff)

struct grc_reg_desc {
	u32 offset;
	u32 pm_ignore_bits;
	u32 pm_data;
	u32 write_en:1;
};

struct grc_data {
	struct device *dev;
	struct reset_controller_dev rcdev;
	struct grc_reg_desc *desc;
	u32 num_desc;
	u32 chip_rev;
	unsigned long (*id_xlate)(struct grc_data *data,
				  unsigned long id);
	/* register */
	void *reg;

	/* mmio regmap of reg */
	struct regmap *regmap;
	int offset;
};
#define to_grc_data(_p) container_of((_p), struct grc_data, rcdev)

#ifdef CONFIG_ARCH_RTD16xx

#include <dt-bindings/reset/rtk,reset-rtd16xx.h>

static struct grc_reg_desc rtd16xx_desc[] = {
	{ .offset = 0x00, .write_en = 1, },
	{ .offset = 0x04, .write_en = 1, },
	{ .offset = 0x08, .write_en = 1, .pm_ignore_bits = 0x50150000, },
	{ .offset = 0x0c, .write_en = 1, .pm_ignore_bits = 0x40000000, },
	{ .offset = 0x10, .write_en = 1, .pm_ignore_bits = 0x00000001, },
	{ .offset = 0x14, .write_en = 1, .pm_ignore_bits = 0x00140014, },
	{ .offset = 0x68, .write_en = 1, },
};

static unsigned long rtd16xx_id_xlate(struct grc_data *data,
				      unsigned long id)
{
	if (data->chip_rev != RTD_CHIP_A00) {
		switch (id) {
		case RSTN_HSE:
			return RSTN_HSE_2;
		case RSTN_R2RDSC:
			return RSTN_R2RDSC_2;
		case RSTN_EMMC:
			return RSTN_EMMC_2;
		case RSTN_NF:
			return RSTN_NF_2;
		case RSTN_MD:
			return RSTN_MD_2;
		case RSTN_TPB:
			return RSTN_TPB_2;
		case RSTN_TP:
			return RSTN_TP_2;
		case RSTN_MIPI:
			return RSTN_MIPI_2;
		}
	}
	return id;
}


#define PLATFORM_GRC_DATA     (&rtd16xx_grc_data)
static struct grc_data rtd16xx_grc_data = {
	.desc = rtd16xx_desc,
	.num_desc = ARRAY_SIZE(rtd16xx_desc),
	.id_xlate = rtd16xx_id_xlate,
};

#elif defined (CONFIG_ARCH_RTD13xx)

static struct grc_reg_desc rtd13xx_desc[] = {
	{ .offset = 0x00, .write_en = 1, },
	{ .offset = 0x04, .write_en = 1, },
	{ .offset = 0x08, .write_en = 1, .pm_ignore_bits = 0x50150000, },
	{ .offset = 0x0c, .write_en = 1, .pm_ignore_bits = 0x40000000, },
	{ .offset = 0x10, .write_en = 1, .pm_ignore_bits = 0x00000001, },
	{ .offset = 0x14, .write_en = 1, .pm_ignore_bits = 0x00140014, },
	{ .offset = 0x68, .write_en = 1, },
	{ .offset = 0x88, .write_en = 1, },
	{ .offset = 0x90, .write_en = 1, },
	{ .offset = 0x94, .write_en = 1, },
	{ .offset = 0x454, },
	{ .offset = 0x458, },
};

#define PLATFORM_GRC_DATA     (&rtd13xx_grc_data)
static struct grc_data rtd13xx_grc_data = {
	.desc = rtd13xx_desc,
	.num_desc = ARRAY_SIZE(rtd13xx_desc),
};

#endif

static inline u32 bits_to_set(struct grc_reg_desc *reg, u32 bits_to_set)
{
	return bits_to_set * (reg->write_en ? 3 : 1);
}

static inline u32 bits_to_clear(struct grc_reg_desc *reg, u32 bits_to_clear)
{
	return reg->write_en ? (bits_to_clear << 1) : 0;
}

static inline u32 bits_to_mask(struct grc_reg_desc *reg, u32 bits)
{
	return bits_to_set(reg, bits);
}

static inline u32 rtk_grc_read(struct grc_data *data,
			      struct grc_reg_desc *reg)
{
	u32 val = 0;

	if (data->regmap)
		regmap_read(data->regmap, data->offset + reg->offset, &val);
	else if (data->reg)
		val = readl(data->reg + reg->offset);
	else
		WARN_ON_ONCE(1);
	return val;
}

static inline void rtk_grc_update_bits(struct grc_data *data,
				      struct grc_reg_desc *reg,
				      u32 mask,
				      u32 val)
{
	dev_dbg(data->dev, "%s: offset=%03x: flags=%c, mask=%08x, val=%08x\n",
		__func__, reg->offset, reg->write_en ? 'w' : '-' , mask, val);
	if (data->regmap) {
		regmap_update_bits(data->regmap, data->offset + reg->offset, mask, val);
	} else if (data->reg) {
		unsigned int rval;

		rval = readl(data->reg + reg->offset);
		rval = (rval & ~mask) | (val & mask);
		writel(rval, data->reg + reg->offset);
	} else
		WARN_ON_ONCE(1);
}

static int rtk_grc_assert(struct reset_controller_dev *rcdev,
			 unsigned long id)
{
	struct grc_data *data = to_grc_data(rcdev);
	struct grc_reg_desc *reg = &data->desc[GRC_DESC_ID(id)];
	u32 val, mask;

	mask = bits_to_mask(reg, GRC_RESET_BIT_MASK(id));
	val = bits_to_clear(reg, GRC_RESET_BIT_MASK(id));
	rtk_grc_update_bits(data, reg, mask, val);
	return 0;
}

static int rtk_grc_deassert(struct reset_controller_dev *rcdev,
			   unsigned long id)
{
	struct grc_data *data = to_grc_data(rcdev);
	struct grc_reg_desc *reg = &data->desc[GRC_DESC_ID(id)];
	u32 val, mask;

	mask = bits_to_mask(reg, GRC_RESET_BIT_MASK(id));
	val = bits_to_set(reg, GRC_RESET_BIT_MASK(id));
	rtk_grc_update_bits(data, reg, mask, val);
	return 0;
}

static int rtk_grc_reset(struct reset_controller_dev *rcdev,
			unsigned long id)
{
	int ret;

	ret = rtk_grc_assert(rcdev, id);
	if (ret)
		return ret;
	return rtk_grc_deassert(rcdev, id);
}

static int rtk_grc_status(struct reset_controller_dev *rcdev,
			 unsigned long id)
{
	struct grc_data *data = to_grc_data(rcdev);
	struct grc_reg_desc *reg = &data->desc[GRC_DESC_ID(id)];
	u32 val = 0;

	val = rtk_grc_read(data, reg);
	return !(val & GRC_RESET_BIT_MASK(id));
}

static struct reset_control_ops rtk_grc_ops = {
	.assert   = rtk_grc_assert,
	.deassert = rtk_grc_deassert,
	.reset    = rtk_grc_reset,
	.status   = rtk_grc_status,
};

static int rtk_grc_suspend(struct device *dev)
{
	struct grc_data *data = dev_get_drvdata(dev);
	struct grc_reg_desc *reg;
	int i;

	dev_info(dev, "enter %s\n", __func__);
	for (i = 0; i < data->num_desc; i++) {
		reg = &data->desc[i];

		reg->pm_data = rtk_grc_read(data, reg);
		dev_info(dev, "save: reg=%03x, val=%08x\n",
			reg->offset, reg->pm_data);
	}
	dev_info(dev, "exit %s\n", __func__);
	return 0;
}

static int rtk_grc_resume(struct device *dev)
{
	struct grc_data *data = dev_get_drvdata(dev);
	struct grc_reg_desc *reg;
	u32 mask, val;
	int i;

	dev_info(dev, "enter %s\n", __func__);
	for (i = 0; i < data->num_desc; i++) {
		reg = &data->desc[i];

		dev_info(dev, "restore: reg=%03x, val=%08x\n",
			reg->offset, reg->pm_data);

		val = reg->pm_data | (reg->write_en ? 0xaaaaaaaa : 0);
		mask = ~bits_to_mask(reg, reg->pm_ignore_bits);

		dev_info(dev, "update_bits: reg=%03x, mask=%08x, val=%08x\n",
			reg->offset, mask, val);
		rtk_grc_update_bits(data, reg, mask, val);

		dev_info(dev, "final_val: reg=%03x, val=%08x\n",
			reg->offset, rtk_grc_read(data, reg));
	}
	dev_info(dev, "exit %s\n", __func__);
	return 0;
}

static const struct dev_pm_ops rtk_grc_pm_ops = {
	.suspend_noirq = rtk_grc_suspend,
	.resume_noirq = rtk_grc_resume,
};

static int rtk_grc_of_reset_xlate(struct reset_controller_dev *rcdev,
				  const struct of_phandle_args *reset_spec)
{
	struct grc_data *data = to_grc_data(rcdev);
	int val;

	val = reset_spec->args[0];
	if (val >= rcdev->nr_resets)
		return -EINVAL;

	if (data->id_xlate)
		return data->id_xlate(data, val);
	return val;
}

static int rtk_grc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct grc_data *data = PLATFORM_GRC_DATA;
	struct regmap *regmap;
	void *reg;
	int offset = 0;
	int ret;

	reg = of_iomap(np, 0);
	regmap = of_get_rtk_mmio_regmap_with_offset(np, 0, &offset);
	if (IS_ERR(regmap))
		regmap = NULL;
	if (!reg && IS_ERR_OR_NULL(regmap))
		return -EINVAL;

	data->dev = dev;
	data->reg = reg;
	data->chip_rev = get_rtd_chip_revision();

	if (regmap) {
		data->regmap = regmap;
		data->offset = offset;
		dev_info(dev, "use mmio regmap\n");
	}

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.ops = &rtk_grc_ops;
	data->rcdev.of_node = np;
	data->rcdev.nr_resets = 0xffff;
	data->rcdev.of_reset_n_cells = 1;
	data->rcdev.of_xlate = rtk_grc_of_reset_xlate;
	ret = reset_controller_register(&data->rcdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, data);
	dev_info(dev, "initialized\n");

	return ret;
}

static const struct of_device_id rtk_grc_match[] = {
	{ .compatible = "realtek,grouped-reset-controller", },
	{}
};

static struct platform_driver rtk_grc_driver = {
	.probe = rtk_grc_probe,
	.driver = {
		.name   = "rtk-reset-grouped",
		.of_match_table = rtk_grc_match,
		.pm = &rtk_grc_pm_ops,
	},
};

static int __init rtk_grc_init(void)
{
	return platform_driver_register(&rtk_grc_driver);
}
core_initcall(rtk_grc_init);

