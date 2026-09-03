/**
 * @file battery_secret_logic.c
 * @brief 电池加密芯片逻辑模块实现
 * 
 * 该模块通过与底层电池设备交互，提供读写soh/cycle count部分的逻辑，并支持多线程环境下的数据同步。
 * 
 * @author longcheer
 * @date 2025年02月14日
 */

#define pr_fmt(fmt) "[BatSecLogic %s,%d] " fmt "\n", __func__, __LINE__

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include "battery_secret_logic.h"
#include "battery_secret_class.h"
#include "secret_common.h"

#define MODULE_TAG "secret_manager"
//#include "lc_logfs_class.h"

//#define DUAL_BATTERY_SECRET	 1
// #define IS_AUTH_WRITE       1
#define INVALID_VALUE 		0xFFFF
#define MAX_RETRY_COUNT		5 // 读取或者写设备失败，最大retry次数
#define RETRY_INTERVAL_MS 	10 // 读写设备失败后，retry的时间间隔

// g_xxx 用于缓存，防止反复从IC中读取数据
#define PARAM_LEN	32
#define BATT_SN_LEN	40

u32 w1_timings_cfg[16];
EXPORT_SYMBOL(w1_timings_cfg);

//static int g_uisoh = INVALID_VALUE;
static int g_cycle_count = INVALID_VALUE;
static int g_rawsoh = INVALID_VALUE;
static int g_batt_id = INVALID_VALUE;
static int g_is_battery_auth = INVALID_VALUE;
static bool g_clear_flag = false;
static char batt_manufacture_date[PARAM_LEN] = {0};
static char batt_first_use_time[PARAM_LEN] = {0};
static char batt_sn[BATT_SN_LEN] = {0};
static u8 batt_uisoh[UISOH_LEN] = {0};
static const u8 *g_uisoh = NULL;
static const char *g_batt_manufacture_date = NULL;
static const char *g_batt_first_use_time = NULL;
static const char *g_batt_sn = NULL;


// for parse cmdline
#define SECRET_ATTRS_PREFIX "secret_attrs="
static struct secret_attr  *g_s_attr_head = NULL;
static char *secret_attrs = NULL;

// 用于测试调试
static struct kobject *battery_secret_kobj;
static int g_fake_cycle_count = INVALID_VALUE;
static int g_fake_rawsoh = INVALID_VALUE;
static int g_fake_auth = INVALID_VALUE;
// end

static struct secret_device *master = NULL;
#ifdef DUAL_BATTERY_SECRET
static struct secret_device *slave = NULL;
#endif
static struct power_supply *batt_verify_psy = NULL;

#define DEFAULT_UPDATE_LOOP_INTERVAL_MS	 (10*60*60*1000)  //10h
struct sync_update_work_data *sync_update_data = NULL;
static int loop_interval_index = 0;
unsigned int loop_interval_ms_configs[] = { DEFAULT_UPDATE_LOOP_INTERVAL_MS, \
					(5*60*1000)/* 5min*/, \
					(10*60*1000)/* 10min */ ,\
					(30*60*1000)/* 30min */ ,\
					(60*60*1000)/* 60min */ ,\
					DEFAULT_UPDATE_LOOP_INTERVAL_MS, \
				};

static void get_secret_dev(void){
	// TODO
	master = get_secret_by_name(MASTER_SECRET);
	if(IS_ERR_OR_NULL(master)){
		pr_err("Failed to get master secret device\n");
	}
#ifdef DUAL_BATTERY_SECRET
	slave = get_secret_by_name(SLAVE_SECRET);
	if(IS_ERR_OR_NULL(slave)){
		pr_err("Failed to get slave secret device\n");
	}
#endif
}

// 唤醒update线程的接口
static void trigger_wakeup(struct sync_update_work_data *sync_update_data)
{
    if (!atomic_cmpxchg(&sync_update_data->condition, 0, 1)) {
        wake_up_interruptible(&sync_update_data->wq);
    }
}

/**
 * ll_set_obj_to_dev() 或者 ll_get_obj_from_dev()
 * 这种类型的函数直接调用芯片驱动的ops
 * 双IC逻辑，都在这个类型的函数中处理
 *
 */
 static int ll_set_obj_to_dev(struct secret_device *secret_dev, enum SECRET_OBJ obj, union SECRET_OBJ_VAL val)
 {
	 int ret = 0;
	if(!secret_dev || !(secret_dev->ops)) {
		pr_err("secret_dev device not found\n");
		return -ENODEV;
	}
	mutex_lock(&secret_dev->ops_lock);
	switch(obj){
		case UISOH:
			if(g_rawsoh == INVALID_VALUE){
				pr_err("rawsoh is invalid!\n");
				break;
			}
			if(secret_dev->ops->set_uisoh) {
				ret = secret_dev->ops->set_uisoh(secret_dev, val.battery_uisoh, UISOH_LEN, g_rawsoh);
			} else {
				ret = -ENOEXEC;
			}
			if(ret == 0){
				memcpy(batt_uisoh, val.battery_uisoh, sizeof(batt_uisoh));
				g_uisoh = batt_uisoh; // 更新设置成功的最新值到缓存中
				pr_err("update uisoh(%d) to device success!\n", UISOH_LEN);
			} else {
				pr_err("update uisoh to device fail, ret=%d!\n", ret);
			}
			break;
		case RAWSOH:
			if(secret_dev->ops->set_rawsoh) {
				ret = secret_dev->ops->set_rawsoh(secret_dev, val.rawsoh);
			} else { 
				ret = -ENOEXEC;
			}
			if(ret == 0){
				g_rawsoh = val.rawsoh; // 更新设置成功的最新值到缓存中
				pr_err("update rawsoh(%d) to device success!\n", val.rawsoh);
			} else {
				pr_err("update rawsoh to device fail, ret=%d!\n", ret);
			}
			break;
		case CYCLE_COUNT:
			if(secret_dev->ops->set_cycle_count) {
				ret = secret_dev->ops->set_cycle_count(secret_dev, val.cycle_count);
			} else { 
				ret = -ENOEXEC;
			}
			if(ret == 0){
				g_cycle_count = val.cycle_count; // 更新设置成功的最新值到缓存中
				pr_err("update cycle count(%d) to device success!\n", val.cycle_count);
			} else {
				pr_err("update cycle count to device fail, ret=%d!\n", ret);
			}
			break;
		case BATTERY_FIRST_USE_TIME:
			g_batt_first_use_time = NULL;
			memset(batt_first_use_time, 0, sizeof(batt_first_use_time));
			if(secret_dev->ops->set_battery_first_use_time) {
				ret=secret_dev->ops->set_battery_first_use_time(secret_dev,val.battery_first_use_time);
			} else {
				ret = -ENOEXEC;
			}
			if(ret == 0) {
				// 设置成功后，把新的值更新到缓存中
				strlcpy(batt_first_use_time, val.battery_first_use_time, sizeof(batt_first_use_time));
				g_batt_first_use_time = batt_first_use_time;
				pr_err("set battery first use time(%s) to dev success!\n", g_batt_first_use_time);
			}
			break;
		case CLEAR_CYCLE:
			if(secret_dev->ops->clear_cycle_count) {
				ret = secret_dev->ops->clear_cycle_count(secret_dev);
			} else {
				ret = -ENOEXEC;
			}
			if(ret == 0) {
				g_clear_flag = true; // 更新从设备中读取到的值到缓存中
				pr_err("set clear_cycle to dev success!\n");
			} else {
				pr_err("set clear_cycle to dev fail, ret=%d!\n", ret);
			}
			break;
		default:
			pr_err("%d Invaild secret object!", obj);
			ret =  -EINVAL;
	}
	mutex_unlock(&secret_dev->ops_lock);
	return ret;
}

 static int ll_get_obj_from_dev(struct secret_device *secret_dev, enum SECRET_OBJ obj, union SECRET_OBJ_VAL *val)
 {
	 int ret = 0;
	 int intval;
	if(!secret_dev || !(secret_dev->ops)) {
		pr_err("secret_dev not found\n");
		return -ENODEV;
	}
	if(!val) {
		pr_err("invaild paramters!\n");
		return -EINVAL;
	}
	mutex_lock(&secret_dev->ops_lock);
	switch(obj){
		case UISOH:
			pr_debug(" UISOH entry \n");
			memset(val->battery_uisoh, 0, UISOH_LEN);
			if(secret_dev->ops->get_uisoh) {
				ret = secret_dev->ops->get_uisoh(secret_dev, val->battery_uisoh, UISOH_LEN);
			} else { 
				ret = -ENOEXEC;
			}
			if(ret == 0) {
				memset(batt_uisoh, 0, sizeof(batt_uisoh));
				memcpy(batt_uisoh, val->battery_uisoh, sizeof(batt_uisoh));
				g_uisoh = batt_uisoh; // 更新从设备中读取到的值到缓存中
				pr_err("get battery uisoh(%lubytes) form device sucess\n", sizeof(batt_uisoh));
			} else {
				pr_err("get battery uisoh form device fail, ret=%d!\n", ret);
			}
			break;
		case RAWSOH:
			if(secret_dev->ops->get_rawsoh) {
				ret = secret_dev->ops->get_rawsoh(secret_dev, &intval);
			} else { 
				ret = -ENOEXEC;
			}
			if(ret == 0) {
				g_rawsoh = intval; // 更新从设备中读取到的值到缓存中
				val->rawsoh = intval;
				pr_err("get rawsoh(%d) form device success!\n", intval);
			} else {
				pr_err("get rawsoh form dev fail, ret=%d!\n", ret );
			}
			break;
		case CYCLE_COUNT:
			if(secret_dev->ops->get_cycle_count) {
				ret = secret_dev->ops->get_cycle_count(secret_dev, &intval);
			} else { 
				ret = -ENOEXEC;
			}
			if ( ret == 0) {
				g_cycle_count = intval; // 更新从设备中读取到的值到缓存中
				val->cycle_count = intval;
				pr_err("get cycle count(%d) form dev success!\n", intval);
			} else {
				pr_err("get cycle count form dev fail!\n");
			}
			break;
		case BATTERY_ID:
			if(secret_dev->ops->get_battery_id) {
				ret = secret_dev->ops->get_battery_id(secret_dev, &intval);
			} else {
				ret = -ENOEXEC;
			}
			if ( ret == 0) {
				g_batt_id = intval; // 更新从设备中读取到的值到缓存中
				val->battery_id = intval;
				pr_err("get battery id(%d) form dev success!\n", intval);
			} else {
				pr_err("get battery id form dev fail!\n");
			}
			break;
		case IS_BATTERY_AUTH:
			if(secret_dev->ops->is_battery_auth) {
				ret = secret_dev->ops->is_battery_auth(secret_dev, &intval);
			} else { 
				ret = -ENOEXEC;
			}
			if (ret == 0) {
				g_is_battery_auth = intval; // 更新从设备中读取到的值到缓存中
				val->is_auth = intval;
				pr_debug("get is_auth(%d) form device success!\n", intval);
			} else {
				pr_err("get is_auth form device fail, ret=%d!\n", ret);
			}
			break;
		case BATTERY_SN:
			g_batt_sn = NULL;
			memset(batt_sn, 0, sizeof(batt_sn));
			if(secret_dev->ops->get_battery_sn) {
				ret = secret_dev->ops->get_battery_sn(secret_dev, batt_sn, sizeof(batt_sn));
			} else { 
				ret = -ENOEXEC;
			}
			if(ret == 0) {
				g_batt_sn = batt_sn; // 更新从设备中读取到的值到缓存中
				val->battery_sn = batt_sn;
				pr_err("get battery sn(%s) form device sucess\n", batt_sn);
			} else {
				pr_err("get battery sn form device fail, ret=%d!\n", ret);
			}
			break;
		case BATTERY_MANUFACTURE_DATE:
			g_batt_manufacture_date = NULL;
			memset(batt_manufacture_date, 0, sizeof(batt_manufacture_date));
			if(secret_dev->ops->get_battery_manufacture_date) {
				ret = secret_dev->ops->get_battery_manufacture_date(secret_dev, batt_manufacture_date, sizeof(batt_manufacture_date));
			} else { 
				ret = -ENOEXEC;
			}
			if (ret == 0) {
				g_batt_manufacture_date = batt_manufacture_date; // 更新从设备中读取到的值到缓存中
				val->battery_manufacture_date = batt_manufacture_date;
				pr_err("get batt_manufacture_date(%s) form device sucess\n", batt_manufacture_date);
			} else {
				pr_err("get batt_manufacture_date form device fail, ret=%d!\n", ret);
			}
			break;
		case BATTERY_FIRST_USE_TIME:
			g_batt_first_use_time = NULL;
			memset(batt_first_use_time, 0, sizeof(batt_first_use_time));
			if(secret_dev->ops->get_battery_first_use_time) {
				ret = secret_dev->ops->get_battery_first_use_time(secret_dev, batt_first_use_time, (sizeof(batt_first_use_time) - 1));
			} else { 
				ret = -ENOEXEC;
			}
			if (ret == 0) {
				g_batt_first_use_time = batt_first_use_time; // 更新从设备中读取到的值到缓存中
				val->battery_first_use_time = batt_first_use_time;
				pr_err("get batt_first_use_time(%s) form device sucess\n", batt_first_use_time);
			} else {
				pr_err("get batt_first_use_time form device fail, ret=%d!\n", ret);
			}
			break;
		default:
			pr_err("%d Invaild secret object!", obj);
			ret = -EINVAL;
	}
	mutex_unlock(&secret_dev->ops_lock);
	return ret;
 }

static void do_update_work(void)
{
	//int ret;
	int retry = 0;
	int update_fail_flag = 0; // 1 for debug
	int sleep_time_ms = RETRY_INTERVAL_MS;
	static int retry_uisoh = 0;
	static int retry_rawsoh = 0;
	static int retry_cycle_count = 0;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	// update uisoh
	pr_err("enter.\n");
	retry = 0;
	val.battery_uisoh = sync_update_data->uisoh;

	if(sync_update_data->uisoh_len){
		while(++retry <= MAX_RETRY_COUNT){
			if(ll_set_obj_to_dev(secret_dev, UISOH, val) == 0){
				memcpy(batt_uisoh, sync_update_data->uisoh, sync_update_data->uisoh_len);
				g_uisoh = batt_uisoh; // 同时把更新后的值，更新到缓存的全局变量中
				retry = 0;
				retry_uisoh = 0;
				// 更新成功，则清除当前设置值
				sync_update_data->uisoh_len = 0;
				break;
			} else {
				pr_err("update UISOH fail, retry...\n");
			}
			msleep(sleep_time_ms);
		}
		if(retry>MAX_RETRY_COUNT){
			retry_uisoh++;
			update_fail_flag ++;
			pr_err("update uisoh fail count:%d \n", retry_uisoh);
		}
	}
	
	// update raw soh
	retry = 0;
	val.rawsoh = sync_update_data->rawsoh;
	if(sync_update_data->rawsoh != INVALID_VALUE && g_rawsoh != sync_update_data->rawsoh){
		while(++retry <= MAX_RETRY_COUNT ){
			if(ll_set_obj_to_dev(secret_dev, RAWSOH, val) == 0){
				g_rawsoh = sync_update_data->rawsoh; // 同时把更新后的值，更新到缓存的全局变量中
				retry = 0;
				retry_rawsoh = 0;
				sync_update_data->rawsoh = INVALID_VALUE; // 更新成功，则清除当前设置值
				break;
			} else {
				pr_err("update rawsoh fail, retry...\n");
			}
			msleep(sleep_time_ms);
		}
		if(retry>MAX_RETRY_COUNT){
			update_fail_flag ++;
			retry_rawsoh++;
			pr_err("update rawsoh fail count:%d \n", retry_rawsoh);
		}
	}
	// update cycle count
	retry = 0;
	val.cycle_count = sync_update_data->cycle_count;
	if(sync_update_data->cycle_count != INVALID_VALUE && g_cycle_count != sync_update_data->cycle_count){
		while(++retry <= MAX_RETRY_COUNT ){
			if(ll_set_obj_to_dev(secret_dev, CYCLE_COUNT, val) == 0){
				g_cycle_count = sync_update_data->cycle_count; // 同时把更新后的值，更新到缓存的全局变量中
				retry = 0;
				retry_cycle_count = 0;
				sync_update_data->cycle_count = INVALID_VALUE; // 更新成功，则清除当前设置值
				break;
			} else {
				pr_err("update cycle_count fail, retry...\n");
			}
			msleep(sleep_time_ms);
		}
		if(retry>MAX_RETRY_COUNT){
			update_fail_flag ++;
			retry_cycle_count++;
			pr_err("update cycle count fail count:%d \n", retry_cycle_count);
		}
	}
	if(update_fail_flag > 0) {
		loop_interval_index ++;
		if(loop_interval_index >= ARRAY_SIZE(loop_interval_ms_configs)){
			loop_interval_index = (ARRAY_SIZE(loop_interval_ms_configs)-1);
		}
	} else {
		loop_interval_index = 0;
	}
	pr_debug("loop_interval_index = %d \n", loop_interval_index);
	return;
}

static int battery_secret_update_work_thread(void *data)
{
    struct sync_update_work_data *sync_update_data = data;
	pr_info("start. \n");
    while (!kthread_should_stop()) {
        // 执行具体工作任务
        do_update_work();

        // 重置条件并进入等待
        atomic_set(&sync_update_data->condition, 0);
        
        // 等待唤醒或超时（同时处理两种唤醒方式）
        wait_event_interruptible_timeout(
            sync_update_data->wq,
            atomic_read(&sync_update_data->condition) || kthread_should_stop(),
            msecs_to_jiffies(loop_interval_ms_configs[loop_interval_index])
        );

        // 检查是否超时
        if (!atomic_read(&sync_update_data->condition)) {
            pr_info("Wakeup by timeout\n");
        }
    }
    return 0;
}

static int init_sync_update_data(void)
{
	sync_update_data = kzalloc(sizeof(*sync_update_data), GFP_KERNEL);
	if(!sync_update_data){
		pr_err("Failed to allocate memory for sync_update_data\n");
		return -ENOMEM;
	}
	memset(sync_update_data->uisoh, 0, 11);
	//sync_update_data->uisoh[11] = {0,};
	sync_update_data->uisoh_len = 0;
	sync_update_data->rawsoh = INVALID_VALUE;
	sync_update_data->cycle_count = INVALID_VALUE;
	init_waitqueue_head(&sync_update_data->wq);
	atomic_set(&sync_update_data->condition, 0);
	// 启动更新任务线程
	sync_update_data->update_thread = kthread_run(battery_secret_update_work_thread, sync_update_data, "secret_upd_work");
	if (IS_ERR(sync_update_data->update_thread)) {
        pr_err("Failed to start update thread\n");
        kfree(sync_update_data);
        return PTR_ERR(sync_update_data->update_thread);
    }
	return 0;
}

/***** 以下ll_get_xxx和ll_set_xxx 接口用于给class层调用 ****/
int ll_get_uisoh(u8 *data, size_t len){
	int ret = 0;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if( len!= UISOH_LEN ) {
		pr_err("INVALID uisoh len:%zu !\n", len);
		return -EINVAL;
	}
	if(g_uisoh != NULL) {
		memcpy(data, g_uisoh, len);
		return 0;
	}
	// 从设备中读取
	val.battery_uisoh = data;
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, UISOH, &val);
		if(ret == 0){
			pr_debug("get uisoh sucess!\n");
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret;
}

int ll_set_uisoh(u8 *data, size_t len)
{
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	int i = 0;
	int ret;
	if( len!= UISOH_LEN ) {
		pr_err("INVALID uisoh len:%zu !\n", len);
		return -EINVAL;
	}
	if(g_uisoh!=NULL && memcmp(g_uisoh, data, len) == 0  ){
		// 要设置的值和缓存值相同，无需重复设置
		return 0;
	}
	if(g_rawsoh == INVALID_VALUE) {
		ret = ll_get_obj_from_dev(secret_dev, RAWSOH, &val);
		if(ret) {
			pr_err("get rawsoh fail\n");
			return -EINVAL;
		}
	}
	//sync_update_data->uisoh = data;
	for (i = 0; i < len; i++) {
		sync_update_data->uisoh[i] = data[i];
	}
	sync_update_data->uisoh_len = len;
	trigger_wakeup(sync_update_data);
	return 0;
}

int ll_get_rawsoh(void)
{
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_fake_rawsoh!=INVALID_VALUE && g_fake_rawsoh>0){
		return g_fake_rawsoh;
	}
	if(g_rawsoh!=INVALID_VALUE){
		return g_rawsoh;
	}
	// 从设备中读取
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, RAWSOH, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret==0?val.rawsoh:ret;
}

int ll_set_rawsoh(int rawsoh)
{
	int ret = 0;
	if(rawsoh!=g_rawsoh) {
		sync_update_data->rawsoh = rawsoh;
		trigger_wakeup(sync_update_data);
	}
	return ret;
}

int ll_get_cycle_count(void){
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_fake_cycle_count!=INVALID_VALUE && g_fake_cycle_count>0)
	{
		return g_fake_cycle_count;
	}
	if(g_cycle_count!=INVALID_VALUE){
		return g_cycle_count;
	}
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, CYCLE_COUNT, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret==0?val.cycle_count:ret;
}

int ll_set_cycle_count(int cycle_count)
{
	int ret = 0;

	if (g_cycle_count != INVALID_VALUE && cycle_count <= g_cycle_count) {
		ret = -EINVAL;
		return ret;
	}
	sync_update_data->cycle_count = cycle_count;
	trigger_wakeup(sync_update_data);

	return ret;
}

int ll_clear_cycle_count(void)
{
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);

	val.clear_cycle = 0;
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_set_obj_to_dev(secret_dev, CLEAR_CYCLE, val);
		if(ret == 0){
			g_cycle_count = INVALID_VALUE;
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret;
}

const char *ll_get_battery_manufacture_date(void)
{	// 电池出厂时间
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_batt_manufacture_date)
		return g_batt_manufacture_date;

	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, BATTERY_MANUFACTURE_DATE, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return val.battery_manufacture_date;
}

const char *ll_get_battery_first_use_time(void)
{ // 电池首次使用时间
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_batt_first_use_time)
		return g_batt_first_use_time;

	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, BATTERY_FIRST_USE_TIME, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return val.battery_first_use_time;
}

int ll_set_battery_first_use_time(char *time)
{ 	// 电池首次使用时间
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	char temp_buf[PARAM_LEN];
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(!time || strlen(time)==0 || strlen(time) >= (sizeof(temp_buf)-1)){
		pr_err("time is null, set battery first use time fail \n");
		return -EINVAL;
	}
	strlcpy(temp_buf, time, sizeof(temp_buf));
	val.battery_first_use_time = temp_buf;
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_set_obj_to_dev(secret_dev, BATTERY_FIRST_USE_TIME, val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret;
}
int ll_get_battery_id(void)
{	// 电池ID
	int ret = 0;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_batt_id!=INVALID_VALUE){
		return g_batt_id;
	}
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, BATTERY_ID, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret==0?val.battery_id:ret;
}
const char *ll_get_battery_secret_name(void)
{	// 加密芯片名称
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(!secret_dev) {
		return NULL;
	}
	return secret_dev->chip_name;
}
int ll_is_battery_auth_success(void)
{	// 鉴权是否成功
	int ret = 0;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_fake_auth != INVALID_VALUE) {
		pr_err("use fake_auth=%d \n", g_fake_auth);
		return g_fake_auth;
	}
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, IS_BATTERY_AUTH, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return ret==0?val.is_auth:0;
}
const char *ll_get_battery_sn(void)
{	// 电池SN号
	int ret;
	unsigned int retry = 0;
	int sleep_time_ms = RETRY_INTERVAL_MS;
	union SECRET_OBJ_VAL val;
	struct secret_device *secret_dev = get_secret_by_name(MASTER_SECRET);
	if(g_batt_sn){
		return g_batt_sn;
	}
	while(++retry <= MAX_RETRY_COUNT) {
		ret = ll_get_obj_from_dev(secret_dev, BATTERY_SN, &val);
		if(ret == 0){
			break;
		} else {
			msleep(sleep_time_ms);
		}
	}
	return val.battery_sn;
}

//// fake api

int ll_get_fake_rawsoh(void){
	return g_fake_rawsoh;
}
int ll_set_fake_rawsoh(int value){
	g_fake_rawsoh = value;
	return 0;
}
int ll_get_fake_cycle_count(void){
	return g_fake_cycle_count;
}
int ll_set_fake_cycle_count(int value){
	g_fake_cycle_count = value;
	return 0;
}
//// end fake api

/** 以上ll_get_xxx和ll_set_xxx 接口用于给class层调用 */

static ssize_t fake_rawsoh_show(struct kobject *kobj, struct kobj_attribute *attr,  char *buf)
{
    return sysfs_emit(buf, "%d\n", g_fake_rawsoh); // 格式化为字符串，末尾必须加换行符
}
static ssize_t fake_rawsoh_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    int value;
    int ret;

    // 使用内核安全的解析函数
    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        pr_err("Invalid input: not an integer\n");
        return ret; // 返回错误码（如 -EINVAL）
    }
    g_fake_rawsoh = value;
    return count; // 返回成功处理的字节数
}
//static DEVICE_ATTR(fake_rawsoh, 0644, fake_rawsoh_show, fake_rawsoh_store);
static struct kobj_attribute fake_rawsoh_attr = __ATTR(fake_rawsoh, 0644, fake_rawsoh_show, fake_rawsoh_store);

static ssize_t fake_cycle_count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%d\n", g_fake_cycle_count); // 格式化为字符串，末尾必须加换行符
}
static ssize_t fake_cycle_count_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buf, size_t count)
{
    int value;
    int ret;

    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        pr_err("Invalid input: not an integer\n");
        return ret; // 返回错误码（如 -EINVAL）
    }
    g_fake_cycle_count = value;
    return count; // 返回成功处理的字节数
}
//static DEVICE_ATTR(fake_cycle_count, 0644, fake_cycle_count_show, fake_cycle_count_store);
static struct kobj_attribute fake_cycle_count_attr = __ATTR(fake_cycle_count, 0644, fake_cycle_count_show, fake_cycle_count_store);


static ssize_t uisoh_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int i;
	int len = 0;
	//unsigned char uisoh[UISOH_LEN] = {0};
	unsigned char uisoh[UISOH_LEN] = {"xxx"};
	union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	pr_info("enter.\n");
	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	w1sec_info->uisoh_valid = 0;
	w1sec_info->page2_valid = 0;
	val.battery_uisoh = uisoh;
	ret = ll_get_obj_from_dev(secret_dev, UISOH, &val);
	if(ret){
		pr_err("get uisoh from device error:%d \n", ret);
		return ret;
	}
	for (i = 0; i < UISOH_LEN; i++) {
		len += sprintf(buf + len, "%02X", uisoh[i]);
	}
	len += sprintf(buf + len, "\n");
    return len;
}
static ssize_t uisoh_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    int ret;
	int i;
	union SECRET_OBJ_VAL val;
	char str[3] = {0};
	struct w1_data *w1sec_info = NULL;
	unsigned char uisoh[UISOH_LEN] = {0};
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	pr_info(" entry. count=%zu \n", count);
	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	g_uisoh = NULL;
	w1sec_info->uisoh_valid = 0;
	w1sec_info->page2_valid = 0;
	if(count != (UISOH_LEN*2+1) ) {
		pr_err("uisoh len(%zu) is invalid!\n", count);
		return -EINVAL;
	}

    for(i=0; i<UISOH_LEN; i++) {
		memcpy(str, buf+(2*i), 2);
		str[2] = '\0';
		ret = kstrtou8(str, 16, &uisoh[i]);
		if(ret<0){
			pr_err("convert string to HEX fail, i=%d \n", i);
			return -EINVAL;
		}
		pr_debug("str:%s data:%2X \n", str, uisoh[i]);
	}
    val.battery_uisoh = uisoh;
	ret = ll_set_obj_to_dev(secret_dev, UISOH, val);
	if(ret){
		return ret;
	}
    return count; // 返回成功处理的字节数
}
static DEVICE_ATTR(uisoh, 0644, uisoh_show, uisoh_store);

static ssize_t rawsoh_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	w1sec_info->page2_valid = 0;
	w1sec_info->rawsoh_curr = W1_ERR_VALUE;
	ret = ll_get_obj_from_dev(secret_dev, RAWSOH, &val);
	if(ret){
		return ret;
	}
    return sysfs_emit(buf, "%d\n", val.rawsoh); // 格式化为字符串，末尾必须加换行符
}
static ssize_t rawsoh_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    int value;
    int ret;
	union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	g_rawsoh = INVALID_VALUE;
	w1sec_info->page2_valid = 0;
	w1sec_info->rawsoh_curr = W1_ERR_VALUE;
    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        dev_err(dev, "Invalid input: not an integer\n");
        return ret; // 返回错误码（如 -EINVAL）
    }
    val.rawsoh = value;
	ret = ll_set_obj_to_dev(secret_dev, RAWSOH, val);
	if(ret){
		return ret;
	}
    return count; // 返回成功处理的字节数
}
static DEVICE_ATTR(rawsoh, 0644, rawsoh_show, rawsoh_store);


static ssize_t cycle_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	w1sec_info->page2_valid = 0;
	w1sec_info->dc_value_curr = W1_ERR_VALUE;
	w1sec_info->cycle_count_curr = W1_ERR_VALUE;
	ret = ll_get_obj_from_dev(secret_dev, CYCLE_COUNT, &val);
	if(ret){
		return ret;
	}
    return sysfs_emit(buf, "%d\n", val.cycle_count); // 格式化为字符串，末尾必须加换行符
}
static ssize_t cycle_count_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t count)
{
    int value;
    int ret;
	union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	// reset cache value
	g_cycle_count = INVALID_VALUE;
	w1sec_info->page2_valid = 0;
	w1sec_info->dc_value_curr = W1_ERR_VALUE;
	w1sec_info->cycle_count_curr = W1_ERR_VALUE;
    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        dev_err(dev, "Invalid input: not an integer\n");
        return ret; // 返回错误码（如 -EINVAL）
    }
    val.cycle_count = value;
	ret = ll_set_obj_to_dev(secret_dev, CYCLE_COUNT, val);
	if(ret){
		return ret;
	}
    return count; // 返回成功处理的字节数
}
static DEVICE_ATTR(cycle_count, 0644, cycle_count_show, cycle_count_store);

static ssize_t first_use_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    union SECRET_OBJ_VAL val;
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	w1sec_info->page1_valid = 0;
	w1sec_info->fst_use_time_valid = 0;
    // 调用 ll_get_obj_from_dev 获取 first_use_time
    ret = ll_get_obj_from_dev(secret_dev, BATTERY_FIRST_USE_TIME, &val);
    if (ret < 0) {
        dev_err(dev, "Failed to get battery first use time: %d\n", ret);
        return ret;
    }

    // 将获取到的时间写入 buf
    return sysfs_emit(buf, "%s\n", val.battery_first_use_time);
}

static ssize_t first_use_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int ret;
    union SECRET_OBJ_VAL val;
	char temp_buf[PARAM_LEN];
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	g_batt_first_use_time = NULL;
	w1sec_info->page1_valid = 0;
	w1sec_info->fst_use_time_valid = 0;
    // 检查输入长度
    if (count >= sizeof(temp_buf)) {
        dev_err(dev, "Input too long\n");
        return -EINVAL;
    }

    // 复制输入的时间字符串
    strlcpy(temp_buf, buf, sizeof(temp_buf));
    val.battery_first_use_time = temp_buf;
    // 调用 ll_set_obj_to_dev 设置 first_use_time
    ret = ll_set_obj_to_dev(secret_dev, BATTERY_FIRST_USE_TIME, val);
    if (ret < 0) {
        dev_err(dev, "Failed to set battery first use time: %d\n", ret);
        return ret;
    }

    return count;
}
static DEVICE_ATTR(first_use_time, 0644, first_use_time_show, first_use_time_store);

static ssize_t reset_cycle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 flag_data[32];
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	w1sec_info->page1_valid = 0;
	if (w1sec_info->page_read(FIRST_USE_PAGE, flag_data)) {
		pr_err("read reset_flag failed");
		return -EINVAL;
	}

	return sprintf(buf, "%u\n", flag_data[7]);
}

static ssize_t reset_cycle_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, val;
	u8 flag_data[32];
	struct w1_data *w1sec_info = NULL;
	struct secret_device *secret_dev = container_of(dev, struct secret_device, dev);

	ret = sscanf(buf, "%d", &val);
	w1sec_info = dev_get_drvdata(&secret_dev->dev);
	if (!w1sec_info) {
		pr_err("get w1sec_info drv data failed\n");
		return -EINVAL;
	}
	w1sec_info->page1_valid = 0;
	if (w1sec_info->page_read(FIRST_USE_PAGE, flag_data)) {
		pr_err("read clear_flag failed");
		return -EINVAL;
	}

	flag_data[7] = (u8)val;
	if (w1sec_info->page_write(FIRST_USE_PAGE, flag_data)) {
		pr_err("update clear_flag failed");
		return -EINVAL;
	}

	return size;
}
static struct device_attribute dev_attr_reset_cycle =
	__ATTR(reset_cycle, S_IRUSR | S_IWUSR, reset_cycle_show, reset_cycle_store);

static ssize_t chip_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct secret_device *secret_dev = NULL;
	secret_dev = container_of(dev, struct secret_device, dev);
	if(!secret_dev) {
		pr_err("cannot get secret_dev!\n");
		return -EINVAL;
	}

    // 将获取到的时间写入 buf
    return sysfs_emit(buf, "%s\n", secret_dev->chip_name);
}
static DEVICE_ATTR(chip_name, 0444, chip_name_show, NULL);

static ssize_t w1_timings_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct w1_data *w1_info;
	struct secret_device *master = NULL;

	master = get_secret_by_name(MASTER_SECRET);
	if (IS_ERR_OR_NULL(master)) {
		pr_err("search secret device failed\n");
		return -EINVAL;
	}
	w1_info = dev_get_drvdata(&master->dev);

	return snprintf(buf, PAGE_SIZE,
		"w1_timing_cfg: %u %u %u %u %u %u %u %u %u\n", w1_info->w1_timings_cfg[0],
		w1_info->w1_timings_cfg[1], w1_info->w1_timings_cfg[2], w1_info->w1_timings_cfg[3], w1_info->w1_timings_cfg[4],
		w1_info->w1_timings_cfg[5], w1_info->w1_timings_cfg[6], w1_info->w1_timings_cfg[7], w1_info->w1_timings_cfg[8]);
}

static ssize_t w1_timings_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int  num = 0;
	char txbuf[128];
	char *tx_ptr, *hexstr;
	struct w1_data *w1_info;
	struct secret_device *master = NULL;

	master = get_secret_by_name(MASTER_SECRET);
	if (IS_ERR_OR_NULL(master)) {
		pr_err("search secret device failed\n");
		return -EINVAL;
	}
	w1_info = dev_get_drvdata(&master->dev);
	tx_ptr = txbuf;
	strcpy(txbuf, buf);
	while ((hexstr = strsep(&tx_ptr, " ")) != NULL) {
		if (kstrtouint(hexstr, 10, &w1_info->w1_timings_cfg[num]) < 0) {
			pr_err("parse byte(%u) failed\n", num);
			return -EINVAL;
		}
		num++;
	}

	return count;
}

static DEVICE_ATTR(w1_timings, 0644, w1_timings_show, w1_timings_store);

void debugfs_device_node_init(struct device *dev)
{
	// sysfs node for read/write data from/to device
	//node path: /sys/devices/virtual/battery_secret/master_secret
	device_create_file(dev, &dev_attr_uisoh);
	device_create_file(dev, &dev_attr_rawsoh);
	device_create_file(dev, &dev_attr_cycle_count);
	device_create_file(dev, &dev_attr_first_use_time);
	device_create_file(dev, &dev_attr_reset_cycle);
	device_create_file(dev, &dev_attr_chip_name);
	device_create_file(dev, &dev_attr_w1_timings);
}

#if 1
void debugfs_fake_node_init(void)
{
	battery_secret_kobj = kobject_create_and_add("battery_secret", kernel_kobj);
	if (!battery_secret_kobj) {
		pr_err("Failed to create kobject for battery\n");
		return;
	}

	// 创建 fake 节点
	// fake node path: /sys/kernel/battery_secret/
	if (sysfs_create_file(battery_secret_kobj, &fake_rawsoh_attr.attr)) {
		pr_err("Failed to create fake_rawsoh attribute\n");
	}
	if (sysfs_create_file(battery_secret_kobj, &fake_cycle_count_attr.attr))  {
		pr_err("Failed to create fake_cycle_count attribute\n");
	}
}
void debugfs_fake_node_deinit(void)
{
	if (battery_secret_kobj) {
		sysfs_remove_file(battery_secret_kobj, &fake_rawsoh_attr.attr);
		sysfs_remove_file(battery_secret_kobj, &fake_cycle_count_attr.attr);
		kobject_put(battery_secret_kobj);
	}
}
#endif
void debugfs_device_node_deinit(struct device *dev)
{
	device_remove_file(dev, &dev_attr_uisoh);
	device_remove_file(dev, &dev_attr_rawsoh);
	device_remove_file(dev, &dev_attr_cycle_count);
	device_remove_file(dev, &dev_attr_first_use_time);
}

const char *ll_find_secret_attr_by_name(const char *name)
{
	struct secret_attr  *s_attr = NULL;
	s_attr = g_s_attr_head;
	while(s_attr!=NULL){
		if(strcmp(name, s_attr->name) == 0){
			return s_attr->value;
		}
		s_attr = s_attr->next;
	}
	return NULL;
}

static void secret_cmldline_data_init(void)
{
	char *cmdline = NULL;
	char *p,*q;
	//int flag = 0;
	int cmdline_len = 0;
	char *bootargs = NULL;
	struct device_node *of_chosen = NULL;
	struct secret_attr  *s_attr = NULL;
	pr_debug("enter.\n");
	g_s_attr_head = NULL;
	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		bootargs = (char *)of_get_property(of_chosen, "bootargs", NULL);
		if (!bootargs) {
			pr_err("failed to get bootargs\n");
			return;
		}
		p = strstr(bootargs, SECRET_ATTRS_PREFIX);
		if(p) {
			secret_attrs = p;
			q = strstr(p," ");
		} else {
			pr_err("cmdline no include [%s] \n", SECRET_ATTRS_PREFIX);
			return;
		}
		if(q){
			cmdline_len = strlen(p) - strlen(q) + 1;
		} else {
			cmdline_len = strlen(p) +1;
		}
		pr_debug("cmdline(%s) len:%d \n", SECRET_ATTRS_PREFIX, cmdline_len);
	}else {
		pr_err("failed to get /chosen\n");
		return;
	}

	if(!secret_attrs || strlen(secret_attrs)<5){
		pr_err("no cmdline data!\n");
		return;
	}
	pr_debug("secret_attrs:%s\n", secret_attrs==NULL?"NULL":secret_attrs);
	
	cmdline = kzalloc(cmdline_len, GFP_KERNEL);
	if(!cmdline){
		pr_err("no memory!\n");
		return;
	}
	memcpy(cmdline, secret_attrs, cmdline_len-1);
	secret_attrs = cmdline+strlen(SECRET_ATTRS_PREFIX);
	pr_debug("secret_attrs = [%s] \n", secret_attrs);
	p = secret_attrs;
	while( (p=strstr(secret_attrs, ":")) !=NULL){
		s_attr = kzalloc(sizeof(struct secret_attr ), GFP_KERNEL);
		if(!s_attr){
			pr_err("no memory!\n");
			return;
		}
		*p = '\0'; // : -> \0
		p++; 
		s_attr->name = secret_attrs;
		s_attr->value = p;
		if((q=strstr(p, ";"))!=NULL){
			*q = '\0'; // ; -> \0
			secret_attrs = q+1;
		} else {
			pr_debug("maybe no ; at the end\n");
		}
		if(*s_attr->value == '\0') {
			kfree(s_attr);
			pr_err("attrs value is null!\n");
			break;
		}
		pr_debug("secret attr: [%s:%s] \n", s_attr->name, s_attr->value);
		if(!g_s_attr_head){
			g_s_attr_head = s_attr;
			g_s_attr_head->next = NULL;
		} else {
			s_attr->next = g_s_attr_head;
			g_s_attr_head = s_attr;
		}
		if(*secret_attrs == '\0'){
			break;
		}
	}
	return; 
}

static enum power_supply_property batt_verify_psy_properties[] = {
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_SCOPE, // for get battery_id
};

static int psy_batt_verify_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret;
	//struct secret_device *secret_dev = power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_AUTHENTIC:
		ret = ll_is_battery_auth_success();
		val->intval = !!ret;
		break;
	case POWER_SUPPLY_PROP_SCOPE: // for battery id
		ret = ll_get_battery_id();
		if(ret<0){
			pr_err("fail get battery id, ret:(%d) \n", ret);
			return -EINVAL;
		}
		val->intval = ret;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int psy_batt_verify_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	//struct secret_device *secret_dev = power_supply_get_drvdata(psy);
	switch (psp) {
#ifdef IS_AUTH_WRITE
	case POWER_SUPPLY_PROP_AUTHENTIC:
		g_fake_auth = val->intval;
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

struct power_supply_desc batt_verify_psy_desc = {
	.name = "batt_verify",
	.properties = batt_verify_psy_properties,
	.num_properties = ARRAY_SIZE(batt_verify_psy_properties),
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property = psy_batt_verify_get_property,
	.set_property = psy_batt_verify_set_property,
};

void init_batt_verify_psy(void)
{
	struct power_supply_config cfg = {
		.drv_data = NULL,
	};

	batt_verify_psy = power_supply_register( NULL,
		&batt_verify_psy_desc, &cfg);
	if (IS_ERR_OR_NULL(batt_verify_psy)) {
		pr_err("register battery verify psy fail(%ld)\n", 
			PTR_ERR(batt_verify_psy));
		return ;
	}
	pr_info("Register batt verify psy sucess\n");
	return;
}

void battery_secret_logic_layer_init(void){
	/*
	 * 1. 获取相关的secret device
	 * 2. 初始化相关的work
	 * 3. 相关节点或者调试接口的初始化
	 *
	 */
	init_sync_update_data();
	get_secret_dev();
	secret_cmldline_data_init();
	// TODO增加测试节点或者调试节点
	debugfs_fake_node_init();
	init_batt_verify_psy();
	//return 0;
}

void battery_secret_logic_layer_deinit(void) {
    // 停止并清理更新线程
    if (sync_update_data) {
        if (sync_update_data->update_thread) {
            kthread_stop(sync_update_data->update_thread);
            sync_update_data->update_thread = NULL;
        }
        kfree(sync_update_data);
        sync_update_data = NULL;
    }
	if (!IS_ERR_OR_NULL(batt_verify_psy)) {
		power_supply_unregister(batt_verify_psy);
		batt_verify_psy = NULL;
	}
    debugfs_fake_node_deinit();
    // 重置全局缓存
    g_uisoh = NULL;
    g_rawsoh = INVALID_VALUE;
    g_cycle_count = INVALID_VALUE;
    g_batt_id = INVALID_VALUE;
    g_is_battery_auth = INVALID_VALUE;
    g_clear_flag = false;
    memset(batt_manufacture_date, 0, sizeof(batt_manufacture_date));
    memset(batt_first_use_time, 0, sizeof(batt_first_use_time));
    memset(batt_sn, 0, sizeof(batt_sn));
    g_batt_manufacture_date = NULL;
    g_batt_first_use_time = NULL;
    g_batt_sn = NULL;

    // 清理设备指针
    master = NULL;
#ifdef DUAL_BATTERY_SECRET
    slave = NULL;
#endif

}
