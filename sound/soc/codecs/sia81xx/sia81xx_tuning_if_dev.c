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
#define LOG_FLAG	"gift_tuning"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "sia81xx_tuning_if.h"


#define DEVICE_NAME "sia81xx_tuning_if"

#define TIMEOUT_MS				(200)
#define MAX_CMD_LEN				(4096)
#define MAX_FILE_MAP_NUM		(16)

/* same as audio_task.h file in android project */
enum {
    TASK_SCENE_PHONE_CALL = 0,
    TASK_SCENE_VOICE_ULTRASOUND,
    TASK_SCENE_PLAYBACK_MP3,
    TASK_SCENE_RECORD,
    TASK_SCENE_VOIP,
    TASK_SCENE_SPEAKER_PROTECTION,
    TASK_SCENE_VOW,
    TASK_SCENE_SIZE, /* the size of tasks */
    TASK_SCENE_INVALID,
    TASK_SCENE_CONTROLLER = 0xFF
};

enum {
	CMD_SIA81XX_TUNING_IF_ADD_CAL_ID = _IOW(0x10, 0xE0, uint32_t),
	CMD_SIA81XX_TUNING_IF_RM_CAL_ID = _IOW(0x10, 0xE1, uint32_t),
	CMD_SIA81XX_TUNING_IF_PICK_CMD = _IOR(0x10, 0xE2, uint64_t),
	CMD_SIA81XX_TUNING_IF_TEST = _IOWR(0x10, 0xE3, uint32_t),
};


enum {
	MODULE_STATE_IDLE,
	MODULE_STATE_WRITEN	
};

enum param_opt {
	OPT_SET_CAL_VAL,
	OPT_GET_CAL_VAL,
	OPT_REPLY,//only aurisys effect write this opt when answer the OPT_GET_CAL_VAL opt
	OPT_INVALID,
};

struct dev_comm_data {
	uint32_t opt;//get/set or reply
	uint32_t param_id;//the same to qcom define, to distinguish the param type(set topo or set/read parameter)
	uint32_t payload_size;
	uint8_t payload[];
} __packed;

#define DEV_COMM_DATA_LEN(data) \
	(sizeof(struct dev_comm_data) + data->payload_size)

struct cal_module_unit {
	struct file *fp;
	uint32_t cal_id;//task scene:task_scene_t
	uint32_t module_id;//the same to qcom define, although this module id has nothing todo
	struct dev_comm_data *cmd;
	atomic_t state;//MODULE_STATE_IDLE or MODULE_STATE_WRITEN
	spinlock_t lock;	
};

struct cal_module_unit cal_module_table[MAX_FILE_MAP_NUM];



static struct cal_module_unit *is_cal_id_exist(uint32_t cal_id)
{
	int i = 0;

	for(i = 0; i < MAX_FILE_MAP_NUM; i++) {
		if((cal_id == cal_module_table[i].cal_id) && 
			(NULL != cal_module_table[i].fp))
			return &cal_module_table[i];
	}

	return NULL;
}

static struct cal_module_unit *is_file_exist(struct file *filp)
{
	int i = 0;

	if(NULL == filp)
		return NULL;

	for(i = 0; i < MAX_FILE_MAP_NUM; i++) {
		if(filp == cal_module_table[i].fp)
			return &cal_module_table[i];
	}

	return NULL;
}


static struct cal_module_unit *get_one_can_use_cal_module(uint32_t cal_id) 
{
	struct cal_module_unit *module = NULL;
	int i = 0;

	if(NULL != (module = is_cal_id_exist(cal_id))) {
		return module;
	}

	for(i = 0; i < MAX_FILE_MAP_NUM; i++) {
		if(NULL == cal_module_table[i].fp)
			return &cal_module_table[i];
	}

	return NULL;
}

static int record_cal_module(struct file *filp, 
	uint32_t cal_id, uint32_t module_id)
{
	struct cal_module_unit *module = NULL;

	module = get_one_can_use_cal_module(cal_id);
	if(NULL == module)
		return -EINVAL;

	pr_debug("[debug][%s] %s: cal_id = 0x%08x, module_id = 0x%08x \r\n", 
		LOG_FLAG, __func__, cal_id, module_id);

	module->fp= filp;
	module->cal_id = cal_id;
	module->module_id = module_id;
	atomic_set(&module->state, MODULE_STATE_IDLE);
	if(NULL == module->cmd)
		module->cmd = (struct dev_comm_data *)kmalloc(MAX_CMD_LEN, GFP_KERNEL);
	
	spin_lock_init(&module->lock);

	return 0;
}

static void delete_cal_module(struct file *filp)
{
	struct cal_module_unit *module = NULL;

	module = is_file_exist(filp);
	if(NULL == module)
		return ;

	spin_lock(&module->lock);

	module->fp = NULL;
	module->cal_id = 0;
	atomic_set(&module->state, MODULE_STATE_IDLE);
	if(NULL != module->cmd) {
		kfree(module->cmd);
		module->cmd = NULL;
	}

	spin_unlock(&module->lock);

	return ;
}

/*************************************************************************
* tuning interface provide for sia81xx_socket.c
* ************************************************************************/
static int write_payload_to_cal_module(struct cal_module_unit *module, 
	uint32_t opt, uint32_t param_id, uint32_t size, uint8_t *payload)
{
	if(NULL == module)
		return -EINVAL;

	if(NULL == module->cmd)
		return -EINVAL;

	spin_lock(&module->lock);

	module->cmd->opt = opt;
	module->cmd->param_id = param_id;
	module->cmd->payload_size = size;
	if(NULL != payload)
		memcpy(module->cmd->payload, payload, size);

	atomic_set(&module->state, MODULE_STATE_WRITEN);

	spin_unlock(&module->lock);

	return 0;
}

static int read_payload_from_cal_module(struct cal_module_unit *module, 
	uint32_t *param_id, uint32_t size, uint8_t *payload)
{
	if(NULL == module)
		return -EINVAL;

	if(NULL == module->cmd)
		return -EINVAL;

	if(module->cmd->payload_size > size)
		return -EINVAL;

	spin_lock(&module->lock);

	if(NULL != param_id)
		*param_id = module->cmd->param_id;
	
	memcpy(payload, module->cmd->payload, module->cmd->payload_size);
	
	atomic_set(&module->state, MODULE_STATE_IDLE);

	spin_unlock(&module->lock);

	return 0;
}

static uint32_t get_cal_module_param_opt(struct cal_module_unit *module) 
{
	if(NULL == module)
		return OPT_INVALID;

	if(NULL == module->cmd)
		return OPT_INVALID;

	return module->cmd->opt;
}

static unsigned long sia81xx_tuning_if_dev_type_open(uint32_t cal_id)
{
	struct cal_module_unit *module = NULL;

	module = is_cal_id_exist(cal_id);
	if(NULL == module)
		return 0;

	return (unsigned long)module;
}

static int sia81xx_tuning_if_dev_type_close(unsigned long handle)
{
	return 0;
}

static int sia81xx_tuning_if_dev_type_read(unsigned long handle, 
	uint32_t mode_id, uint32_t param_id, uint32_t size, uint8_t *payload)
{
	int ret = 0, timer = TIMEOUT_MS;
	struct cal_module_unit *module = NULL;
	uint32_t reply_pid = 0;

	module = (struct cal_module_unit *)handle;
	if(NULL == module)
		return -EINVAL;

	if(0 != (ret = write_payload_to_cal_module(module, 
		OPT_GET_CAL_VAL, param_id, size, payload))) {
		
		pr_err("[  err][%s] %s: write_payload_to_cal_module ret = %d !! \r\n", 
			LOG_FLAG, __func__, ret);
		
		return -EINVAL;
	}

	/* wait for cal module read payload */
	while(atomic_read(&module->state) != MODULE_STATE_IDLE) {

		/* if had been recv reply, then read the reply */
		if(OPT_REPLY == get_cal_module_param_opt(module))
			break;
		
		timer --;
		if(timer <= 0) {
			pr_err("[  err][%s] %s: wait read event timeout !! \r\n", 
				LOG_FLAG, __func__);
			return -ENOTBLK;
		}

		msleep(1);
	}

	timer = TIMEOUT_MS;

	/* wait for cal module write reply payload */
	while(atomic_read(&module->state) != MODULE_STATE_WRITEN) {

		timer --;
		if(timer <= 0) {
			pr_err("[  err][%s] %s: wait reply event timeout !! \r\n", 
				LOG_FLAG, __func__);
			return -ENOTBLK;
		}

		msleep(1);
	}

	/* recv data is not the type of reply */
	if(OPT_REPLY != get_cal_module_param_opt(module)) {
		pr_err("[  err][%s] %s: module opt = %u \r\n", 
			LOG_FLAG, __func__, get_cal_module_param_opt(module));
		return -EINVAL;
	}

	if(0 != (ret = read_payload_from_cal_module(module, 
		&reply_pid, size, payload))) {
		
		pr_err("[  err][%s] %s: write_payload_to_cal_module ret = %d !! \r\n", 
			LOG_FLAG, __func__, ret);
		
		return -EINVAL;
	}

	if(reply_pid != param_id) {
		pr_err("[  err][%s] %s: reply_pid = %u, param_id = %u \r\n", 
			LOG_FLAG, __func__, reply_pid, param_id);
		return -EINVAL;
	}

	return 0;
}

static int sia81xx_tuning_if_dev_type_write(unsigned long  handle, 
	uint32_t mode_id, uint32_t param_id, uint32_t size, uint8_t *payload)
{
	int ret = 0, timer = TIMEOUT_MS;
	struct cal_module_unit *module = NULL;

	module = (struct cal_module_unit *)handle;
	if(NULL == module)
		return -EINVAL;

	if(0 != (ret = write_payload_to_cal_module(module, 
		OPT_SET_CAL_VAL, param_id, size, payload))) {
		
		pr_err("[  err][%s] %s: write_payload_to_cal_module ret = %d !! \r\n", 
			LOG_FLAG, __func__, ret);
		
		return -EINVAL;
	}

	/* wait for cal module read payload */
	while(atomic_read(&module->state) != MODULE_STATE_IDLE) {
		
		timer --;
		if(timer <= 0) {
			pr_err("[  err][%s] %s: wait read event timeout !! \r\n", 
				LOG_FLAG, __func__);
			return -ENOTBLK;
		}

		msleep(1);
	}

	return size;
}

/*************************************************************************
* end - tuning interface provide for sia81xx_socket.c
* ************************************************************************/

static ssize_t sia81xx_tuning_if_dev_read(struct file *fp, 
	char __user *buf, size_t count, loff_t *f_pos)
{
	pr_debug("[debug][%s] %s: run !! \r\n", LOG_FLAG, __func__);
	
	return 0;
}

static ssize_t sia81xx_tuning_if_dev_write(struct file *fp, 
	const char __user *buf, size_t count, loff_t *f_pos)
{
	struct cal_module_unit *module = NULL;

	pr_debug("[debug][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	module = is_file_exist(fp);
	if(NULL == module)
		return -ENOENT;

	if(0 != copy_from_user(module->cmd, buf, count))
		return 0;

	/* fixed me : to option "state" should be in only one place, 
	 * but there will change the "state" value, it is not safe */
	atomic_set(&module->state, MODULE_STATE_WRITEN);
	
	return count;
}

static long sia81xx_tuning_if_dev_unlocked_ioctl(struct file *fp, 
	unsigned int cmd, unsigned long arg)
{	
	switch(cmd) {
		case CMD_SIA81XX_TUNING_IF_ADD_CAL_ID :
		{
			uint32_t *args;
			uint32_t cal_id, module_id ;
			int ret = 0;

			args = (uint32_t *)arg;

			if(0 != copy_from_user(&cal_id, args, sizeof(cal_id)))
				return -EINVAL;
			args ++;
			if(0 != copy_from_user(&module_id, args, sizeof(module_id)))
				return -EINVAL;
			
			if(0 != (ret = record_cal_module(fp, cal_id, module_id))) {
				pr_err("[  err][%s] %s: err !!record_cal_module ret = %d, "
					"cal_id = %u \r\n", 
					LOG_FLAG, __func__, ret, cal_id);
			}
			
			break;
		}
		
		case CMD_SIA81XX_TUNING_IF_RM_CAL_ID :
		{
			delete_cal_module(fp);
			
			break;
		}

		case CMD_SIA81XX_TUNING_IF_PICK_CMD :
		{
			struct cal_module_unit *module = NULL;

			module = is_file_exist(fp);
			if(NULL == module)
				return -ENOENT;

			if(MODULE_STATE_WRITEN != atomic_read(&module->state))
				return -ENOEXEC;

			if(OPT_REPLY == get_cal_module_param_opt(module))
				return -ENOEXEC;

			if(0 != copy_to_user((void *)arg, module->cmd, 
				DEV_COMM_DATA_LEN(module->cmd)))
				return -EINVAL;

			/* fixed me : to option "state" should be in only one place, 
			 * but there will change the "state" value, it is not safe */
			atomic_set(&module->state, MODULE_STATE_IDLE);
			
			break;
		}
		
		case CMD_SIA81XX_TUNING_IF_TEST :
		{
			break;
		}
		
		default :
		{
			return -EPERM;
		}
	}
	
	return 0 ;
}

static int sia81xx_tuning_if_dev_open(struct inode *inode, struct file *fp)
{
	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);
	
	return 0 ;
}

static int sia81xx_tuning_if_dev_close(struct inode *inode, struct file *fp)
{
	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	delete_cal_module(fp);
	
	return 0 ;
}

static struct file_operations sia81xx_tuning_if_ops = {
	.owner = THIS_MODULE ,
	.open = sia81xx_tuning_if_dev_open,
	.release = sia81xx_tuning_if_dev_close,
	.read = sia81xx_tuning_if_dev_read,
	.write = sia81xx_tuning_if_dev_write,
	.unlocked_ioctl = sia81xx_tuning_if_dev_unlocked_ioctl,
	.compat_ioctl = sia81xx_tuning_if_dev_unlocked_ioctl,
};

static struct miscdevice sia81xx_tuning_if_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR ,
	.name = DEVICE_NAME ,
	.fops = &sia81xx_tuning_if_ops,
};

static int open_tuning_if_dev(void)
{
	int ret_error ;

	int ret = misc_register(&sia81xx_tuning_if_misc_dev);
	if(ret != 0){
	   ret_error = ret ;
	   pr_info("[ info][%s] %s: err !! \r\n", LOG_FLAG, __func__);
	   goto fair ;
	}

	pr_info("[ info][%s] %s: success !! \r\n", LOG_FLAG, __func__);
	return ret ;
	
	fair:
	return ret_error ;
}

static void close_tuning_if_dev(void)
{

	misc_deregister(&sia81xx_tuning_if_misc_dev);

	return;
}

static int sia81xx_tuning_if_init(void) 
{
	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	memset(cal_module_table, 0, sizeof(cal_module_table));

	open_tuning_if_dev();

	return 0;
}

static void sia81xx_tuning_if_exit(void) 
{
	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	close_tuning_if_dev();

	memset(cal_module_table, 0, sizeof(cal_module_table));
}


/* provide for sia81xx_socket.c */
struct sia81xx_cal_opt tuning_if_opt = {
	.init = sia81xx_tuning_if_init,
	.exit = sia81xx_tuning_if_exit,
	.open = sia81xx_tuning_if_dev_type_open,
	.close = sia81xx_tuning_if_dev_type_close,
	.read = sia81xx_tuning_if_dev_type_read,
	.write = sia81xx_tuning_if_dev_type_write,
};




