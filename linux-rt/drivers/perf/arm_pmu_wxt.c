#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hrtimer.h>
#include <linux/perf/arm_pmu.h>
#include <linux/perf/arm_pmuv3.h>
#include <asm/irq_regs.h>

struct armpmu_wxt_data {
	struct list_head list;
	struct hrtimer hrtimer;
	struct perf_event *event;
};

static LIST_HEAD(wxt_data_list);

static struct armpmu_wxt_data *event_to_wxt_data(struct perf_event *event)
{
	struct armpmu_wxt_data *xtimer;

	list_for_each_entry(xtimer, &wxt_data_list, list)
		if (xtimer->event == event)
			return xtimer;
	return NULL;
}

static int perf_exclude_event(struct perf_event *event,
			     struct pt_regs *regs)
{
	if (event->hw.state & PERF_HES_STOPPED)
		return 1;

	if (regs) {
		if (event->attr.exclude_user && user_mode(regs))
			return 1;

		if (event->attr.exclude_kernel && !user_mode(regs))
			return 1;
	}

	return 0;
}

static enum hrtimer_restart armpmu_wxt_hrtimer(struct hrtimer *hrtimer)
{
	struct armpmu_wxt_data *wxt = container_of(hrtimer, struct armpmu_wxt_data, hrtimer);
	enum hrtimer_restart ret = HRTIMER_RESTART;
	struct perf_sample_data data;
	struct pt_regs *regs;
	struct perf_event *event;
	u64 period;

	event = wxt->event;

	if (event->state != PERF_EVENT_STATE_ACTIVE)
		return HRTIMER_NORESTART;

	event->pmu->read(event);

	perf_sample_data_init(&data, 0, event->hw.last_period);
	regs = get_irq_regs();

	if (regs && !perf_exclude_event(event, regs)) {
		if (!(event->attr.exclude_idle && is_idle_task(current)))
			if (perf_event_overflow(event, &data, regs))
				ret = HRTIMER_NORESTART;
	}

	period = max_t(u64, 10000, event->hw.sample_period);
	hrtimer_forward_now(hrtimer, ns_to_ktime(period));

	return ret;
}

void armpmu_wxt_start_hrtimer(struct perf_event *event)
{
	struct armpmu_wxt_data *wxt = event_to_wxt_data(event);
	struct hw_perf_event *hwc = &event->hw;
	s64 period;

	if (!wxt)
		return;

	if (!is_sampling_event(event))
		return;

	period = local64_read(&hwc->period_left);
	if (period) {
		if (period < 0)
			period = 10000;

		local64_set(&hwc->period_left, 0);
	} else {
		period = max_t(u64, 10000, hwc->sample_period);
	}
	hrtimer_start(&wxt->hrtimer, ns_to_ktime(period),
		      HRTIMER_MODE_REL_PINNED);
}

void armpmu_wxt_cancel_hrtimer(struct perf_event *event)
{
	struct armpmu_wxt_data *wxt = event_to_wxt_data(event);
	struct hw_perf_event *hwc = &event->hw;

	if (!wxt)
		return;

	if (is_sampling_event(event)) {
		ktime_t remaining = hrtimer_get_remaining(&wxt->hrtimer);
		local64_set(&hwc->period_left, ktime_to_ns(remaining));

		hrtimer_cancel(&wxt->hrtimer);
	}
}

void armpmu_wxt_init_hrtimer(struct perf_event *event)
{
	struct armpmu_wxt_data *wxt;
	struct hw_perf_event *hwc = &event->hw;

	if (strcmp(current->comm, "simpleperf") != 0)
		return;

	wxt = kzalloc(sizeof(*wxt), GFP_KERNEL);
	if (!wxt) {
		pr_warn("%s: failed to alloc memory\n", __func__);
		return;
	}

	if (!is_sampling_event(event))
		return;

	hrtimer_init(&wxt->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wxt->hrtimer.function = armpmu_wxt_hrtimer;

	/*
	 * Since hrtimers have a fixed rate, we can do a static freq->period
	 * mapping and avoid the whole period adjust feedback stuff.
	 */
	if (event->attr.freq) {
		long freq = event->attr.sample_freq;

		event->attr.sample_period = NSEC_PER_SEC / freq;
		hwc->sample_period = event->attr.sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
		hwc->last_period = hwc->sample_period;
		event->attr.freq = 0;
	}

	wxt->event = event;

	list_add(&wxt->list, &wxt_data_list);
}

void armpmu_wxt_fini_hrtimer(struct perf_event *event)
{
	struct armpmu_wxt_data *wxt = event_to_wxt_data(event);

	if (!wxt)
		return;

	list_del(&wxt->list);
	kfree(wxt);
}
