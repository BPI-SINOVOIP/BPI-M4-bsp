/*
 * g22xx-regulator.h - GMT-G22xx series Regulator
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 * Copyright (C) 2017 Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __G22XX_REGULATOR_H__
#define __G22XX_REGULATOR_H__

#include <linux/i2c.h>
#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <soc/realtek/rtk_regmap.h>
#include <dt-bindings/regulator/gmt,g22xx.h>

struct g22xx_device {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct list_head list;

	int g2237_int_gpio;
	struct delayed_work work;
	struct input_dev *input_dev;
};

struct g22xx_regulator_desc {
	struct regulator_desc desc;
	u8 nmode_reg;
	u8 nmode_mask;
	u8 smode_reg;
	u8 smode_mask;
	u8 svsel_reg;
	u8 svsel_mask;
};

struct g22xx_regulator_data {
	struct list_head list;
	struct regulator_dev *rdev;
	struct g22xx_regulator_desc *gd;

	struct regmap_field *svsel;
	struct regmap_field *nmode;
	struct regmap_field *smode;

	struct regulator_state state_mem;
	struct regulator_state state_coldboot;

	u32 fixed_uV;
};

#define to_g22xx_regulator_desc(_desc) container_of(_desc, struct g22xx_regulator_desc, desc)


static inline struct g22xx_device *devm_g22xx_create_device(struct i2c_client *client,
	const struct regmap_config *config)
{
	struct device *dev = &client->dev;
	struct g22xx_device *gdev;

	gdev = devm_kzalloc(dev, sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return ERR_PTR(-ENOMEM);

	gdev->regmap = devm_rtk_regmap_init_i2c(client, config);
	if (IS_ERR(gdev->regmap))
		return ERR_CAST(gdev->regmap);

	gdev->client = client;
	gdev->dev    = dev;
	INIT_LIST_HEAD(&gdev->list);

	return gdev;
}


extern const struct regulator_ops g22xx_regulator_ops;
extern const struct regulator_ops g22xx_regulator_fixed_uV_ops;

int g22xx_regulator_of_parse_cb(struct device_node *np,
	const struct regulator_desc *desc, struct regulator_config *config);
unsigned int g22xx_regulator_dc_of_map_mode(unsigned int mode);
unsigned int g22xx_regulator_ldo_of_map_mode(unsigned int mode);
void g22xx_prepare_suspend_state(struct regulator_dev *rdev, int is_coldboot);
struct g22xx_regulator_data *g22xx_device_get_regulator_data_by_desc(
        struct g22xx_device *gdev, struct g22xx_regulator_desc *gd);
int g22xx_regulator_register(struct g22xx_device *gdev, struct g22xx_regulator_desc *gd);

static inline int g22xx_regulator_type_is_ldo(struct g22xx_regulator_desc *gd)
{
	return gd->desc.of_map_mode == g22xx_regulator_ldo_of_map_mode;
}


/* pm_power_off setup */
int g22xx_setup_pm_power_off(struct g22xx_device *gdev, u32 reg, u32 mask);

#endif
