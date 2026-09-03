#ifndef __XM_DFX_CHG_H__
#define __XM_DFX_CHG_H__

#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/notifier.h>
#include <linux/time64.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/bitops.h>
#include <linux/ktime.h>

#define XM_CHG_DFS_FEATURE                     	1
#define XM_DFS_LOG_LEVER                        3
#define XM_DFS_INFO_DEBUGFS						1

#define dfx_err(fmt, ...)     \
do {                                    \
    if (XM_DFS_LOG_LEVER > 0)                  \
        printk(KERN_ERR "[DFX]" fmt, ##__VA_ARGS__);   \
}while (0)
#define dfx_info(fmt, ...)    \
do {                                    \
    if (XM_DFS_LOG_LEVER > 1)          \
        printk(KERN_ERR "[DFX]" fmt, ##__VA_ARGS__);    \
}while (0)
#define dfx_dbg(fmt, ...)    \
do {                                        \
    if (XM_DFS_LOG_LEVER >=2 )          \
        printk(KERN_ERR "[DFX]" fmt, ##__VA_ARGS__);    \
}while (0)

#define DFX_ID_CHG_PD_AUTHEN_FAIL              909001004
#define DFX_ID_CHG_CP_ENABLE_FAIL              909001005

#define DFX_ID_CHG_NONE_STANDARD_CHG           909002001
#define DFX_ID_CHG_RP_SHORT_VBUS_DETECTED      909002002
#define DFX_ID_CHG_LPD_DETECTED                909002003
#define DFX_ID_CHG_CP_VBUS_OVP                 909002004
#define DFX_ID_CHG_CP_IBUS_OCP                 909002005
#define DFX_ID_CHG_CP_VBAT_OVP                 909002006
#define DFX_ID_CHG_CP_IBAT_OCP                 909002007

#define DFX_ID_CHG_BATTERY_CYCLECOUNT          909003001
#define DFX_ID_CHG_UISOC_NOT_FULL              909003002
#define DFX_ID_CHG_SMART_ENDURANCE_TRIGGERED   909003004
#define DFX_ID_CHG_SMART_NAVIGATION_TRIGGERED  909003006

#define DFX_ID_CHG_BATT_IIC_ERR                909005001
#define DFX_ID_CHG_CP_ABSENT                   909005002
#define DFX_ID_CHG_BATT_LINKER_ABSENT          909005003
#define DFX_ID_CHG_LOW_TEMP_DISCHARGING        909005007
#define DFX_ID_CHG_HIGH_TEMP_DISCHARGING       909005008

#define DFX_ID_CHG_SMART_ENDURANCE_SOC_ERR     909006010
#define DFX_ID_SMART_NAVIGATION_SOC_ERR        909006011

#define DFX_ID_CHG_BATTERY_AUTH_FAIL           909007001

#define DFX_ID_CHG_BATTERY_TEMP_HOT            909009001
#define DFX_ID_CHG_BATTERY_TEMP_COLD           909009002

enum xm_chg_dfx_type {
    CHG_DFX_DEFAULT,
    CHG_DFX_PD_AUTH_ERR,
	CHG_DFX_CP_ENABLE_FAIL,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_RP_SHORT_VBUS_DETECTED,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_BATT_CYCLE_COUNT,
	CHG_DFX_UISOC_NOT_FULL,
	CHG_DFX_SMART_ENDURANCE_TRIGGERED,
	CHG_DFX_NAVIGATION,
	CHG_DFX_FG_IIC_ERR,
	CHG_DFX_CP_ABSENT,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_LOW_TEMP_DISCHARGING,
	CHG_DFX_HIGH_TEMP_DISCHARGING,
	CHG_DFX_SMART_ENDURANCE_SOC_ERR,
	CHG_DFX_NAVIGATION_OVER_SOC,
	CHG_DFX_BATT_AUTH_ERR,
	CHG_DFX_BATTERY_TEMP_HIGH,
	CHG_DFX_BATTERY_TEMP_LOW,
    CHG_DFX_MAX_INDEX,
};

struct dfs_data_cp_i2c_err{
    int master_ok;
    int slave_ok;
};

struct dfs_data_battery {
    int vbat;
	int uisoc;
	int rawsoc;
	int cycle;
	int tbat_x10;
	int tbat_max_x10;
	int tbat_min_x10;
	int is_charging;
	int real_type;
	int status;
};

struct dfx_data_struct {
    int adapter_id;
	int tboard_x1000;
	struct dfs_data_cp_i2c_err data_cp;
	struct dfs_data_battery data_batt;
	int dfx_cycle_count_flag;
	struct delayed_work dfx_monitor_work;
	u16 dfx_work_delay;
	struct power_supply *batt_psy;
	struct power_supply *bms_psy;
};

struct dfs_data_info{
    char *dev_name;
    enum xm_chg_dfx_type type;
    struct dfx_data_struct dfx_data;
};

struct xm_dfs_info {
	wait_queue_head_t wq;
	atomic_t condition;
	int charger_online;
	struct mutex lock;
	unsigned long evt_dfs_type; // 用于相关事件的flag
	unsigned long evt_en_mask;  // 用于是否使能相关事件的上报
	struct task_struct *task;
};

struct xm_dfs_evt_condition {
	enum xm_chg_dfx_type evt;
	bool is_first_report; // 当前事件是否是开机后第一次上报，默认值是1，上报一次之后配置为0
	bool support_boot_report; // 是否需要开机上报一次
	bool require_charge_stat; // 是否需要再充电状态才允许上报
	int max_report_times; // 最大上报的次数
	int report_times; // 当前事件上报的次数
};

extern void xm_dfs_work_report_event(u8 type, bool flag);
extern int init_xm_dfs_info(struct device *pdev);
extern void deinit_xm_dfs_info(void);

#endif