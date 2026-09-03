#ifndef _BATTERY_SECRET_CLASS_H
#define _BATTERY_SECRET_CLASS_H

#include "battery_secret_common.h"

#define to_secret_device(obj) container_of(obj, struct secret_device, dev)

extern struct secret_device *get_secret_by_name(const char *name);
extern struct secret_device *secret_device_register(const char *name,
		const char *chip_name,
		struct device *parent, void *devdata,
		const struct secret_ops *ops);
extern void secret_device_unregister(struct secret_device *secret_dev);
extern const char *find_secret_attr_by_name(const char *name);

extern int lc_get_uisoh(u8 *data, size_t size); // 设置成功返回0，失败返回负数
extern int lc_set_uisoh(u8 *data, size_t size); // 设置成功返回0，失败返回负数
extern int lc_get_rawsoh(void); // get成功，返回正数表示要获取的值，失败返回负数错误码
extern int lc_set_rawsoh(int rawsoh);  // set成功返回0， 失败返回负数错误码
extern int lc_get_cycle_count(void); // get成功，返回正数表示要获取的值，失败返回负数错误码
extern int lc_set_cycle_count(int value); // set成功返回0， 失败返回负数错误码
extern int lc_clear_cycle_count(void); // 设置成功返回0，失败返回负数

extern const char *lc_get_battery_manufacture_date(void);  // 电池出厂时间，失败返回NULL
extern const char *lc_get_battery_first_use_time(void); // 电池首次使用时间，失败返回NULL
extern int lc_set_battery_first_use_time(char *time); // 电池首次使用时间,成功返回0
extern int lc_get_battery_id(void); // 电池ID,get成功，返回正数表示要获取的值，失败返回负数错误码
extern const char *lc_get_battery_secret_name(void); // 加密芯片名称，失败返回NULL
extern int lc_is_battery_auth_success(void); // 鉴权是否成功,鉴权成功返回1，鉴权失败返回0
extern const char *lc_get_battery_sn(void); // 电池SN号，失败返回NULL


//// fake api
extern int lc_get_fake_uisoh(void);
extern int lc_set_fake_uisoh(int value);
extern int lc_get_fake_rawsoh(void);
extern int lc_set_fake_rawsoh(int value);
extern int lc_get_fake_cycle_count(void);
extern int lc_set_fake_cycle_count(int value);


#endif // _BATTERY_SECRET_CLASS_H