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
#define LOG_FLAG	"sia81xx_timer_task"

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include "sia81xx_timer_task.h"

#define MAX_PROC_INFO_NAME_LEN			(32)
#define MAX_TIMER_TASK_NUM				(SIA81XX_TIMER_TASK_INVALID_HDL)
#define MAX_TIMER_PROCESSOR_NUM			(8)
#define TIMER_TASK_CYCLE_TIME_MS		(100)

#define INVALID_USER_ID					(0xFFFFFFFF)

typedef enum sia81xx_task_state_e {
	SIA81XX_TASK_DISABLE = 0,
	SIA81XX_TASK_ENABLE,
}sia81xx_task_state_t;

typedef struct sia81xx_timer_proc_info_s {
	char name[MAX_PROC_INFO_NAME_LEN];
	uint32_t user_id;//id is unique in one timer task handle, and provide by user
	uint32_t run_time_ms;
	uint32_t wake_up_interval_ms;
	int (*process)(int is_first, void *data);
	void *data;
}sia81xx_timer_proc_info_t;

typedef struct sia81xx_timer_task_s {
	atomic_t task_state;
	atomic_t switch_flag;
	struct task_struct *task;
	struct mutex task_mutex;
	
	sia81xx_timer_proc_info_t proc[MAX_TIMER_PROCESSOR_NUM];//must be a array
}sia81xx_timer_task_t;

static sia81xx_timer_task_t timer_tasks[MAX_TIMER_TASK_NUM];

static inline void sia81xx_process_one_timer_callback(
	sia81xx_timer_proc_info_t *proc, uint32_t time, int is_first)
{
	int ret = 0;
	
	if(NULL == proc)
		return ;

	if(NULL == proc->process)
		return ;

	proc->run_time_ms += time;
	if(proc->run_time_ms < proc->wake_up_interval_ms)
		return ;

	ret = proc->process(is_first, proc->data);
	if(0 != ret) {
		pr_err("[  err][%s] %s: %s run with ret : %d \r\n", 
			LOG_FLAG, __func__, proc->name, ret);
	}

	proc->run_time_ms = 0;
}

static int sia81xx_timer_task_processor(
	void *data) 
{
	static unsigned int cur_run_cnt = 0;//only for debug
	int i = 0;
	uint32_t switch_flag = 0;
	sia81xx_timer_task_t *timer = data;
	if(NULL == timer) {
		pr_err("[  err][%s] %s: NULL == timer \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cur_run_cnt ++;
	pr_debug("[debug][%s] %s: cur_run_cnt %u\r\n", 
					LOG_FLAG, __func__, cur_run_cnt);
	
	while(SIA81XX_TASK_ENABLE == atomic_read(&timer->task_state)) {

		mutex_lock(&timer->task_mutex);
		switch_flag = atomic_read(&timer->switch_flag);
		atomic_set(&timer->switch_flag, 0);
		mutex_unlock(&timer->task_mutex);

		if(SIA81XX_TASK_ENABLE != atomic_read(&timer->task_state))
			goto next_cycle;

		for(i = 0; i < ARRAY_SIZE(timer->proc); i ++) {

			if(1 == atomic_read(&timer->switch_flag)) {
				goto next_cycle; 
			}

			if(1 == switch_flag) {
				//let first trigger immediately
				if(NULL != timer->proc[i].process) {
					timer->proc[i].run_time_ms = 
						timer->proc[i].wake_up_interval_ms;
				}
			}
			
			sia81xx_process_one_timer_callback(
				&timer->proc[i], 
				TIMER_TASK_CYCLE_TIME_MS, 
				switch_flag);
		}
		
next_cycle :	
		msleep(TIMER_TASK_CYCLE_TIME_MS);
	}
	
	cur_run_cnt --;

	return 0;
}

static void sia81xx_timer_task_record_proc_info(
	sia81xx_timer_proc_info_t *proc,
	const char *name, 
	uint32_t user_id, 
	uint32_t wake_up_interval_ms,
	int (*process)(int is_first, void *data),
	void *data)
{
	char dummy_name[MAX_PROC_INFO_NAME_LEN];
	const char *real_name = NULL;
	
	if(NULL == proc)
		return ;

	memset(dummy_name, 0, sizeof(dummy_name));
	sprintf(dummy_name, "dummy@%p", process);

	if(NULL == name) {
		real_name = (const char *)dummy_name;
	} else {
		if((strlen(name) < sizeof(proc->name)) && (0 != strlen(name))) {
			real_name = (const char *)name;
		} else {
			real_name = (const char *)dummy_name;
		}
	}
	
	strcpy(proc->name, real_name);
	proc->user_id = user_id;
	proc->process = process;
	proc->data = data;
	proc->wake_up_interval_ms = wake_up_interval_ms;
	proc->run_time_ms = 0;

	return ;
}

int sia81xx_timer_task_register(
	uint32_t hdl,
	const char *name, 
	uint32_t user_id, 
	uint32_t wake_up_interval_ms,
	int (*process)(int is_first, void *data),
	void *data)
{
	int i = 0;
	sia81xx_timer_task_t *timer = NULL;

	if(hdl >= ARRAY_SIZE(timer_tasks)) {
		pr_err("[  err][%s] %s: bad hdl : %u \r\n", 
			LOG_FLAG, __func__, hdl);
		return -EINVAL;
	}
	timer = &timer_tasks[hdl];

	if(NULL == process) {
		pr_err("[  err][%s] %s: NULL == process \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(INVALID_USER_ID == user_id) {
		pr_err("[  err][%s] %s: user_id invalid : 0x%08x \r\n", 
			LOG_FLAG, __func__, user_id);
		return -EINVAL;
	}

	/* check is this user id have been used */
	for(i = 0; i < ARRAY_SIZE(timer->proc); i ++) {
		if(user_id == timer->proc[i].user_id)
			return i;
	}

	/* find an unuse proc and allocate */
	for(i = 0; i < ARRAY_SIZE(timer->proc); i ++) {
		if(INVALID_USER_ID == timer->proc[i].user_id) {
			sia81xx_timer_task_record_proc_info(
				&timer->proc[i], 
				name, 
				user_id, 
				wake_up_interval_ms, 
				process, 
				data);

			pr_debug("[debug][%s] %s: i(%d), hdl(%u), "
				"user_id(0x%08x), time(%u)\r\n", 
				LOG_FLAG, __func__, i,
				(unsigned int)hdl, 
				(unsigned int)user_id, 
				(unsigned int)wake_up_interval_ms);
			
			return i;
		}
	}

	pr_err("[  err][%s] %s: no enough proc info \r\n", 
				LOG_FLAG, __func__);

	return -EINVAL;
}

int sia81xx_timer_task_unregister(
	uint32_t hdl, 
	uint32_t user_id)
{
	sia81xx_timer_task_t *timer = NULL;
	int i = 0;

	if(hdl >= ARRAY_SIZE(timer_tasks)) {
		pr_err("[  err][%s] %s: bad hdl : %u \r\n", 
			LOG_FLAG, __func__, hdl);
		return -EINVAL;
	}
	timer = &timer_tasks[hdl];

	if(INVALID_USER_ID == user_id) {
		pr_err("[  err][%s] %s: user_id invalid : 0x%08x \r\n", 
			LOG_FLAG, __func__, user_id);
		return -EINVAL;
	}

	/* check is this user id have been exist */
	for(i = 0; i < ARRAY_SIZE(timer->proc); i ++) {
		if(user_id == timer->proc[i].user_id) {
			memset(&timer->proc[i], 0, sizeof(timer->proc[i]));
			timer->proc[i].user_id = INVALID_USER_ID;
		}
	}

	return 0;
}

int sia81xx_timer_task_start(
	uint32_t hdl)
{
	int ret = 0;
	sia81xx_timer_task_t *timer = NULL;

	if(hdl >= ARRAY_SIZE(timer_tasks)) {
		pr_err("[  err][%s] %s: bad hdl : %u \r\n", 
			LOG_FLAG, __func__, hdl);
		return -EINVAL;
	}
	timer = &timer_tasks[hdl];

	mutex_lock(&timer->task_mutex);

	if(SIA81XX_TASK_ENABLE != atomic_read(&timer->task_state)) {
		atomic_set(&timer->switch_flag, 1);
	} else {
		goto done;
	}

	if(SIA81XX_TASK_DISABLE != atomic_read(&timer->task_state)) {
		pr_err("[  err][%s] %s: task_state error, state : %d \r\n", 
			LOG_FLAG, __func__, atomic_read(&timer->task_state));
		ret = -EINVAL;
		goto err;
	}

	//set task state flag first
	atomic_set(&timer->task_state, SIA81XX_TASK_ENABLE);

	timer->task = kthread_create(
		sia81xx_timer_task_processor, timer, "sia81xx_timer_task_processor");
	if(IS_ERR(timer->task)) {
		pr_err("[  err][%s] %s: kthread_create fail, err code : %ld \r\n", 
			LOG_FLAG, __func__, PTR_ERR(timer->task));
		ret = -ECHILD;
		goto err;
	}
	
	if((ret = wake_up_process(timer->task)) < 0) {
		pr_err("[  err][%s] %s: wake_up_process fail, err code : %d \r\n", 
			LOG_FLAG, __func__, ret);
		ret = -EFAULT;
		goto err;
	}

done :
	mutex_unlock(&timer->task_mutex);
	return 0;

err :
	timer->task = NULL;
	atomic_set(&timer->task_state, SIA81XX_TASK_DISABLE);

	mutex_unlock(&timer->task_mutex);
	return ret;
}

int sia81xx_timer_task_stop(
	uint32_t hdl)
{
	sia81xx_timer_task_t *timer = NULL;

	if(hdl >= ARRAY_SIZE(timer_tasks)) {
		pr_err("[  err][%s] %s: bad hdl : %u \r\n", 
			LOG_FLAG, __func__, hdl);
		return -EINVAL;
	}
	timer = &timer_tasks[hdl];

	mutex_lock(&timer->task_mutex);

	if(SIA81XX_TASK_DISABLE != atomic_read(&timer->task_state)) {
		atomic_set(&timer->switch_flag, 1);
	}
	
	atomic_set(&timer->task_state, SIA81XX_TASK_DISABLE);
	timer->task = NULL;

	mutex_unlock(&timer->task_mutex);
	
	return 0;
}

int sia81xx_timer_task_init(void)
{
	int i = 0, j = 0;
	
	pr_info("[ info][%s] %s: run !! ", 
			LOG_FLAG, __func__);

	for(i = 0; i < ARRAY_SIZE(timer_tasks); i++) {
		atomic_set(&timer_tasks[i].task_state, SIA81XX_TASK_DISABLE);
		atomic_set(&timer_tasks[i].switch_flag, 0);
		timer_tasks[i].task = NULL;
		mutex_init(&timer_tasks[i].task_mutex);
		memset(timer_tasks[i].proc, 0, sizeof(timer_tasks[i].proc));

		for(j = 0; j < ARRAY_SIZE(timer_tasks[i].proc); j++) {
			/* to avoid zero user_id is considered has been used */
			timer_tasks[i].proc[j].user_id = INVALID_USER_ID;
		}
	}

	return 0;
}

void sia81xx_timer_task_exit(void)
{
	int i = 0;
	
	pr_info("[ info][%s] %s: run !! ", 
		LOG_FLAG, __func__);

	for(i = 0; i < ARRAY_SIZE(timer_tasks); i++) {
		if(NULL != timer_tasks[i].task) {
			kthread_stop(timer_tasks[i].task);
		}
	}

	return ;
}


