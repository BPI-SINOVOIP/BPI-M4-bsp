#ifndef __SOC_REALTEK_POWER_TRACKER_H
#define __SOC_REALTEK_POWER_TRACKER_H

struct device;
struct power_tracker;

int power_tracker_get(struct device *dev, const char *id);
void power_tracker_put(struct power_tracker *pt);
int power_tracker_start(struct power_tracker *pt, u32 timeout_ms);
void power_tracker_stop(struct power_tracker *pt);
#endif
