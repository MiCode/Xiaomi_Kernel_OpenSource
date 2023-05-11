/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_MFD_MAX77729_UIC_H
#define __LINUX_MFD_MAX77729_UIC_H
#include <linux/usb/typec.h>
#include <linux/pmic-voter.h>
#include <linux/extcon-provider.h>
#define MAX77729_SYS_FW_UPDATE

#include "max77729_pd.h"
#include "max77729_cc.h"

#define MAX_PDO_NUM 8
#define AVAILABLE_VOLTAGE 9000
#define UNIT_FOR_VOLTAGE 50
#define UNIT_FOR_CURRENT 10
#define UNIT_FOR_APDO_VOLTAGE 100
#define UNIT_FOR_APDO_CURRENT 50



typedef enum {
	TYPE_C_DETACH = 0,
	TYPE_C_ATTACH_DFP = 1, /* Host */
	TYPE_C_ATTACH_UFP = 2, /* Device */
	TYPE_C_ATTACH_DRP = 3, /* Dual role */
	TYPE_C_ATTACH_SRC = 4, /* SRC */
	TYPE_C_ATTACH_SNK = 5, /* SNK */
	TYPE_C_RR_SWAP = 6,
	TYPE_C_DR_SWAP = 7,
} PDIC_OTP_MODE;

typedef enum {
	TRY_ROLE_SWAP_NONE = 0,
	TRY_ROLE_SWAP_PR = 1, /* pr_swap */
	TRY_ROLE_SWAP_DR = 2, /* dr_swap */
	TRY_ROLE_SWAP_TYPE = 3, /* type */
} PDIC_ROLE_SWAP_MODE;

typedef enum {
	TRY_PPS_NONE = 0,
	TRY_PPS_ENABLE = 1, /* pr_swap */
} PDIC_PPS_MODE;


struct max77729_opcode {
	unsigned char opcode;
	unsigned char data[OPCODE_DATA_LENGTH];
	int read_length;
	int write_length;
};

typedef struct max77729_usbc_command_data {
	u8	opcode;
	u8  prev_opcode;
	u8	response;
	u8	read_data[OPCODE_DATA_LENGTH];
	u8	write_data[OPCODE_DATA_LENGTH];
	int read_length;
	int write_length;
	u8	reg;
	u8	val;
	u8	mask;
	u8  seq;
	int noti_cmd;
	u8	is_uvdm;
} usbc_cmd_data;

typedef struct max77729_usbc_command_node {
	usbc_cmd_data				cmd_data;
	struct max77729_usbc_command_node	*next;
} usbc_cmd_node;

typedef struct max77729_usbc_command_node	*usbc_cmd_node_p;

typedef struct max77729_usbc_command_queue {
	struct mutex			command_mutex;
	usbc_cmd_node			*front;
	usbc_cmd_node			*rear;
	usbc_cmd_node			tmp_cmd_node;
} usbc_cmd_queue_t;

#if defined(CONFIG_SEC_FACTORY)
#define FAC_ABNORMAL_REPEAT_STATE			12
#define FAC_ABNORMAL_REPEAT_RID				5
#define FAC_ABNORMAL_REPEAT_RID0			3
struct AP_REQ_GET_STATUS_Type {
	uint32_t FAC_Abnormal_Repeat_State;
	uint32_t FAC_Abnormal_Repeat_RID;
	uint32_t FAC_Abnormal_RID0;
};
#endif

#define DATA_ROLE_SWAP 1
#define POWER_ROLE_SWAP 2
#define VCONN_ROLE_SWAP 3
#define TRY_ROLE_SWAP_WAIT_MS 5000
#define TYPEC_HOST 1
#define TYPEC_DEVICE  0x0

#define USB_PD_MI_SVID			0x2717

// typedef enum {
	// TRY_ROLE_SWAP_NONE = 0,
	// TRY_ROLE_SWAP_PR = 1, [> pr_swap <]
	// TRY_ROLE_SWAP_DR = 2, [> dr_swap <]
	// TRY_ROLE_SWAP_TYPE = 3, [> type <]
// } CCIC_ROLE_SWAP_MODE;

enum uvdm_state {
	USBPD_UVDM_DISCONNECT,
	USBPD_UVDM_CHARGER_VERSION,
	USBPD_UVDM_CHARGER_VOLTAGE,
	USBPD_UVDM_CHARGER_TEMP,
	USBPD_UVDM_SESSION_SEED,
	USBPD_UVDM_AUTHENTICATION,
	USBPD_UVDM_VERIFIED,
	USBPD_UVDM_REMOVE_COMPENSATION,
	USBPD_UVDM_CONNECT,
	USBPD_UVDM_NAN_ACK,
};

#define UVDM_HDR_CMD(hdr)	((hdr) & 0xFF)


#define USBPD_UVDM_SS_LEN		4

struct usbpd_vdm_data {
	int ta_version;
	int ta_temp;
	int ta_voltage;
	unsigned long s_secert[USBPD_UVDM_SS_LEN];
	unsigned long digest[USBPD_UVDM_SS_LEN];
};

enum {
	SENT_REQ_MSG = 0,
	ERR_SNK_RDY = 5,
	ERR_PD20,
	ERR_SNKTXNG,
};

#define NAME_LEN_HMD	14
#define MAX_NUM_HMD	32
#define TAG_HMD	"HMD"

struct max77729_hmd_power_dev {
	uint vid;
	uint pid;
	char hmd_name[NAME_LEN_HMD];
};

struct max_adapter_device {
	struct device dev;
};

struct max77729_usbc_platform_data {
	struct max77729_dev *max77729;
	struct device *dev;
	struct i2c_client *i2c; /*0xCC */
	struct i2c_client *muic; /*0x4A */
	struct i2c_client *charger; /*0x2A; Charger */

 	struct votable          *icl_votable;
	struct votable          *fv_votable;
	struct votable          *chgen_votable;
	struct extcon_dev		*extcon;

	int irq_base;

	/* interrupt pin */
	int irq_apcmd;
	int irq_sysmsg;

	/* VDM pin */
	int irq_vdm0;
	int irq_vdm1;
	int irq_vdm2;
	int irq_vdm3;
	int irq_vdm4;
	int irq_vdm5;
	int irq_vdm6;
	int irq_vdm7;
	int irq_vir0;

	int get_src_ext;

	/* register information */
	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;

	uint32_t adapter_svid;
	uint32_t adapter_id;
	uint32_t xid;

	/* opcode register information */
	u8 op_ctrl1_w;

	int watchdog_count;
	int por_count;

	u8 opcode_res;
	/* USBC System message interrupt */
	u8 sysmsg;
	u8 pd_msg;

	/* F/W state */
	u8 HW_Revision;
	u8 FW_Revision;
	u8 FW_Minor_Revision;
	u8 plug_attach_done;
	int op_code_done;
    /* F/W opcode Thread */

	struct work_struct op_wait_work;
	struct work_struct op_send_work;
	struct work_struct cc_open_req_work;
#ifdef MAX77729_SYS_FW_UPDATE
	struct work_struct fw_update_work;
#endif
	struct workqueue_struct	*op_wait_queue;
	struct workqueue_struct	*op_send_queue;
	struct completion op_completion;
	int op_code;
	int is_first_booting;
	usbc_cmd_data last_opcode;
	unsigned long opcode_stamp;
	struct mutex op_lock;

	/* F/W opcode command data */
	usbc_cmd_queue_t usbc_cmd_queue;

	uint32_t alternate_state;
	uint32_t acc_type;
	uint32_t Vendor_ID;
	uint32_t Product_ID;
	uint32_t Device_Version;
	uint32_t SVID_0;
	uint32_t SVID_1;
	uint32_t SVID_DP;
	struct delayed_work acc_detach_work;
	uint32_t dp_is_connect;
	uint32_t dp_hs_connect;
	uint32_t dp_selected_pin;
	u8 pin_assignment;
	uint32_t is_sent_pin_configuration;
	wait_queue_head_t host_turn_on_wait_q;
	wait_queue_head_t device_add_wait_q;
	int host_turn_on_event;
	int host_turn_on_wait_time;
	int device_add;
	int is_samsung_accessory_enter_mode;
	int send_enter_mode_req;
	int send_vdm_identity;

	u8 sbu[2];
	struct completion ccic_sysfs_completion;
	struct completion psrdy_wait;
	struct max77729_muic_data *muic_data;
	struct max77729_pd_data *pd_data;
	struct max77729_cc_data *cc_data;

	struct max77729_platform_data *max77729_data;

	struct workqueue_struct *ccic_wq;
	int manual_lpm_mode;
	int fac_water_enable;
	int cur_rid;
	int pd_state;
	u8  vconn_test;
	u8  vconn_en;
	u8  fw_update;
	int is_host;
	int is_client;
	bool auto_vbus_en;
	u8 cc_pin_status;
	int ccrp_state;
	int vsafe0v_status;

	struct typec_port *port;
	struct max_adapter_device *adapter_dev;
	struct typec_partner *partner;
	struct usb_pd_identity partner_identity;
	struct typec_capability typec_cap;
	// struct completion typec_reverse_completion;
	int typec_power_role;
	int typec_data_role;
	int typec_try_state_change;
	int typec_try_pps_enable;
	int pwr_opmode;
	bool pd_support;
	struct delayed_work usb_external_notifier_register_work;
	struct notifier_block usb_external_notifier_nb;
	int mpsm_mode;
	bool mdm_block;
	int vbus_enable;
	int pd_pr_swap;
	int src_cap_flag;
	int shut_down;
	struct delayed_work vbus_hard_reset_work;
	uint8_t ReadMSG[32];
	int ram_test_enable;
	int ram_test_retry;
	int ram_test_result;

    struct completion typec_reverse_completion;
    struct usbpd_vdm_data   vdm_data;
    int			uvdm_state;
    struct completion uvdm_longpacket_out_wait;
	struct completion pps_in_wait;
	int is_in_first_sec_uvdm_req;
	int is_in_sec_uvdm_out;
	bool pn_flag;
	int uvdm_error;

	int detach_done_wait;
	int set_altmode;
	int set_altmode_error;

	u8 control3_reg;
	int cc_open_req;

	bool recover_opcode_list[OPCODE_NONE];
	int need_recover;
	bool srcccap_request_retry;

	int ovp_gpio;
	struct mutex hmd_power_lock;
	struct max77729_hmd_power_dev  *hmd_list;

	/* xiaomi add start */
	struct power_supply	*usb_psy;
	int pd_active;
	bool verify_process;
	bool verifed;
	uint32_t received_pdos[7];
	bool sink_Ready;
	bool source_Ready;
	bool is_hvdcp;
	/* xiaomi add start */
};

/* Function Status from s2mm005 definition */
typedef enum {
	max77729_State_PE_Initial_detach	= 0,
	max77729_State_PE_SRC_Send_Capabilities = 3,
	max77729_State_PE_SNK_Wait_for_Capabilities = 17,
} max77729_pd_state_t;

typedef enum {
	MPSM_OFF = 0,
	MPSM_ON = 1,
} CCIC_DEVICE_MPSM;

#define DATA_ROLE_SWAP 1
#define POWER_ROLE_SWAP 2
#define VCONN_ROLE_SWAP 3
#define MANUAL_ROLE_SWAP 4
#define ROLE_ACCEPT			0x1
#define ROLE_REJECT			0x2
#define ROLE_BUSY			0x3

int max77729_pd_init(struct max77729_usbc_platform_data *usbc_data);
int max77729_cc_init(struct max77729_usbc_platform_data *usbc_data);
int max77729_muic_init(struct max77729_usbc_platform_data *usbc_data);
int max77729_i2c_opcode_read(struct max77729_usbc_platform_data *usbc_data,
		u8 opcode, u8 length, u8 *values);

void init_usbc_cmd_data(usbc_cmd_data *cmd_data);
void max77729_usbc_clear_queue(struct max77729_usbc_platform_data *usbc_data);
void max77729_usbc_opcode_rw(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *opcode_r, usbc_cmd_data *opcode_w);
void max77729_usbc_opcode_write(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op);
void max77729_usbc_opcode_write_immediately(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *write_op);
void max77729_usbc_opcode_read(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op);
void max77729_usbc_opcode_push(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op);
void max77729_usbc_opcode_update(struct max77729_usbc_platform_data *usbc_data,
	usbc_cmd_data *read_op);

void max77729_ccic_event_work(void *data, int dest, int id,
		int attach, int event, int sub);
void max77729_pdo_list(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data);
void max77729_response_pdo_request(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data);
void max77729_response_apdo_request(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data);
void max77729_response_set_pps(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data);
void max77729_current_pdo(struct max77729_usbc_platform_data *usbc_data,
		unsigned char *data);
void max77729_check_pdo(struct max77729_usbc_platform_data *usbc_data);
void max77729_detach_pd(struct max77729_usbc_platform_data *usbc_data);
void max77729_notify_rp_current_level(struct max77729_usbc_platform_data *usbc_data);
extern void max77729_vbus_turn_on_ctrl(struct max77729_usbc_platform_data *usbc_data, bool enable, bool swaped);
extern void max77729_dp_detach(void *data);
void max77729_usbc_disable_auto_vbus(struct max77729_usbc_platform_data *usbc_data);
int max77729_get_pd_support(struct max77729_usbc_platform_data *usbc_data);
bool max77729_sec_pps_control(int en);

extern const uint8_t BOOT_FLASH_FW_PASS2[];

// #define DEBUG_MAX77729
// #ifdef DEBUG_MAX77729
// #define msg_maxim(format, args...) \
		// pr_err("max77729: %s: " format "\n", __func__, ## args)
// #else
#define msg_maxim(format, args...)
// #endif [> DEBUG_MAX77766<]
#endif
