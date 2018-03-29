
#ifndef ANX_OHIO_PUBLIC_INTERFACE_H
#define ANX_OHIO_PUBLIC_INTERFACE_H
#include <linux/types.h>


typedef enum {
	TYPE_PWR_SRC_CAP = 0x00,
	TYPE_PWR_SNK_CAP = 0x01,
	TYPE_DP_SNK_IDENDTITY = 0x02,
	TYPE_SVID = 0x03,
	TYPE_GET_DP_SNK_CAP = 0x04,
	TYPE_ACCEPT = 0x05,
	TYPE_REJECT  = 0x06,
	TYPE_PSWAP_REQ = 0x10,
	TYPE_DSWAP_REQ = 0x11,
	TYPE_GOTO_MIN_REQ =  0x12,
	TYPE_VCONN_SWAP_REQ = 0x13,
	TYPE_VDM = 0x14,
	TYPE_DP_SNK_CFG = 0x15,
	TYPE_PWR_OBJ_REQ = 0x16,
	TYPE_PD_STATUS_REQ = 0x17,
	TYPE_DP_ALT_ENTER = 0x19,
	TYPE_DP_ALT_EXIT = 0x1A,
	TYPE_RESPONSE_TO_REQ = 0xF0,
	TYPE_SOFT_RST = 0xF1,
	TYPE_HARD_RST = 0xF2,
	TYPE_RESTART = 0xF3
} PD_MSG_TYPE;

/* PDO : Power Data Object */
/*
* 1. The vSafe5V Fixed Supply Object shall always be the first object.
* 2. The remaining Fixed Supply Objects,
* if present, shall be sent in voltage order; lowest to highest.
* 3. The Battery Supply Objects,
* if present shall be sent in Minimum Voltage order; lowest to highest.
* 4. The Variable Supply (non battery) Objects,
* if present, shall be sent in Minimum Voltage order; lowest to highest.
*/
#define PDO_TYPE_FIXED ((u32)0 << 30)
#define PDO_TYPE_BATTERY ((u32)1 << 30)
#define PDO_TYPE_VARIABLE ((u32)2 << 30)
#define PDO_TYPE_MASK ((u32)3 << 30)

/* Dual role device */
#define PDO_FIXED_DUAL_ROLE ((u32)1 << 29)

/* USB Suspend supported */
#define PDO_FIXED_SUSPEND ((u32)1 << 28)

/* Externally powered */
#define PDO_FIXED_EXTERNAL ((u32)1 << 27)

/* USB Communications Capable */
#define PDO_FIXED_COMM_CAP ((u32)1 << 26)

/* Data role swap command supported */
#define PDO_FIXED_DATA_SWAP ((u32)1 << 25)

/* [21..20] Peak current */
#define PDO_FIXED_PEAK_CURR ((u32)1 << 20)

/* Voltage in 50mV units */
#define PDO_FIXED_VOLT(mv) (u32)((((u32)mv)/50) << 10)

/* Max current in 10mA units */
#define PDO_FIXED_CURR(ma) (u32)((((u32)ma)/10))

/*build a fixed PDO packet*/
#define PDO_FIXED(mv, ma, flags) \
		(PDO_FIXED_VOLT(mv) |\
		PDO_FIXED_CURR(ma) | (flags))

/*Pos in Data Object, the first index number begin from 0 */
#define PDO_INDEX(n, dat) (dat << (n * PD_ONE_DATA_OBJECT_SIZE*sizeof(u8)))

#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma) ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR(min_mv, max_mv, op_ma) \
		(PDO_VAR_MIN_VOLT(min_mv) | \
		PDO_VAR_MAX_VOLT(max_mv) | \
		PDO_VAR_OP_CURR(op_ma) | \
		PDO_TYPE_VARIABLE)

#define PDO_BATT_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_BATT_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_BATT_OP_POWER(mw) ((((mw) / 250) & 0x3FF) << 0)

#define PDO_BATT(min_mv, max_mv, op_mw) \
		(PDO_BATT_MIN_VOLT(min_mv) | \
		PDO_BATT_MAX_VOLT(max_mv) | \
		PDO_BATT_OP_POWER(op_mw) | \
		PDO_TYPE_BATTERY)


#define GET_PDO_TYPE(PDO) ((PDO & PDO_TYPE_MASK) >> 30)
#define GET_PDO_FIXED_DUAL_ROLE(PDO) ((PDO & PDO_FIXED_DUAL_ROLE) >> 29)
#define GET_PDO_FIXED_SUSPEND(PDO) ((PDO & PDO_FIXED_SUSPEND) >> 28)
#define GET_PDO_FIXED_EXTERNAL(PDO) ((PDO & PDO_FIXED_EXTERNAL) >> 27)
#define GET_PDO_FIXED_COMM_CAP(PDO) ((PDO & PDO_FIXED_COMM_CAP) >> 26)
#define GET_PDO_FIXED_DATA_SWAP(PDO) ((PDO & PDO_FIXED_DATA_SWAP) >> 25)
#define GET_PDO_FIXED_PEAK_CURR(PDO) ((PDO >> 20) & 0x03)

#define GET_PDO_FIXED_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_PDO_FIXED_CURR(PDO) ((PDO & 0x3FF) * 10)

#define GET_VAR_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_VAR_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_VAR_MAX_CURR(PDO) ((PDO & 0x3FF) * 10)

#define GET_BATT_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_BATT_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_BATT_OP_POWER(PDO) (((PDO) & 0x3FF) * 250)



/**
 * @desc: The Interface that AP sends the specific USB PD command to Ohio
 *
 * @param:
 *  type: PD message type, define enum PD_MSG_TYPE.
 *  buf: the sepecific parameter pointer according to the message type:
 *       eg: when AP update its source capability type=TYPE_PWR_SRC_CAP,
 *       "buf" contains the content of PDO object,its format USB PD spec
 *       customer can easily packeted it through PDO_FIXED_XXX macro:
 *       default5Vsafe 5V, 0.9A fixed --> PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *  size: the parameter ponter's content length, if buf is null, it should be 0
 *
 * @return:   0: success Error value 1: reject 2: fail, 3, busy
 *
 */
u8 send_pd_msg(PD_MSG_TYPE type, const char *buf, u8 size);

/**
 * @desc: The Interface that AP handle the specific USB PD command from Ohio
 *        it's callbacked by ohio's private interface module, when one msg
 *        arrived  what customer can do is to register your callback
 *        function under its framework.
 * @param:
 *  type: PD message type, define enum PD_MSG_TYPE.
 *  buf: the sepecific parameter pointer according to the message type:
 *       eg: when AP update its source capability type=TYPE_PWR_SRC_CAP,
 *       "buf" contains the content of PDO object,its format USB PD spec
 *       customer can easily packeted it through PDO_FIXED_XXX macro:
 *       default5Vsafe 5V, 0.9A fixed --> PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *  size: the parameter ponter's content length, if buf is null, it should be 0
 *
 * @return:  1: success 0: fail
 *
 */
u8 dispatch_rcvd_pd_msg(PD_MSG_TYPE type, void *buf, u8 size);

typedef u8 (*pd_callback_t)(void *, u8);

u8 register_pd_msg_callback_func(PD_MSG_TYPE type, pd_callback_t fnc);

#endif
