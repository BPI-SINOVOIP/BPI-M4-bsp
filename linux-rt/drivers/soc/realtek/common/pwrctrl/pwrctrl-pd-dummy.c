/*
 * pwrctrl-pd-dummy.c - Dummy PD
 *
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Copyright (C) 2018 Cheng-Yu Lee <cylee12@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <soc/realtek/power-control.h>
#include "pwrctrl_driver.h"
#include "pwrctrl-pd.h"

static int dummy_power_on(struct power_control *pwrctrl)
{
	struct dummy_pd *dpd = pc_to_dummy_pd(pwrctrl);

	pr_info("%s: %s\n", pwrctrl->name, __func__);
	dpd->on = 1;
	return 0;
}

static int dummy_power_off(struct power_control *pwrctrl)
{
	struct dummy_pd *dpd = pc_to_dummy_pd(pwrctrl);

	pr_info("%s: %s\n", pwrctrl->name, __func__);
	dpd->on = 0;
	return 0;
}

static int dummy_is_powered_on(struct power_control *pwrctrl)
{
	struct dummy_pd *dpd = pc_to_dummy_pd(pwrctrl);

	pr_info("%s: %s\n", pwrctrl->name, __func__);
	return dpd->on == 1;
}

const struct power_control_ops dummy_ops = {
	.power_off     = dummy_power_off,
	.power_on      = dummy_power_on,
	.is_powered_on = dummy_is_powered_on,
};

