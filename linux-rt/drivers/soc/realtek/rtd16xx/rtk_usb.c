/*
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/reset-helper.h> // rstc_get
#include <linux/reset.h>
#include <soc/realtek/rtk_usb.h>
#include <soc/realtek/rtk_chip.h>

struct rtk_usb {
	struct power_ctrl_reg {
		/* l4_icg */
		u32 p0_l4_icg;
		u32 p1_l4_icg;
		u32 p2_l4_icg;
	} power_ctrl_reg;

	bool usb_power_cut;
};

static struct rtk_usb *rtk_usb;

/* set usb charger power */
void rtk_usb_set_charger_power(struct rtk_usb *rtk_usb, unsigned int val)
{
	if (!rtk_usb)
		return;

	return;
}

int rtk_usb_set_hw_l4icg_on_off(struct rtk_usb *rtk_usb,
	    enum usb_port_num port_num, bool on)
{
	void __iomem *reg;

	if (!rtk_usb)
		return 0;

	switch (port_num) {
	case USB_PORT_0:
		if (rtk_usb->power_ctrl_reg.p0_l4_icg) {
			pr_info("%s set l4_icg port %d\n", __func__, port_num);
			reg = ioremap(rtk_usb->power_ctrl_reg.p0_l4_icg, 0x4);
			writel((on&BIT(0)) | readl(reg), reg);
			iounmap(reg);
		}
		break;
	case USB_PORT_1:
		if (rtk_usb->power_ctrl_reg.p1_l4_icg) {
			pr_info("%s set l4_icg port %d\n", __func__, port_num);
			reg = ioremap(rtk_usb->power_ctrl_reg.p1_l4_icg, 0x4);
			writel((on&BIT(0)) | readl(reg), reg);
			iounmap(reg);
		}
		break;
	case USB_PORT_2:
		if (rtk_usb->power_ctrl_reg.p2_l4_icg) {
			pr_info("%s set l4_icg port %d\n", __func__, port_num);
			reg = ioremap(rtk_usb->power_ctrl_reg.p2_l4_icg, 0x4);
			writel((on&BIT(0)) | readl(reg), reg);
			iounmap(reg);
		}
		break;
	default:
		pr_err("%s Error Port num %d\n", __func__, port_num);
		break;
	}

	return 0;
}

static inline struct reset_control *USB_reset_get(struct device *dev,
	    const char *str)
{
	struct reset_control *reset;

	reset = reset_control_get(dev, str);
	if (IS_ERR(reset)) {
		dev_dbg(dev, "No controller reset %s\n", str);
		reset = NULL;
	}
	return reset;
}

static inline void USB_reset_put(struct reset_control *reset)
{
	if (reset) {
		reset_control_put(reset);
	}
}

static inline int USB_reset_deassert(struct reset_control *reset)
{
	if (!reset) return 0;

	return reset_control_deassert(reset);
}

static inline int USB_reset_assert(struct reset_control *reset)
{
	if (!reset) return 0;

	return reset_control_assert(reset);
}

int rtk_usb_iso_power_ctrl(struct rtk_usb *rtk_usb,
	    bool power_on)
{
	struct platform_device *pdev;
	struct device_node *node;
	struct device *dev;
	struct reset_control *reset_u2phy0;
	struct reset_control *reset_u2phy1;
	struct reset_control *reset_u2phy2;
	struct reset_control *reset_u2phy3;
	struct reset_control *reset_u3phy0;
	struct reset_control *reset_u3phy1;
	struct reset_control *reset_usb_apply;

	if (!rtk_usb)
		return 0;

	pr_debug("%s START power %s\n", __func__, power_on?"on":"off");

	node = of_find_compatible_node(NULL, NULL,
		    "Realtek,usb-manager");
	if (node != NULL)
		pdev = of_find_device_by_node(node);
	if (pdev != NULL) {
		dev = &pdev->dev;
	} else {
		pr_err("%s ERROR no usb-manager", __func__);
		return -ENODEV;
	}

	/* GET reset controller */
	reset_u2phy0 = USB_reset_get(dev, "u2phy0");
	reset_u2phy1 = USB_reset_get(dev, "u2phy1");
	reset_u2phy2 = USB_reset_get(dev, "u2phy2");
	reset_u2phy3 = USB_reset_get(dev, "u2phy3");

	reset_u3phy0 = USB_reset_get(dev, "u3phy0");
	reset_u3phy1 = USB_reset_get(dev, "u3phy1");

	reset_usb_apply = USB_reset_get(dev, "apply");

	if (rtk_usb->usb_power_cut) {
		if (power_on) {
			// Enable usb phy reset
			/* DEASSERT: set rstn bit to 1 */
			pr_debug("usb power cut No set phy reset by rtk_usb\n");
		} else {
			pr_debug("usb power cut set phy reset to 0\n");
			USB_reset_assert(reset_u2phy0);
			USB_reset_assert(reset_u2phy1);
			USB_reset_assert(reset_u2phy2);
			USB_reset_assert(reset_u2phy3);
			USB_reset_assert(reset_u3phy0);
			USB_reset_assert(reset_u3phy1);

			USB_reset_assert(reset_usb_apply);
		}
	}
	pr_debug("Realtek RTD16xx USB power %s OK\n", power_on?"on":"off");

	USB_reset_put(reset_u2phy0);
	USB_reset_put(reset_u2phy1);
	USB_reset_put(reset_u2phy2);
	USB_reset_put(reset_u2phy3);

	USB_reset_put(reset_u3phy0);
	USB_reset_put(reset_u3phy1);

	USB_reset_put(reset_usb_apply);

	return 0;
}

int rtk_usb_port_suspend_resume(struct rtk_usb *rtk_usb,
	    enum usb_port_num port_num, bool is_suspend)
{
	if (!rtk_usb)
		return 0;

	return 0;
}

struct rtk_usb *rtk_usb_soc_init(struct device_node *sub_node)
{
	if (!rtk_usb)
		rtk_usb = kzalloc(sizeof(struct rtk_usb), GFP_KERNEL);

	pr_info("%s START (%s)\n", __func__, __FILE__);
	if (sub_node) {
		pr_debug("%s sub_node %s\n", __func__, sub_node->name);

		of_property_read_u32(sub_node,
			    "p0_l4_icg", &rtk_usb->power_ctrl_reg.p0_l4_icg);
		of_property_read_u32(sub_node,
			    "p1_l4_icg", &rtk_usb->power_ctrl_reg.p1_l4_icg);
		of_property_read_u32(sub_node,
			    "p2_l4_icg", &rtk_usb->power_ctrl_reg.p2_l4_icg);

		if (of_property_read_bool(sub_node, "usb_power_cut"))
			rtk_usb->usb_power_cut = true;
		else
			rtk_usb->usb_power_cut = false;

	}
	pr_info("%s END\n", __func__);
	return rtk_usb;
}

int rtk_usb_soc_free(struct rtk_usb **rtk_usb)
{
	if (*rtk_usb) {
		kfree(*rtk_usb);
		*rtk_usb = NULL;
	}
	return 0;
}
