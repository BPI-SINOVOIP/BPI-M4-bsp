#define pr_fmt(fmt) "power_tracker: " fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/notifier.h>
#include "power-tracker.h"

MODULE_LICENSE("GPL");

static DEFINE_MUTEX(power_tracker_list_lock);
static LIST_HEAD(power_tracker_list);

static void __ktime_to_str(char *buf, ktime_t t)
{
	s64 us = ktime_to_us(t);

	sprintf(buf, "%llu.%06llu", us / 1000000, us % 1000000);
}

static void power_tracker_timer_expired(unsigned long data)
{
	struct power_tracker *pt = (void *)data;

	atomic_set(&pt->fired, 1);
	pt->timer_time = ktime_get();
}

int power_tracker_register(struct power_tracker *pt)
{
	if (!pt || !pt->id)
		return -EINVAL;

	init_timer(&pt->timer);
	pt->timer.function = power_tracker_timer_expired;
	pt->timer.data = (unsigned long)pt;

	mutex_lock(&power_tracker_list_lock);
	list_add(&pt->list, &power_tracker_list);
	mutex_unlock(&power_tracker_list_lock);

	pr_debug("%s: Registered\n", pt->id);
	return 0;
}
EXPORT_SYMBOL_GPL(power_tracker_register);

void power_tracker_unregister(struct power_tracker *pt)
{
	mutex_lock(&power_tracker_list_lock);
	list_del(&pt->list);
	mutex_unlock(&power_tracker_list_lock);
}
EXPORT_SYMBOL_GPL(power_tracker_unregister);

static struct power_tracker *__find_power_tracker(const char *id)
{
	struct power_tracker *pt;

	list_for_each_entry(pt, &power_tracker_list, list) {
		if (!strcmp(pt->id, id))
			return pt;
	}
	return NULL;
}

struct power_tracker *power_tracker_get(struct device *dev, const char *id)
{
	struct power_tracker *pt;

	if (!dev || !id)
		return ERR_PTR(-EINVAL);

	mutex_lock(&power_tracker_list_lock);
	pt = __find_power_tracker(id);
	mutex_unlock(&power_tracker_list_lock);
	if (!pt)
		return ERR_PTR(-ENODEV);

	if (pt->dev) {
		pr_warn("%s: Already used by %s\n", pt->id, dev_name(pt->dev));
		return ERR_PTR(-EBUSY);
	}

	pt->dev = dev;
	pr_debug("%s: Used by %s\n", pt->id, dev_name(pt->dev));
	return pt;
}
EXPORT_SYMBOL_GPL(power_tracker_get);

void power_tracker_put(struct power_tracker *pt)
{
	pt->dev = NULL;
}
EXPORT_SYMBOL_GPL(power_tracker_put);

int power_tracker_start(struct power_tracker *pt, u32 timeout_ms)
{
	u64 timeout = msecs_to_jiffies(timeout_ms);

	pr_debug("%s: Start tracking (timeout=%u)\n", pt->id, timeout_ms);

	if (WARN_ON(timer_pending(&pt->timer)))
		return -EBUSY;

	atomic_set(&pt->fired, 0);
	pt->timer.expires = get_jiffies_64() + timeout;
	add_timer(&pt->timer);
	pt->start = ktime_get();
	return 0;
}
EXPORT_SYMBOL_GPL(power_tracker_start);

void power_tracker_stop(struct power_tracker *pt)
{
	ktime_t cur = ktime_get();

	del_timer(&pt->timer);
	pt->end = cur;
	if (atomic_read(&pt->fired) == 1) {
		char sb[20], tb[20], cb[20];

		__ktime_to_str(sb, pt->start);
		__ktime_to_str(tb, pt->timer_time);
		__ktime_to_str(cb, cur);

		pr_err("%s: Timer fired (s=%s, f=%s, c=%s)\n", pt->id, sb, tb, cb);
	}
	pr_debug("%s: Stop tracking\n", pt->id);
}
EXPORT_SYMBOL_GPL(power_tracker_stop);

static int power_tracker_info_show(struct seq_file *s, void *data)
{
	char sb[20], tb[20], eb[20];
	struct power_tracker *pt;

	seq_printf(s, "trackers:\n");
	seq_printf(s, "  %-8s  %-10s %-10s %-10s %s\n", "name", "start", "end", "timer", "device");
	seq_printf(s, "-----------------------------------------------------------\n");
	mutex_lock(&power_tracker_list_lock);
	list_for_each_entry(pt, &power_tracker_list, list) {

		__ktime_to_str(sb, pt->start);
		__ktime_to_str(tb, pt->timer_time);
		__ktime_to_str(eb, pt->end);

		seq_printf(s, "  %-8s: %-10s %-10s %-10s %s\n", pt->id, sb, eb, tb,
			pt->dev ? dev_name(pt->dev) : "no device");
	}
	mutex_unlock(&power_tracker_list_lock);

	return 0;
}

static int power_tracker_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, power_tracker_info_show, NULL);
}

static const struct file_operations power_tracker_info_ops = {
	.owner   = THIS_MODULE,
	.open    = power_tracker_info_open,
	.read    = seq_read,
	.release = single_release,
};

extern struct atomic_notifier_head panic_notifier_list;

static int power_tracker_panic_cb(struct notifier_block *nb, unsigned long action, void *data)
{
	char sb[20], tb[20], eb[20];
	struct power_tracker *pt;

	printk(KERN_ERR "Power trackers:\n");
	list_for_each_entry(pt, &power_tracker_list, list) {

		__ktime_to_str(sb, pt->start);
		__ktime_to_str(tb, pt->timer_time);
		__ktime_to_str(eb, pt->end);
		printk(KERN_ERR "%s (%s): %s %s %s\n", pt->id,
			pt->dev ? dev_name(pt->dev) : "no device",
			sb, eb, tb);
	}
	return NOTIFY_OK;
}

static struct notifier_block power_tracker_panic_nb = {
	.notifier_call = power_tracker_panic_cb,
};

static __init int power_tracker_init(void)
{
	debugfs_create_file("power_tracker_info", 0444, NULL, NULL, &power_tracker_info_ops);
	atomic_notifier_chain_register(&panic_notifier_list, &power_tracker_panic_nb);
	return 0;
}
module_init(power_tracker_init);
