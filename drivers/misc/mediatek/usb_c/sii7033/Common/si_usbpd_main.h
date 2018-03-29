/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#ifndef SII_USBPD_MAIN_H
#define SII_USBPD_MAIN_H
#include <Wrap.h>
#include "si_usbpd_core.h"

#define SII_DRV_DEVICE_I2C_ADDR 0x68
#define I2C_RETRY_MAX           10

#define SII_DRIVER_NAME "sii70xx"
#define COMPATIBLE_NAME "simg,sii70xx"

extern struct usbtypc *g_exttypec;
extern struct sii70xx_drv_context *g_drv_context;
extern irqreturn_t sii7033_eint_isr(int irqnum, void *data);
extern irqreturn_t usbpd_irq_handler(int irq, void *data);
extern int trigger_driver(struct usbtypc *typec, int type, int stat, int dir);

extern int drp_mode;
#define DRP 0
#define DFP 1
#define UFP 2

/*#include "si_usbpd.h"*/
#define USBPD_EH_CLOSE       (1 << 0)
#define USBPD_EH_RESET       (1 << 1)

#define DEFAULT_MIN	6
#define DEFAULT_MAX	15
#define MIN_1_5	20
#define MAX_1_5	30
#define MIN_3	36
#define MAX_3	52

#define INT_INDEX	0
#define VBUS_SRC	1
#define VBUS_SNK	2
#define RESET_CTRL	3
#define NUM_GPIO	4
#define VBUS_DEFAULT	5



#define PDO_TYPE_BATTERY	(1 << 30)
#define PDO_TYPE_VARIABLE	(2 << 30)
#define PDO_TYPE_MASK		(3 << 30)
#define PD_MAX_VOLTAGE_MV	20000
#define PD_MAX_POWER_MW		100000

#define ATTACH 1
#define DETTACH 0

#define SII_DRV_NAME "sii70xx"
#define SII_PLATFORM_DEBUG_ASSERT(expr) ((void)((/*lint -e{506}*/!(expr)) ?\
SiiPlatformDebugAssert(__FILE__, __LINE__, (uintptr_t)(expr), NULL),\
	((void)1) : ((void)0)))

#define MIN(a, b)                                       \
	({                                              \
	 __typeof__(a) temp_a = (a);             \
	 __typeof__(b) temp_b = (b);             \
	 \
	 temp_a < temp_b ? temp_a : temp_b;      \
	 })

enum request_type {
	REQUEST_VSAFE5V,
	REQUEST_MAX
};

#define PD_MAX_CURRENT_MA     3000
#define DEFAULT_VCONN_MIN 0
#define DEFAULT_VCONN_MAX 4

/*struct gpio;*/
void SiiPlatformDebugAssert
(const char *pFileName, uint32_t lineNumber,
uint32_t expressionEvaluation, const char *pConditionText);
void usbpd_pf_i2c_init(struct i2c_adapter *adapter);

void sii_platform_wr_reg8(uint8_t addr, uint8_t val);
uint8_t sii_platform_rd_reg8(uint8_t addr);
void sii_platform_set_bit8(uint8_t addr, uint8_t mask);
void sii_platform_clr_bit8(uint8_t addr, uint8_t mask);
void sii_platform_put_bit8(uint8_t addr, uint8_t mask, uint8_t val);

void sii_platform_fifo_write8(uint8_t addr, const uint8_t *p_data, uint16_t size);
void sii_platform_fifo_read8(uint8_t addr, uint8_t *p_data, uint16_t size);
void sii_platform_block_write8(uint8_t addr, const uint8_t *p_data, uint16_t size);
void sii_platform_block_read8(uint8_t addr, uint8_t *p_data, uint16_t size);
void sii_platform_vbus_control(struct sii70xx_drv_context *drv_context, uint8_t vbus_gpio);
void sii_platform_read_70xx_gpio(struct sii70xx_drv_context
				 *drv_context);
void sii_platform_vbus_gpio_control(struct sii70xx_drv_context *drv_context, uint8_t vbus_type);

enum typec_orientation {
	TYPEC_CABLE_FLIPPED = 0,
	TYPEC_CABLE_NOT_FLIPPED,
	TYPEC_UNDEFINED
};
bool sm_reset_sysfs(uint8_t bus_id, bool data);
bool send_hard_reset_sysfs(uint8_t bus_id, bool data);
bool set_custom_msg_sysfs(uint8_t bus_id, uint8_t data, bool);
bool alt_mdoe_est_sysfs(uint8_t bus_id, bool data);
bool ufp_dfp_on_the_fly_sysfs(uint8_t bus_id, bool data);
bool process_sysfs_commands(uint8_t bus_id, enum ctrl_msg type, uint32_t *src_pdo, uint8_t count);
bool xmit_vdm_resp_sysfs(uint8_t type, uint8_t port);


enum source_port_engine {
	PE_SRC_Startup,
	PE_SRC_Transition_to_default,
	PE_SRC_Discovery,
	PE_SRC_Disabled,
	PE_SRC_Send_Capabilities,
	PE_SRC_Soft_Reset,
	PE_SRC_Send_Soft_Reset,
	PE_SRC_Hard_Reset,
	PE_SRC_Negotiate_Capability,
	PE_SRC_Capability_Response,
	PE_SRC_Transition_Supply,
	PE_SRC_Ready,
	PE_SRC_Give_Source_Cap,
	PE_SRC_Get_Sink_Cap,
	PE_SWAP_Wait_Good_Crc_Received,
	PE_SRC_Wait_Ps_Rdy_Good_Crc_Received,
	PE_SRC_Wait_Accept_Good_Crc_Received,
	PE_SRC_Wait_Ps_Rdy_sent,
	PE_SRC_Wait_Src_rdy_Good_Crc_Received,
	PE_SRC_Wait_cap_met_Good_Crc_Received,
	PE_SRC_Wait_sft_rst_Good_Crc_Received,
	PE_SRC_Wait_Req_Msg_Received,
	PE_SRC_Wait_HR_Good_Crc_Received,
	PE_SRC_Src_rdy_Good_Crc_Received,

	DFP_Identity_Request,
	DFP_Identity_NAKed,
	DFP_SVIDs_Request,
	DFP_SVIDs_NAKed,
	drp_modes_Request,
	drp_modes_NAKed,
	drp_mode_Entry_Request,
	drp_mode_Entry_ACKed,
	drp_mode_Entry_NAKed,


	DFP_CUSTOM_STATE,
	PE_DFP_VDM_Identity_Request,
	PE_DFP_VDM_Identity_ACKed,
	PE_DFP_VDM_Identity_NAKed,
	PE_DFP_VDM_SVIDs_Request,
	PE_DFP_VDM_SVIDs_ACKed,
	PE_DFP_VDM_SVIDs_NAKed,
	PE_DFP_VDM_Modes_Request,
	PE_DFP_VDM_Modes_ACKed,
	PE_DFP_VDM_Modes_NAKed,
	PE_DFP_VDM_Mode_Entry_Request,
	PE_DFP_VDM_Mode_Entry_ACKed,
	PE_DFP_VDM_Mode_Entry_NAKed,
	PE_DFP_VDM_Mode_Exit_Request,
	PE_drp_mode_Exit_ACKed,
	PE_DFP_VDM_Attention_Request,

	PE_SRC_Swap_Init,
	PE_PRS_Wait_Accept_PR_Swap,
	PE_PRS_SRC_SNK_Evaluate_Swap,
	PE_PRS_SRC_SNK_Accept_Swap,
	PE_Wait_Accept_Good_Crc_Received,
	PE_PRS_SRC_SNK_Transition_to_off,
	PE_PRS_SRC_SNK_Source_off,
	PE_PRS_SRC_SNK_Send_Swap,
	PE_PRS_SRC_SNK_Assert_Rd,
	PE_PRS_SRC_SNK_Reject_PR_Swap,
	PE_DRS_DFP_UFP_Evaluate_DR_Swap,
	PE_DRS_DFP_UFP_Change_to_UFP,
	PE_DRS_DFP_UFP_Send_DR_Swap,
	PE_DRS_DFP_UFP_Reject_DR_Swap,
	PE_SRC_Wait_Reject_Good_Crc_Received,

	PE_SNK_Startup,
	PE_SNK_Discovery,
	PE_SNK_Wait_for_Capabilities,
	PE_SNK_Soft_Reset,
	PE_SNK_Send_Soft_Reset,
	PE_SNK_Evaluate_Capability,
	PE_SNK_Select_Capability,
	PE_SNK_Transition_Sink,
	PE_SNK_Ready,
	PE_SNK_Give_Sink_Cap,
	PE_SNK_Get_Source_Cap,
	PE_SNK_Transition_to_default,
	PE_SNK_Hard_Reset,
	PE_DB_CP_Check_for_VBUS,
	PE_SNK_Wait_Request_Good_Crc_Received,
	PE_SNK_Wait_sft_rst_Good_Crc_Received,
	PE_SNK_Wait_Accept_Good_Crc_Received,
	PE_SNK_Wait_Accept_Sft_Rst_Received,
	PE_SNK_Wait_HR_Good_Crc_Received,
	PE_SNK_Wait_Reject_Good_Crc_Received,
	ErrorRecovery,


	UFP_Get_Identity,
	UFP_Send_Identity,
	UFP_Get_SVIDs,
	UFP_Send_SVIDs,
	UFP_Get_Modes,
	UFP_Send_Modes,
	UFP_CUSTOM_STATE,


	PE_UFP_VDM_Get_Identity,
	PE_UFP_VDM_Send_Identity,
	PE_UFP_VDM_Get_SVIDs,
	PE_UFP_VDM_Send_SVIDs,
	PE_UFP_VDM_Get_Modes,
	PE_UFP_VDM_Send_Modes,
	PE_UFP_VDM_Evaluate_Mode_Entry,
	PE_UFP_VDM_Mode_Entry_ACK,
	PE_UFP_VDM_Mode_Entry_NAK,
	PE_UFP_VDM_Mode_Exit,
	PE_UFP_VDM_Mode_Exit_ACK,
	PE_UFP_VDM_Attention_Request,
	PE_PRS_SNK_SRC_Evaluate_Swap,
	PE_PRS_SRC_SNK_Transition_to_off_wait,
	PE_PRS_SNK_SRC_Accept_Swap,
	PE_PRS_SNK_SRC_Transition_to_off,
	PE_PRS_SNK_SRC_Source_on,
	PE_PRS_SNK_SRC_Assert_Rp,
	PE_PRS_SNK_SRC_Send_Swap,
	PE_PRS_SNK_SRC_Reject_Swap,
	PE_DRS_UFP_DFP_Evaluate_DR_Swap,
	PE_DRS_UFP_DFP_Accept_DR_Swap,
	PE_DRS_UFP_DFP_Change_to_DFP,
	PE_DRS_UFP_DFP_Reject_DR_Swap,
	PE_DRS_UFP_DFP_Send_DR_Swap,

	PE_SNK_Swap_Init,
	DFP_SVIDs_ENTER_MODE_ACK_RCVD,
	DFP_SVIDs_Enter_Modes_Good_Crc_Rcvd,
	DFP_DISCOVER_SVID_MODES_ACK,
	DFP_SVIDs_Modes_Good_Crc_Rcvd,
	DFP_SVIDs_Wait_Good_Crc_Rcvd,
	DFP_DISCOVER_SVID_ACK,
	PE_VDM_Wait_Good_Src_Received,
	PE_DFP_VDM_Wait_Identity_ACK_Rcvd,
	PE_SWAP_Wait_Accept_Good_CRC_Received,
	PE_SWAP_Wait_Ps_Rdy_Received,
	PE_SWAP_PS_RDY_Ack_Rcvd,
	PE_DFP_VDM_Init,
	PE_VDM_SVID_Wait_Good_Src_Received,
	PE_VDM_Modes_Wait_Good_Src_Received,
	PE_VDM_Enter_Wait_Good_Src_Received,
	PE_drp_mode_Wait_Exit_ACKed,
	PE_Exit_Mode_Good_Crc_Received,
	PE_SRC_DR_Swap_Init,
	PE_PRS_Wait_Accept_DR_Swap,
	PE_DRS_DFP_UFP_Accept_DR_Swap,
	PE_DRS_Swap_exit,
	PE_PRS_Swap_exit,
	PE_SNK_DR_Swap_Init,
	PE_SNK_Wait_Ps_Rdy_Received,
	DFP_SVIDs_ENTERY_NAK,
	DFP_SVIDs_MODES_NAK,
	DFP_SVIDs_SVID_NAK,
	DFP_SVIDs_EXIT_NAK,
	DFP_SVIDs_IDENTITY_NAK,
	PE_VCS_DFP_Send_Swap,
	PE_VCS_DFP_Wait_for_UFP_VCONN,
	PE_VCS_DFP_Turn_Off_VCONN,
	PE_VCS_DFP_Turn_On_VCONN,
	PE_VCS_DFP_Send_PS_Rdy,
	PE_PRS_Wait_Accept_VCONN_Swap,
	PE_VCS_DFP_Turn_ON_VCONN,

	PE_VCS_UFP_Evaluate_Swap,
	PE_VCS_UFP_Accept_Swap,
	PE_VCS_UFP_Wait_for_DFP_VCONN,
	PE_VCS_UFP_Turn_On_VCONN,
	PE_VCS_UFP_Send_PS_Rdy,

	PE_VCS_UFP_Turn_Off_VONN,
	PE_VCS_UFP_Reject_VCONN_Swap,
	PE_SNK_Wait_Good_Crc_Goto_Min,
	PE_SNK_Send_Goto_Min,
};

enum usbpd_role {
	USBPD_UFP,
	USBPD_DFP,
	USBPD_DRP
};

enum usbpd_device_status {
	PDEV_AVAILABLE = 0x01,
	PDEV_USED,
	PDEV_ERROR
};


enum phy_sm_state {
	DISABLED,
	ERROR_RECOVERY,
	UNATTACHED,
	UFP_UNATTACHED,
	ATTACHED_UFP,
	ACCESSORY_PRESENT,
	DFP_UNATTACHED,
	POWER_DEFAULT,
	POWER_1_5_UFP,
	POWER_3_UFP,
	ATTACHED_DFP,
	DFP_DRP_WAIT,
	LOCK_UFP,
	TRY_DFP,
	AUDIO_ACCESSORY,
	DEBUG_ACCESSORY
};

struct sii_usbpd_intf {
	struct pd_cb_params param;
	uint8_t *pd_msg;
};

struct si_data_intf {
	uint8_t count;
	uint8_t recv_msg[30];
};


enum req_status {
	VAR_CAN_BE_MET,
	VAR_CAN_NOT_BE_MET,
	VAR_LATER
};

#define ADC_ENABLE		(1 << 1)
#define VBUS_ENABLE		(1 << 2)
#define NO_DEVICE_CONNECTED	(1 << 3)
#define CONNECTION_DONE		(1 << 4)
#define TYPEC_MASK		0xFFFFFFFF

enum usb_pd_timers {
	USB_PD_CRC_RECV_TMR = 0,	/* CRC receive Timer */
	USB_PD_SENDR_RESP_TMR,	/* Sender response timer */
	USB_PD_SOURCE_ACT_TMR,	/* Source Activity timer */
	USB_PD_PS_TRNS_TMR,	/* ps transition timer */
	USB_PD_SINK_ACT_TMR,	/* sink activity timer */
	USB_PD_NO_RESP_TMR,	/* No response timer */
	USB_PD_SINK_WAIT_CAP_TMR,	/* sink wait capability timer */
	USB_PD_VDM_MODE_ENTRY_TMR,	/* vdm mode entry timer */
	USB_PD_VDM_RESP_TMR,	/* vdm response timer */
	USB_PD_VDM_EXIT_TMR,	/* vdm exit timer */
	USB_PD_PS_SOURCE_ON_TMR,	/* ps source on timer */
	USB_PS_SOURCE_OFF_TMR,	/* ps source off timer */
	USB_PD_SOURCE_CAP_TMR,	/* source capability timer */
	USB_PD_SOURCE_SWAP_TMR,	/* source swap timer */
	USB_PD_MAX_TIMERS
};


#define HARD_RESET_COMPLETE	(1 << 1)
#define EVALUATE_PR_SWAP	(1 << 2)
#define POWER_SUPPLY_ENABLE	(1 << 3)
#define	POWER_SUPPLY_DISABLE	(1 << 4)
#define	ASSERT_RD		(1 << 5)
#define	ASSERT_RP		(1 << 6)
#define	HARD_RESET_STATE_TRANS	(1 << 7)
#define	TRANSITION_DEFAULT	(1 << 8)
#define	EVALUATE_DR_SWAP	(1 << 9)
#define	SRC_DR_SWAP_COMPLETE	(1 << 10)
#define	SNK_DR_SWAP_COMPLETE	(1 << 11)
#define	PROTOCOL_ERROR		(1 << 12)
#define	NO_FEATURE		(1 << 13)
#define	EVALUATE_IDENTITY	(1 << 14)
#define	EVALUATE_SVIDS		(1 << 15)
#define	EVALUATE_MODES		(1 << 16)
#define	EVALUATE_ENTER_MODE	(1 << 17)
#define	SET_EXIT_MODE		(1 << 18)
#define	SUPPLY_ENABLE		(1 << 19)
#define	EVAL_REQ		(1 << 20)
#define	PD_DONE			(1 << 21)
#define	RESET_DONE		(1 << 22)
#define	NO_CONNECTION		(1 << 23)
#define	GET_SRC_CAPS		(1 << 24)
#define	GET_SNK_CAPS		(1 << 25)
#define	PROTOCOL_RESET	(1 << 26)
#define	MASK			0xFFFFFFFF

enum usbpd_mode {
	USB,
	MHL
};

enum ufp_volsubstate {
	UFP_DEFAULT = 0,
	UFP_1P5V,
	UFP_3V
};

enum phy_drp_config {
	TYPEC_DRP_TOGGLE_RD = 1,
	TYPEC_DRP_TOGGLE_RP = 2,
	TYPEC_DRP_DFP = 4,
	TYPEC_DRP_UFP = 8
};

enum xceiv_config {
	PD_TX,
	PD_RX,
	PD_NONE
};

struct swap_config {
	bool req_send;
	bool req_rcv;
	bool in_progress;
	bool done;
	bool enable;
};

struct sii_usbp_device_policy {
	bool power_request;
	bool go_to_min;
	bool get_sink_cap;
	bool pr_swap_supp;
	bool identity_request;
	bool svid_request;
	bool modes_request;
	bool entry_request;
	bool dr_swap_supp;
	bool vconn_swap;
};

struct vbus_status_reg {
	uint8_t ccctr22_reg;
	uint8_t ccctr21_reg;
	uint8_t ccctr03_reg;
	uint8_t ccctr02_reg;
	uint8_t work;
};
struct sii70xx_drv_context {
	void *ptypec;
	void *pusbpd_policy;
	void *pUsbpd_prot;
	void *usbpd_inst_disconnect_tmr;
	void *usbpd_inst_stat_mon_tmr;
	struct sii_usbp_device_policy *pUsbpd_dp_mngr;
	struct completion disconnect_done_complete;
	struct vbus_status_reg *vbus_status;
	struct device *dev;
	struct gpio *sii_gpio;
	struct class *usbpd_class;
	struct i2c_client *client;
	spinlock_t irq_lock;
	struct semaphore isr_lock;
	dev_t dev_num;
	enum phy_drp_config phy_mode;
#define	PD_UFP_ATTACHED		0x01
#define	PD_UFP_DETACHED		0x02
#define	PD_DFP_ATTACHED		0x03
#define	PD_DFP_DETACHED		0x04
#define	PD_POWER_LEVELS		0x05
#define	PD_UNSTRUCTURED_VDM	0x06
#define	PD_DFP_EXIT_MODE_DONE	0x07
#define	PD_DFP_ENTER_MODE_DONE	0x08
#define	PD_UFP_EXIT_MODE_DONE	0x09
#define	PD_UFP_ENTER_MODE_DONE	0x10
#define PD_PR_SWAP_DONE		0x11
#define PD_PR_SWAP_EXIT		0x12
#define PD_DR_SWAP_DONE		0x13
#define PD_DR_SWAP_EXIT		0x14

	struct cdev usbpd_cdev;
	int irq;
#if defined(I2C_DBG_SYSFS)
#define DEV_FLAG_RESET          0x00
#define DEV_FLAG_SHUTDOWN       0x01	/* Device is shutting down */
#define DEV_FLAG_COMM_MODE      0x02	/* Halt INTR processing */
	uint16_t debug_i2c_address;
	uint16_t debug_i2c_offset;
	uint16_t debug_i2c_xfer_length;
	uint8_t dev_flags;
#define DEV_FLAG_SHUTDOWN	0x01	/* Device is shutting down */
#define DEV_FLAG_COMM_MODE	0x02	/* Halt INTR processing */

#endif
	enum usbpd_role drp_config;
	enum usbpd_role old_drp_config;

	bool irq_disable;
	bool drp_mode;
	bool connection_status;
};

struct sii_typec {
	struct sii70xx_drv_context *drv_context;
	WORK_STRUCT sm_work;
	WORK_QUEUE_STRUCT *typec_work_queue;
	TASK_STRUCT typec_sm;
	void *phy_inst_cc_vol_timer;
	struct completion adc_read_done_complete;
	WORK_STRUCT adc_work;
	struct mutex typec_lock;
	struct mutex typec_adc_lock;

	enum ufp_volsubstate pwr_ufp_sub_state;
	enum ufp_volsubstate pwr_dfp_sub_state;
	enum phy_sm_state state;
	enum phy_sm_state prev_state;
	enum phy_sm_state next_state;
	enum typec_orientation is_flipped;

	unsigned long inputs;
#define DFP_ATTACHED	1
#define DFP_DETACHED	2
#define UFP_ATTACHED	3	/*Interrupt status */
#define UFP_DETACHED	4

#define FEAT_VCONN	5
#define FEAT_PR_SWP	6
#define FEAT_DR_SWP	7
#define FEAT_VCONN_SWP	8

#define PR_SWAP_DONE	9
#define DR_SWAP_DONE	10
#define VCON_SWAP_DONE	11

	uint8_t rcd_data1, rcd_data2;
	uint8_t cc_mode;

	bool typec_event;
	bool adc_vol_chng_event;
	bool dead_battery;
	bool dfp_attached;
	bool ufp_attached;
	bool is_vbus_detected;
	bool typecDrp;
};

/*usbp PD device policy Engine*/
struct sii_usbp_policy_engine {
	struct sii70xx_drv_context *drv_context;
	WORK_STRUCT pd_dfp_work;
	WORK_STRUCT pd_ufp_work;
	WORK_QUEUE_STRUCT *dfp_work_queue;
	WORK_QUEUE_STRUCT *ufp_work_queue;
	WAIT_QUEUE_HEAD_T typec_sm_waitq;

	 bool (*evnt_notify_fn)(void *, uint32_t);
	void *usbpd_inst_sendr_resp_tmr;
	void *usbpd_inst_source_act_tmr;
	void *usbpd_tSnkTransition_tmr;
	void *usbpd_inst_ps_trns_tmr;
	void *usbpd_inst_sink_act_tmr;
	void *usbpd_source_cap_tmr;
	void *usbpd_source_swap_tmr;
	void *usbpd_ps_source_off_tmr;
	void *usbpd_inst_ps_source_on_tmr;
	void *usbpd_vdm_exit_tmr;
	void *usbpd_vdm_resp_tmr;
	void *usbpd_vdm_mode_entry_tmr;
	void *usbpd_inst_sink_wait_cap_tmr;
	void *usbpd_inst_no_resp_tmr;
	void *usbpd_inst_crc_recv_tmr;
	void *usbpd_inst_sink_req_tmr;
	void *usbpd_inst_snk_sendr_resp_tmr;
	void *usbpd_inst_tbist_tmr;
	void *usbpd_inst_vconn_on_tmr;

	struct sii_usbpd_intf intf;
	struct config_param cfg;
	struct swap_config pr_swap;
	struct swap_config dr_swap;
	struct swap_config vconn_swap;
	struct mutex dfp_lock;
	struct mutex ufp_lock;

	enum source_port_engine state;
	enum source_port_engine next_state;
	enum source_port_engine pr_swap_state;
	enum source_port_engine alt_mode_state;
	enum source_port_engine dr_swap_state;
	enum source_port_engine vconn_swap_state;

	uint8_t svid_mode;
	uint8_t caps_counter;
	uint8_t hard_reset_counter;
	uint8_t hard_reset_in_progress;
	uint8_t pd_snk_cap_cnt;
	uint8_t pd_src_cap_cnt;

	uint32_t pd_src_caps[PDO_MAX_OBJECTS];
	uint32_t pd_snk_caps[PDO_MAX_OBJECTS];
	uint32_t vdm_cnt;
	uint32_t rdo;

	bool busy_flag;
	bool crc_timer_inpt;
	bool api_dr_swap;
	bool alt_mode_req_rcv;
	bool alt_mode_req_send;
	bool at_mode_established;
	bool src_cap_req;
	bool alt_mode_cmnd_xmit;
	bool exit_mode_req_rcv;
	bool exit_mode_req_send;
	bool pd_connected;
	bool src_vconn_swap_req;
	bool vconn_is_on;
	bool prot_timeout;
	bool tx_good_crc_received;
	bool custom_msg;
	bool is_event;
};

void UART_SYS_Init(void);
void UART1_Init(void);

/**************************TYPE C******************************/
void phy_exit(void *ptypec_dev);
void *phy_init(struct sii70xx_drv_context *);
/****************************SYSFS****************************/
#if defined(I2C_DBG_SYSFS)
void usbpd_event_notify(struct sii70xx_drv_context *pdev,
			unsigned int events, unsigned int event_param, void *data);
void sii_drv_sysfs_exit(struct sii70xx_drv_context *pdev);
bool sii_drv_sysfs_init(struct sii70xx_drv_context *pdev, struct device *dev);
#endif
/*************************Driver*******************************/
void usbpd_device_exit(struct device *dev);
void process_hard_reset(struct sii70xx_drv_context *drv_context);
int queue_usbpd_req(struct sii70xx_drv_context *drv_context);
void check_drp_status(struct sii70xx_drv_context *drv_context);
bool usbpd_set_ufp_init(struct sii_usbp_policy_engine *pdev);
bool usbpd_set_dfp_init(struct sii_usbp_policy_engine *pdev);
void sii70xx_drv_init(struct sii70xx_drv_context *drv_context, int);
void sii70xx_pd_reset_variables(struct sii_usbp_policy_engine *pd_dev);
void *usbpd_init(struct sii70xx_drv_context *drv_context,
		 bool (*event_notify_fn)(void *, uint32_t));
void usbpd_exit(void *context);
void sii70xx_pd_reset_variables(struct sii_usbp_policy_engine *pd_dev);
int usbpd_process_rx_data(struct sii70xx_drv_context *drv_context, uint8_t *buf, uint8_t count);
void wakeup_pd_queues(struct sii70xx_drv_context *drv_context);
void sii70xx_pd_sm_reset(struct sii_usbp_policy_engine *pUsbpd);
bool send_hardreset(struct sii_usbp_policy_engine *pdev);
bool set_pwr_params_default(struct sii_usbp_policy_engine *pUsbpd);
void set_pd_reset(struct sii70xx_drv_context *drv_context, bool is_set);
uint8_t sii_usbpd_get_src_cap(struct sii_usbp_policy_engine *pUsbpd,
			      uint32_t *src_pdo, enum pdo_type type_sup);
void send_request_msg(struct sii_usbp_policy_engine *pUsbpd);
uint8_t sii_usbpd_get_snk_cap(struct sii_usbp_policy_engine *pUsbpd,
			      uint32_t *snk_pdo, enum pdo_type type_sup);
void sii_update_inf_params(struct sii_usbpd_protocol *pusbpd_protlyr,
			   struct pd_cb_params *sm_inputs);
void wakeup_dfp_queue(struct sii70xx_drv_context *drv_context);
void wakeup_ufp_queue(struct sii70xx_drv_context *drv_context);
void sii70xx_vbus_enable(struct sii70xx_drv_context *drv_context, uint8_t is_src);
void sii_rcv_usbpd_data(struct sii70xx_drv_context *drv_context);
bool usbipd_send_soft_reset(struct sii70xx_drv_context *drv_context, enum ctrl_msg type);
void change_drp_pwr_role(struct sii_usbp_policy_engine *pUsbpd);
void set_70xx_mode(struct sii70xx_drv_context *drv_context, enum phy_drp_config drp_role);
void update_typec_status(struct sii_typec *ptypec_dev, bool status, bool is_dfp);
void set_cc_reset(struct sii70xx_drv_context *drv_context, bool is_set);
void set_pd_reset(struct sii70xx_drv_context *drv_context, bool is_set);
void usbpd_ufp_exit(struct sii_usbp_policy_engine *pUsbpd);
void usbpd_dfp_exit(struct sii_usbp_policy_engine *pUsbpd);
void sii_reset_dfp(struct sii_usbp_policy_engine *pUsbpd);
int sii_drv_set_alt_mode(struct sii70xx_drv_context *drv_context, uint8_t portnum, uint8_t);
int sii_drv_set_exit_mode(struct sii70xx_drv_context *drv_context, uint8_t portnum);

int sii_drv_set_pr_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_req_power_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_src_pwr_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
bool sii_src_alt_mode_engine(struct sii_usbp_policy_engine *pdev);
void sii_reset_ufp(struct sii_usbp_policy_engine *pUsbpd);
void si_enable_switch_control(struct sii70xx_drv_context *drv_context,
			      enum xceiv_config rx_tx, bool off_on);
int sii_usbpd_req_alt_mode(struct sii70xx_drv_context *drv_context, uint8_t portnum, uint8_t);
int sii_usbpd_src_alt_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii_usbpd_src_data_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii_usbpd_snk_data_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii_usbpd_snk_pwr_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii_usbpd_snk_alt_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii_usbpd_req_exit_mode(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_src_exit_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
void change_drp_data_role(struct sii_usbp_policy_engine *pUsbpd);
bool sii_update_pd_status(void *context, uint32_t events);
void sii_typec_events(struct sii_typec *ptypec_dev, uint32_t events);
void si_update_pd_status(struct sii_usbp_policy_engine *pUsbpd);
int sii_drv_set_dr_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_req_data_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
void send_device_softreset(struct sii70xx_drv_context *drv_context);

bool usbpd_set_dfp_swap_init(struct sii_usbp_policy_engine *pUsbpd);
bool usbpd_set_ufp_swap_init(struct sii_usbp_policy_engine *pUsbpd);
void source_policy_engine(WORK_STRUCT *w);
void sink_policy_engine(WORK_STRUCT *w);
int sii_drv_get_sr_cap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_req_src_cap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_give_src_cap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
void sii70xx_platform_reset(struct sii70xx_drv_context *drv_context);
bool sii_drv_set_custom_msg(struct sii70xx_drv_context *drv_context,
			    uint8_t bus_id, uint8_t data, bool enable);
bool usbpd_svdm_init_resp_nak(struct sii_usbpd_protocol *pd, uint8_t cmd,
			      uint16_t svid0, bool is_rcvd, uint32_t *vdo);
void si_custom_msg_xmit(struct sii_usbp_policy_engine *pdev);
int sii_usbpd_req_custom_data(struct sii70xx_drv_context *drv_context);
int sii_drv_set_vconn_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
bool sii_usbpd_req_vconn_swap(struct sii70xx_drv_context *drv_context, uint8_t portnum);
int sii_usbpd_req_src_vconn_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum);
int sii70xx_device_init(struct device *dev, struct gpio *sk_gpios);
void drp_phy_config(struct sii_typec *phy, enum phy_drp_config drp_role);
void sii_create_workqueue(FUNCPtr task,
			  WORK_STRUCT *out_ptr, uint8_t *thread_name, struct mutex *plock);
WORK_QUEUE_STRUCT *sii_create_single_thread_workqueue(uint8_t *thread_name,
						      FUNCPtr task, WORK_STRUCT *out_ptr,
						      struct mutex *plock);
void sii_wakeup_queues(WORK_QUEUE_STRUCT *workqueue, WORK_STRUCT *work, bool *event,
		       bool is_wakeup);

void typec_sm0_work(WORK_STRUCT *w);
TASK_RET_TYPE typec_detection(TASK_ARG_TYPE *w);
TASK_RET_TYPE adc_voltage_detection(TASK_ARG_TYPE *w);
TASK_RET_TYPE src_pe_sm_work(TASK_ARG_TYPE *w);
TASK_RET_TYPE sink_pe_sm_work(TASK_ARG_TYPE *w);
void sii_update_70xx_mode(struct sii70xx_drv_context *drv_context, enum phy_drp_config drp_mode);
bool usbpd_source_timer_delete(struct sii_usbp_policy_engine *pUsbpd);
bool usbpd_sink_timer_delete(struct sii_usbp_policy_engine *pUsbpd);
void sii_mask_detach_interrupts(struct sii70xx_drv_context *drv_context);
void sii_mask_attach_interrupts(struct sii70xx_drv_context *drv_context);
void sii_70xx_enable_interrupts(struct sii70xx_drv_context *drv_context, int drp_mode);
uint8_t sii_check_data_role_status(struct sii70xx_drv_context *drv_context);
uint8_t sii_check_power_role_status(struct sii70xx_drv_context *drv_context);
void sii_update_power_role(struct sii70xx_drv_context *drv_context, bool enable);
uint8_t sii_check_tx_busy(struct sii70xx_drv_context *drv_context);

void sii_update_data_role(struct sii70xx_drv_context *drv_context, bool enable);
uint8_t sii_check_cc_toggle_status(struct sii70xx_drv_context *drv_context);
void ufp_cc_adc_work(WORK_STRUCT *w);
void sii70xx_phy_sm_reset(struct sii_typec *phy);
void typec_sm_state_init(struct sii_typec *ptypec_dev, enum phy_drp_config drp_role);
#endif
