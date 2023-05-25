/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2021. All rights reserved.
 * 2022-01-17 File created.
 */
#include "fsm_public.h"
#if defined(CONFIG_FSM_VBAT_MONITOR)
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#define FSM_DSP_TRY_TIME 	(3)
#define FSM_DSP_SLEEP_TIME 	(10)
#define FSMDSP_RX_SET_ENABLE 	(0x10013D11)
#define FSMDSP_RX_VMAX_0	(0x10013D17)
#define FSMDSP_RX_VMAX_1	(0x10013D18)
#define FSM_PRE_VMAX_UNKNOW	(-1)

#define FSM_VBAT_MAX (100)
#define FSM_VMAX_MAX (0)
#define TIME_COUNT   (5)

enum fsm_rx_module_enable {
	FSM_RX_MODULE_DISABLE = 0,
	FSM_RX_MODULE_ENABLE,
};

enum fsm_dsp_channel {
	FSM_DSP_CHANNEL_0 = 0,
	FSM_DSP_CHANNEL_1,
	FSM_DSP_CHANNEL_MAX,
};

struct vmax_step_config {
	uint32_t vbat_min;
	uint32_t vbat_max;
	int vbat_val;
};

struct fsm_monitor {
	struct workqueue_struct *vbat_mntr_wq;
	struct delayed_work vbat_mntr_work;
	uint32_t vbat_sum;
	uint8_t time_cnt;
	int pre_vmax;
	int ndev;
	int state; // 0: off, 1: on
};

static struct vmax_step_config g_vmax_step[] = {
	{ 50, 100, 0x00000000 },
	{ 30,  50, 0xfff8df1f },
	{  0,  30, 0xfff1568c },
};

static DEFINE_MUTEX(g_dsp_lock);
static struct fsm_monitor g_fsm_monitor = { 0 };

extern int aw_check_dsp_ready(void);
// extern int afe_get_topology(int port_id);
extern int aw_send_afe_cal_apr(uint32_t param_id,
	void *buf, int cmd_size, bool write);


static int fsm_qcom_write_data_to_dsp(int32_t param_id,
	void *data, int data_size)
{
	int ret = -EINVAL;
	int try = 0;

	mutex_lock(&g_dsp_lock);
	while (try++ < FSM_DSP_TRY_TIME) {
		if (aw_check_dsp_ready()) {
			ret = aw_send_afe_cal_apr(param_id, data, data_size, true);
			if (!ret)
				break;
		}
		pr_info("afe not ready, try again");
		msleep(FSM_DSP_SLEEP_TIME);
	}
	mutex_unlock(&g_dsp_lock);
	if (ret)
		pr_err("send data fail:%d", ret);

	return ret;
}

static int fsm_qcom_read_data_from_dsp(int32_t param_id,
	void *data, int data_size)
{
	int ret = -EINVAL;
	int try = 0;

	mutex_lock(&g_dsp_lock);
	while (try++ < FSM_DSP_TRY_TIME) {
		if (aw_check_dsp_ready()) {
			ret = aw_send_afe_cal_apr(param_id, data,
				data_size, false);
			if (!ret)
				break;
		}
		pr_info("afe not ready, try again");
		msleep(FSM_DSP_SLEEP_TIME);
	}
	mutex_unlock(&g_dsp_lock);
	if (ret)
		pr_err("read data fail:%d", ret);

	return ret;
}

static int fsm_dsp_get_rx_module_enable(int *enable)
{
	int ret;

	if (enable == NULL) {
		pr_err("enable is null");
		return -EINVAL;
	}

	ret = fsm_qcom_read_data_from_dsp(FSMDSP_RX_SET_ENABLE,
		(void *)enable, sizeof(uint32_t));

	return ret;
}

/* static int fsm_dsp_set_rx_module_enable(int enable)
{
	int ret;

	switch (enable) {
	case FSM_RX_MODULE_DISABLE:
		pr_info("set enable=%d", enable);
		break;
	case FSM_RX_MODULE_ENABLE:
		pr_info("set enable=%d", enable);
	default:
		pr_err("unsupport enable=%d", enable);
		return -EINVAL；
	}
	ret = fsm_qcom_write_data_to_dsp(FSMDSP_RX_SET_ENABLE,
		(void *)&enable, sizeof(uint32_t));

	return ret;
}


static int fsm_dsp_get_vmax(int32_t *vmax, int dev_index)
{
	int32_t param_id;
	int ret;

	switch (dev_index % FSM_DSP_CHANNEL_MAX) {
	case FSM_DSP_CHANNEL_0:
		param_id = FSMDSP_RX_VMAX_0;
		break;
	case FSM_DSP_CHANNEL_1:
		param_id = FSMDSP_RX_VMAX_1;
		break;
	default:
		pr_err("algo only support double PA channel：%d", dev_index);
		return -EINVAL;
	}
	ret = fsm_qcom_read_data_from_dsp(param_id, 
		(void *)vmax, sizeof(uint32_t));

	return ret;
} */

static int fsm_dsp_set_vmax(int32_t vmax, int dev_index)
{
	int32_t param_id;
	int ret;

	switch (dev_index % FSM_DSP_CHANNEL_MAX) {
	case FSM_DSP_CHANNEL_0:
		param_id = FSMDSP_RX_VMAX_0;
		break;
	case FSM_DSP_CHANNEL_1:
		param_id = FSMDSP_RX_VMAX_1;
		break;
	default:
		pr_err("algo only support double PA channel: %d", dev_index);
		return -EINVAL;
	}
	ret = fsm_qcom_write_data_to_dsp(param_id,
		(void *)&vmax, sizeof(uint32_t));

	return ret;
}

static int fsm_monitor_get_battery_capacity(uint32_t *vbat_capacity)
{
	union power_supply_propval prop = { 0 };
	struct power_supply *psy;
	char name[] = "battery";
	int ret;

	if (vbat_capacity == NULL) {
		pr_err("invalid paramter");
		return -EINVAL;
	}

	psy = power_supply_get_by_name(name);
	if (psy == NULL) {
		pr_err("get power supply failed");
		return -EINVAL;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret < 0) {
		pr_err("get vbat capacity failed");
		return ret;
	}
	*vbat_capacity = prop.intval;

	return 0;
}

static int fsm_search_vmax_from_table(const int vbat_capacity, int *vmax_val)
{
	struct vmax_step_config *vmax_cfg = g_vmax_step;
	int idx;

	if (vmax_val == NULL) {
		pr_err("invalid paramter");
		return -EINVAL;
	}

	if (vbat_capacity >= FSM_VBAT_MAX) {
		*vmax_val = FSM_VMAX_MAX;
		return 0;
	}
	for (idx = 0; idx < ARRAY_SIZE(g_vmax_step); idx++) {
		if (vbat_capacity >= vmax_cfg[idx].vbat_min
				&& vbat_capacity < vmax_cfg[idx].vbat_max) {
			*vmax_val = vmax_cfg[idx].vbat_val;
			return 0;
		}
	}
	pr_err("vmax_val not found!");
	*vmax_val = 0;

	return -ENODATA;
}

static int fsm_monitor_update_vmax_to_dsp(int vmax_set)
{
	struct fsm_monitor *monitor = &g_fsm_monitor;
	uint32_t enable = 0;
	int dev_idx;
	int ret;

	if (monitor->pre_vmax == vmax_set) {
		pr_info("vmax:0x%X not changed", vmax_set);
		return 0;
	}
	ret = fsm_dsp_get_rx_module_enable(&enable);
	if (!enable || ret < 0) {
		pr_err("get rx module error:%d, enable:%d", ret, enable);
		return -EPERM;
	}
	for (dev_idx = 0; dev_idx < monitor->ndev; dev_idx++) {
		ret = fsm_dsp_set_vmax(vmax_set, dev_idx);
		if (ret) {
			pr_err("set vmax fail:%d", ret);
			return ret;
		}
	}
	pr_info("vmax updated: 0x%X", vmax_set);
	monitor->pre_vmax = vmax_set;

	return 0;
}

static int fsm_monitor_with_dsp_vmax_work(struct fsm_monitor *monitor)
{
	uint32_t vbat_capacity;
	uint32_t ave_capacity;
	int vmax_set = 0;
	int ret;

	if (monitor == NULL)
		return -EINVAL;

	ret = fsm_monitor_get_battery_capacity(&vbat_capacity);
	if (ret) {
		pr_err("get bat copacity fail:%d", ret);
		return ret;
	}
	if (monitor->time_cnt >= TIME_COUNT) {
		monitor->time_cnt = 0;
		monitor->vbat_sum = 0;
	}
	monitor->time_cnt++;
	monitor->vbat_sum += vbat_capacity;
	pr_info("vbat_capacity.%d: %d", monitor->time_cnt, vbat_capacity);

	if ((monitor->pre_vmax != FSM_PRE_VMAX_UNKNOW)
			&& (monitor->time_cnt < TIME_COUNT))
		return 0;

	ave_capacity = monitor->vbat_sum / monitor->time_cnt;
	pr_info("ave_capacity: %d", ave_capacity);
	ret = fsm_search_vmax_from_table(ave_capacity, &vmax_set);
	if (ret < 0) {
		pr_err("not find vmax_set");
		return ret;
	}
	ret = fsm_monitor_update_vmax_to_dsp(vmax_set);

	return ret;
}

static void fsm_work_vbat_monitor(struct work_struct *work)
{
	struct fsm_monitor *monitor = &g_fsm_monitor;

	if (monitor == NULL || monitor->state == 0)
		return;

	fsm_monitor_with_dsp_vmax_work(monitor);
	/* reschedule */
	queue_delayed_work(monitor->vbat_mntr_wq, &monitor->vbat_mntr_work,
		3*HZ);
}

void fsm_set_vbat_monitor(bool enable)
{
	struct fsm_monitor *monitor= &g_fsm_monitor;
	struct fsm_config *cfg = fsm_get_config();
	struct preset_file *file;

	if (monitor == NULL || monitor->vbat_mntr_wq == NULL)
		return;

	file = fsm_get_presets();
	if (file == NULL || file->hdr.ndev == 0)
		return;

	monitor->ndev = file->hdr.ndev;
	pr_info("ndev:%d, enable:%d", monitor->ndev, enable);
	if (enable && !monitor->state) {
		if (cfg->f0_test || cfg->force_calib)
			return;
		monitor->time_cnt = 0;
		monitor->vbat_sum = 0;
		monitor->pre_vmax = FSM_PRE_VMAX_UNKNOW;
		monitor->state = 1; // enable monitor
		queue_delayed_work(monitor->vbat_mntr_wq,
			&monitor->vbat_mntr_work, 3*HZ);
	}
	if (!enable && monitor->state) {
		monitor->state = 0; // disable monitor
		monitor->ndev = 0;
		// if (delayed_work_pending(&monitor->vbat_mntr_work))
		cancel_delayed_work_sync(&monitor->vbat_mntr_work);
	}
}

void fsm_vbat_monitor_vmax(int *vmax)
{
	if (vmax)
		*vmax = g_fsm_monitor.pre_vmax;
}

void fsm_vbat_monitor_state(int *state)
{
	if (state)
		*state = g_fsm_monitor.state;
}

void fsm_vbat_monitor_init(void)
{
	g_fsm_monitor.vbat_mntr_wq = create_singlethread_workqueue("fs16xx_vbat");
	INIT_DELAYED_WORK(&g_fsm_monitor.vbat_mntr_work, fsm_work_vbat_monitor);
	g_fsm_monitor.state = 0;
}

void fsm_vbat_monitor_deinit(void)
{
	cancel_delayed_work_sync(&g_fsm_monitor.vbat_mntr_work);
	destroy_workqueue(g_fsm_monitor.vbat_mntr_wq);
}
#endif
