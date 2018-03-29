/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCCI_H__
#define __CCCI_H__
#include "ccci_common.h"
#define BOOT_TIMER_HS1 10
#define BOOT_TIMER_HS2 10

typedef void (*ccci_aed_cb_t) (unsigned int flag, char *aed_str);
/******************************************************************************/
/** mdlogger mode define                                                                                           **/
/******************************************************************************/
enum LOGGING_MODE {
	MODE_UNKNOWN = -1,	/*  -1 */
	MODE_IDLE,		/*  0 */
	MODE_USB,		/*  1 */
	MODE_SD,		/*  2 */
	MODE_POLLING,		/*  3 */
	MODE_WAITSD,		/*  4 */
};

/* ================================================================================== */
/*  IOCTL commands */
/* ================================================================================== */
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET			_IO(CCCI_IOC_MAGIC, 0)	/*  mdlogger META  */
#define CCCI_IOC_GET_MD_STATE			_IOR(CCCI_IOC_MAGIC, 1, unsigned int)	/*  audio */
#define CCCI_IOC_PCM_BASE_ADDR			_IOR(CCCI_IOC_MAGIC, 2, unsigned int)	/*  audio */
#define CCCI_IOC_PCM_LEN			_IOR(CCCI_IOC_MAGIC, 3, unsigned int)	/*  audio */
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4)	/*  muxreport  mdlogger */
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5)	/*  mdlogger */
#define CCCI_IOC_DO_MD_RST			_IO(CCCI_IOC_MAGIC, 6)	/*  md_init */
#define CCCI_IOC_SEND_RUN_TIME_DATA		_IO(CCCI_IOC_MAGIC, 7)	/*  md_init */
#define CCCI_IOC_GET_MD_INFO			_IOR(CCCI_IOC_MAGIC, 8, unsigned int)	/*  md_init */
#define CCCI_IOC_GET_MD_EX_TYPE			_IOR(CCCI_IOC_MAGIC, 9, unsigned int)	/*  mdlogger */
#define CCCI_IOC_SEND_STOP_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 10)	/*  muxreport */
#define CCCI_IOC_SEND_START_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 11)	/*  muxreport */
#define CCCI_IOC_DO_STOP_MD			_IO(CCCI_IOC_MAGIC, 12)	/*  md_init */
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13)	/*  md_init */
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14)	/*  RILD   */
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15)	/*  RILD */
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16)	/*  md_init */
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17)	/*  md_init */
#define CCCI_IOC_POWER_ON_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 18)
#define CCCI_IOC_POWER_OFF_MD_REQUEST		_IO(CCCI_IOC_MAGIC, 19)
#define CCCI_IOC_SIM_SWITCH			_IOW(CCCI_IOC_MAGIC, 20, unsigned int)	/*  RILD  */
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21)	/*  md_init  */
#define CCCI_IOC_SIM_SWITCH_TYPE		_IOR(CCCI_IOC_MAGIC, 22, unsigned int)	/*  RILD */
#define CCCI_IOC_STORE_SIM_MODE			_IOW(CCCI_IOC_MAGIC, 23, unsigned int)	/*  RILD */
#define CCCI_IOC_GET_SIM_MODE			_IOR(CCCI_IOC_MAGIC, 24, unsigned int)	/*  RILD */
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25)	/*  META  md_init  */
#define CCCI_IOC_GET_SIM_TYPE			_IOR(CCCI_IOC_MAGIC, 26, unsigned int)	/*  terservice */
#define CCCI_IOC_ENABLE_GET_SIM_TYPE		_IOW(CCCI_IOC_MAGIC, 27, unsigned int)	/*  terservice */
#define CCCI_IOC_SEND_ICUSB_NOTIFY		_IOW(CCCI_IOC_MAGIC, 28, unsigned int)	/*  icusbd */
#define CCCI_IOC_SET_MD_IMG_EXIST		_IOW(CCCI_IOC_MAGIC, 29, unsigned int)	/*  md_init */
#define CCCI_IOC_GET_MD_IMG_EXIST		_IOR(CCCI_IOC_MAGIC, 30, unsigned int)
#define CCCI_IOC_GET_MD_TYPE			_IOR(CCCI_IOC_MAGIC, 31, unsigned int)	/*  RILD */
#define CCCI_IOC_STORE_MD_TYPE			_IOW(CCCI_IOC_MAGIC, 32, unsigned int)	/*  RILD */
#define CCCI_IOC_GET_MD_TYPE_SAVING		_IOR(CCCI_IOC_MAGIC, 33, unsigned int)	/*  META */
#define CCCI_IOC_GET_EXT_MD_POST_FIX		_IOR(CCCI_IOC_MAGIC, 34, char[32])	/*  eemcs_fsd */
#define CCCI_IOC_FORCE_FD			_IOW(CCCI_IOC_MAGIC, 35, unsigned int)	/*  RILD(6577) */
#define CCCI_IOC_AP_ENG_BUILD			_IOW(CCCI_IOC_MAGIC, 36, unsigned int)	/*  md_init(6577) */
#define CCCI_IOC_GET_MD_MEM_SIZE		_IOR(CCCI_IOC_MAGIC, 37, unsigned int)	/*  md_init(6577) */
#define CCCI_IOC_UPDATE_SIM_SLOT_CFG		_IOW(CCCI_IOC_MAGIC, 38, unsigned int)	/*  RILD */
#define CCCI_IOC_GET_CFG_SETTING		_IOW(CCCI_IOC_MAGIC, 39, unsigned int)	/*  md_init */

#define CCCI_IOC_SET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 40, unsigned int)	/*  md_init */
#define CCCI_IOC_GET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 41, unsigned int)	/*  md_init */
/*metal tool to get modem protocol type: AP_TST or DHL */
#define CCCI_IOC_GET_MD_PROTOCOL_TYPE		_IOR(CCCI_IOC_MAGIC, 42, char[16])
#define CCCI_IOC_SEND_SIGNAL_TO_USER		_IOW(CCCI_IOC_MAGIC, 43, unsigned int) /* md_init */
/*md_init set MD boot env data before power on MD */
#define CCCI_IOC_SET_BOOT_DATA				_IOW(CCCI_IOC_MAGIC, 47, unsigned int[16])

/* ================================================================================== */
/*  API functions exported in ccci */
/* ================================================================================== */
int ccci_md_ctrl_init(int md_id);
void ccci_md_ctrl_exit(int md_id);
int ccci_chrdev_init(int md_id);
void ccci_chrdev_exit(int md_id);
int ccci_tty_init(int md_id);
void ccci_tty_exit(int md_id);
int ccci_ipc_init(int md_id);
void ccci_ipc_exit(int md_id);
int ccci_rpc_init(int md_id);
void ccci_rpc_exit(int md_id);
int ccci_fs_init(int md_id);
void ccci_fs_exit(int md_id);
int ccmni_init(int md_id);
void ccmni_exit(int md_id);
int ccci_vir_chrdev_init(int md_id);
void ccci_vir_chrdev_exit(int md_id);
int init_ccci_dev_node(void);
void release_ccci_dev_node(void);
int mk_ccci_dev_node(int md_id);
void ccci_dev_node_exit(int md_id);
int statistics_init(int md_id);
void statistics_exit(int md_id);

int get_dev_id_by_md_id(int md_id, char node_name[], int *major, int *minor);
int get_md_id_by_dev_major(int dev_major);
int init_ccci_dev_node(void);
int send_md_reset_notify(int);
int ccci_trigger_md_assert(int);

int get_md_exception_type(int md_id);
int send_md_stop_notify(int md_id);
int send_md_start_notify(int md_id);
int ccci_start_modem(int md_id);
int ccci_set_md_boot_data(int md_id, unsigned int data[], int len);
int ccci_stop_modem(int md_id, unsigned int timeout);
int ccci_set_reload_modem(int md_id);
int ccci_send_run_time_data(int md_id);
int statistics_init_ch_dir(int md_sys_id, int ch, int dir, char *name);
void dump_logical_layer_tx_rx_histroy(int md_id);
void logic_layer_ch_record_dump(int md_id, int ch);
void add_logic_layer_record(int md_id, struct ccci_msg_t *data, int drop);
void ccci_dump_logic_layer_info(int md_id, unsigned int buf[], int len);
void ccci_dump_hw_reg_val(int md_id, unsigned int buf[], int len);
int send_enter_flight_mode_request(int md_id);
int send_leave_flight_mode_request(int md_id);
int send_power_on_md_request(int md_id);
int send_power_down_md_request(int md_id);
int send_update_cfg_request(int md_id, unsigned int val);
int ccci_md_ctrl_common_init(void);
int bind_to_low_layer_notify(int md_id, void (*isr_func)(int),
			     void (*send_func)(int, unsigned int));
struct ccif_t *ccif_create_instance(struct ccif_hw_info_t *info, void *ctl_b, int md_id);
int register_ccci_attr_func(const char *buf, ssize_t (*show)(char *str),
			    ssize_t (*store)(const char *str, size_t len));
int get_common_cfg_setting(int md_id, int cfg[], int *num);
unsigned int get_sim_switch_type(void);
void ccmni_v2_dump(int md_id);
void ccci_start_bootup_timer(int md_id, int second);
void ccci_stop_bootup_timer(int md_id);
/* ================================================================================== */
/*  API functions for IPO-H */
/* ================================================================================== */
int ccci_uart_ipo_h_restore(int md_id);
int ccci_ipc_ipo_h_restore(int md_id);
int ccmni_ipo_h_restore(int md_id);
int ccci_ipo_h_restore(int md_id, char buf[], unsigned int len);

int ccci_misc_ipo_h_restore(int md_id);
extern unsigned long long lg_ch_tx_debug_enable[];
extern unsigned long long lg_ch_rx_debug_enable[];
extern unsigned int tty_debug_enable[];
extern unsigned int fs_tx_debug_enable[];
extern unsigned int fs_rx_debug_enable[];
extern void ccci_md_logger_notify(void);
extern void ccci_fs_resetfifo(int md_id);
extern int get_md_wakeup_src(int md_id, char *buf, unsigned int len);
extern struct kobject *kernel_kobj;
extern int ccmni_v1_init(int md_id);
extern void ccmni_v1_exit(int md_id);
extern int ccmni_v2_init(int md_id);
extern void ccmni_v2_exit(int md_id);
extern int ccmni_v1_ipo_h_restore(int md_id);
extern int ccmni_v2_ipo_h_restore(int md_id);

/* common func declare */
void ccci_attr_release(struct kobject *kobj);
ssize_t ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buf);
ssize_t ccci_attr_store(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
ssize_t show_attr_md1_postfix(char *buf);
ssize_t show_attr_md2_postfix(char *buf);

#endif				/* __CCCI_H__ */
