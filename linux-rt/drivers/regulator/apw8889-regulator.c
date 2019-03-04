 /*
 * apw8889-regulator.c - Anpec APW8889 Regulator driver
 *
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Copyright (C) 2018 Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "apw8889: " fmt

#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/mfd/apw8889.h>
#include <soc/realtek/rtk_regmap.h>
#include <dt-bindings/regulator/anpec,apw8889.h>

struct apw8889_device {
	struct device *dev;
	struct list_head list;
	struct regmap *regmap;
};

struct apw8889_regulator_desc {
	struct regulator_desc desc;
	u8 nmode_reg;
	u8 nmode_mask;
	u8 smode_reg;
	u8 smode_mask;
	u8 svsel_reg;
	u8 svsel_mask;
	u8 clamp_reg;
	u8 clamp_mask;
};

struct apw8889_regulator_data {
	struct list_head list;
	struct regulator_dev *rdev;
	struct apw8889_regulator_desc *desc;

	struct regmap_field *svsel;
	struct regmap_field *smode;
	struct regmap_field *nmode;
	struct regmap_field *clamp;

	struct regulator_state state_mem;
	struct regulator_state state_coldboot;

	u32 fixed_uV;
};

/* of_parse_cb */
static int apw8889_regulator_of_parse_cb(struct device_node *np,
	const struct regulator_desc *desc, struct regulator_config *config)
{
	struct apw8889_regulator_data *data = config->driver_data;
	struct device_node *child;
	unsigned int val;

	child = of_get_child_by_name(np, "regulator-state-coldboot");
	if (!child)
		child = of_get_child_by_name(np, "regulator-state-mem");
	if (child) {
		struct regulator_state *state = &data->state_coldboot;

		if (of_property_read_bool(child, "regulator-on-in-suspend"))
			state->enabled = true;

		if (of_property_read_bool(child, "regulator-off-in-suspend"))
			state->disabled = true;

		if (!of_property_read_u32(child, "regulator-suspend-microvolt",
			&val))
			state->uV = val;
	}

	if (desc->n_voltages == 1 && desc->fixed_uV == 0) {
		u32 min = 0, max = 0;

		of_property_read_u32(np, "regulator-min-microvolt", &min);
		of_property_read_u32(np, "regulator-max-microvolt", &max);
		pr_err("%s: %s: %u-%u\n",__func__, desc->name,  min, max);
		WARN_ON(min != max);
		data->fixed_uV = max;
	}

	return 0;
}

static unsigned int apw8889_regulator_dc_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case APW8889_DC_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	default:
		break;
	}
	return REGULATOR_MODE_NORMAL;
}

static unsigned int apw8889_regulator_ldo_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case APW8889_LDO_MODE_ECO:
		return REGULATOR_MODE_IDLE;
	default:
		break;
	}
	return REGULATOR_MODE_NORMAL;
}

static inline int apw8889_regulator_type_is_ldo(struct apw8889_regulator_desc *gd)
{
	return gd->desc.of_map_mode == apw8889_regulator_ldo_of_map_mode;
}

/* regulator_ops */
static int apw8889_regulator_set_mode_regmap(struct regulator_dev *rdev,
	unsigned int mode)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);
	struct apw8889_regulator_desc *gd = data->desc;
	unsigned int val = 0;

	dev_dbg(rdev_get_dev(rdev), "%s\n", __func__);
	if (!data->nmode)
		return -EINVAL;

	if (apw8889_regulator_type_is_ldo(gd))
		val = (mode & REGULATOR_MODE_IDLE) ? 2 : 0;
	else
		val =  (mode & REGULATOR_MODE_FAST) ? 2 : 0;

	dev_dbg(rdev_get_dev(rdev), "%s: val=%02x\n", __func__, val);
	return regmap_field_write(data->nmode, val);
}

static unsigned int apw8889_regulator_get_mode_regmap(struct regulator_dev *rdev)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);
	struct apw8889_regulator_desc *gd = data->desc;
	unsigned int val;
	int ret;

	dev_dbg(rdev_get_dev(rdev), "%s\n", __func__);
	if (!data->nmode)
		return -EINVAL;

	ret = regmap_field_read(data->nmode, &val);
	if (ret)
		return 0;

	dev_dbg(rdev_get_dev(rdev), "%s: val=%02x\n", __func__, val);

	if (apw8889_regulator_type_is_ldo(gd) && val == 2)
		return REGULATOR_MODE_IDLE;
	else if (val == 2)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}


static int apw8889_set_suspend_enable(struct regulator_dev *rdev)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);

	dev_dbg(rdev_get_dev(rdev), "%s\n", __func__);
	regmap_field_write(data->smode, 0x2);
	return 0;
}

static int apw8889_set_suspend_disable(struct regulator_dev *rdev)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);

	dev_dbg(rdev_get_dev(rdev), "%s\n", __func__);
	if (!data->smode)
		return -EINVAL;
	regmap_field_write(data->smode, 0x3);
	return 0;
}

static int apw8889_set_suspend_voltage(struct regulator_dev *rdev,
	int uV)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);
	int vsel;

	dev_dbg(rdev_get_dev(rdev), "%s\n", __func__);
	vsel = regulator_map_voltage_iterate(rdev, uV, uV);
	if (vsel < 0)
		return -EINVAL;

	if (!data->smode || !data->svsel)
		return -EINVAL;

	regmap_field_write(data->smode, 0x2);
	regmap_field_write(data->svsel, vsel);
	return 0;
}

static int apw8889_regulator_get_voltage_fixed(struct regulator_dev *rdev)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);

	if (data->fixed_uV)
		return data->fixed_uV;
	return -EINVAL;
}

static int apw8889_regulator_set_clamp_en(struct regulator_dev *rdev,
					  int enable)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);

	if (!data->clamp)
		return -EINVAL;
	dev_dbg(rdev_get_dev(rdev), "%s: set clamp_en val=%x\n",
		__func__, enable);
	return regmap_field_write(data->clamp, enable);
}

static int apw8889_regulator_get_clamp_en(struct regulator_dev *rdev)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);
	unsigned int enable;
	int ret;

	if (!data->clamp)
		return -EINVAL;
	ret = regmap_field_read(data->clamp, &enable);
	if (!ret)
		dev_dbg(rdev_get_dev(rdev), "%s: get clamp_en val=%x\n",
			__func__, enable);
	return ret ? ret : enable;
}

static int apw8889_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev,
						    unsigned sel)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);

	if (data->clamp)
		apw8889_regulator_set_clamp_en(rdev, sel < 0x34);
	return regulator_set_voltage_sel_regmap(rdev, sel);
}

static const struct regulator_ops apw8889_regulator_ops = {
	.list_voltage         = regulator_list_voltage_linear,
	.map_voltage          = regulator_map_voltage_linear,
	.set_voltage_sel      = apw8889_regulator_set_voltage_sel_regmap,
	.get_voltage_sel      = regulator_get_voltage_sel_regmap,
	.enable               = regulator_enable_regmap,
	.disable              = regulator_disable_regmap,
	.is_enabled           = regulator_is_enabled_regmap,
	.get_mode             = apw8889_regulator_get_mode_regmap,
	.set_mode             = apw8889_regulator_set_mode_regmap,
	.set_suspend_voltage  = apw8889_set_suspend_voltage,
	.set_suspend_enable   = apw8889_set_suspend_enable,
	.set_suspend_disable  = apw8889_set_suspend_disable,
};

static const struct regulator_ops apw8889_regulator_fixed_uV_ops = {
	.enable               = regulator_enable_regmap,
	.disable              = regulator_disable_regmap,
	.is_enabled           = regulator_is_enabled_regmap,
	.get_mode             = apw8889_regulator_get_mode_regmap,
	.set_mode             = apw8889_regulator_set_mode_regmap,
	.set_suspend_voltage  = apw8889_set_suspend_voltage,
	.set_suspend_enable   = apw8889_set_suspend_enable,
	.set_suspend_disable  = apw8889_set_suspend_disable,
	.get_voltage          = apw8889_regulator_get_voltage_fixed,
};

/* helper function */
/*
 * apw8889_prepare_suspend_state - prepare apw8889 suspend state, copy the correct
 *                               state into state_mem of decs, then call
 *                               regulator_suspend_prepare() to do regulator
 *                               suspend.
 *
 * @rdev:        regulator device
 * @is_coldboot: state selection
 */
static void apw8889_prepare_suspend_state(struct regulator_dev *rdev,
	int is_coldboot)
{
	struct apw8889_regulator_data *data = rdev_get_drvdata(rdev);
	struct regulator_state *state;

	state = &rdev->constraints->state_mem;
	*state = is_coldboot ? data->state_coldboot : data->state_mem;

	dev_info(rdev_get_dev(rdev), "%s: state={enabled=%d, disabled=%d, mode=%d, uV=%d}, is_coldboot=%d\n",
		__func__, state->enabled, state->disabled, state->mode,
		state->uV, is_coldboot);
}

static struct regmap_field *create_regmap_field(struct apw8889_device *adev,
	u32 reg, u32 mask)
{
	u32 msb = fls(mask) - 1;
	u32 lsb = ffs(mask) - 1;
	struct reg_field map = REG_FIELD(reg, lsb, msb);
	struct regmap_field *rmap;

	if (reg == 0 && mask == 0)
		return NULL;

	dev_dbg(adev->dev, "reg=%02x, mask=%02x, lsb=%d, msb=%d\n",
		reg, mask, lsb, msb);
	rmap = devm_regmap_field_alloc(adev->dev, adev->regmap, map);
	if (IS_ERR(rmap)) {
		dev_err(adev->dev, "regmap_field_alloc() for (reg=%02x, mask=%02x) returns %ld\n",
			reg, mask, PTR_ERR(rmap));
	}
	return rmap;
}

static inline void setup_regulator_state(struct regulator_state *state)
{
	/*
	 * copy state_mem to data for mem/coldboot switching. if there is no
	 * sleep uV and mode, set the state to enable
	 */
	if (state->uV != 0)
		return;
	if (state->disabled)
		return;
	state->enabled = true;
}

#define APW8889_DC_MODE_MASK    (REGULATOR_MODE_NORMAL | REGULATOR_MODE_FAST)
#define APW8889_LDO_MODE_MASK   (REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE)

static ssize_t apw8889_regulator_clamp_en_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	return -EINVAL;
}

static ssize_t apw8889_regulator_clamp_en_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{

	struct regulator_dev *rdev = dev_get_drvdata(dev);
	int val = apw8889_regulator_get_clamp_en(rdev);

	switch (val) {
	case 1:
		return sprintf(buf, "enable\n");
	case 0:
		return sprintf(buf, "disable\n");
	case -EINVAL:
		return sprintf(buf, "invalid\n");
	}
	return sprintf(buf, "unknown\n");
}

static DEVICE_ATTR(clamp_en, 0644,
		   apw8889_regulator_clamp_en_show,
		   apw8889_regulator_clamp_en_store);

static struct attribute *apw8889_regulator_dev_attrs[] = {
	&dev_attr_clamp_en.attr,
	NULL
};

static const struct attribute_group apw8889_regulator_dev_group = {
	.name = "apw8889",
	.attrs = apw8889_regulator_dev_attrs,
};

static int apw8889_regulator_add_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &apw8889_regulator_dev_group);
}

static int apw8889_regulator_register(struct apw8889_device *adev,
	struct apw8889_regulator_desc *gd)
{
	struct device *dev = adev->dev;
	struct apw8889_regulator_data *data;
	struct regulator_config config = {};
	struct regulation_constraints *c;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->desc = gd;

	data->nmode = create_regmap_field(adev, gd->nmode_reg, gd->nmode_mask);
	if (IS_ERR(data->nmode))
		return PTR_ERR(data->nmode);
	data->smode = create_regmap_field(adev, gd->smode_reg, gd->smode_mask);
	if (IS_ERR(data->smode))
		return PTR_ERR(data->smode);
	data->svsel = create_regmap_field(adev, gd->svsel_reg, gd->svsel_mask);
	if (IS_ERR(data->svsel))
		return PTR_ERR(data->svsel);

	if (gd->clamp_reg) {
		data->clamp = create_regmap_field(adev, gd->clamp_reg, gd->clamp_mask);
		if (IS_ERR(data->clamp))
			return PTR_ERR(data->clamp);
	}

	config.dev         = adev->dev;
	config.regmap      = adev->regmap;
	config.driver_data = data;

	data->rdev = devm_regulator_register(dev, &gd->desc, &config);
	if (IS_ERR(data->rdev))
		return PTR_ERR(data->rdev);

	apw8889_regulator_add_sysfs(rdev_get_dev(data->rdev));
	apw8889_regulator_set_clamp_en(data->rdev, 1);

	c = data->rdev->constraints;
	/* setup regulator state */
	setup_regulator_state(&c->state_mem);
	data->state_mem = c->state_mem;
	setup_regulator_state(&data->state_coldboot);
	/* enable change mode */
	c->valid_ops_mask |= REGULATOR_CHANGE_MODE;
	c->valid_modes_mask |= apw8889_regulator_type_is_ldo(data->desc) ?
		APW8889_LDO_MODE_MASK : APW8889_DC_MODE_MASK;
	list_add_tail(&data->list, &adev->list);
	return 0;
}

/* regulator id */
enum {
	APW8889_ID_DC1 = 0,
	APW8889_ID_DC2,
	APW8889_ID_DC3,
	APW8889_ID_DC4,
	APW8889_ID_DC5,
	APW8889_ID_DC6,
	APW8889_ID_LDO1,
	APW8889_ID_MAX
};

#define APW8889_DESC(_id, _name, _min_uV, _uV_step, _n_volt, _type)\
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &apw8889_regulator_ops,             \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = _n_volt,                            \
		.min_uV      = _min_uV,                            \
		.uV_step     = _uV_step,                           \
		.id          = APW8889_ID_ ## _id,                 \
		.vsel_reg    = APW8889_REG_ ## _id ## _NRMVOLT,    \
		.vsel_mask   = APW8889_ ## _id ## _NRMVOLT_MASK,   \
		.enable_reg  = APW8889_REG_ONOFF,                  \
		.enable_mask = APW8889_ ## _id ## _ON_MASK,        \
		.enable_val  = APW8889_ ## _id ## _ON_MASK,        \
		.of_parse_cb = apw8889_regulator_of_parse_cb,      \
		.of_map_mode = apw8889_regulator_ ## _type ##_of_map_mode, \
	},                                                         \
	.nmode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.nmode_mask = APW8889_ ## _id ## _NRMMODE_MASK,            \
	.smode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.smode_mask = APW8889_ ## _id ## _SLPMODE_MASK,            \
	.svsel_reg  = APW8889_REG_ ## _id ## _SLPVOLT,             \
	.svsel_mask = APW8889_ ## _id ## _SLPVOLT_MASK,            \
}

#define APW8889_DESC_CLAMP(_id, _name, _min_uV, _uV_step, _n_volt, _type) \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &apw8889_regulator_ops,             \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = _n_volt,                            \
		.min_uV      = _min_uV,                            \
		.uV_step     = _uV_step,                           \
		.id          = APW8889_ID_ ## _id,                 \
		.vsel_reg    = APW8889_REG_ ## _id ## _NRMVOLT,    \
		.vsel_mask   = APW8889_ ## _id ## _NRMVOLT_MASK,   \
		.enable_reg  = APW8889_REG_ONOFF,                  \
		.enable_mask = APW8889_ ## _id ## _ON_MASK,        \
		.enable_val  = APW8889_ ## _id ## _ON_MASK,        \
		.of_parse_cb = apw8889_regulator_of_parse_cb,      \
		.of_map_mode = apw8889_regulator_ ## _type ##_of_map_mode, \
	},                                                         \
	.nmode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.nmode_mask = APW8889_ ## _id ## _NRMMODE_MASK,            \
	.smode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.smode_mask = APW8889_ ## _id ## _SLPMODE_MASK,            \
	.svsel_reg  = APW8889_REG_ ## _id ## _SLPVOLT,             \
	.svsel_mask = APW8889_ ## _id ## _SLPVOLT_MASK,            \
	.clamp_reg  = APW8889_REG_CLAMP,                           \
	.clamp_mask = APW8889_ ## _id ## _CLAMP_MASK,              \
}

#define APW8889_DESC_FIXED_UV(_id, _name, _fixed_uV)               \
{                                                                  \
	.desc = {                                                  \
		.owner       = THIS_MODULE,                        \
		.type        = REGULATOR_VOLTAGE,                  \
		.ops         = &apw8889_regulator_fixed_uV_ops,    \
		.name        = _name,                              \
		.of_match    = _name,                              \
		.n_voltages  = 1,                                  \
		.fixed_uV    = _fixed_uV,                          \
		.id          = APW8889_ID_ ## _id,                 \
		.enable_reg  = APW8889_REG_ONOFF,                  \
		.enable_mask = APW8889_ ## _id ## _ON_MASK,        \
		.enable_val  = APW8889_ ## _id ## _ON_MASK,        \
		.of_parse_cb = apw8889_regulator_of_parse_cb,      \
		.of_map_mode = apw8889_regulator_dc_of_map_mode,   \
	},                                                         \
	.nmode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.nmode_mask = APW8889_ ## _id ## _NRMMODE_MASK,            \
	.smode_reg  = APW8889_REG_ ## _id ## _MODE,                \
	.smode_mask = APW8889_ ## _id ## _SLPMODE_MASK,            \
}

static struct apw8889_regulator_desc desc[] = {
	[APW8889_ID_DC1]  = APW8889_DESC(DC1, "dc1", 2200000, 25000, 64, dc),
	[APW8889_ID_DC2]  = APW8889_DESC_CLAMP(DC2, "dc2", 550000, 12500, 64, dc),
	[APW8889_ID_DC3]  = APW8889_DESC_CLAMP(DC3, "dc3", 550000, 12500, 64, dc),
	[APW8889_ID_DC4]  = APW8889_DESC_CLAMP(DC4, "dc4", 550000, 12500, 64, dc),
	[APW8889_ID_DC5]  = APW8889_DESC_FIXED_UV(DC5, "dc5", 0),
	[APW8889_ID_DC6]  = APW8889_DESC(DC6, "dc6",  800000, 20000, 64, dc),
	[APW8889_ID_LDO1] = APW8889_DESC(LDO1, "ldo1", 1780000, 40000, 64, ldo),
};

/* regmap */
static const struct reg_default apw8889_reg_defaults[] = {
	{ .reg = APW8889_REG_ONOFF,        .def = 0xFD, },
	{ .reg = APW8889_REG_DISCHG,       .def = 0xFD, },
	{ .reg = APW8889_REG_DC1DC2_MODE,  .def = 0x22, },
	{ .reg = APW8889_REG_DC3DC4_MODE,  .def = 0x22, },
	{ .reg = APW8889_REG_DC5DC6_MODE,  .def = 0x22, },
	{ .reg = APW8889_REG_LDO1_MODE,    .def = 0x20, },
	{ .reg = APW8889_REG_DC1_NRMVOLT,  .def = 0x2C, },
	{ .reg = APW8889_REG_DC2_NRMVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC3_NRMVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC4_NRMVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC6_NRMVOLT,  .def = 0x32, },
	{ .reg = APW8889_REG_LDO1_NRMVOLT, .def = 0x12, },
	{ .reg = APW8889_REG_DC1_SLPVOLT,  .def = 0x2C, },
	{ .reg = APW8889_REG_DC2_SLPVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC3_SLPVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC4_SLPVOLT,  .def = 0x24, },
	{ .reg = APW8889_REG_DC6_SLPVOLT,  .def = 0x32, },
	{ .reg = APW8889_REG_LDO1_SLPVOLT, .def = 0x12, },
	{ .reg = APW8889_REG_CLAMP,        .def = 0x00, },
};

static bool apw8889_regmap_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case APW8889_REG_ONOFF ... APW8889_REG_LDO1_SLPVOLT:
	case APW8889_REG_CLAMP:
	case APW8889_REG_CHIP_ID:
	case APW8889_REG_VERSION:
		return true;
	}
	return false;
}

static bool apw8889_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case APW8889_REG_ONOFF ... APW8889_REG_LDO1_SLPVOLT:
	case APW8889_REG_CLAMP:
		return true;
	}
	return false;
}

static bool apw8889_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case APW8889_REG_CLAMP:
	case APW8889_REG_CHIP_ID:
	case APW8889_REG_VERSION:
		return true;
	}
	return false;
}

static const struct regmap_config apw8889_regmap_config = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = 0x1F,
	.cache_type       = REGCACHE_FLAT,
	.reg_defaults     = apw8889_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(apw8889_reg_defaults),
	.readable_reg     = apw8889_regmap_readable_reg,
	.writeable_reg    = apw8889_regmap_writeable_reg,
	.volatile_reg     = apw8889_regmap_volatile_reg,
};

/* pm */
static int apw8889_regulator_suspend(struct device *dev)
{
	struct apw8889_regulator_data *pos;
	struct apw8889_device *adev = dev_get_drvdata(dev);

#ifdef CONFIG_SUSPEND
	if (RTK_PM_STATE == PM_SUSPEND_STANDBY)
		goto done;
#endif

	dev_info(dev, "Enter %s\n", __func__);
	list_for_each_entry(pos, &adev->list, list)
		apw8889_prepare_suspend_state(pos->rdev, 0);
	regulator_suspend_prepare(PM_SUSPEND_MEM);
	dev_info(dev, "Exit %s\n", __func__);
done:
	return 0;
}

static const struct dev_pm_ops apw8889_regulator_pm_ops = {
	.suspend = apw8889_regulator_suspend,
};

static int apw8889_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct apw8889_device *adev;
	int i, ret;
	u32 chip_id, rev;

	dev_info(dev, "%s\n", __func__);

	adev = devm_kzalloc(dev, sizeof(*adev), GFP_KERNEL);
	if (!adev)
		return -ENOMEM;
	adev->regmap = devm_rtk_regmap_init_i2c(client, &apw8889_regmap_config);
	if (IS_ERR(adev->regmap))
		return PTR_ERR(adev->regmap);
	adev->dev = dev;
	INIT_LIST_HEAD(&adev->list);

	/* show chip info */
	ret = regmap_read(adev->regmap, APW8889_REG_CHIP_ID, &chip_id);
	if (ret) {
		dev_err(dev, "failed to read chip_id: %d\n", ret);
		return ret;
	}
	if (chip_id != 0x5a) {
		dev_err(dev, "chip_id(%02x) not match\n", chip_id);
		return -EINVAL;
	}
	regmap_read(adev->regmap, APW8889_REG_VERSION, &rev);
	dev_info(dev, "apw8889(%02x) rev%d\n", chip_id, rev);

	for (i = 0; i < ARRAY_SIZE(desc); i++) {
		ret = apw8889_regulator_register(adev, &desc[i]);
		if (ret) {
			dev_err(dev, "Failed to register %s: %d\n",
				desc[i].desc.name, ret);
			return ret;
		}
	}

	i2c_set_clientdata(client, adev);
	return 0;
}

static void apw8889_regulator_shutdown(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct apw8889_device *adev = i2c_get_clientdata(client);
	struct apw8889_regulator_data *pos;

	dev_info(dev, "Enter %s\n", __func__);
	list_for_each_entry(pos, &adev->list, list)
		apw8889_prepare_suspend_state(pos->rdev, 1);
	regulator_suspend_prepare(PM_SUSPEND_MEM);
	dev_info(dev, "Exit %s\n", __func__);
}

static const struct i2c_device_id apw8889_regulator_ids[] = {
	{"apw8889", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, apw8889_regulator_ids);

struct i2c_driver apw8889_regulator_driver = {
	.driver = {
		.name = "apw8889",
		.owner = THIS_MODULE,
		.pm = &apw8889_regulator_pm_ops,
	},
	.id_table = apw8889_regulator_ids,
	.probe    = apw8889_regulator_probe,
	.shutdown = apw8889_regulator_shutdown,
};
module_i2c_driver(apw8889_regulator_driver);
EXPORT_SYMBOL_GPL(apw8889_regulator_driver);

MODULE_DESCRIPTION("Anpec APW8889 Regulator Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL");


