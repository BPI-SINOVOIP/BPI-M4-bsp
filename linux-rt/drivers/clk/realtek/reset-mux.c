/*
 * reset-rtk.c - Realtek reset controller & reset control
 *
 * Copyright (C) 2016-2017 Realtek Semiconductor Corporation
 * Copyright (C) 2016-2017 Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/list.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <soc/realtek/rtk_chip.h>

#define MAX_RESET_MUX_NUM     2

struct mux_entry {
	unsigned long id;
	const char *ref_name;
	struct reset_control *rstc;
};

struct controller_data {
	struct reset_controller_dev rcdev;
	struct device *dev;
	struct mux_entry *muxs;
	u32 mux_size;
	u32 (*eval_mux)(struct controller_data *);
};


#include <dt-bindings/reset/rtk,reset-rtd16xx.h>

#define DEFINE_MUX_ENTRY(_id, _prefix) \
{ .id = _id, .ref_name = _prefix, }

static struct mux_entry rtd16xx_entries[] = {
	DEFINE_MUX_ENTRY(RSTN_MUX_HSE,    "hse"),
	DEFINE_MUX_ENTRY(RSTN_MUX_R2RDSC, "r2rdsc"),
	DEFINE_MUX_ENTRY(RSTN_MUX_EMMC,   "emmc"),
	DEFINE_MUX_ENTRY(RSTN_MUX_NF,     "nf"),
	DEFINE_MUX_ENTRY(RSTN_MUX_MD,     "md"),
	DEFINE_MUX_ENTRY(RSTN_MUX_TPB,    "tpb"),
	DEFINE_MUX_ENTRY(RSTN_MUX_TP,     "tp"),
	DEFINE_MUX_ENTRY(RSTN_MUX_MIPI,   "mipi"),
};

static u32 rtd16xx_eval_mux(struct controller_data *data)
{
	if (get_rtd_chip_revision() == RTD_CHIP_A00)
		return 0;
	return 1;
}

static struct controller_data rtd16xx_mux_controller = {
	.muxs = rtd16xx_entries,
	.mux_size = ARRAY_SIZE(rtd16xx_entries),
	.eval_mux = rtd16xx_eval_mux,
};

static struct mux_entry *find_mux_entry(struct controller_data *data, unsigned long id)
{
	int i;

	for (i = 0; i < data->mux_size; i++)
		if (data->muxs[i].id == id)
			return &data->muxs[i];
	return NULL;
}

enum {
	RESET_MUX_ACTION_ASSERT,
	RESET_MUX_ACTION_DEASSERT,
	RESET_MUX_ACTION_RESET,
	RESET_MUX_ACTION_STATUS,
};

static int reset_mux_action(struct reset_controller_dev *rcdev,
			    unsigned long id,
			    unsigned long action)
{
	struct controller_data *data = container_of(rcdev, struct controller_data, rcdev);
	struct mux_entry *mux = find_mux_entry(data, id);

	if (!mux)
		return -EINVAL;

	if (!mux->rstc) {
		u32 mux_val = 0;
		char rstc_name[40];
		int ret;

		if (data->eval_mux)
			mux_val = data->eval_mux(data);

		snprintf(rstc_name, sizeof(rstc_name), "%s%u", mux->ref_name, mux_val);
		mux->rstc = devm_reset_control_get(data->dev, rstc_name);
		if (IS_ERR(mux->rstc)) {
			ret = PTR_ERR(mux->rstc);
			mux->rstc = NULL;
			dev_err(data->dev, "failed to get reset control for %ld/%s%u, error code is %d\n",
				id, mux->ref_name, mux_val, ret);
			return ret;
		}
	}

	switch (action) {
	case RESET_MUX_ACTION_ASSERT:
		return reset_control_assert(mux->rstc);
	case RESET_MUX_ACTION_DEASSERT:
		return reset_control_deassert(mux->rstc);
	case RESET_MUX_ACTION_RESET:
		return reset_control_reset(mux->rstc);
	case RESET_MUX_ACTION_STATUS:
		return reset_control_status(mux->rstc);
	}
	return -EINVAL;
}

static int reset_mux_assert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return reset_mux_action(rcdev, id, RESET_MUX_ACTION_ASSERT);
}

static int reset_mux_deassert(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return reset_mux_action(rcdev, id, RESET_MUX_ACTION_DEASSERT);
}

static int reset_mux_reset(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return reset_mux_action(rcdev, id, RESET_MUX_ACTION_RESET);
}

static int reset_mux_status(struct reset_controller_dev *rcdev,
		unsigned long id)
{
	return reset_mux_action(rcdev, id, RESET_MUX_ACTION_STATUS);
}

static struct reset_control_ops reset_mux_ops = {
	.assert   = reset_mux_assert,
	.deassert = reset_mux_deassert,
	.reset    = reset_mux_reset,
	.status   = reset_mux_status,
};

static const struct of_device_id reset_mux_match[] = {
	{ .compatible = "realtek,rtd16xx-mux-reset-controller", .data = &rtd16xx_mux_controller, },
	{}
};

static int reset_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct controller_data *data;
	const struct of_device_id *id;
	int ret;

	dev_info(dev, "%s\n", __func__);

	id = of_match_node(reset_mux_match, np);
	if (!id)
		return -EINVAL;
	data = (struct controller_data *)id->data;

	data->dev = dev;
	data->rcdev.owner = THIS_MODULE;
	data->rcdev.ops = &reset_mux_ops;
	data->rcdev.of_node = np;
	data->rcdev.of_reset_n_cells = 1;
	data->rcdev.nr_resets = data->mux_size;

	ret = reset_controller_register(&data->rcdev);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, data);

	return 0;
}


static struct platform_driver reset_mux_driver = {
	.probe = reset_mux_probe,
	.driver = {
		.name           = "reset-mux",
		.of_match_table = reset_mux_match,
	},
};

static int __init reset_mux_init(void)
{
	return platform_driver_register(&reset_mux_driver);
}
core_initcall(reset_mux_init);


