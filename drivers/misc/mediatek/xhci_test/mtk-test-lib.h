#ifndef __MTK_TEST_LIB_H
#define __MTK_TEST_LIB_H
#include "mtk-usb-hcd.h"
#include <linux/mu3phy/mtk-phy.h>

#define KERNEL_30	1	//if port to Linux 3.0, enable this
#define TEST_OTG	1

extern void __iomem *xhci_usbif_base;
extern void __iomem *xhci_usbif_sif_base;
extern int xhci_usbif_nirq; 
#define USB_XHCI_COMPATIBLE_NAME "mediatek,USB3_XHCI"

#define SSUSB_U3_BASE	xhci_usbif_base	
#define SSUSB_U3_XHCI_BASE	SSUSB_U3_BASE
#define SSUSB_U3_MAC_BASE	(SSUSB_U3_BASE + 0x2400) /* U3 MAC */
#define SSUSB_U3_SYS_BASE	(SSUSB_U3_BASE + 0x2600) /* U3 sys */
#define SSUSB_U2_SYS_BASE	(SSUSB_U3_BASE + 0x3400) /* U2 MAC + SYS */
                                /* ref doc ssusb_xHCI_exclude_port_csr.xlsx */
#define SSUSB_XHCI_EXCLUDE_BASE (SSUSB_U3_BASE + 0x900)

#define SIFSLV_IPPC 		(xhci_usbif_sif_base + 0x700)

#define U3_PIPE_LATCH_SEL_ADD 		SSUSB_U3_MAC_BASE + 0x130
#define U3_PIPE_LATCH_TX	0
#define U3_PIPE_LATCH_RX	0

#define U3_UX_EXIT_LFPS_TIMING_PAR	0xa0
#define U3_REF_CK_PAR	0xb0
#define U3_RX_UX_EXIT_LFPS_REF_OFFSET	8

#define U3_RX_UX_EXIT_LFPS_REF	3	// 10MHz:3, 20MHz: 6, 25MHz: 8, 27MHz: 9, 50MHz: 15, 100MHz: 30
#define	U3_REF_CK_VAL	10			//MHz = value

#define U3_TIMING_PULSE_CTRL	0xb4
#define CNT_1US_VALUE			63	//62.5MHz:63, 70MHz:70, 80MHz:80, 100MHz:100, 125MHz:125

#define USB20_TIMING_PARAMETER	0x40
#define TIME_VALUE_1US			63	//62.5MHz:63, 80MHz:80, 100MHz:100, 125MHz:125
#define USB20_LPM_ENTRY_COUNT	(SSUSB_U2_SYS_BASE + 0x48)

#define LINK_PM_TIMER	0x8
#define PM_LC_TIMEOUT_VALUE	3

                                /* ref doc ssusb_xHCI_exclude_port_csr.xlsx */
#define SSUSB_XHCI_HDMA_CFG     (SSUSB_XHCI_EXCLUDE_BASE + 0x50)
#define SSUSB_XHCI_U2PORT_CFG   (SSUSB_XHCI_EXCLUDE_BASE + 0x78)
#define SSUSB_XHCI_HSCH_CFG2    (SSUSB_XHCI_EXCLUDE_BASE + 0x7c)

#define SSUSB_XHCI_ACTIVE_MASK  0x1f1f

/******** Defined for PHY calibration ************/

#define U3_CNR						(1<<11)
#define U3_PLS_OFFSET				5
#define U3_P1_SC					(SSUSB_U3_XHCI_BASE+0x420)	//0xf0040420
#define U3_P1_PMSC					(SSUSB_U3_XHCI_BASE+0x424)	//0xf0040424
#define U3_P1_LTSSM					(SSUSB_U3_MAC_BASE+0x134)	//0xf0042534
#define U3_LINK_ERR_COUNT			(SSUSB_U3_SYS_BASE+0x14)	//0xf0042614
#define U3_RECOVERY_COUNT			(SSUSB_U3_SYS_BASE+0xd8)	//0xf00426d8
#define U3_XHCI_CMD_ADDR			(SSUSB_U3_XHCI_BASE+0x20)
#define U3_XHCI_STS_ADDR			(SSUSB_U3_XHCI_BASE+0x24)

#define SSUSB_IP_PW_CTRL	(SIFSLV_IPPC+0x0)
#define SSUSB_IP_SW_RST		(1<<0)
#define SSUSB_IP_PW_CTRL_1	(SIFSLV_IPPC+0x4)
#define SSUSB_IP_PDN		(1<<0)
#define SSUSB_U3_CTRL(p)	(SIFSLV_IPPC+0x30+(p*0x08))
#define SSUSB_U3_PORT_DIS	(1<<0)
#define SSUSB_U3_PORT_PDN	(1<<1)
#define SSUSB_U3_PORT_HOST_SEL	(1<<2)
#define SSUSB_U3_PORT_CKBG_EN	(1<<3)
#define SSUSB_U3_PORT_MAC_RST	(1<<4)
#define SSUSB_U3_PORT_PHYD_RST	(1<<5)
#define SSUSB_U2_CTRL(p)	(SIFSLV_IPPC+(0x50)+(p*0x08))
#define SSUSB_U2_PORT_DIS	(1<<0)
#define SSUSB_U2_PORT_PDN	(1<<1)
#define SSUSB_U2_PORT_HOST_SEL	(1<<2)
#define SSUSB_U2_PORT_CKBG_EN	(1<<3)
#define SSUSB_U2_PORT_MAC_RST	(1<<4)
#define SSUSB_U2_PORT_PHYD_RST	(1<<5)
#define SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL	(1<<9)
#define SSUSB_IP_CAP			(SIFSLV_IPPC+0x024)


#define SSUSB_XHCI_RST_CTRL	(SIFSLV_IPPC+0x0094)
#define SSUSB_XHCI_SW_RST	(0x1<<0) //0:0


#define SSUSB_U3_PORT_NUM(p)	(p & 0xff)
#define SSUSB_U2_PORT_NUM(p)	((p>>8) & 0xff)
/***************************************************/
/******* Defined for OTG usage ********************************************/
#define SSUSB_OTG_STS			(SIFSLV_IPPC+0x18)
#define SSUSB_ATTACH_A_ROLE		(1<<0)
#define SSUSB_CHG_A_ROLE_A		(1<<1)
#define SSUSB_CHG_B_ROLE_A		(1<<2)
#define SSUSB_ATTACH_B_ROLE		(1<<3)
#define SSUSB_CHG_A_ROLE_B		(1<<4)
#define SSUSB_CHG_B_ROLE_B		(1<<5)
#define VBUS_CHG_INTR			(1<<6)
#define SSUSB_DEV_USBRST_INTR	(1<<7)
#define SSUSB_HOST_DEV_MODE		(1<<8)
#define SSUSB_VBUS_VALID		(1<<9)
#define SSUSB_IDDIG				(1<<10)
#define SSUSB_SRP_REQ_INTR		(1<<11)
#define SSUSB_DEV_DMA_REQ		(1<<13)
#define SSUSB_XHCI_MAS_DMA_REQ	(1<<14)

#define SSUSB_OTG_STS_CLR		(SIFSLV_IPPC+0x1c)
#define SSUSB_ATTACH_A_ROLE_CLR	(1<<0)
#define SSUSB_CHG_A_ROLE_A_CLR	(1<<1)
#define SSUSB_CHG_B_ROLE_A_CLR	(1<<2)
#define SSUSB_ATTACH_B_ROLE_CLR	(1<<3)
#define SSUSB_CHG_A_ROLE_B_CLR	(1<<4)
#define SSUSB_CHG_B_ROLE_B_CLR	(1<<5)
#define SSUSB_VBUS_INTR_CLR		(1<<6)
#define SSUSB_DEV_USBRST_INTR_CLR	(1<<7)
#define SSUSB_SRP_REQ_INTR_CLR	(1<<11)

#define SSUSB_OTG_INT_EN		(SIFSLV_IPPC+0x2c)
#define SSUSB_ATTACH_A_ROLE_INT_EN	(1<<0)
#define SSUSB_CHG_A_ROLE_A_INT_EN	(1<<1)
#define SSUSB_CHG_B_ROLE_A_INT_EN	(1<<2)
#define SSUSB_ATTACH_B_ROLE_INT_EN	(1<<3)
#define SSUSB_CHG_A_ROLE_B_INT_EN	(1<<4)
#define SSUSB_CHG_B_ROLE_B_INT_EN	(1<<5)
#define SSUSB_VBUS_CHG_INT_B_EN		(1<<6)
#define SSUSB_VBUS_CHG_INT_A_EN		(1<<7)
#define SSUSB_DEV_USBRST_INT_EN		(1<<8)
#define SSUSB_SRP_REQ_INTR_EN		(1<<11)

#define SSUSB_CSR_CK_CTRL		(SIFSLV_IPPC+0x88)
#define SSUSB_SIFSLV_MCU_BUS_CK_GATE_EN	(1<<1)

#define SSUSB_U2_PORT_OTG_SEL		(1<<7)
#define SSUSB_U2_PORT_OTG_MAC_AUTO_SEL	(1<<8)
#define SSUSB_U2_PORT_OTG_HOST_VBUSVALID_SEL   (1<<9)

#define DEVICE_CONTROL			(SSUSB_U2_SYS_BASE+0xc)
#define DEVICE_CONTROL_session	(1<<0)
#define DEVICE_CONTROL_hostreq	(1<<1)
#define DEVICE_CONTROL_vbus		(3<<3)
#define DEVICE_CONTROL_b_dev	(1<<7)

#define POWER_MANAGEMENT			(SSUSB_U2_SYS_BASE+04)
#define HS_ENABLE			(1<<5)


#define USB2_TEST_MODE			(SSUSB_U2_SYS_BASE+014)
#define USB2_TEST_NAK_MODE			(1<<0)
#define USB2_TEST_J			(1<<1)
#define USB2_TEST_K			(1<<2)
#define USB2_TEST_PACKET			(1<<3)
#define USB2_FORCE_HS			(1<<4)
#define USB2_FORCE_FS			(1<<5)
#define USB2_FORCE_HOST			(1<<7)





/****************************************************************/

struct ixia_dev
{
	struct usb_device *udev;
	int ep_out;
	int ep_in;
};

int f_enable_port(int index);
int f_disconnect_port(int index);
int f_enable_slot(struct usb_device *dev);
int f_disable_slot();
int f_address_slot(char isBSR, struct usb_device *dev);
int f_slot_reset_device(int slot_id, char isWarmReset);
int f_udev_add_ep(struct usb_host_endpoint *ep, struct usb_device *udev);
int f_xhci_config_ep(struct usb_device *udev);
int f_evaluate_context(int max_exit_latency, int maxp0, int preping_mode, int preping, int besl, int besld);

int f_power_suspend();
int f_power_resume();
int f_power_remotewakeup();
int f_power_set_u1u2(int u_num, int value1, int value2);
int f_power_send_fla(int value);

int f_ring_stop_cmd();
int f_ring_abort_cmd();
int f_ring_enlarge(int ep_dir, int ep_num, int dev_num);
int f_ring_stop_ep(int slot_id, int ep_index);
int f_ring_set_tr_dequeue_pointer(int slot_id, int ep_index, struct urb *urb);

int f_hub_setportfeature(int hdev_num, int wValue, int wIndex);
int f_hub_clearportfeature(int hdev_num, int wValue, int wIndex);
int f_hub_sethubfeature(int hdev_num, int wValue);
int f_hub_config_subhub(int parent_hub_num, int hub_num, int port_num);
int f_hub_configep(int hdev_num, int rh_port_index);
int f_hub_configuredevice(int hub_num, int port_num, int dev_num
		, int transfer_type, int maxp, int bInterval, char is_config_ep, char is_stress, int stress_config);
int f_hub_reset_dev(struct usb_device *udev,int dev_num, int port_num, int speed);
int f_hub_configure_eth_device(int hub_num, int port_num, int dev_num);

int f_random_stop(int ep_1_num, int ep_2_num, int stop_count_1, int stop_count_2, int urb_dir_1, int urb_dir_2, int length);
int f_add_random_stop_ep_thread(struct xhci_hcd *xhci, int slot_id, int ep_index);
int f_add_random_ring_doorbell_thread(struct xhci_hcd *xhci, int slot_id, int ep_index);
int f_config_ep(char ep_num,int ep_dir,int transfer_type, int maxp,int bInterval, int burst, int mult, struct usb_device *udev,int config_xhci);
int f_deconfig_ep(char is_all, char ep_num,int ep_dir,struct usb_device *usbdev,int config_xhci);
int f_add_str_threads(int dev_num, int ep_num, int maxp, char isCompare, struct usb_device *usbdev, char isEP0);
int f_add_ixia_thread(struct xhci_hcd *xhci, int dev_num, struct ixia_dev *ix_dev);
int f_fill_urb(struct urb *urb,int ep_num,int data_length, int start_add,int dir, int iso_num_packets, int psize, struct usb_device *usbdev);
int f_fill_urb_with_buffer(struct urb *urb,int ep_num,int data_length,void *buffer,int start_add,int dir, int iso_num_packets, int psize
	, dma_addr_t dma_mapping, struct usb_device *usbdev);
int f_queue_urb(struct urb *urb,int wait, struct usb_device *dev);
int f_update_hub_device(struct usb_device *udev, int num_ports);

int f_loopback_loop(int ep_out, int ep_in, int data_length, int start_add, struct usb_device *usbdev);
int f_loopback_sg_loop(int ep_out, int ep_in, int data_length, int start_add, int sg_len, struct usb_device *usbdev);
int f_loopback_loop_gpd(int ep_out, int ep_in, int data_length, int start_add, int gpd_length, struct usb_device *usbdev);
int f_loopback_sg_loop_gpd(int ep_out, int ep_in, int data_length, int start_add, int sg_len, int gpd_length, struct usb_device *usbdev);

int mtk_xhci_handshake(struct xhci_hcd *xhci, void __iomem *ptr,
		      u32 mask, u32 done, int msec);

int otg_dev_A_host_thread(void *data);
int otg_dev_B_hnp(void * data);
int otg_dev_A_hnp(void * data);
int otg_dev_A_hnp_back(void * data);
int otg_dev_B_hnp_back(void *data);
int otg_dev_A_srp(void * data);
int otg_opt_uut_a(void *data);
int otg_opt_uut_b(void *data);
int otg_pet_uut_a(void *data);
int otg_pet_uut_b(void *data);

int f_test_lib_init();
int f_test_lib_cleanup();

int f_drv_vbus();
int f_stop_vbus();

struct urb *alloc_ctrl_urb(struct usb_ctrlrequest *dr, char *buffer, struct usb_device *udev);
int f_ctrlrequest(struct urb *urb, struct usb_device *udev);

int get_port_index(int port_id);
void print_speed(int speed);
int wait_event_on_timeout(int *ptr, int done, int msecs);
//int poll_event_on_timeout(int *ptr, int done, int msecs);

void start_port_reenabled(int index, int speed);
int f_reenable_port(int index);

int f_enable_dev_note();

int f_add_rdn_len_str_threads(int dev_num, int ep_num, int maxp, char isCompare, struct usb_device *usbdev, char isEP0);
int f_add_random_access_reg_thread(struct xhci_hcd *xhci, int port_id, int port_rev
	, int power_required);
int f_power_reset_u1u2_counter(int u_num);
int f_power_get_u1u2_counter(int u_num);
int f_power_config_lpm(u32 slot_id, u32 hirdm, u32 L1_timeout, u32 rwe, u32 besl, u32 besld, u32 hle
	, u32 int_nak_active, u32 bulk_nyet_active);
int f_power_reset_L1_counter(int direct);
int f_power_get_L1_counter(int direct);
int f_port_set_pls(int port_id, int pls);
int f_ctrlrequest_nowait(struct urb *urb, struct usb_device *udev);

int f_host_test_mode(int case_num) ;

int f_host_test_mode_stop() ;

int wait_not_event_on_timeout(int *ptr, int value, int msecs);

/* IP configuration */
#define RH_PORT_NUM 2
#define DEV_NUM 4
#define HUB_DEV_NUM 4

/* constant define */
#define MAX_DATA_LENGTH 65535

/* timeout */
#define ATTACH_TIMEOUT 20000
#define CMD_TIMEOUT    80
#define TRANS_TIMEOUT  100
#define SYNC_DELAY     100

/* return code */
#define RET_SUCCESS 0
#define RET_FAIL 1

/* command stage */
#define CMD_RUNNING 0
#define CMD_DONE 1
#define CMD_FAIL 2

/* transfer stage */
#define TRANS_INPROGRESS 0
#define TRANS_DONE 1

/* loopback stage */
#define LOOPBACK_OUT  0
#define LOOPBACK_IN   1

/* USB spec constant */
/* EP descriptor */
#define EPADD_NUM(n) ((n)<<0)
#define EPADD_OUT 0
#define EPADD_IN  (1<<7)

#define EPATT_CTRL 0
#define EPATT_ISO  1
#define EPATT_BULK 2
#define EPATT_INT  3

#define MAX_DATA_LENGTH 65535

/* control request code */
#define REQ_GET_STATUS		0
#define REQ_SET_FEATURE 	3
#define REQ_CLEAR_FEATURE   1

/* hub request code */
#define HUB_FEATURE_PORT_POWER 		  8
#define HUB_FEATURE_PORT_RESET		  4
#define HUB_FEATURE_PORT_SUSPEND	2
#define HUB_FEATURE_C_PORT_CONNECTION 16
#define HUB_FEATURE_C_PORT_RESET	  20

#define HUB_FEATURE_PORT_LINK_STATE		5

/*macro for USB-IF for OTG driver*/
#define OTG_CMD_E_ENABLE_VBUS       0x00
#define OTG_CMD_E_ENABLE_SRP        0x01
#define OTG_CMD_E_START_DET_SRP     0x02
#define OTG_CMD_E_START_DET_VBUS    0x03
#define OTG_CMD_P_A_UUT             0x04
#define OTG_CMD_P_B_UUT             0x05
#define HOST_CMD_TEST_SE0_NAK    		0x6
#define HOST_CMD_TEST_J          		0x7
#define HOST_CMD_TEST_K          		0x8
#define HOST_CMD_TEST_PACKET     		0x9
#define HOST_CMD_SUSPEND_RESUME  		0xa
#define HOST_CMD_GET_DESCRIPTOR  		0xb
#define HOST_CMD_SET_FEATURE     		0xc
#define OTG_CMD_P_B_UUT_TD59        	0xd
#define HOST_CMD_ENV_INIT				0xe
#define HOST_CMD_ENV_EXIT				0xf

/* global structure */
typedef enum
{
    DISCONNECTED = 0,
    CONNECTED,
    RESET,
    ENABLED
}XHCI_PORT_STATUS;

struct xhci_port
{
	int port_id;
	XHCI_PORT_STATUS port_status;
	int port_speed;
	int port_reenabled;
};

/* for stress test */
#define TOTAL_URB	30
#define URB_STATUS_IDLE	150
#define URB_STATUS_RX	151
#define GPD_LENGTH	(16*1024)
#define GPD_LENGTH_RDN	(10*1024)

/* global parameters */
#ifdef MTK_TEST_LIB
#define AUTOEXT
#else
#define AUTOEXT  extern
#endif
AUTOEXT volatile int g_port_connect;
AUTOEXT volatile int g_port_reset;
AUTOEXT volatile int g_port_id;
AUTOEXT volatile int g_slot_id;
AUTOEXT volatile int g_speed;
AUTOEXT volatile int g_cmd_status;
AUTOEXT volatile char g_event_full;
AUTOEXT volatile char g_got_event_full;
AUTOEXT volatile int g_device_reconnect;
AUTOEXT struct usb_device *dev_list[DEV_NUM];
AUTOEXT struct usb_device *hdev_list[HUB_DEV_NUM];
AUTOEXT struct usb_hcd *my_hcd;
AUTOEXT struct xhci_port *rh_port[RH_PORT_NUM];
AUTOEXT volatile int g_stress_status;
AUTOEXT struct ixia_dev *ix_dev_list[4];
AUTOEXT volatile char g_exec_done;
AUTOEXT volatile char g_stopped;
AUTOEXT volatile char g_correct;
AUTOEXT volatile int g_dev_notification;
AUTOEXT volatile long g_dev_not_value;
AUTOEXT volatile long g_intr_handled;
AUTOEXT volatile int g_mfindex_event;
AUTOEXT volatile char g_port_occ;
AUTOEXT volatile char g_is_bei;
AUTOEXT volatile char g_td_to_noop;
AUTOEXT volatile char g_iso_frame;
AUTOEXT volatile char g_test_random_stop_ep;
AUTOEXT volatile char g_stopping_ep;
AUTOEXT volatile char g_port_resume;
AUTOEXT volatile int g_cmd_ring_pointer1;
AUTOEXT volatile int g_cmd_ring_pointer2;
AUTOEXT volatile char g_idt_transfer;
AUTOEXT volatile char g_port_plc;
AUTOEXT volatile char g_power_down_allowed;
AUTOEXT volatile char g_hs_block_reset;
AUTOEXT volatile char g_concurrent_resume;
AUTOEXT volatile int g_otg_test;
AUTOEXT volatile int g_otg_dev_B;
AUTOEXT volatile int g_otg_hnp_become_host;
AUTOEXT volatile int g_otg_hnp_become_dev;
AUTOEXT volatile int g_otg_srp_pend;
AUTOEXT volatile int g_otg_pet_status;
AUTOEXT volatile int g_otg_csc;
AUTOEXT volatile int g_otg_iddig;
AUTOEXT volatile int g_otg_wait_con;
AUTOEXT volatile int g_otg_dev_conf_len;
AUTOEXT volatile int g_otg_unsupported_dev;
AUTOEXT volatile int g_otg_slot_enabled;
AUTOEXT volatile int g_otg_iddig_toggled;
typedef enum
{
    OTG_DISCONNECTED = 0,
    OTG_POLLING_STATUS,
    OTG_SET_NHP,
    OTG_HNP_INIT,
    OTG_HNP_SUSPEND,
    OTG_HNP_DEV,
    OTG_HNP_DISCONNECTED,
    OTG_DEV	
    
}PET_STATUS;


// Billionton Definition
#define AX_CMD_SET_SW_MII               0x06
#define AX_CMD_READ_MII_REG             0x07
#define AX_CMD_WRITE_MII_REG            0x08
#define AX_CMD_READ_MII_OPERATION_MODE      0x09
#define AX_CMD_SET_HW_MII               0x0a
#define AX_CMD_READ_EEPROM              0x0b
#define AX_CMD_WRITE_EEPROM             0x0c
#define AX_CMD_READ_RX_CONTROL_REG             0x0f
#define AX_CMD_WRITE_RX_CTL             0x10
#define AX_CMD_READ_IPG012              0x11
#define AX_CMD_WRITE_IPG0               0x12
#define AX_CMD_WRITE_IPG1               0x13
#define AX_CMD_WRITE_IPG2               0x14
#define AX_CMD_READ_MULTIFILTER_ARRAY           0x15
#define AX_CMD_WRITE_MULTI_FILTER       0x16
#define AX_CMD_READ_NODE_ID             0x17
#define AX_CMD_READ_PHY_ID                  0x19
#define AX_CMD_READ_MEDIUM_MODE    0x1a
#define AX_CMD_WRITE_MEDIUM_MODE        0x1b
#define AX_CMD_READ_MONITOR_MODE        0x1c
#define AX_CMD_WRITE_MONITOR_MODE       0x1d
#define AX_CMD_READ_GPIOS                   0x1e
#define AX_CMD_WRITE_GPIOS                  0x1f

#define AX_CMD_GUSBKR3_BREQ         0x05
#define AX_CMD_GUSBKR3_12e          0x12e
#define AX_CMD_GUSBKR3_120          0x120
#define AX_CMD_GUSBKR3_126          0x126
#define AX_CMD_GUSBKR3_134          0x134
#define AX_CMD_GUSBKR3_12f          0x12f
#define AX_CMD_GUSBKR3_130          0x130
#define AX_CMD_GUSBKR3_137          0x137
#define AX_CMD_GUSBKR3_02           0x02
#define AX_CMD_GUSBKR3_13e          0x13e

/*
* USB directions
*/
#define MUSB_DIR_OUT			0
#define MUSB_DIR_IN			0x80

/*
* USB request types
*/
#define MUSB_TYPE_MASK			(0x03 << 5)
#define MUSB_TYPE_STANDARD		(0x00 << 5)
#define MUSB_TYPE_CLASS			(0x01 << 5)
#define MUSB_TYPE_VENDOR		(0x02 << 5)
#define MUSB_TYPE_RESERVED		(0x03 << 5)

/*
* USB recipients
*/
#define MUSB_RECIP_MASK			0x1f
#define MUSB_RECIP_DEVICE		0x00
#define MUSB_RECIP_INTERFACE    0x01
#define MUSB_RECIP_ENDPOINT		0x02
#define MUSB_RECIP_OTHER		0x03

/*
* Standard requests
*/
#define MUSB_REQ_GET_STATUS		           0x00
#define MUSB_REQ_CLEAR_FEATURE	           0x01
#define MUSB_REQ_SET_FEATURE		           0x03
#define MUSB_REQ_SET_ADDRESS		           0x05
#define MUSB_REQ_GET_DESCRIPTOR		    0x06
#define MUSB_REQ_SET_DESCRIPTOR		    0x07
#define MUSB_REQ_GET_CONFIGURATION	    0x08
#define MUSB_REQ_SET_CONFIGURATION	    0x09
#define MUSB_REQ_GET_INTERFACE		    0x0A
#define MUSB_REQ_SET_INTERFACE		    0x0B
#define MUSB_REQ_SYNCH_FRAME                  0x0C
#define VENDOR_CONTROL_NAKTIMEOUT_TX    0x20
#define VENDOR_CONTROL_NAKTIMEOUT_RX    0x21
#define VENDOR_CONTROL_DISPING                0x22
#define VENDOR_CONTROL_ERROR                   0x23
#define VENDOR_CONTROL_RXSTALL                0x24

/*
* Descriptor types
*/
#define MUSB_DT_DEVICE			0x01
#define MUSB_DT_CONFIG			0x02
#define MUSB_DT_STRING			0x03
#define MUSB_DT_INTERFACE		0x04
#define MUSB_DT_ENDPOINT		0x05
#define MUSB_DT_DEVICE_QUALIFIER	0x06
#define MUSB_DT_OTHER_SPEED		0X07
#define MUSB_DT_INTERFACE_POWER		0x08
#define MUSB_DT_OTG			0x09

struct MUSB_DeviceRequest
{
    unsigned char bmRequestType;
    unsigned char bRequest;
    unsigned short wValue;
    unsigned short wIndex;
    unsigned short wLength;
};


struct ethenumeration_t
{
    unsigned char* pDesciptor;
    struct MUSB_DeviceRequest sDevReq;
};

struct MUSB_ConfigurationDescriptor
{
    unsigned char bLength;
    unsigned char bDescriptorType;
    unsigned short wTotalLength;
    unsigned char bNumInterfaces;
    unsigned char bConfigurationValue;
    unsigned char iConfiguration;
    unsigned char bmAttributes;
    unsigned char bMaxPower;
};

struct MUSB_InterfaceDescriptor
{
    unsigned char bLength;
    unsigned char bDescriptorType;
    unsigned char bInterfaceNumber;
    unsigned char bAlternateSetting;
    unsigned char bNumEndpoints;
    unsigned char bInterfaceClass;
    unsigned char bInterfaceSubClass;
    unsigned char bInterfaceProtocol;
    unsigned char iInterface;
};

struct MUSB_EndpointDescriptor
{
    unsigned char bLength;
    unsigned char bDescriptorType;
    unsigned char bEndpointAddress;
    unsigned char bmAttributes;
    unsigned short wMaxPacketSize;
    unsigned char bInterval;
};

struct MUSB_DeviceDescriptor
{
    unsigned char bLength;
    unsigned char bDescriptorType;
    unsigned short bcdUSB;
    unsigned char bDeviceClass;
    unsigned char bDeviceSubClass;
    unsigned char bDeviceProtocol;
    unsigned char bMaxPacketSize0;
    unsigned short idVendor;
    unsigned short idProduct;
    unsigned short bcdDevice;
    unsigned char iManufacturer;
    unsigned char iProduct;
    unsigned char iSerialNumber;
    unsigned char bNumConfigurations;
};

#define MTK_TEST_DBG

#ifdef MTK_TEST_DBG
#define mtk_test_dbg(fmt, args...) \
	do { printk("%s(%d):" fmt, __func__, __LINE__, ##args); } while (0)
#else
#define mtk_test_dbg(fmt, args...)
#endif


#endif
