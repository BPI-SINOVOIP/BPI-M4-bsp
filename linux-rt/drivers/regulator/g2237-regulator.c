/*
 * g2237-regulator.c - GMT-G2237 Regulator driver
 *
 * Copyright (C) 2017-2018 Realtek Semiconductor Corporation
 * Copyright (C) 2017-2018 Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "g2237: " fmt

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/mfd/g2237.h>
#include "g22xx-regulator.h"

/* regulator id */
enum {
	G2237_ID_DC1 = 0,
	G2237_ID_DC2,
	G2237_ID_DC3,
	G2237_ID_DC4,
	G2237_ID_DC5,
	G2237_ID_LDO1,
	G2237_ID_MAX
};

/* voltage table */
static const unsigned int dc1_vtbl[] = {
	2200000, 2300000, 2400000, 2500000,
	2600000, 2700000, 2800000, 2900000,
	3000000, 3100000, 3200000, 3300000,
	3400000, 3500000, 3600000, 3700000,
};

static const unsigned int dcx_vtbl[] = {
	 800000,  812500,  825000,  837500,
	 850000,  862500,  875000,  887500,
	 900000,  912500,  925000,  937500,
	 950000,  962500,  975000,  987500,
	1000000, 1012500, 1025000, 1037500,
	1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500,
	1150000, 1162500, 1175000, 1187500,
};

static const unsigned int dc5_vtbl[] = {
	 800000,  850000,  900000,  950000,
	1000000, 1050000, 1100000, 1200000,
	1300000, 1500000, 1600000, 1700000,
	1800000, 1900000, 2000000, 2500000,
};

static const unsigned int ldo1_vtbl[] = {
	2200000, 2300000, 2400000, 2500000,
	2600000, 2700000, 2800000, 2900000,
	3000000, 3000000, 3000000, 3000000,
	3000000, 3000000, 3000000, 3000000,
};

#define G2237_DESC(_id, _name, _vtbl, _type)                       \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &g22xx_regulator_ops,               \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = ARRAY_SIZE(_vtbl),                  \
		.volt_table  = _vtbl,                              \
		.id          = G2237_ID_ ## _id,                   \
		.vsel_reg    = G2237_REG_ ## _id ## _NRMVOLT,      \
		.vsel_mask   = G2237_ ## _id ## _NRMVOLT_MASK,     \
		.enable_reg  = G2237_REG_ONOFF,                    \
		.enable_mask = G2237_ ## _id ## _ON_MASK,          \
		.enable_val  = G2237_ ## _id ## _ON_MASK,          \
		.of_map_mode = g22xx_regulator_ ## _type ##_of_map_mode, \
		.of_parse_cb = g22xx_regulator_of_parse_cb,        \
	},                                                         \
	.nmode_reg  = G2237_REG_ ## _id ## _MODE,                  \
	.nmode_mask = G2237_ ## _id ## _NRMMODE_MASK,              \
	.smode_reg  = G2237_REG_ ## _id ## _MODE,                  \
	.smode_mask = G2237_ ## _id ## _SLPMODE_MASK,              \
	.svsel_reg  = G2237_REG_ ## _id ## _SLPVOLT,               \
	.svsel_mask = G2237_ ## _id ## _SLPVOLT_MASK,              \
}

#define G2237_DESC_FIXED_UV(_id, _name, _fixed_uV)                 \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &g22xx_regulator_fixed_uV_ops,      \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = 1,                                  \
		.fixed_uV    = _fixed_uV,                          \
		.id          = G2237_ID_ ## _id,                   \
		.enable_reg  = G2237_REG_ONOFF,                    \
		.enable_mask = G2237_ ## _id ## _ON_MASK,          \
		.enable_val  = G2237_ ## _id ## _ON_MASK,          \
		.of_map_mode = g22xx_regulator_dc_of_map_mode,     \
		.of_parse_cb = g22xx_regulator_of_parse_cb,        \
	},                                                         \
	.nmode_reg  = G2237_REG_ ## _id ## _MODE,                  \
	.nmode_mask = G2237_ ## _id ## _NRMMODE_MASK,              \
	.smode_reg  = G2237_REG_ ## _id ## _MODE,                  \
	.smode_mask = G2237_ ## _id ## _SLPMODE_MASK,              \
}

/* regulator_desc */
static struct g22xx_regulator_desc desc[G2237_ID_MAX] = {
	[G2237_ID_DC1]  = G2237_DESC(DC1,  "dc1",  dc1_vtbl, dc),
	[G2237_ID_DC2]  = G2237_DESC(DC2,  "dc2",  dcx_vtbl, dc),
	[G2237_ID_DC3]  = G2237_DESC(DC3,  "dc3",  dcx_vtbl, dc),
	[G2237_ID_DC4]  = G2237_DESC_FIXED_UV(DC4, "dc4", 0),
	[G2237_ID_DC5]  = G2237_DESC(DC5,  "dc5",  dc5_vtbl, dc),
	[G2237_ID_LDO1] = G2237_DESC(LDO1, "ldo1", ldo1_vtbl,  ldo),
};

/* regmap */
static const struct reg_default g2237_reg_defaults[] = {
	{ .reg = G2237_REG_ONOFF,        .def = 0xF9, },
	{ .reg = G2237_REG_DISCHG,       .def = 0xF9, },
	{ .reg = G2237_REG_DC1DC2_MODE,  .def = 0x22, },
	{ .reg = G2237_REG_DC3DC4_MODE,  .def = 0x22, },
	{ .reg = G2237_REG_DC5LDO1_MODE, .def = 0x22, },
	{ .reg = G2237_REG_DC1_NRMVOLT,  .def = 0x0B, },
	{ .reg = G2237_REG_DC2_NRMVOLT,  .def = 0x10, },
	{ .reg = G2237_REG_DC3_NRMVOLT,  .def = 0x10, },
	{ .reg = G2237_REG_DC5_NRMVOLT,  .def = 0x0C, },
	{ .reg = G2237_REG_LDO1_NRMVOLT, .def = 0x03, },
	{ .reg = G2237_REG_DC1_SLPVOLT,  .def = 0x0B, },
	{ .reg = G2237_REG_DC2_SLPVOLT,  .def = 0x10, },
	{ .reg = G2237_REG_DC3_SLPVOLT,  .def = 0x10, },
	{ .reg = G2237_REG_DC5_SLPVOLT,  .def = 0x0C, },
	{ .reg = G2237_REG_LDO1_SLPVOLT, .def = 0x03, },
};

static bool g2237_regmap_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2237_REG_ONOFF ... G2237_REG_VERSION:
		return true;
	}
	return false;
}

static bool g2237_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2237_REG_ONOFF ... G2237_REG_LDO1_SLPVOLT:
		return true;
	}
	return false;
}

static bool g2237_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2237_REG_CHIP_ID:
	case G2237_REG_VERSION:
		return true;
	}
	return false;
}

static const struct regmap_config g2237_regmap_config = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = 0x15,
	.cache_type       = REGCACHE_FLAT,
	.reg_defaults     = g2237_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(g2237_reg_defaults),
	.readable_reg     = g2237_regmap_readable_reg,
	.writeable_reg    = g2237_regmap_writeable_reg,
	.volatile_reg     = g2237_regmap_volatile_reg,
};

/* pm */
static int g2237_regulator_suspend(struct device *dev)
{
	struct g22xx_device *gdev = dev_get_drvdata(dev);
	struct g22xx_regulator_data *pos;

#ifdef CONFIG_SUSPEND
	if (RTK_PM_STATE == PM_SUSPEND_STANDBY)
		goto done;
#endif

	dev_info(dev, "Enter %s\n", __func__);
	list_for_each_entry(pos, &gdev->list, list)
		g22xx_prepare_suspend_state(pos->rdev, 0);
	regulator_suspend_prepare(PM_SUSPEND_MEM);
	dev_info(dev, "Exit %s\n", __func__);
done:
	return 0;
}

static const struct dev_pm_ops g2237_regulator_pm_ops = {
	.suspend = g2237_regulator_suspend,
};

static int g2237_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct g22xx_device *gdev;
	int i, ret;
	unsigned int chip_id, rev;

	dev_info(dev, "%s\n", __func__);

	gdev = devm_g22xx_create_device(client, &g2237_regmap_config);
	if (IS_ERR(gdev)) {
		ret = PTR_ERR(gdev);
		dev_err(dev, "g22xx_create_device() returns %d\n", ret);
		return ret;
	}

	ret = regmap_read(gdev->regmap, G2237_REG_CHIP_ID, &chip_id);
	if (ret) {
		dev_err(dev, "failed to read chip_id: %d\n", ret);
		return ret;
	}
	if (chip_id != 0x25) {
		dev_err(dev, "chip_id(%02x) not match\n", chip_id);
		return -EINVAL;
	}
	regmap_read(gdev->regmap, G2237_REG_VERSION, &rev);
	dev_info(dev, "g2237(%02x) rev%d\n", chip_id, rev);

	for (i = 0; i < ARRAY_SIZE(desc); i++) {
		ret = g22xx_regulator_register(gdev, &desc[i]);
		if (ret) {
			dev_err(dev, "Failed to register %s: %d\n",
				desc[i].desc.name, ret);
			return ret;
		}
	}

	i2c_set_clientdata(client, gdev);
	g22xx_setup_pm_power_off(gdev, G2237_REG_SYS_CONTROL, G2237_SOFTOFF_MASK);
	return 0;
}

static void g2237_regulator_shutdown(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct g22xx_device *gdev = i2c_get_clientdata(client);
	struct g22xx_regulator_data *pos;

	dev_info(dev, "Enter %s\n", __func__);
	list_for_each_entry(pos, &gdev->list, list)
		g22xx_prepare_suspend_state(pos->rdev, 1);
	regulator_suspend_prepare(PM_SUSPEND_MEM);
	dev_info(dev, "Exit %s\n", __func__);
}

static const struct i2c_device_id g2237_regulator_ids[] = {
	{"g2237", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, g2237_regulator_ids);

struct i2c_driver g2237_regulator_driver = {
	.driver = {
		.name = "gmt-g2237",
		.owner = THIS_MODULE,
		.pm = &g2237_regulator_pm_ops,
	},
	.id_table = g2237_regulator_ids,
	.probe    = g2237_regulator_probe,
	.shutdown = g2237_regulator_shutdown,
};
module_i2c_driver(g2237_regulator_driver);
EXPORT_SYMBOL_GPL(g2237_regulator_driver);

MODULE_DESCRIPTION("GMT G2237 PMIC Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL");

