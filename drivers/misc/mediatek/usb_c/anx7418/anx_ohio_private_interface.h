#ifndef ANX_OHIO_PRIVATE_INTERFACE_H
#define ANX_OHIO_PRIVATE_INTERFACE_H
#include <linux/types.h>
#include "anx_ohio_public_interface.h"

#define YES     1
#define NO      0

#define PD_ONE_DATA_OBJECT_SIZE  4
#define PD_MAX_DATA_OBJECT_NUM  7
#define VDO_SIZE (PD_ONE_DATA_OBJECT_SIZE * PD_MAX_DATA_OBJECT_NUM)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP)

#define MV 1
#define MA 1
#define MW 1

/*5000mv voltage*/
#define PD_VOLTAGE_5V 5000

#define PD_MAX_VOLTAGE_20V 20000
/*0.9A current */
#define PD_CURRENT_900MA   900

#define PD_CURRENT_3A   3000

#define PD_POWER_15W  15000

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((u32)(n) & 0x7) << 28)
#define RDO_POS(rdo)               ((((32)rdo) >> 28) & 0x7)
#define RDO_GIVE_BACK              ((u32)1 << 27)
#define RDO_CAP_MISMATCH           ((u32)1 << 26)
#define RDO_COMM_CAP               ((u32)1 << 25)
#define RDO_NO_SUSPEND             ((u32)1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  (((((u32)ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) (((((u32)ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      (((((u32)mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     (((((u32)mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags) \
				(RDO_OBJ_POS(n) | (flags) | \
				RDO_FIXED_VAR_OP_CURR(op_ma) | \
				RDO_FIXED_VAR_MAX_CURR(max_ma))

#define EXTERNALLY_POWERED  YES

/* Source Capabilities*/
#define SOURCE_PROFILE_NUMBER   1 /*1 to 5*/

/*0 = Fixed, 1 = Battery, 2 = Variable*/
#define SRC_PDO_SUPPLY_TYPE1    0
/* 0 to 3 */
#define SRC_PDO_PEAK_CURRENT1   0
/* 5000mV (5V) */
#define SRC_PDO_VOLTAGE1        5000
/* 500mA (0.5A) */
#define SRC_PDO_MAX_CURRENT1    500

/**
 * @desc:   The interface AP will set(fill) the source capability to Ohio
 *
 * @param:
 *    src_caps: PDO buffer pointer of source capability whose bit formats
 *              follow the rules:
 *              it's little-endian, defined in USB PD spec 5.5 Transmitted
 *              Bit Ordering source capability's specific format defined in
 *              USB PD spec 6.4.1 Capabilities Message
 *              PDO refer to Table 6-4 Power Data Object
 *     Variable PDO : Table 6-8 Variable Supply (non-battery) PDO  --source
 *     Battery PDO : Table 6-9 Battery Supply PDO --source
 *     Fixed PDO refer to  Table 6-6 Fixed Supply PDO --source
 *     eg: default5Vsafe src_cap(5V, 0.9A fixed) -->
 *                                    PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *
 *
 *    src_caps_size: source capability's size, if the source capability obtains
 *              one PDO object, src_caps_size is 4, two PDO objects,
 *              src_caps_size is 8.
 *
 * @return:  1: success 0: fail
 *
 */
u8 send_src_cap(const u8 *src_caps, u8 src_caps_size);




/**
 * @desc:   The interface AP will set(fill) the sink capability to Ohio
 *
 * @param:
 *    snk_caps: PDO buffer pointer of sink capability whose bit formats
 *              follow the rules:
 *              it's little-endian, defined in USB PD spec 5.5 Transmitted
 *              Bit Ordering source capability's specific format defined in
 *              USB PD spec 6.4.1 Capabilities Message
 *              PDO refer to Table 6-4 Power Data Object
 *     Variable PDO : Table 6-8 Variable Supply (non-battery) PDO --source
 *     Battery PDO : Table 6-9 Battery Supply PDO --source
 *     Fixed PDO refer to  Table 6-6 Fixed Supply PDO --source
 *     eg: default5Vsafe snk_cap(5V, 0.9A fixed) -->
 *                                    PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *
 *    snk_caps_size: sink capability's size, if the sink capability obtains
 *              one PDO object, snk_caps_size is 4, two PDO objects,
 *              snk_caps_size is 8.
 *
 * @return:  1: success 0: fail
 *
 */
u8 send_snk_cap(const u8 *snk_caps, u8 snk_caps_size);
u8 send_rdo(const u8 *rdo, u8 size);
u8 send_data_swap(void);
u8 send_power_swap(void);
u8 send_accept(void);


/* 0, send interface msg timeout
  * 1 successful */
u8 interface_send_msg_timeout(u8 type, u8 *pbuf, u8 len, int timeout_ms);
u8 wait_pd_cmd_timeout(int pd_cmd_timeout);

u8 recv_pd_source_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_sink_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_dswap_default_callback(void *para, u8 para_len);
u8 recv_pd_pswap_default_callback(void *para, u8 para_len);

u8 recv_pd_cmd_rsp_default_callback(void *para, u8 para_len);

pd_callback_t get_pd_callback_fnc(PD_MSG_TYPE type);

void set_pd_callback_fnc(PD_MSG_TYPE type, pd_callback_t fnc);

u8 is_recvd_msg_ok(void);

#define IRQ_STATUS 0x53
#define IRQ_EXT_MASK_2 0x3d
#define IRQ_EXT_SOFT_RESET_BIT 0x04
#define IRQ_EXT_SOURCE_2 0x4F
/***************************************/
#define REG_ANALOG_STATUS 0x40

#define PIN_VCONN_2_IN_MSK (1<<0x6)
#define PIN_VCONN_1_IN_MSK (1<<0x5)
#define UFP_PLUG_MSK (1<<0x4) /*[4] 0:unplug 1:plug*/
#define DFP_OR_UFP_MSK (1<<0x3) /*[3] 0:DFP 1:UFP*/
/***************************************/
#define REG_ANALOG_CTRL_1 0x42
/***************************************/
#define REG_ANALOG_CTRL_2 0x43

/*[2] 1: CC1 connect to cap. 0: CC1 not connected to cap*/
#define R_CC_CAP_CC1 (1<<2)
/*[1] 1: CC2 connect to cap. 0: CC2 not connected to cap*/
#define R_CC_CAP_CC2 (1<<1)
/***************************************/
#define REG_ANALOG_CTRL_3 0x44
/***************************************/
#define REG_ANALOG_CTRL_4 0x45
/***************************************/
#define REG_ANALOG_CTRL_5 0x46
/***************************************/
#define REG_ANALOG_CTRL_6 0x47

/*[2] DRP support indicator. 1: supported, 0: unsupported.*/
#define DRP_EN (1<<2)
/*[1:0] 00: 36k, 01: 12k, 10: 4.7k*/
#define R_RP (3<<0)
/***************************************/
#define REG_ANALOG_CTRL_7 0x48

/*[3:2] 00: no connect 01: Rd connected 10: reserved 11: Ra connected*/
#define CC1_DETECT_RESULT (3<<2)
/*[1:0] 00: no connect 01: Rd connected 10: reserved 11: Ra connected*/
#define CC2_DETECT_RESULT (3<<0)

#define NO_DETECT 0x0
#define RD_CONNECTED 0x1
#define RA_CONNECTED 0x3
/***************************************/
#define REG_HPD_CTRL_0 0x36

/*[5] HPD output enable. 1: HPD input enable, 0: HPD output enable.*/
#define R_HPD_OEN (1<<5)

/*[4] HPD output data.*/
#define R_HPD_OUT_DATA (1<<4)
/***************************************/

/* check soft interrupt happens or not */
#define is_soft_reset_intr() \
	(OhioReadReg(IRQ_EXT_SOURCE_2) & IRQ_EXT_SOFT_RESET_BIT)

/* clear the Ohio's soft  interrupt bit */
#define clear_soft_interrupt() \
	OhioWriteReg(IRQ_EXT_SOURCE_2, IRQ_EXT_SOFT_RESET_BIT)

#define MSEC_TO_JIFFIES(msec) ((msec) * HZ / 1000)

#define RESPONSE_REQ_TYPE() InterfaceRecvBuf[2]
#define RESPONSE_REQ_RESULT() InterfaceRecvBuf[3]

unsigned char OhioReadReg(unsigned char RegAddr);
void OhioWriteReg(unsigned char RegAddr, unsigned char RegVal);

/*Interface header*/
struct tagInterfaceHeader {
	unsigned char Indicator :1; /*indicator*/
	unsigned char Type      :3; /*data type*/
	unsigned char Length    :4; /*data length*/
};

struct tagInterfaceData {
	unsigned long SrcPDO[7];
	unsigned long SnkPDO[7];
	unsigned long RDO;
	unsigned long VDMHeader;
	unsigned long IDHeader;
	unsigned long CertStatVDO;
	unsigned long ProductVDO;
	unsigned long CableVDO;
	unsigned long AMM_VDO;
};

/*Comands status*/
enum interface_status {
	CMD_SUCCESS,
	CMD_REJECT,
	CMD_FAIL,
	CMD_BUSY,
	CMD_STATUS
};

#define MAX_INTERFACE_COUNT 32
#define MAX_INTERFACE_MSG_LEN  32

typedef struct interface_msg {
	u8 data[MAX_INTERFACE_MSG_LEN];
} interface_msg_t;

interface_msg_t *imsg_fetch(void);

#define INTERFACE_TIMEOUT 400

extern u8 pd_src_pdo_cnt;
extern u8 pd_src_pdo[];
extern u8 sink_svid_vdo[];
extern u8 pd_snk_pdo_cnt;
extern u8 pd_snk_pdo[];
extern u8 pd_rdo[];
extern u8 DP_caps[];
extern u8 configure_DP_caps[];
extern u8 src_dp_caps[];

#if 1
/*control cmd*/
#define interface_pr_swap() \
	interface_send_msg_timeout(TYPE_PSWAP_REQ, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_dr_swap() \
	interface_send_msg_timeout(TYPE_DSWAP_REQ, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_vconn_swap() \
	interface_send_msg_timeout(TYPE_VCONN_SWAP_REQ, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_get_dp_caps() \
	interface_send_msg_timeout(TYPE_GET_DP_SNK_CAP, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_gotomin() \
	interface_send_msg_timeout(TYPE_GOTO_MIN_REQ, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_soft_rst() \
	interface_send_msg_timeout(TYPE_SOFT_RST, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_hard_rst() \
	interface_send_msg_timeout(TYPE_HARD_RST, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_restart() \
	interface_send_msg_timeout(TYPE_RESTART, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_accept() \
	interface_send_msg_timeout(TYPE_ACCEPT, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_reject() \
	interface_send_msg_timeout(TYPE_REJECT, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_get_pd_status()\
	interface_send_msg_timeout(TYPE_PD_STATUS_REQ, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_dp_enter()\
	interface_send_msg_timeout(TYPE_DP_ALT_ENTER, \
					0, 0, INTERFACE_TIMEOUT)

#define interface_send_dp_exit()\
	interface_send_msg_timeout(TYPE_DP_ALT_EXIT, \
					0, 0, INTERFACE_TIMEOUT)


#define interface_send_src_cap() \
	interface_send_msg_timeout(TYPE_PWR_SRC_CAP, \
			pd_src_pdo, \
			pd_src_pdo_cnt * 4, \
			INTERFACE_TIMEOUT)

#define interface_send_snk_cap() \
	interface_send_msg_timeout(TYPE_PWR_SNK_CAP, \
			pd_snk_pdo, \
			pd_snk_pdo_cnt * 4, \
			INTERFACE_TIMEOUT)

#define interface_send_src_dp_cap() \
	interface_send_msg_timeout(TYPE_DP_SNK_IDENDTITY, \
			src_dp_caps, \
			4, \
			INTERFACE_TIMEOUT)

#define interface_config_dp_caps() \
	interface_send_msg_timeout(TYPE_DP_SNK_CFG, \
			configure_DP_caps, \
			4, \
			INTERFACE_TIMEOUT)

#define interface_send_request() \
	interface_send_msg_timeout(TYPE_PWR_OBJ_REQ, \
			pd_rdo, \
			4, \
			INTERFACE_TIMEOUT)

#define interface_send_vdm() \
	interface_send_msg_timeout(TYPE_VDM, \
			vdm_data_buf + 2, \
			pd_port_vdo_count * 4, \
			INTERFACE_TIMEOUT)

#define interface_send_svid() \
	interface_send_msg_timeout(TYPE_SVID, \
			sink_svid_vdo, \
			4, \
			INTERFACE_TIMEOUT)
#endif

void send_initialized_setting(void);
void interface_init(void);
void ap_send_msg(unsigned char  *pSendBuf, unsigned char len);
u8 polling_interface_msg(int timeout_ms);

void interface_send_dp_caps(void);
void interface_send_status(u8 cmd_type, u8 status);
u8 send_dp_snk_cfg(const u8 *dp_snk_caps, u8 dp_snk_caps_size);
u8 send_dp_snk_identity(void);
u8 send_vdm(const u8 *vdm, u8 size);

u8 recv_pd_pwr_object_req_default_callback(void *para, u8 para_len);
u8 recv_pd_dswap_default_callback(void *para, u8 para_len);
u8 recv_pd_pswap_default_callback(void *para, u8 para_len);
u8 recv_pd_sink_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_source_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_cmd_rsp_default_callback(void *para, u8 para_len);
u8 recv_pd_goto_min_default_callback(void *para, u8 para_len);
u8 recv_pd_accept_default_callback(void *para, u8 para_len);
u8 recv_pd_reject_default_callback(void *para, u8 para_len);

char *interface_to_str(unsigned char header_type);
void update_pwr_src_caps(void);

#endif
