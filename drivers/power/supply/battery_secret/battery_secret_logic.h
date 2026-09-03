
#ifndef _BATTERY_SECRET_LOGIC_H
#define _BATTERY_SECRET_LOGIC_H

#include "battery_secret_common.h"
enum SECRET_OBJ {
	UISOH = 1,
	RAWSOH,
	CYCLE_COUNT,
	BATTERY_MANUFACTURE_DATE,
	BATTERY_FIRST_USE_TIME,
	BATTERY_SN,
	BATTERY_ID,
	IS_BATTERY_AUTH,
	CLEAR_CYCLE
};
union SECRET_OBJ_VAL {
	//int uisoh;
	int rawsoh;
	int cycle_count;
	char *battery_manufacture_date;
	char *battery_first_use_time;
	char *battery_sn;
	u8 *battery_uisoh;
	//int battery_uisoh_len;
	int battery_id;
	int is_auth;
	int clear_cycle;
};

struct secret_attr {
	const char *name;
	const char *value;
	struct secret_attr *next;
};

/* 以下ll_get_xxx和ll_set_xxx 接口用于给class层调用 */
// 但是不要EXPORT出去，不给其模块使用
int ll_get_uisoh(unsigned char *data, size_t len);
int ll_set_uisoh(unsigned char *data, size_t len);
int ll_get_rawsoh(void);
int ll_set_rawsoh(int rawsoh);
int ll_get_cycle_count(void);
int ll_set_cycle_count(int value);
int ll_clear_cycle_count(void);

const char *ll_get_battery_manufacture_date(void);  // 电池出厂时间
const char *ll_get_battery_first_use_time(void); // 电池首次使用时间
int ll_set_battery_first_use_time(char *time); // 电池首次使用时间
int ll_get_battery_id(void); // 电池ID
const char *ll_get_battery_secret_name(void); // 加密芯片名称
int ll_is_battery_auth_success(void); // 鉴权是否成功
const char *ll_get_battery_sn(void); // 电池SN号
const char *ll_find_secret_attr_by_name(const char *name);
int ll_check_secret_chip_online(void);

//// fake api
int ll_get_fake_uisoh(void);
int ll_set_fake_uisoh(int value);
int ll_get_fake_rawsoh(void);
int ll_set_fake_rawsoh(int value);
int ll_get_fake_cycle_count(void);
int ll_set_fake_cycle_count(int value);
//// end fake api
/* 以上ll_get_xxx和ll_set_xxx 接口用于给class层调用 */

void debugfs_device_node_init(struct device *dev);
void debugfs_device_node_deinit(struct device *dev);
void battery_secret_logic_layer_init(void);
void battery_secret_logic_layer_deinit(void);

#endif // _BATTERY_SECRET_LOGIC_H