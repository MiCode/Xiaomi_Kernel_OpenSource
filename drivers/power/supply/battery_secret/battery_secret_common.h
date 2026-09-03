
#ifndef _BATTERY_SECRET_COMMON_H
#define _BATTERY_SECRET_COMMON_H
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>

#define CHIP_ONLINE             1
#define CHIP_UNKNOWN           -1
#define CHIP_UNSUPRT           -2
#define UISOH_LEN	           11
#define FST_USE_TIME_LEN       6
#define VDD_POWER_VOLTAGE 3300000

#define MASTER_SECRET 	"master_secret"
#define SLAVE_SECRET 	"slave_secret"

// defined/used in logic layer
struct sync_update_work_data {
    wait_queue_head_t wq;     // 等待队列
    atomic_t condition;       // 唤醒条件
	//int uisoh;
	int rawsoh;
	int cycle_count;
	u8 uisoh[11];
	size_t uisoh_len;
	//int first_use_time;
	struct task_struct *update_thread;
};

struct secret_device {
	const struct secret_ops *ops;
	raw_spinlock_t io_lock;
	struct mutex ops_lock;
	struct device dev;
	void *driver_data;
	char *secret_name; // master, or slave
	char chip_name[32]; // 加密IC的芯片型号
	//struct gpio_desc *gpiod;
	struct power_supply *batt_verify_psy;
};

// defined/used in class layer
struct secret_ops {
	int (*get_uisoh)(struct secret_device *secret_dev, u8 *data, int len); // 执行成功返回0
	int (*set_uisoh)(struct secret_device *secret_dev, u8 *data, int len, int rawsoh); // 执行成功返回0
	int (*get_rawsoh)(struct secret_device *secret_dev, int *val); // 执行成功返回0
	int (*set_rawsoh)(struct secret_device *secret_dev, int val); // 执行成功返回0
	int (*get_cycle_count)(struct secret_device *secret_dev, int *val); // 执行成功返回0
	int (*set_cycle_count)(struct secret_device *secret_dev, int val); // 执行成功返回0
	int (*clear_cycle_count)(struct secret_device *secret_dev); // 执行成功返回0
	int (*get_battery_manufacture_date)(struct secret_device *secret_dev, char * const buff, size_t size);  // 电池出厂时间
	int (*get_battery_first_use_time)(struct secret_device *secret_dev, char * const buff, size_t size); // 电池首次使用时间
	int (*set_battery_first_use_time)(struct secret_device *secret_dev, const char *time); // 电池首次使用时间
	int (*get_battery_id)(struct secret_device *secret_dev, int *batt_id); // 电池ID
	//char* (*dev_get_battery_secret_name)(); // 加密芯片名称
	int (*is_battery_auth)(struct secret_device *secret_dev, int *is_auth); // 执行成功返回0
	int (*get_battery_sn)(struct secret_device *secret_dev, char * const buff, size_t size); // 电池SN号
};



#endif // _BATTERY_SECRET_COMMON_H
