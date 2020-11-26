/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define DEBUG
#define LOG_FLAG	"sia8101_set_vdd"


#include <linux/version.h>
#include <linux/power_supply.h>
#include "sia81xx_common.h"
#include "sia81xx_tuning_if.h"
#include "sia81xx_socket.h"
#include "sia81xx_timer_task.h"
#include "sia81xx_set_vdd.h"

#define TIMER_TASK_PROC_NAME			("auto_set_vdd")

#define MAX_SET_VDD_INFO_NUM			(16)

#define AUTO_SET_THREAD_CYCLE_TIME_MS	(500)//ms
#define AUTO_SET_INTERVAL_TIME_MS		(10 * 1000)//ms
#define AUTO_SET_FIRST_SET_SAMPLES		(3)
#define AUTO_SET_NORMAL_SET_SAMPLES		\
	(AUTO_SET_INTERVAL_TIME_MS / AUTO_SET_THREAD_CYCLE_TIME_MS)

#define DEFAULT_MODULE_ID				(0x1000E900)
#define DEFAULT_PARAM_ID				(0x1000EA03)
#define COMPONENT_ID					(50)//ID_VDD,50 0x32
#define VDD_DEFAULT_VAL					(3300000)//3.3v
#define VDD_MSG_INVALID_VAL				(0xFFFFFFFF);


typedef struct sia81xx_set_vdd_info {
	uint32_t timer_task_hdl;
	uint32_t channel_num;
	unsigned long cal_handle;	//afe handle(qcom) or cal module unit(mtk)
	uint32_t cal_id; //afe port id(qcom) or task scene(mtk)	
	uint32_t vdd_val_pool[AUTO_SET_NORMAL_SET_SAMPLES];
	uint32_t vdd_val_pool_pos;
	volatile uint32_t vdd_sample_cnt;
	uint32_t vdd_sample_send;
	uint32_t is_enable;

	uint32_t module_id;
	uint32_t param_id;
}SIA81XX_SET_VDD_INFO;

typedef struct sia81xx_vdd_msg {
	uint32_t vdd; //uv
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
} __packed SIA81XX_VDD_MSG;

typedef struct sia81xx_vdd_param {
	uint32_t proc_code;
	uint32_t id;
	uint32_t msg_len;
	SIA81XX_VDD_MSG msg;
} __packed SIA81XX_VDD_PARAM;

static struct sia81xx_set_vdd_info info_table[MAX_SET_VDD_INFO_NUM];

static uint32_t sia81xx_read_cur_battery_voltage(void)
{
		union power_supply_propval val;
		struct power_supply *psy = power_supply_get_by_name("battery");
		uint32_t vdd_val;
	
		if(NULL == psy) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
		
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0))
		if(NULL == psy->desc->get_property) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
	
		if(0 != psy->desc->get_property(
					psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val)) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
#else 
		if(NULL == psy->get_property) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
	
		if(0 != psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val)) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
#endif
	
		vdd_val = val.intval;

end :
	pr_debug("[debug][%s] %s: current voltage = %u \r\n", 
		LOG_FLAG, __func__, (unsigned int)vdd_val);

	return vdd_val;
}

static void sia81xx_record_cur_battery_voltage(
	struct sia81xx_set_vdd_info *info) 
{
	info->vdd_val_pool_pos ++;
	if(info->vdd_val_pool_pos >= ARRAY_SIZE(info->vdd_val_pool))
		info->vdd_val_pool_pos = 0;

	info->vdd_val_pool[info->vdd_val_pool_pos] = 
		sia81xx_read_cur_battery_voltage();

	return ;
}

static uint32_t sia81xx_get_battery_voltage(
	struct sia81xx_set_vdd_info *info, uint32_t samples) 
{
	uint64_t ave_val = 0;
	uint32_t n = 0, pos = 0;
	int i = 0;
	
	if(NULL == info)
		return VDD_DEFAULT_VAL;

	n = samples < ARRAY_SIZE(info->vdd_val_pool) ? 
		samples : ARRAY_SIZE(info->vdd_val_pool);
	pos = info->vdd_val_pool_pos;

	for(i = 0; i < n; i++) {
		ave_val += info->vdd_val_pool[pos];
		
		if(0 == pos)
			pos = ARRAY_SIZE(info->vdd_val_pool) - 1;
		else
			pos --;
	}

	ave_val = ave_val / n;

	pr_debug("[debug][%s] %s: average vdd = %llu, n = %u \r\n", 
		LOG_FLAG, __func__, ave_val, (unsigned int)n);

	return (uint32_t)ave_val;
}

static struct sia81xx_set_vdd_info *is_cal_id_exist(
	uint32_t timer_task_hdl, 
	uint32_t channel_num)
{
	int i = 0;

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {
		if((timer_task_hdl == info_table[i].timer_task_hdl) && 
			(channel_num == info_table[i].channel_num))
			return &info_table[i];
	}

	return NULL;
}

static struct sia81xx_set_vdd_info *get_one_can_use_info(
	uint32_t timer_task_hdl, 
	uint32_t channel_num) 
{
	struct sia81xx_set_vdd_info *info = NULL;
	int i = 0;

	if(NULL != (info = is_cal_id_exist(timer_task_hdl, channel_num))) {
		return info;
	}

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {
		if((SIA81XX_TIMER_TASK_INVALID_HDL == info_table[i].timer_task_hdl) && 
			(SIA81XX_MAX_CHANNEL_SUPPORT == info_table[i].channel_num))
			return &info_table[i];
	}

	return NULL;
}

static struct sia81xx_set_vdd_info *record_info(
	uint32_t timer_task_hdl, 
	uint32_t channel_num, 
	unsigned long cal_handle, 
	uint32_t cal_id)
{
	struct sia81xx_set_vdd_info *info = NULL;

	info = get_one_can_use_info(timer_task_hdl, channel_num);
	if(NULL == info)
		return NULL;

	info->timer_task_hdl = timer_task_hdl;
	info->channel_num = channel_num;
	info->cal_handle = cal_handle;
	info->cal_id = cal_id;

	return info;
}

static void delete_all_info(void)
{
	int i = 0;

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {
		if(info_table[i].timer_task_hdl < SIA81XX_TIMER_TASK_INVALID_HDL) {
			if(0 != info_table[i].cal_handle) {
				if(0 != tuning_if_opt.close(info_table[i].cal_handle)) {
					pr_err("[  err][%s] %s: tuning_if_opt.close err, "
						"id = %d \r\n",
						LOG_FLAG, __func__, info_table[i].cal_id);
				}
			}
			info_table[i].cal_handle = 0;
			info_table[i].cal_id = 0;
			info_table[i].timer_task_hdl = SIA81XX_TIMER_TASK_INVALID_HDL;
			info_table[i].channel_num = SIA81XX_MAX_CHANNEL_SUPPORT;
		}
	}
}

static void send_set_vdd_msg(
	struct sia81xx_set_vdd_info *info, 
	uint32_t vdd, 
	uint32_t channel_num) 
{
	int ret = 0;
	SIA81XX_VDD_PARAM param;

	if(NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.write \r\n",
			LOG_FLAG, __func__);
		return ;
	}

	param.id = COMPONENT_ID;//ID_VDD
	param.msg_len = sizeof(SIA81XX_VDD_MSG);
	param.proc_code = channel_num;// ch sn
	
	param.msg.vdd = vdd;
	/* don't set these in driver, it should be setted at acdb and it's fixed */
	param.msg.p0 = VDD_MSG_INVALID_VAL;
	param.msg.p1= VDD_MSG_INVALID_VAL;
	param.msg.p2 = VDD_MSG_INVALID_VAL;
	
	ret = tuning_if_opt.write(
		info->cal_handle, 
		info->module_id, 
		info->param_id, 
		(uint32_t)sizeof(param), 
		(uint8_t *)&param);
	
	if(0 > ret) {
		pr_err("[debug][%s] %s: tuning_if_opt.write failed "
			"ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		return ;
	}

	return ;
}

static int sia81xx_open_set_vdd_server(
	uint32_t timer_task_hdl, 
	uint32_t channel_num, 
	uint32_t cal_id) 
{

	unsigned long cal_handle = 0;
	struct sia81xx_set_vdd_info *info = NULL;

	if(NULL == tuning_if_opt.open) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.open \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	cal_handle = tuning_if_opt.open(cal_id);
	if(0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(NULL == 
		(info = record_info(timer_task_hdl, channel_num, cal_handle, cal_id))) {
		pr_err("[  err][%s] %s: 0 != record_info \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	info->module_id = DEFAULT_MODULE_ID;
	info->param_id = DEFAULT_PARAM_ID;

	return 0;
}

#if 0
static int sia81xx_close_set_vdd_server(
	uint32_t cal_id) 
{
	struct sia81xx_set_vdd_info *info = is_cal_id_exist(cal_id);
	if(NULL == info) {
		pr_info("[ info][%s] %s: NULL == map, id = %d \r\n",
			LOG_FLAG, __func__, cal_id);
		return 0;
	}
	
	if(NULL == tuning_if_opt.close) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.close \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	if(0 != tuning_if_opt.close(info->cal_handle)) {
		pr_err("[  err][%s] %s: 0 != tuning_if_opt.close \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	info->cal_handle = 0;
	info->cal_id = 0;

	return 0;
}
#endif

static int sia81xx_auto_set_timer_task_callback(
	int is_first, 
	void *data) 
{
	struct sia81xx_set_vdd_info *info = NULL;
	if(NULL == data) {
		pr_err("[  err][%s] %s: NULL == data \r\n", 
			LOG_FLAG, __func__);
		return -ECHILD;
	}
	info = data;

	if(0 < is_first) {
		/* begain a new timer task */
		info->vdd_sample_send = AUTO_SET_FIRST_SET_SAMPLES;
		info->vdd_sample_cnt = 0;
	}

	sia81xx_record_cur_battery_voltage(info);
	info->vdd_sample_cnt ++;
	if(info->vdd_sample_cnt < info->vdd_sample_send)
		return 0;	

	if(1 == info->is_enable) {
		send_set_vdd_msg(
				info, 
				sia81xx_get_battery_voltage(info, AUTO_SET_FIRST_SET_SAMPLES),
				info->channel_num);
	}

	info->vdd_sample_cnt = 0;
	if(0 >= is_first) {
		/* make sure first send has done */
		info->vdd_sample_send = AUTO_SET_NORMAL_SET_SAMPLES;
	}

	return 0;
}

void sia81xx_set_auto_set_vdd_work_state(
	uint32_t timer_task_hdl, 
	uint32_t channel_num, 
	uint32_t state)
{
	struct sia81xx_set_vdd_info *pInfo = 
		is_cal_id_exist(timer_task_hdl, channel_num);
	if(NULL == pInfo) 
		return ;

	if(state > 0)
		pInfo->is_enable = 1;
	else
		pInfo->is_enable = 0;
}

int sia81xx_auto_set_vdd_probe(
	uint32_t timer_task_hdl, 
	uint32_t channel_num, 
	uint32_t cal_id, 
	uint32_t state)
{
	int ret = 0, i = 0;
	
	struct sia81xx_set_vdd_info *pInfo = 
		is_cal_id_exist(timer_task_hdl, channel_num);
	if(NULL == pInfo) {
		ret = sia81xx_open_set_vdd_server(timer_task_hdl, channel_num, cal_id);
		if(0 != ret) {
			pr_err("[  err][%s] %s: sia81xx_close_set_vdd_server ret : %d \r\n", 
				LOG_FLAG, __func__, ret);
			return -EINVAL;
		}

		pInfo = is_cal_id_exist(timer_task_hdl, channel_num);
		if(NULL == pInfo) {
			pr_err("[  err][%s] %s: NULL == pInfo \r\n", 
				LOG_FLAG, __func__);
			return -EINVAL;
		}
	}

	pInfo->channel_num = channel_num;
	/* clear vdd sample pool state */
	pInfo->vdd_sample_send = AUTO_SET_FIRST_SET_SAMPLES;
	pInfo->vdd_sample_cnt = 0;
	pInfo->vdd_val_pool_pos = 0;
	for(i = 0; i < ARRAY_SIZE(pInfo->vdd_val_pool); i++) {
		pInfo->vdd_val_pool[i] = VDD_DEFAULT_VAL;
	}
	
	ret = sia81xx_timer_task_register(
			timer_task_hdl, 
			TIMER_TASK_PROC_NAME, 
			channel_num, 
			AUTO_SET_THREAD_CYCLE_TIME_MS, 
			sia81xx_auto_set_timer_task_callback, 
			pInfo);

	if(ret < 0){
		pr_err("[  err][%s] %s: ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		return -EINVAL;
	}

	sia81xx_set_auto_set_vdd_work_state(timer_task_hdl, channel_num, state);

	return 0;
}

int sia81xx_auto_set_vdd_remove(
	uint32_t timer_task_hdl, 
	uint32_t channel_num)
{
	sia81xx_timer_task_unregister(timer_task_hdl, channel_num);

	return 0;
}

int sia81xx_set_vdd_init(void) 
{	
	int ret = 0;

	memset(info_table, 0, sizeof(info_table));

	delete_all_info();

	pr_info("[ info][%s] %s: run !! ", 
		LOG_FLAG, __func__);

	return ret;
}


void sia81xx_set_vdd_exit(void) 
{
	delete_all_info();
	
	pr_info("[ info][%s] %s: run !! ", 
		LOG_FLAG, __func__);
}




