/*
 * g2227-regulator.c - GMT-G2227 Regulator Driver
 *
 * Copyright (C) 2016-2018 Realtek Semiconductor Corporation
 * Copyright (C) 2016-2018 Cheng-Yu Lee <cylee12@realtek.com>
 * Copyright (C) 2016 Simon Hsu <simon_hsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "g2227: " fmt

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/mfd/g2227.h>
#include <soc/realtek/rtk_cpu.h>
#include <soc/realtek/rtk_regmap.h>
#include "g22xx-regulator.h"

/* regulator id */
enum g2227_regulator_id {
	G2227_ID_DC1 = 0,
	G2227_ID_DC2,
	G2227_ID_DC3,
	G2227_ID_DC4,
	G2227_ID_DC5,
	G2227_ID_DC6,
	G2227_ID_LDO2,
	G2227_ID_LDO3,
	G2227_ID_MAX
};

/* voltage table */
static const unsigned int dcdc1_vtbl[] = {
	3000000, 3100000, 3200000, 3300000,
};

static const unsigned int dcdcx_vtbl[] = {
	 800000,  812500,  825000,  837500,  850000,  862500,  875000,  887500,
	 900000,  912500,  925000,  937500,  950000,  962500,  975000,  987500,
	1000000, 1012500, 1025000, 1037500, 1050000, 1062500, 1075000, 1087500,
	1100000, 1112500, 1125000, 1137500, 1150000, 1162500, 1175000, 1187500,
};

static const unsigned int ldo_vtbl[] = {
	 800000,  850000,  900000,  950000, 1000000, 1100000, 1200000, 1300000,
	1500000, 1600000, 1800000, 1900000, 2500000, 2600000, 3000000, 3100000,
};

#define G2227_DESC(_id, _name, _vtbl, _type)                       \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &g22xx_regulator_ops,               \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = ARRAY_SIZE(_vtbl),                  \
		.volt_table  = _vtbl,                              \
		.id          = G2227_ID_ ## _id,                   \
		.vsel_reg    = G2227_REG_ ## _id ## _NRMVOLT,      \
		.vsel_mask   = G2227_ ## _id ## _NRMVOLT_MASK,     \
		.enable_reg  = G2227_REG_ONOFF,                    \
		.enable_mask = G2227_ ## _id ## _ON_MASK,          \
		.enable_val  = G2227_ ## _id ## _ON_MASK,          \
		.of_map_mode = g22xx_regulator_ ## _type ##_of_map_mode, \
		.of_parse_cb = g22xx_regulator_of_parse_cb,        \
	},                                                         \
	.nmode_reg  = G2227_REG_ ## _id ## _MODE,                  \
	.nmode_mask = G2227_ ## _id ## _NRMMODE_MASK,              \
	.smode_reg  = G2227_REG_ ## _id ## _MODE,                  \
	.smode_mask = G2227_ ## _id ## _SLPMODE_MASK,              \
	.svsel_reg  = G2227_REG_ ## _id ## _SLPVOLT,               \
	.svsel_mask = G2227_ ## _id ## _SLPVOLT_MASK,              \
}

#define G2227_DESC_FIXED_UV(_id, _name, _fixed_uV)                 \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &g22xx_regulator_fixed_uV_ops,      \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = 1,                                  \
		.fixed_uV    = _fixed_uV,                          \
		.id          = G2227_ID_ ## _id,                   \
		.enable_reg  = G2227_REG_ONOFF,                    \
		.enable_mask = G2227_ ## _id ## _ON_MASK,          \
		.enable_val  = G2227_ ## _id ## _ON_MASK,          \
		.of_map_mode = g22xx_regulator_dc_of_map_mode,     \
		.of_parse_cb = g22xx_regulator_of_parse_cb,        \
	},                                                         \
	.nmode_reg  = G2227_REG_ ## _id ## _MODE,                  \
	.nmode_mask = G2227_ ## _id ## _NRMMODE_MASK,              \
	.smode_reg  = G2227_REG_ ## _id ## _MODE,                  \
	.smode_mask = G2227_ ## _id ## _SLPMODE_MASK,              \
}

/* regulator_desc */
static struct g22xx_regulator_desc desc[G2227_ID_MAX] = {
	[G2227_ID_DC1]  = G2227_DESC(DC1,  "dc1",  dcdc1_vtbl, dc),
	[G2227_ID_DC2]  = G2227_DESC(DC2,  "dc2",  dcdcx_vtbl, dc),
	[G2227_ID_DC3]  = G2227_DESC(DC3,  "dc3",  dcdcx_vtbl, dc),
	[G2227_ID_DC4]  = G2227_DESC_FIXED_UV(DC4, "dc4", 0),
	[G2227_ID_DC5]  = G2227_DESC(DC5,  "dc5",  dcdcx_vtbl, dc),
	[G2227_ID_DC6]  = G2227_DESC(DC6,  "dc6",  dcdcx_vtbl, dc),
	[G2227_ID_LDO2] = G2227_DESC(LDO2, "ldo2", ldo_vtbl,   ldo),
	[G2227_ID_LDO3] = G2227_DESC(LDO3, "ldo3", ldo_vtbl,   ldo),
};

/* regmap */
static const struct reg_default g2227_regmap_defaults[] = {
	{ .reg = G2227_REG_ONOFF,            .def = 0xff, },
	{ .reg = G2227_REG_DISCHG,           .def = 0xff, },
	{ .reg = G2227_REG_DC1DC2_MODE,      .def = 0x22, },
	{ .reg = G2227_REG_DC3DC4_MODE,      .def = 0x22, },
	{ .reg = G2227_REG_DC5DC6_MODE,      .def = 0x22, },
	{ .reg = G2227_REG_LDO2LDO3_MODE,    .def = 0x22, },
	{ .reg = G2227_REG_DC2_NRMVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC3_NRMVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC5_NRMVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC1DC6_NRMVOLT,   .def = 0xd0, },
	{ .reg = G2227_REG_LDO2LDO3_NRMVOLT, .def = 0xa2, },
	{ .reg = G2227_REG_DC2_SLPVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC3_SLPVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC5_SLPVOLT,      .def = 0x10, },
	{ .reg = G2227_REG_DC1DC6_SLPVOLT,   .def = 0xd0, },
	{ .reg = G2227_REG_LDO2LDO3_SLPVOLT, .def = 0xa2, },
};

static bool g2227_regmap_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2227_REG_ONOFF ... G2227_REG_LDO2LDO3_MODE:
	case G2227_REG_DC2_NRMVOLT ... G2227_REG_VERSION:
		return true;
	}
	return false;
}

static bool g2227_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2227_REG_ONOFF ... G2227_REG_LDO2LDO3_MODE:
	case G2227_REG_DC2_NRMVOLT ... G2227_REG_LDO2LDO3_SLPVOLT:
		return true;
	}
	return false;
}

static bool g2227_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case G2227_REG_VERSION:
		return true;
	}
	return false;
}

static const struct regmap_config g2227_regmap_config = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = 0x20,
	.cache_type       = REGCACHE_FLAT,
	.reg_defaults     = g2227_regmap_defaults,
	.num_reg_defaults = ARRAY_SIZE(g2227_regmap_defaults),
	.readable_reg     = g2227_regmap_readable_reg,
	.writeable_reg    = g2227_regmap_writeable_reg,
	.volatile_reg     = g2227_regmap_volatile_reg,
};

/* pm */
static int g2227_regulator_suspend(struct device *dev)
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

static const struct dev_pm_ops g2227_regulator_pm_ops = {
	.suspend = g2227_regulator_suspend,
};

static int g2227_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct g22xx_device *gdev;
	struct regmap *regmap;
	int i, ret;
	unsigned int val;
	int chip_rev;

	dev_info(dev, "%s\n", __func__);

	gdev = devm_g22xx_create_device(client, &g2227_regmap_config);
	if (IS_ERR(gdev)) {
		ret = PTR_ERR(gdev);
		dev_err(dev, "g22xx_create_device() returns %d\n", ret);
		return ret;
	}

	chip_rev = get_rtd129x_cpu_revision();
	regmap = gdev->regmap;

	/* workaround */
	regmap_read(regmap, G2227_REG_VERSION, &val);

	/* show version */
	ret = regmap_read(regmap, G2227_REG_VERSION, &val);
	if (ret) {
		dev_err(dev, "failed to read version: %d\n", ret);
		return ret;
	}
	dev_info(dev, "g2227 rev%d\n", val);

	for (i = 0; i < ARRAY_SIZE(desc); i++) {
		ret = g22xx_regulator_register(gdev, &desc[i]);
		if (ret) {
			dev_err(dev, "Failed to register %s: %d\n",
				desc[i].desc.name, ret);
			return ret;
		}

		/* workaround */
		if (desc[i].desc.id == G2227_ID_DC6 &&
			(chip_rev == RTD129x_CHIP_REVISION_A00 ||
			chip_rev == RTD129x_CHIP_REVISION_B00)) {
			struct g22xx_regulator_data *data =
				g22xx_device_get_regulator_data_by_desc(gdev, &desc[i]);

			BUG_ON(!data);
			data->state_mem.enabled = true;
			data->state_mem.disabled = false;
		}
	}

	i2c_set_clientdata(client, gdev);
	g22xx_setup_pm_power_off(gdev, G2227_REG_SYS_CONTROL, G2227_SOFTOFF_MASK);
	return 0;
}

static void g2227_regulator_shutdown(struct i2c_client *client)
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

static const struct i2c_device_id g2227_regulator_ids[] = {
	{"g2227", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, g2227_regulator_ids);

static struct i2c_driver g2227_regulator_driver = {
	.driver = {
		.name = "gmt-g2227",
		.owner = THIS_MODULE,
		.pm = &g2227_regulator_pm_ops,
	},
	.id_table = g2227_regulator_ids,
	.probe    = g2227_regulator_probe,
	.shutdown = g2227_regulator_shutdown,
};
module_i2c_driver(g2227_regulator_driver);

MODULE_DESCRIPTION("GMT G2227 PMIC Driver");
MODULE_AUTHOR("Simon Hsu <simon_hsu@realtek.com>");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL");
