#ifndef __SOC_REALKEK_POWER_TRACKER_DRIVER_H
#define __SOC_REALKEK_POWER_TRACKER_DRIVER_H

#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/atomic.h>
#include <linux/list.h>
struct power_tracker {
	const char *id;
	struct list_head list;
	struct device *dev;
	struct timer_list timer;
	ktime_t start;
	ktime_t end;
	ktime_t timer_time;
	atomic_t fired;
};

int power_tracker_register(struct power_tracker *pt);
void power_tracker_unregister(struct power_tracker *pt);
#endif
