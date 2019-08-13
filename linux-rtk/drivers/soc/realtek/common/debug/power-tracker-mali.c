
#define pr_fmt(fmt) "power-tracker-mali: " fmt
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ktime.h>
#include <soc/realtek/power-tracker.h>
#include <soc/realtek/rtk_sb2_dbg.h>

#include "power-tracker.h"

static void __ktime_to_str(char *buf, ktime_t t)
{
	s64 us = ktime_to_us(t);

	sprintf(buf, "%llu.%06llu", us / 1000000, us % 1000000);
}

static struct power_tracker mali_pt = {
	.id = "mali",
};
static u32 mali_addr_start;
static u32 mali_addr_end;

static int mali_pt_inv_access_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	struct power_tracker *pt = &mali_pt;

	struct sb2_inv_event_data *d = data;
	char sb[20], eb[20], cb[20];

	if (pt->dev == NULL)
		return NOTIFY_DONE;

	if (d->addr < mali_addr_start || d->addr > mali_addr_end)
		return NOTIFY_DONE;

	__ktime_to_str(sb, pt->start);
	__ktime_to_str(eb, pt->end);
	__ktime_to_str(cb, d->cur_time);
	pr_err("%s: Delect invalid access (s=%s, e=%s, c=%s)\n", pt->id, sb, eb, cb);
	return NOTIFY_OK;
}

static struct notifier_block mali_pt_nb = {
	.notifier_call = mali_pt_inv_access_callback,
};

static __init struct device_node *of_find_mali_node(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "arm,mali-utgard");
	if (node)
		return node;

	node = of_find_compatible_node(NULL, NULL, "arm,mali-midgard");
	if (node)
		return node;

	pr_info("No mali device node\n");
	return NULL;
}


static int __init mali_pt_init(void)
{
	struct device_node *node;
	struct resource r;
	int ret;

	node = of_find_mali_node();
	if (!node)
		return 0;

	ret = of_address_to_resource(node, 0, &r);
	if (ret) {
		pr_err("%s: of_address_to_resource() returns %d\n", node->name, ret);
		return ret;
	}

	pr_info("Mali register space: %pr\n", &r);
	mali_addr_start = (u32)r.start;
	mali_addr_end   = (u32)r.end;

	power_tracker_register(&mali_pt);
	sb2_dbg_register_inv_notifier(&mali_pt_nb);
	return 0;
}
module_init(mali_pt_init);

