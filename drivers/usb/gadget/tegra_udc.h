/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Description:
 * High-speed USB device controller driver.
 * USB device/endpoint management registers.
 * The driver is previously named as fsl_udc_core. Based on Freescale driver
 * code from Li Yang and Jiang Bo.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __TEGRA_UDC_H
#define __TEGRA_UDC_H

#ifdef VERBOSE
#define VDBG(fmt, args...)	printk(KERN_DEBUG "[%s]  " fmt "\n", \
				__func__, ## args)
#else
#define VDBG(fmt, args...)	do {} while (0)
#endif


#ifdef DEBUG
#define DBG(stuff...)	pr_info("tegra_udc: " stuff)
#else
#define DBG(stuff...)	do {} while (0)
#endif

#define ERR(stuff...)		pr_err("tegra_udc: " stuff)
#define WARNING(stuff...)	pr_warning("tegra_udc: " stuff)

#define DMA_ADDR_INVALID	(~(dma_addr_t)0)
#define STATUS_BUFFER_SIZE	8

#define USB_MAX_CTRL_PAYLOAD		64

 /* Charger current limit=1800mA, as per the USB charger spec */
#define USB_CHARGING_DCP_CURRENT_LIMIT_UA 1800000u
#define USB_CHARGING_CDP_CURRENT_LIMIT_UA 1500000u
#define USB_CHARGING_SDP_CURRENT_LIMIT_UA 500000u
#define USB_CHARGING_NV_CHARGER_CURRENT_LIMIT_UA 2000000u
#define USB_CHARGING_NON_STANDARD_CHARGER_CURRENT_LIMIT_UA 500000u

 /* 1 sec wait time for non-std charger detection after vbus is detected */
#define NON_STD_CHARGER_DET_TIME_MS 1000
#define BOOST_TRIGGER_SIZE 4096

#define UDC_RESET_TIMEOUT_MS 1000
#define UDC_RUN_TIMEOUT_MS 1000
#define UDC_FLUSH_TIMEOUT_MS 1000

/* ep0 transfer state */
#define WAIT_FOR_SETUP			0
#define DATA_STATE_XMIT			1
#define DATA_STATE_NEED_ZLP		2
#define WAIT_FOR_OUT_STATUS		3
#define DATA_STATE_RECV			4

/*
 * ### pipe direction macro from device view
 */
#define USB_RECV	0	/* OUT EP */
#define USB_SEND	1	/* IN EP */

/* Device Controller Capability Parameter register */
#define DCCPARAMS_REG_OFFSET		0x124
#define DCCPARAMS_DC				0x00000080
#define DCCPARAMS_DEN_MASK			0x0000001f

/* USB CMD  Register Bit Masks */
#define USB_CMD_REG_OFFSET		((udc->has_hostpc) ? 0x130 : 0x140)
#define  USB_CMD_RUN_STOP		      0x00000001
#define  USB_CMD_CTRL_RESET                   0x00000002
#define  USB_CMD_PERIODIC_SCHEDULE_EN	      0x00000010
#define  USB_CMD_ASYNC_SCHEDULE_EN            0x00000020
#define  USB_CMD_INT_AA_DOORBELL              0x00000040
#define  USB_CMD_ASP                          0x00000300
#define  USB_CMD_ASYNC_SCH_PARK_EN            0x00000800
#define  USB_CMD_SUTW                         0x00002000
#define  USB_CMD_ATDTW                        0x00004000
#define  USB_CMD_ITC                          0x00FF0000
/* bit 15,3,2 are frame list size */
#define  USB_CMD_FRAME_SIZE_1024              0x00000000
#define  USB_CMD_FRAME_SIZE_512               0x00000004
#define  USB_CMD_FRAME_SIZE_256               0x00000008
#define  USB_CMD_FRAME_SIZE_128               0x0000000C
#define  USB_CMD_FRAME_SIZE_64                0x00008000
#define  USB_CMD_FRAME_SIZE_32                0x00008004
#define  USB_CMD_FRAME_SIZE_16                0x00008008
#define  USB_CMD_FRAME_SIZE_8                 0x0000800C
/* bit 9-8 are async schedule park mode count */
#define  USB_CMD_ASP_00                       0x00000000
#define  USB_CMD_ASP_01                       0x00000100
#define  USB_CMD_ASP_10                       0x00000200
#define  USB_CMD_ASP_11                       0x00000300
#define  USB_CMD_ASP_BIT_POS                  8
/* bit 23-16 are interrupt threshold control */
#define  USB_CMD_ITC_NO_THRESHOLD             0x00000000
#define  USB_CMD_ITC_1_MICRO_FRM              0x00010000
#define  USB_CMD_ITC_2_MICRO_FRM              0x00020000
#define  USB_CMD_ITC_4_MICRO_FRM              0x00040000
#define  USB_CMD_ITC_8_MICRO_FRM              0x00080000
#define  USB_CMD_ITC_16_MICRO_FRM             0x00100000
#define  USB_CMD_ITC_32_MICRO_FRM             0x00200000
#define  USB_CMD_ITC_64_MICRO_FRM             0x00400000
#define  USB_CMD_ITC_BIT_POS                  16

/* USB STS Register Bit Masks */
#define USB_STS_REG_OFFSET		((udc->has_hostpc) ? 0x134 : 0x144)
#define  USB_STS_INT                          0x00000001
#define  USB_STS_ERR                          0x00000002
#define  USB_STS_PORT_CHANGE                  0x00000004
#define  USB_STS_FRM_LST_ROLL                 0x00000008
#define  USB_STS_SYS_ERR                      0x00000010
#define  USB_STS_IAA                          0x00000020
#define  USB_STS_RESET                        0x00000040
#define  USB_STS_SOF                          0x00000080
#define  USB_STS_SUSPEND                      0x00000100
#define  USB_STS_HC_HALTED                    0x00001000
#define  USB_STS_RCL                          0x00002000
#define  USB_STS_PERIODIC_SCHEDULE            0x00004000
#define  USB_STS_ASYNC_SCHEDULE               0x00008000

/* USB INTR Register Bit Masks */
#define USB_INTR_REG_OFFSET		((udc->has_hostpc) ? 0x138 : 0x148)
#define  USB_INTR_INT_EN                      0x00000001
#define  USB_INTR_ERR_INT_EN                  0x00000002
#define  USB_INTR_PTC_DETECT_EN               0x00000004
#define  USB_INTR_FRM_LST_ROLL_EN             0x00000008
#define  USB_INTR_SYS_ERR_EN                  0x00000010
#define  USB_INTR_ASYN_ADV_EN                 0x00000020
#define  USB_INTR_RESET_EN                    0x00000040
#define  USB_INTR_SOF_EN                      0x00000080
#define  USB_INTR_DEVICE_SUSPEND              0x00000100

/* Frame Index Register Bit Masks */
#define USB_FRINDEX_REG_OFFSET		((udc->has_hostpc) ? 0x13c : 0x14c)
#define  USB_FRINDEX_MASKS			0x3fff

/* Device Address bit masks */
#define USB_DEVICE_ADDR_REG_OFFSET	((udc->has_hostpc) ? 0x144 : 0x154)
#define  USB_DEVICE_ADDRESS_MASK              0xFE000000
#define  USB_DEVICE_ADDRESS_BIT_POS           25

/* endpoint list address bit masks */
#define USB_EP_LIST_ADDRESS_REG_OFFSET	((udc->has_hostpc) ? 0x148 : 0x158)
#define  USB_EP_LIST_ADDRESS_MASK			0xfffff800

/* PORTSCX  Register Bit Masks */
#define PORTSCX_REG_OFFSET		((udc->has_hostpc) ? 0x174 : 0x184)
#define  PORTSCX_CURRENT_CONNECT_STATUS       0x00000001
#define  PORTSCX_CONNECT_STATUS_CHANGE        0x00000002
#define  PORTSCX_PORT_ENABLE                  0x00000004
#define  PORTSCX_PORT_EN_DIS_CHANGE           0x00000008
#define  PORTSCX_OVER_CURRENT_ACT             0x00000010
#define  PORTSCX_OVER_CURRENT_CHG             0x00000020
#define  PORTSCX_PORT_FORCE_RESUME            0x00000040
#define  PORTSCX_PORT_SUSPEND                 0x00000080
#define  PORTSCX_PORT_RESET                   0x00000100
#define  PORTSCX_LINE_STATUS_BITS             0x00000C00
#define  PORTSCX_LINE_STATUS_DP_BIT           0x00000800
#define  PORTSCX_LINE_STATUS_DM_BIT           0x00000400
#define  PORTSCX_PORT_POWER                   0x00001000
#define  PORTSCX_PORT_INDICTOR_CTRL           0x0000C000
#define  PORTSCX_PORT_TEST_CTRL               0x000F0000
#define  PORTSCX_WAKE_ON_CONNECT_EN           0x00100000
#define  PORTSCX_WAKE_ON_CONNECT_DIS          0x00200000
#define  PORTSCX_WAKE_ON_OVER_CURRENT         0x00400000
#define  PORTSCX_PHY_LOW_POWER_SPD            0x00800000

/* In tegra3 the following fields have moved to new HOSTPC1_DEVLC reg and
 * their offsets have changed.
 * Keeping the name of bit masks same as before (PORTSCX_*) to have
 * minimum changes to code */
#define USB_HOSTPCX_DEVLC_REG_OFFSET			0x1b4
#define  HOSTPC1_DEVLC_ASUS			0x00020000

#define  PORTSCX_PORT_FORCE_FULL_SPEED ((udc->has_hostpc) ? 0x00800000 \
						: 0x01000000)
#define  PORTSCX_PORT_SPEED_MASK  ((udc->has_hostpc) ? 0x06000000 : 0x0C000000)
#define  PORTSCX_PORT_WIDTH	  ((udc->has_hostpc) ? 0x08000000 : 0x10000000)
#define  PORTSCX_PHY_TYPE_SEL	  ((udc->has_hostpc) ? 0xE0000000 : 0xC0000000)

/* bits for port speed */
#define  PORTSCX_PORT_SPEED_FULL  ((udc->has_hostpc) ? 0x00000000 : 0x00000000)
#define  PORTSCX_PORT_SPEED_LOW	  ((udc->has_hostpc) ? 0x02000000 : 0x04000000)
#define  PORTSCX_PORT_SPEED_HIGH  ((udc->has_hostpc) ? 0x04000000 : 0x08000000)
#define  PORTSCX_PORT_SPEED_UNDEF ((udc->has_hostpc) ? 0x06000000 : 0x0C000000)
#define  PORTSCX_SPEED_BIT_POS	  ((udc->has_hostpc) ? 25 : 26)

/* bits for parallel transceiver width for UTMI interface */
#define  PORTSCX_PTW		((udc->has_hostpc) ? 0x08000000 : 0x10000000)
#define  PORTSCX_PTW_8BIT	((udc->has_hostpc) ? 0x00000000 : 0x00000000)
#define  PORTSCX_PTW_16BIT	((udc->has_hostpc) ? 0x08000000 : 0x10000000)

/* bits for port transceiver select */
#define  PORTSCX_PTS_UTMI	((udc->has_hostpc) ? 0x00000000 : 0x00000000)
#define  PORTSCX_PTS_ULPI	((udc->has_hostpc) ? 0x40000000 : 0x80000000)
#define  PORTSCX_PTS_FSLS	((udc->has_hostpc) ? 0x60000000 : 0xC0000000)
#define  PORTSCX_PTS_BIT_POS		((udc->has_hostpc) ? 29 : 30)

/* bit 11-10 are line status */
#define  PORTSCX_LINE_STATUS_SE0              0x00000000
#define  PORTSCX_LINE_STATUS_JSTATE           0x00000400
#define  PORTSCX_LINE_STATUS_KSTATE           0x00000800
#define  PORTSCX_LINE_STATUS_UNDEF            0x00000C00
#define  PORTSCX_LINE_STATUS_BIT_POS          10

/* bit 15-14 are port indicator control */
#define  PORTSCX_PIC_OFF                      0x00000000
#define  PORTSCX_PIC_AMBER                    0x00004000
#define  PORTSCX_PIC_GREEN                    0x00008000
#define  PORTSCX_PIC_UNDEF                    0x0000C000
#define  PORTSCX_PIC_BIT_POS                  14

/* bit 19-16 are port test control */
#define  PORTSCX_PTC_DISABLE                  0x00000000
#define  PORTSCX_PTC_JSTATE                   0x00010000
#define  PORTSCX_PTC_KSTATE                   0x00020000
#define  PORTSCX_PTC_SEQNAK                   0x00030000
#define  PORTSCX_PTC_PACKET                   0x00040000
#define  PORTSCX_PTC_FORCE_EN                 0x00050000
#define  PORTSCX_PTC_BIT_POS                  16


/* USB MODE Register Bit Masks */
#define USB_MODE_REG_OFFSET		((udc->has_hostpc) ? 0x1f8 : 0x1a8)
#define  USB_MODE_CTRL_MODE_IDLE		0x00000000
#define  USB_MODE_CTRL_MODE_DEVICE		0x00000002
#define  USB_MODE_CTRL_MODE_HOST		0x00000003
#define  USB_MODE_CTRL_MODE_RSV			0x00000001
#define  USB_MODE_SETUP_LOCK_OFF		0x00000008
#define  USB_MODE_STREAM_DISABLE		0x00000010

/* Endpoint Setup Status bit masks */
#define EP_SETUP_STATUS_REG_OFFSET	((udc->has_hostpc) ? 0x208 : 0x1ac)
#define  EP_SETUP_STATUS_MASK			0x0000003F
#define  EP_SETUP_STATUS_EP0			0x00000001

/* Endpoint Prime Register */
#define EP_PRIME_REG_OFFSET		((udc->has_hostpc) ? 0x20c : 0x1b0)

/* Endpoint Flush Register */
#define EPFLUSH_REG_OFFSET		((udc->has_hostpc) ? 0x210 : 0x1b4)
#define  EPFLUSH_TX_OFFSET		0x00010000
#define  EPFLUSH_RX_OFFSET		0x00000000

/* Endpoint Status Register */
#define EP_STATUS_REG_OFFSET		((udc->has_hostpc) ? 0x214 : 0x1b8)

/* Endpoint Complete Register */
#define EP_COMPLETE_REG_OFFSET		((udc->has_hostpc) ? 0x218 : 0x1bc)

/* Endpoint Control Registers */
#define EP_CONTROL_REG_OFFSET		((udc->has_hostpc) ? 0x21c : 0x1c0)

/* ENDPOINTCTRLx  Register Bit Masks */
#define  EPCTRL_TX_ENABLE                     0x00800000
#define  EPCTRL_TX_DATA_TOGGLE_RST            0x00400000	/* Not EP0 */
#define  EPCTRL_TX_DATA_TOGGLE_INH            0x00200000	/* Not EP0 */
#define  EPCTRL_TX_TYPE                       0x000C0000
#define  EPCTRL_TX_DATA_SOURCE                0x00020000	/* Not EP0 */
#define  EPCTRL_TX_EP_STALL                   0x00010000
#define  EPCTRL_RX_ENABLE                     0x00000080
#define  EPCTRL_RX_DATA_TOGGLE_RST            0x00000040	/* Not EP0 */
#define  EPCTRL_RX_DATA_TOGGLE_INH            0x00000020	/* Not EP0 */
#define  EPCTRL_RX_TYPE                       0x0000000C
#define  EPCTRL_RX_DATA_SINK                  0x00000002	/* Not EP0 */
#define  EPCTRL_RX_EP_STALL                   0x00000001

/* bit 19-18 and 3-2 are endpoint type */
#define  EPCTRL_EP_TYPE_CONTROL               0
#define  EPCTRL_EP_TYPE_ISO                   1
#define  EPCTRL_EP_TYPE_BULK                  2
#define  EPCTRL_EP_TYPE_INTERRUPT             3
#define  EPCTRL_TX_EP_TYPE_SHIFT              18
#define  EPCTRL_RX_EP_TYPE_SHIFT              2

#define VBUS_SENSOR_REG_OFFSET			0x404
#define VBUS_WAKEUP_REG_OFFSET			0x408

#define  USB_SYS_VBUS_A_VLD_SW_VALUE		BIT(28)
#define  USB_SYS_VBUS_A_VLD_SW_EN		BIT(27)
#define  USB_SYS_VBUS_ASESSION_VLD_SW_VALUE	BIT(20)
#define  USB_SYS_VBUS_ASESSION_VLD_SW_EN	BIT(19)
#define  USB_SYS_VBUS_ASESSION_INT_EN		0x10000
#define  USB_SYS_VBUS_ASESSION_CHANGED		0x20000
#define  USB_SYS_VBUS_ASESSION			0x40000
#define  USB_SYS_VBUS_WAKEUP_ENABLE		0x40000000
#define  USB_SYS_VBUS_WAKEUP_INT_ENABLE		0x100
#define  USB_SYS_VBUS_WAKEUP_INT_STATUS		0x200
#define  USB_SYS_VBUS_STATUS			0x400
#define  USB_SYS_ID_PIN_STATUS			0x4


/* Endpoint Queue Head Bit Masks */
#define  EP_QUEUE_HEAD_MULT_POS               30
#define  EP_QUEUE_HEAD_ZLT_SEL                0x20000000
#define  EP_QUEUE_HEAD_MAX_PKT_LEN_POS        16
#define  EP_QUEUE_HEAD_MAX_PKT_LEN(ep_info)   (((ep_info)>>16)&0x07ff)
#define  EP_QUEUE_HEAD_IOS                    0x00008000
#define  EP_QUEUE_HEAD_NEXT_TERMINATE         0x00000001
#define  EP_QUEUE_HEAD_IOC                    0x00008000
#define  EP_QUEUE_HEAD_MULTO                  0x00000C00
#define  EP_QUEUE_HEAD_STATUS_HALT	      0x00000040
#define  EP_QUEUE_HEAD_STATUS_ACTIVE          0x00000080
#define  EP_QUEUE_CURRENT_OFFSET_MASK         0x00000FFF
#define  EP_QUEUE_HEAD_NEXT_POINTER_MASK      0xFFFFFFE0
#define  EP_QUEUE_FRINDEX_MASK                0x000007FF
#define  EP_MAX_LENGTH_TRANSFER               0x4000



/* Endpoint Transfer Descriptor bit Masks */
#define  DTD_NEXT_TERMINATE                   0x00000001
#define  DTD_IOC                              0x00008000
#define  DTD_STATUS_ACTIVE                    0x00000080
#define  DTD_STATUS_HALTED                    0x00000040
#define  DTD_STATUS_DATA_BUFF_ERR             0x00000020
#define  DTD_STATUS_TRANSACTION_ERR           0x00000008
#define  DTD_RESERVED_FIELDS                  0x80007300
#define  DTD_ADDR_MASK                        0xFFFFFFE0
#define  DTD_PACKET_SIZE                      0x7FFF0000
#define  DTD_LENGTH_BIT_POS                   16
#define  DTD_ERROR_MASK                       (DTD_STATUS_HALTED | \
						DTD_STATUS_DATA_BUFF_ERR | \
						DTD_STATUS_TRANSACTION_ERR)
/* Alignment requirements; must be a power of two */
#define DTD_ALIGNMENT				0x80
#define QH_ALIGNMENT				2048
#define QH_OFFSET				0x1000

/* Controller dma boundary */
#define UDC_DMA_BOUNDARY			0x1000

#define REQ_UNCOMPLETE			1

#define EP_DIR_IN	1
#define EP_DIR_OUT	0

/*
 * Endpoint Queue Head data struct
 * Rem: all the variables of qh are LittleEndian Mode
 * and NEXT_POINTER_MASK should operate on a LittleEndian, Phy Addr
 */
struct ep_queue_head {
	u32 max_pkt_length; /* Mult(31-30), Zlt(29), Max Pkt len and IOS(15) */
	u32 curr_dtd_ptr;	/* Current dTD Pointer(31-5) */
	u32 next_dtd_ptr;	/* Next dTD Pointer(31-5), T(0) */
	u32 size_ioc_int_sts;	/* Total bytes (30-16), IOC (15),
				   MultO(11-10), STS (7-0)	*/
	u32 buff_ptr0;		/* Buffer pointer Page 0 (31-12) */
	u32 buff_ptr1;		/* Buffer pointer Page 1 (31-12) */
	u32 buff_ptr2;		/* Buffer pointer Page 2 (31-12) */
	u32 buff_ptr3;		/* Buffer pointer Page 3 (31-12) */
	u32 buff_ptr4;		/* Buffer pointer Page 4 (31-12) */
	u32 res1;
	u8 setup_buffer[8]; /* Setup data 8 bytes */
	u32 res2[4];
};

/* Endpoint Transfer Descriptor data struct */
/* Rem: all the variables of td are LittleEndian Mode */
struct ep_td_struct {
	u32 next_td_ptr;	/* Next TD pointer(31-5), T(0) set
				   indicate invalid */
	u32 size_ioc_sts;	/* Total bytes (30-16), IOC (15),
				   MultO(11-10), STS (7-0)	*/
	u32 buff_ptr0;		/* Buffer pointer Page 0 */
	u32 buff_ptr1;		/* Buffer pointer Page 1 */
	u32 buff_ptr2;		/* Buffer pointer Page 2 */
	u32 buff_ptr3;		/* Buffer pointer Page 3 */
	u32 buff_ptr4;		/* Buffer pointer Page 4 */
	u32 res;
	/* 32 bytes */
	dma_addr_t td_dma;	/* dma address for this td */
	/* virtual address of next td specified in next_td_ptr */
	struct ep_td_struct *next_td_virt;
};


struct tegra_req {
	struct usb_request req;
	struct list_head queue;
	/* ep_queue() func will add
	   a request->queue into a udc_ep->queue 'd tail */
	struct tegra_ep *ep;
	unsigned mapped:1;

	struct ep_td_struct *head, *tail;	/* For dTD List
						   cpu endian Virtual addr */
	unsigned int dtd_count;
};

struct tegra_ep {
	struct usb_ep ep;
	struct list_head queue;
	struct tegra_udc *udc;
	struct ep_queue_head *qh;
	const struct usb_endpoint_descriptor *desc;
	struct usb_gadget *gadget;
	struct ep_td_struct *last_td;
	int last_dtd_count;

	char name[14];
	unsigned stopped:1;
};

enum tegra_connect_type {
	CONNECT_TYPE_NONE,
	CONNECT_TYPE_SDP,
	CONNECT_TYPE_DCP,
	CONNECT_TYPE_CDP,
	CONNECT_TYPE_NV_CHARGER,
	CONNECT_TYPE_NON_STANDARD_CHARGER
};

struct tegra_udc {
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;
	struct completion *done;	/* to make sure release() is done */
	struct tegra_ep *eps;
	struct platform_device *pdev;
	struct tegra_usb_phy *phy;
	struct usb_ctrlrequest local_setup_buff;
	struct usb_phy *transceiver;
	struct ep_queue_head *ep_qh;	/* Endpoints Queue-Head */
	struct tegra_req *status_req;	/* ep0 status request */
	struct dma_pool *td_pool;	/* dma pool for DTD */
	struct regulator *vbus_reg;	/* regulator for drawing VBUS */
	/* delayed work for non standard charger detection */
	struct delayed_work non_std_charger_work;
	/* work for setting regulator current limit */
	struct work_struct current_work;
	/* work for boosting cpu frequency */
	struct work_struct boost_cpufreq_work;
	/* irq work for controlling the usb power */
	struct work_struct irq_work;
	enum tegra_connect_type connect_type;
	enum tegra_connect_type prev_connect_type;
	void __iomem *regs;
	size_t ep_qh_size;		/* size after alignment adjustment*/
	dma_addr_t ep_qh_dma;		/* dma address of QH */
	unsigned int max_ep;
	unsigned int irq;
	u32 max_pipes;		/* Device max pipes */
	u32 resume_state;	/* USB state to resume */
	u32 usb_state;		/* USB current state */
	u32 ep0_state;		/* Endpoint zero state */
	u32 ep0_dir;	/* Endpoint zero direction: USB_DIR_IN/USB_DIR_OUT */
	u8 device_address;	/* Device USB address */
	u32 current_limit;
	spinlock_t lock;
	struct mutex sync_lock;
	unsigned softconnect:1;
	unsigned vbus_active:1;
	unsigned stopped:1;
	unsigned remote_wakeup:1;
	unsigned selfpowered:1;
	bool has_hostpc;
	bool fence_read;
	bool support_pmu_vbus;
#ifdef CONFIG_EXTCON
	struct extcon_dev *edev;
#endif
};


#endif /* __TEGRA_UDC_H */
