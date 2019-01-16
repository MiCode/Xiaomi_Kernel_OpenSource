
#ifndef DEV_USB_DRV_H
#define DEV_USB_DRV_H

#include <linux/mu3d/hal/mu3d_hal_hw.h>
#include <linux/mu3d/hal/mu3d_hal_usb_drv.h>
#include <linux/mu3phy/mtk-phy.h>



#undef EXTERN

#ifdef _DEV_USB_DRV_EXT_
#define EXTERN
#else
#define EXTERN extern
#endif




 /*
 * USB recipients
 */

#define USB_RECIP_MASK			0x03
#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_INTERFACE    	0x01
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_OTHER			0x03


struct USB_TRANSFER {
    DEV_UINT8    type;
    DEV_UINT8    speed;
    DEV_UINT32   length;
    DEV_UINT16   maxp;
    DEV_UINT8    state;
    DEV_UINT8    status;
};


#define ENDPOINT_HALT  0x00

#define AT_CMD_ACK_DATA_LENGTH 8
#define AT_CMD_SET_BUFFER_OFFSET 6
#define AT_PW_STS_CHK_DATA_LENGTH 8
#define USB_STATUS_SIZE		2

typedef enum {
	READY = 0,
	BUSY,
	ERROR
} Req_Status;

typedef struct USB_AT_REQ {
    DEV_UINT32 bmRequestType;
    DEV_UINT32 bRequest;
    DEV_UINT32 wValue;
    DEV_UINT32 wIndex;
    DEV_UINT32 wLength;
    DEV_UINT8 bValid;
    DEV_UINT32 bCommand;
    void  *buffer;

} DEV_REQ;

typedef struct USB_AT_DATA {
    DEV_UINT16 header;
    DEV_UINT16 length;
    DEV_UINT16 tsfun;
    void  *buffer;
} DEV_AT_CMD;

typedef struct USB_EP_DATA {
    DEV_UINT8  ep_num;
    DEV_UINT8  dir;
    DEV_UINT8  type;
    DEV_UINT8  interval;
    DEV_UINT16 ep_size;
    DEV_UINT8  slot;
    DEV_UINT8  burst;
    DEV_UINT8  mult;
} EP_INFO;

typedef struct USB_LB_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  bdp;
    DEV_UINT8  dram_offset;
    DEV_UINT8  extension;
    DEV_UINT8  dma_burst;
    DEV_UINT8  dma_limiter;

} LOOPBACK_INFO;

typedef struct USB_RS_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  dir_1;
    DEV_UINT8  dir_2;
    DEV_UINT32 stop_count_1;
    DEV_UINT32 stop_count_2;

} RANDOM_STOP_INFO;

typedef struct USB_SQ_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  tx_method;

} STOP_QMU_INFO;

typedef struct USB_PW_DATA {
    DEV_UINT32 mode;
	DEV_UINT8  u1_value;
	DEV_UINT8  u2_value;
    DEV_UINT8  en_u1;
    DEV_UINT8  en_u2;

} POWER_INFO;

typedef struct USB_U1U2_DATA {
	DEV_UINT8  type;
	DEV_UINT8  u_num;
	DEV_UINT8  opt;
	DEV_UINT8  cond;
	DEV_UINT8  u1_value;
	DEV_UINT8  u2_value;

} U1U2_INFO;

typedef struct USB_LPM_DATA {
	DEV_UINT8  lpm_mode;
	DEV_UINT8  wakeup;
	DEV_UINT8  beslck;
	DEV_UINT8  beslck_u3;
	DEV_UINT8  besldck;
	DEV_UINT8  cond;
	DEV_UINT8  cond_en;
} LPM_INFO;

typedef struct USB_STALL_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  bdp;

} STALL_INFO;

typedef struct USB_SINGLE_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  dir;
    DEV_UINT8  num;
    DEV_UINT8  dual;

} SINGLE_INFO;

typedef struct USB_ST_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  bdp;
    DEV_UINT8  num;

}STRESS_INFO;

typedef struct USB_CZ_DATA {
    DEV_UINT32 transfer_length;
    DEV_UINT32 gpd_buf_size;
    DEV_UINT16 bd_buf_size;
    DEV_UINT8  zlp_en;
    DEV_UINT8  coz_en;

}RX_ZLP_INFO;

typedef struct USB_NOTIF_DATA {
    DEV_UINT32 valuel;
    DEV_UINT32 valueh;
    DEV_UINT8  type;

}DEV_NOTIF_INFO;

typedef struct USB_RESET_DATA {
    DEV_UINT8 speed;
}REST_INFO;

typedef struct USB_REMOTE_WAKE_DATA {
	DEV_UINT16 delay;
}REMOTE_WAKE_INFO;

typedef struct USB_CTRL_MODE_DATA {
	DEV_UINT8 mode;
}CTRL_MODE_INFO;

typedef struct USB_OTG_MODE_DATA {
	DEV_UINT8 mode;
}OTG_MODE_INFO;

typedef enum {
	RESET_STATE,
	CONFIG_EP_STATE,
	LOOPBACK_STATE,
	LOOPBACK_EXT_STATE,
	REMOTE_WAKEUP,
	STRESS,
	EP_RESET_STATE,
	WARM_RESET,
	STALL,
	RANDOM_STOP_STATE,
	RX_ZLP_STATE,
	DEV_NOTIFICATION_STATE,
	STOP_QMU_STATE,
	SINGLE,
	POWER_STATE,
	U1U2_STATE,
	LPM_STATE,
	STOP_DEV_STATE,
	CTRL_MODE_STATE,
	OTG_MODE_STATE
} USB3_TEST_CASE;

typedef enum {
	AT_CMD_SET,
	AT_CMD_ACK,
	AT_CTRL_TEST,
	AT_PW_STS_CHK
} USB_AT_CMD;

typedef enum {
	RESERVED=0,
	FUNCTION_WAKE,
	LATENCY_TOLERANCE_MESSAGE,
	BUS_INTERVAL_ADJUSTMENT_MESSAGE,
	HOST_ROLE_REQUEST
} DEV_NOTIFICATION;

struct USB_TEST_STATUS{
    USB_SPEED speed;
    DEV_INT32 reset_received;
	DEV_INT32 suspend;
	DEV_INT32 enterU0;
	DEV_INT32 vbus_valid;
	DEV_INT32 addressed;
} ;

EXTERN  struct USB_TEST_STATUS g_usb_status;
EXTERN DEV_UINT8* g_loopback_buffer[2 * MAX_EP_NUM + 1];
EXTERN DEV_UINT8 g_device_halt;
EXTERN DEV_UINT8 g_sw_rw;
EXTERN DEV_UINT8 g_hw_rw;
EXTERN DEV_UINT8 g_usb_irq;
EXTERN DEV_UINT8 g_u3d_status;
EXTERN DEV_UINT8 g_ep0_mode;
EXTERN DEV_UINT8 g_run;
EXTERN DEV_UINT16 g_hot_rst_cnt;
EXTERN DEV_UINT16 g_warm_rst_cnt;
EXTERN DEV_UINT16 g_rx_len_err_cnt;
EXTERN DEV_REQ *Request;
EXTERN DEV_AT_CMD *AT_CMD;
EXTERN DEV_UINT32 TransferLength;
EXTERN DEV_UINT8 bDramOffset;
EXTERN DEV_UINT8 bdma_burst;
EXTERN DEV_UINT8 bdma_limiter;

EXTERN volatile DEV_UINT32 g_usb_phy_clk_on;
#ifdef SUPPORT_OTG
EXTERN volatile DEV_UINT32 g_otg_exec;
EXTERN volatile DEV_UINT32 g_otg_td_5_9;

EXTERN volatile DEV_UINT32 g_otg_config;
EXTERN volatile DEV_UINT32 g_otg_srp_reqd;
EXTERN volatile DEV_UINT32 g_otg_hnp_reqd;
EXTERN volatile DEV_UINT32 g_otg_b_hnp_enable;

EXTERN volatile DEV_UINT32 g_otg_vbus_chg;
EXTERN volatile DEV_UINT32 g_otg_reset;
EXTERN volatile DEV_UINT32 g_otg_suspend;
EXTERN volatile DEV_UINT32 g_otg_resume;
EXTERN volatile DEV_UINT32 g_otg_connect;
EXTERN volatile DEV_UINT32 g_otg_disconnect;
EXTERN volatile DEV_UINT32 g_otg_chg_a_role_b;
EXTERN volatile DEV_UINT32 g_otg_chg_b_role_b;
EXTERN volatile DEV_UINT32 g_otg_attach_b_role;

EXTERN spinlock_t g_otg_lock;
#endif

EXTERN void u3d_sync_with_bat(int usb_state);

EXTERN void u3d_init_ctrl(void);
EXTERN void u3d_irq_en(void);
EXTERN void u3d_ep0_handler(void);
EXTERN void u3d_epx_handler(DEV_INT32 ep_num, USB_DIR dir);
EXTERN void u3d_dma_handler(DEV_INT32 chan_num);
EXTERN DEV_UINT8 req_complete(DEV_INT32 ep_num, USB_DIR dir);
EXTERN DEV_UINT8 u3d_command(void);
EXTERN void *u3d_req_buffer(void);
EXTERN DEV_UINT8 u3d_req_valid(void);
EXTERN void u3d_rst_request(void);
EXTERN void u3d_alloc_req(void);
EXTERN void u3d_init(void);
// USBIF
EXTERN void u3d_init_mem(void);
EXTERN void u3d_deinit(void);

EXTERN void u3d_allocate_ep0_buffer(void);
EXTERN void u3d_dev_loopback(DEV_INT32 ep_rx,DEV_INT32 ep_tx);
EXTERN DEV_UINT8 u3d_device_halt(void);
EXTERN DEV_UINT8 u3d_transfer_complete(DEV_INT32 ep_num, USB_DIR dir);
EXTERN DEV_INT32 u3d_dev_suspend(void);
EXTERN void u3d_ep0en(void);
EXTERN void u3d_initialize_drv(void);
EXTERN void u3d_set_address(DEV_INT32 addr);
EXTERN void u3d_ep_start_transfer(DEV_INT32 ep_num, USB_DIR dir);
EXTERN void u3d_rxep_dis(DEV_INT32 ep_num);
EXTERN void dev_power_mode(DEV_INT32 mode, DEV_INT8 u1_value, DEV_INT8 u2_value, DEV_INT8 en_u1, DEV_INT8 en_u2);
EXTERN void dev_send_one_packet(DEV_INT32 ep_tx);
EXTERN void dev_send_erdy(DEV_INT8 opt,DEV_INT32 ep_rx , DEV_INT32 ep_tx);
EXTERN void dev_receive_ep0_test_packet(DEV_INT8 opt);
EXTERN void dev_u1u2_en_cond(DEV_INT8 opt,DEV_INT8 cond,DEV_INT32 ep_rx , DEV_INT32 ep_tx);
EXTERN void dev_u1u2_en_ctrl(DEV_INT8 type,DEV_INT8 u_num,DEV_INT8 opt,DEV_INT8 cond,DEV_INT8 u1_value, DEV_INT8 u2_value);
EXTERN DEV_INT8 dev_stschk(DEV_INT8 type, DEV_INT8 change);
EXTERN void reset_dev(USB_SPEED speed, DEV_UINT8 det_speed, DEV_UINT8 sw_rst);
EXTERN void dev_lpm_config_dev(LPM_INFO *lpm_info);

#undef EXTERN


#endif //USB_DRV_H

