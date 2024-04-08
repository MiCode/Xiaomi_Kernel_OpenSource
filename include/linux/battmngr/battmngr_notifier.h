
#ifndef __BATTMNGR_NOTIFIER_H
#define __BATTMNGR_NOTIFIER_H

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

enum battmngr_event_type {
	BATTMNGR_EVENT_NONE = 0,
	BATTMNGR_EVENT_FG,
	BATTMNGR_EVENT_CP,
	BATTMNGR_EVENT_MAINCHG,
	BATTMNGR_EVENT_PD,
	BATTMNGR_EVENT_MCU,
	BATTMNGR_EVENT_IRQ,
	BATTMNGR_EVENT_REPORT,
	BATTMNGR_EVENT_THERMAL,
};

enum battmngr_msg_type {
	BATTMNGR_MSG_NONE = 0,

	/* fg */
	BATTMNGR_MSG_FG,

	/* cp */
	BATTMNGR_MSG_CP_MASTER,
	BATTMNGR_MSG_CP_SLAVE,

	/* mainchg */
	BATTMNGR_MSG_MAINCHG_TYPE,
	BATTMNGR_MSG_MAINCHG_OTG_ENABLE,

	/* pd */
	BATTMNGR_MSG_PD_ACTIVE,
	BATTMNGR_MSG_PD_VERIFED,
	BATTMNGR_MSG_PD_AUDIO,

	/* mcu */
	BATTMNGR_MSG_MCU_TYPE,
	BATTMNGR_MSG_MCU_BOOST,

};

struct battmngr_ny_fg {
	int msg_type;
	int recharge_flag;
	int chip_ok;
};

struct battmngr_ny_cp {
	int msg_type;
};

struct battmngr_ny_mainchg {
	int msg_type;
	int chg_plugin;
	int sw_disable;
	int otg_enable;
	int chg_done;
	int chg_type;
	int dc_plugin;
};

struct battmngr_ny_pd {
	int msg_type;
	int pd_active;
	int pd_verified;
	int pd_curr_max;
	int accessory_mode;
};

struct battmngr_ny_misc {
	int disable_thermal;
	int vindpm_temp;
	u8 thermal_level;
};

struct battmngr_ny_irq {
	int irq_type;
	int value;
};

struct read_mcu_data
{
    /* keyboard data */
    int input_volt; // 0x0c
    uint8_t input_watt; // 0x0d
    uint8_t vbat_set; // 0x0e
    uint8_t ibat_set; // 0x0f
    uint8_t work_state; //0x10
    uint8_t typec_state; // 0x11
    uint8_t dpdm_in_state; // 0x12
    int protocol_volt; // 0x13
    int protocol_curr; //0x14
    int vbus_pwr; // 0x15
    int vbus_mon; //0x16
    int typec_curr; //0x17
    uint8_t ntc_l; //0x1c
    uint8_t ntc_h; //0x1d
    uint8_t dpdm0_state; //0x1e
    uint8_t sink_comm; //0x1f

    /* holder data */
    int sys_ctl0; // 0x00
    uint8_t sys_ctl1; // 0x01
    //uint8_t typec_state; // 0x10
    uint8_t ident_code; // 0x11
    uint8_t state_ctl3; //0x12
    //uint8_t vin_low;    // 0x13
    //uint8_t vin_high;   // 0x14
    uint8_t vout_low;   // 0x15
    uint8_t vout_high;  // 0x16
    int vout;
    //uint8_t pd_V;       // 0x1c
    //uint8_t pd_I;       // 0x1d
    //uint8_t iout_low;       // 0x1e
    //uint8_t iout_high;       // 0x1f
    int iout;
    uint8_t addr;
    uint8_t code;
};

struct battmngr_ny_mcu {
	u8 object_type;
	int chg_plugin;
	int pd_active;
	int pd_verified;
	int msg_type;
	int kb_attach;
	struct read_mcu_data mcu_data;
};

struct battmngr_notify {
	struct battmngr_ny_fg fg_msg;
	struct battmngr_ny_cp cp_msg;
	struct battmngr_ny_mainchg mainchg_msg;
	struct battmngr_ny_pd pd_msg;
	struct battmngr_ny_misc misc_msg;
	struct battmngr_ny_irq irq_msg;
	struct battmngr_ny_mcu mcu_msg;
	struct mutex notify_lock;
};

extern struct battmngr_notify *g_battmngr_noti;
extern struct xm_battmngr *g_battmngr;
extern int battmngr_notifier_register(struct notifier_block *n);
extern int battmngr_notifier_unregister(struct notifier_block *n);
extern int battmngr_notifier_call_chain(unsigned long event,
			struct battmngr_notify *data);

#endif /* __BATTMNGR_NOTIFIER_H */

