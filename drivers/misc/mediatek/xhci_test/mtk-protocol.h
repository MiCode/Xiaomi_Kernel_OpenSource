#include "mtk-usb-hcd.h"

#define POLLING_COUNT 100;
#define POLLING_DELAY_MSECS 20;
#define POLLING_STOP_DELAY_MSECS 200;

#define RESET_STATE_DATA_LENGTH 7
//#define DATAIN_STATE_DATA_LENGTH 15
//#define DATAOUT_STATE_DATA_LENGTH 15
#define CONFIGEP_STATE_DATA_LENGTH 15
#define LOOPBACK_STATE_DATA_LENGTH 21
#define LOOPBACK_EXT_STATE_DATA_LENGTH 19
//#define UNITGPD_STATE_DATA_LENGTH 15
//#define LOOPBACK_2T2R_STATE_DATA_LENGTH 15
#define Loopback_STRESS_DATA_LENGTH 3
#define STOP_QMU_DATA_LENGTH 8
#define STRESS_DATA_LENGTH 18
#define STALL_DATA_LENGTH 17
#define WARMRESET_DATA_LENGTH 17
#define EP_RESET_DATA_LENGTH 6
#define RANDOM_STOP_STATE_DATA_LENGTH 26
#define RX_ZLP_STATE_DATA_LENGTH 18
#define DEV_NOTIFICATION_DATA_LENGTH 15
#define STOP_QMU_STATE_DATA_LENGTH 17
#define SINGLE_DATA_LENGTH 18
#define POWER_STATE_DATA_LENGTH 14
#define U1U2_STATE_DATA_LENGTH 12
#define LPM_STATE_DATA_LENGTH 13
#define AT_CMD_ACK_DATA_LENGTH 8
#define REMOTE_WAKEUP_DATA_LENGTH 8

#define DUMMY_DATA_LENGTH 256

#define STALL_COUNT 3
#define GET_STATUS  0x00
#define CLEAR_FEATURE 0x01
#define SET_FEATURE  0x03
#define ENDPOINT_HALT  0x00
#define EP0_IN_STALL  0xFD
#define EP0_OUT_STALL  0xFE
#define EP0_STALL  0xFF

#if 0
#define MULTIGPD_STATE_DATA_LENGTH 15
#define MULTIGPD_IN_STATE_DATA_LENGTH 15
#define MULTIGPD_OUT_STATE_DATA_LENGTH 15
#define MULTIGPD_IN_BPS_STATE_DATA_LENGTH 15
#define MULTIGPD_OUT_BPS_STATE_DATA_LENGTH 15
#define DUMMY_DATA_LENGTH 256
#endif

typedef enum {
    DEV_SPEED_INACTIVE = 0,
    DEV_SPEED_FULL = 1,
    DEV_SPEED_HIGH = 3,
    DEV_SPEED_SUPER = 4,
} USB_DEV_SPEED;

typedef enum 
{
	RESET_STATE,
	CONFIGEP_STATE,
	LOOPBACK_STATE,
	LOOPBACK_EXT_STATE,
	REMOTE_WAKEUP,
	STRESS,
	EP_RESET,
	WARM_RESET,
	STALL,
	RANDOM_STOP_STATE,
	RX_ZLP_STATE,
	DEV_NOTIFICATION_STATE,
	STOP_QMU_STATE,
	SINGLE,
	POWER_STATE,
	U1U2_STATE,
	LPM_STATE
} USB_U3_TEST_CASE;

typedef enum 
{
	AT_CMD_SET,
	AT_CMD_ACK,
	AT_CTRL_TEST
} USB_AT_CMD;



typedef enum
{
	STATUS_READY=0,
	STATUS_BUSY,
	STATUS_FAIL
} USB_U3_QUERY_STATUS;

struct protocol_query {
	__u16 header;
	__u16  length;
	__u16  status;
	__u16  result;
} __attribute__ ((packed));



int dev_reset(USB_DEV_SPEED speed, struct usb_device *dev);
int dev_config_ep(char ep_num,char dir,char type,short int maxp,char bInterval,char slot, char burst, char mult, struct usb_device *dev);

int dev_config_ep0(short int maxp, struct usb_device *dev);
int dev_query_status(struct usb_device *dev);
int dev_polling_status(struct usb_device *dev);
int dev_polling_stop_status(struct usb_device *dev);
int dev_loopback(char bdp,int length,int gpd_buf_size,int bd_buf_size, char dram_offset, char extension, struct usb_device *dev);
int dev_ctrl_loopback(int length, struct usb_device *dev);
//int dev_remotewakeup(char bdp,int length,int gpd_buf_size,int bd_buf_size, struct usb_device *dev);
int dev_remotewakeup(int delay);
int dev_stress(char bdp,int length,int gpd_buf_size,int bd_buf_size,char num, struct usb_device *usbdev);
int dev_random_stop(int length,int gpd_buf_size,int bd_buf_size,char dev_dir_1,char dev_dir_2,int stop_count_1,int stop_count_2);
int dev_power(int test_mode, char u1_value, char u2_value,char en_u1, char en_u2, struct usb_device *usbdev);
int dev_notifiaction(int type,int valuel,int valueh);
int dev_lpm_config_host(char lpm_mode, char wakeup, char beslck, char beslck_u3, char beslckd, char cond, char cond_en);

