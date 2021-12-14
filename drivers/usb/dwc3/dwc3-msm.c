// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/pm_runtime.h>
#include <linux/ratelimit.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/clk/qcom.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_qos.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/interconnect.h>
#include <linux/irq.h>
#include <linux/extcon.h>
#include <linux/reset.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/usb/role.h>
#include <linux/usb/redriver.h>
#include <linux/dma-iommu.h>
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
#include <soc/qcom/boot_stats.h>
#endif

#include "core.h"
#include "gadget.h"
#include "debug.h"
#include "xhci.h"

#define SDP_CONNECTION_CHECK_TIME 10000 /* in ms */

/* time out to wait for USB cable status notification (in ms)*/
#define SM_INIT_TIMEOUT 30000

/* AHB2PHY register offsets */
#define PERIPH_SS_AHB2PHY_TOP_CFG 0x10

/* AHB2PHY read/write waite value */
#define ONE_READ_WRITE_WAIT 0x11

/* XHCI registers */
#define USB3_HCSPARAMS1		(0x4)
#define USB3_PORTSC		(0x420)

/**
 *  USB QSCRATCH Hardware registers
 *
 */
#define QSCRATCH_REG_OFFSET	(0x000F8800)
#define QSCRATCH_GENERAL_CFG	(QSCRATCH_REG_OFFSET + 0x08)
#define CGCTL_REG		(QSCRATCH_REG_OFFSET + 0x28)
#define PWR_EVNT_IRQ_STAT_REG    (QSCRATCH_REG_OFFSET + 0x58)
#define PWR_EVNT_IRQ_MASK_REG    (QSCRATCH_REG_OFFSET + 0x5C)
#define PWR_EVNT_IRQ_STAT_REG1   (QSCRATCH_REG_OFFSET + 0x1DC)

#define PWR_EVNT_POWERDOWN_IN_P3_MASK		BIT(2)
#define PWR_EVNT_POWERDOWN_OUT_P3_MASK		BIT(3)
#define PWR_EVNT_LPM_IN_L2_MASK			BIT(4)
#define PWR_EVNT_LPM_OUT_L2_MASK		BIT(5)
#define PWR_EVNT_LPM_OUT_RX_ELECIDLE_IRQ_MASK	BIT(12)
#define PWR_EVNT_LPM_OUT_L1_MASK		BIT(13)

/* QSCRATCH_GENERAL_CFG register bit offset */
#define PIPE_UTMI_CLK_SEL	BIT(0)
#define PIPE3_PHYSTATUS_SW	BIT(3)
#define PIPE_UTMI_CLK_DIS	BIT(8)

#define HS_PHY_CTRL_REG		(QSCRATCH_REG_OFFSET + 0x10)
#define UTMI_OTG_VBUS_VALID	BIT(20)
#define SW_SESSVLD_SEL		BIT(28)

#define SS_PHY_CTRL_REG		(QSCRATCH_REG_OFFSET + 0x30)
#define LANE0_PWR_PRESENT	BIT(24)

/* USB DBM Hardware registers */
#define DBM_REG_OFFSET		0xF8000

/* DBM_GEN_CFG */
#define DBM_EN_USB3		0x00000001

/* DBM_EP_CFG */
#define DBM_EN_EP		0x00000001
#define USB3_EPNUM		0x0000003E
#define DBM_BAM_PIPE_NUM	0x000000C0
#define DBM_PRODUCER		0x00000100
#define DBM_DISABLE_WB		0x00000200
#define DBM_INT_RAM_ACC		0x00000400

/* DBM_DATA_FIFO_SIZE */
#define DBM_DATA_FIFO_SIZE_MASK	0x0000ffff

/* DBM_GEVNTSIZ */
#define DBM_GEVNTSIZ_MASK	0x0000ffff

/* DBM_DBG_CNFG */
#define DBM_ENABLE_IOC_MASK	0x0000000f

/* DBM_SOFT_RESET */
#define DBM_SFT_RST_EP0		0x00000001
#define DBM_SFT_RST_EP1		0x00000002
#define DBM_SFT_RST_EP2		0x00000004
#define DBM_SFT_RST_EP3		0x00000008
#define DBM_SFT_RST_EPS_MASK	0x0000000F
#define DBM_SFT_RST_MASK	0x80000000
#define DBM_EN_MASK		0x00000002

/* DBM TRB configurations */
#define DBM_TRB_BIT		0x80000000
#define DBM_TRB_DATA_SRC	0x40000000
#define DBM_TRB_DMA		0x20000000
#define DBM_TRB_EP_NUM(ep)	(ep<<24)

/* GSI related registers */
#define GSI_TRB_ADDR_BIT_53_MASK	(1 << 21)
#define GSI_TRB_ADDR_BIT_55_MASK	(1 << 23)

#define	GSI_GENERAL_CFG_REG(reg)	(QSCRATCH_REG_OFFSET + \
						reg[GENERAL_CFG_REG])
#define	GSI_RESTART_DBL_PNTR_MASK	BIT(20)
#define	GSI_CLK_EN_MASK			BIT(12)
#define	BLOCK_GSI_WR_GO_MASK		BIT(1)
#define	GSI_EN_MASK			BIT(0)

#define GSI_DBL_ADDR_L(reg, n)		(QSCRATCH_REG_OFFSET + \
						reg[DBL_ADDR_L] + (n*4))
#define GSI_DBL_ADDR_H(reg, n)		(QSCRATCH_REG_OFFSET + \
						reg[DBL_ADDR_H] + (n*4))
#define GSI_RING_BASE_ADDR_L(reg, n)	(QSCRATCH_REG_OFFSET + \
						reg[RING_BASE_ADDR_L] + (n*4))
#define GSI_RING_BASE_ADDR_H(reg, n)	(QSCRATCH_REG_OFFSET + \
						reg[RING_BASE_ADDR_H] + (n*4))

#define	GSI_IF_STS(reg)			(QSCRATCH_REG_OFFSET + reg[IF_STS])
#define	GSI_WR_CTRL_STATE_MASK	BIT(15)

#define DWC3_GEVNTCOUNT_EVNTINTRPTMASK		(1 << 31)
#define DWC3_GEVNTADRHI_EVNTADRHI_GSI_EN(n)	(n << 22)
#define DWC3_GEVNTADRHI_EVNTADRHI_GSI_IDX(n)	(n << 16)
#define DWC3_GEVENT_TYPE_GSI			0x3

/* BAM pipe mask */
#define MSM_PIPE_ID_MASK	(0x1F)

enum dbm_reg {
	DBM_EP_CFG,
	DBM_DATA_FIFO,
	DBM_DATA_FIFO_SIZE,
	DBM_DATA_FIFO_EN,
	DBM_GEVNTADR,
	DBM_GEVNTSIZ,
	DBM_DBG_CNFG,
	DBM_HW_TRB0_EP,
	DBM_HW_TRB1_EP,
	DBM_HW_TRB2_EP,
	DBM_HW_TRB3_EP,
	DBM_PIPE_CFG,
	DBM_DISABLE_UPDXFER,
	DBM_SOFT_RESET,
	DBM_GEN_CFG,
	DBM_GEVNTADR_LSB,
	DBM_GEVNTADR_MSB,
	DBM_DATA_FIFO_LSB,
	DBM_DATA_FIFO_MSB,
	DBM_DATA_FIFO_ADDR_EN,
	DBM_DATA_FIFO_SIZE_EN,
};

enum charger_detection_type {
	REMOTE_PROC,
	IIO,
	PSY,
};

struct dbm_reg_data {
	u32 offset;
	unsigned int ep_mult;
};

#define DBM_1_4_NUM_EP		4
#define DBM_1_5_NUM_EP		8

static const struct dbm_reg_data dbm_1_4_regtable[] = {
	[DBM_EP_CFG]		= { 0x0000, 0x4 },
	[DBM_DATA_FIFO]		= { 0x0010, 0x4 },
	[DBM_DATA_FIFO_SIZE]	= { 0x0020, 0x4 },
	[DBM_DATA_FIFO_EN]	= { 0x0030, 0x0 },
	[DBM_GEVNTADR]		= { 0x0034, 0x0 },
	[DBM_GEVNTSIZ]		= { 0x0038, 0x0 },
	[DBM_DBG_CNFG]		= { 0x003C, 0x0 },
	[DBM_HW_TRB0_EP]	= { 0x0040, 0x4 },
	[DBM_HW_TRB1_EP]	= { 0x0050, 0x4 },
	[DBM_HW_TRB2_EP]	= { 0x0060, 0x4 },
	[DBM_HW_TRB3_EP]	= { 0x0070, 0x4 },
	[DBM_PIPE_CFG]		= { 0x0080, 0x0 },
	[DBM_SOFT_RESET]	= { 0x0084, 0x0 },
	[DBM_GEN_CFG]		= { 0x0088, 0x0 },
	[DBM_GEVNTADR_LSB]	= { 0x0098, 0x0 },
	[DBM_GEVNTADR_MSB]	= { 0x009C, 0x0 },
	[DBM_DATA_FIFO_LSB]	= { 0x00A0, 0x8 },
	[DBM_DATA_FIFO_MSB]	= { 0x00A4, 0x8 },
};

static const struct dbm_reg_data dbm_1_5_regtable[] = {
	[DBM_EP_CFG]		= { 0x0000, 0x4 },
	[DBM_DATA_FIFO]		= { 0x0280, 0x4 },
	[DBM_DATA_FIFO_SIZE]	= { 0x0080, 0x4 },
	[DBM_DATA_FIFO_EN]	= { 0x026C, 0x0 },
	[DBM_GEVNTADR]		= { 0x0270, 0x0 },
	[DBM_GEVNTSIZ]		= { 0x0268, 0x0 },
	[DBM_DBG_CNFG]		= { 0x0208, 0x0 },
	[DBM_HW_TRB0_EP]	= { 0x0220, 0x4 },
	[DBM_HW_TRB1_EP]	= { 0x0230, 0x4 },
	[DBM_HW_TRB2_EP]	= { 0x0240, 0x4 },
	[DBM_HW_TRB3_EP]	= { 0x0250, 0x4 },
	[DBM_PIPE_CFG]		= { 0x0274, 0x0 },
	[DBM_DISABLE_UPDXFER]	= { 0x0298, 0x0 },
	[DBM_SOFT_RESET]	= { 0x020C, 0x0 },
	[DBM_GEN_CFG]		= { 0x0210, 0x0 },
	[DBM_GEVNTADR_LSB]	= { 0x0260, 0x0 },
	[DBM_GEVNTADR_MSB]	= { 0x0264, 0x0 },
	[DBM_DATA_FIFO_LSB]	= { 0x0100, 0x8 },
	[DBM_DATA_FIFO_MSB]	= { 0x0104, 0x8 },
	[DBM_DATA_FIFO_ADDR_EN]	= { 0x0200, 0x0 },
	[DBM_DATA_FIFO_SIZE_EN]	= { 0x0204, 0x0 },
};

enum usb_gsi_reg {
	GENERAL_CFG_REG,
	DBL_ADDR_L,
	DBL_ADDR_H,
	RING_BASE_ADDR_L,
	RING_BASE_ADDR_H,
	IF_STS,
	GSI_REG_MAX,
};

struct dwc3_msm_req_complete {
	struct list_head list_item;
	struct usb_request *req;
	void (*orig_complete)(struct usb_ep *ep,
			      struct usb_request *req);
};

enum dwc3_drd_state {
	DRD_STATE_UNDEFINED = 0,

	DRD_STATE_IDLE,
	DRD_STATE_PERIPHERAL,
	DRD_STATE_PERIPHERAL_SUSPEND,

	DRD_STATE_HOST_IDLE,
	DRD_STATE_HOST,
};

static const char *const state_names[] = {
	[DRD_STATE_UNDEFINED] = "undefined",
	[DRD_STATE_IDLE] = "idle",
	[DRD_STATE_PERIPHERAL] = "peripheral",
	[DRD_STATE_PERIPHERAL_SUSPEND] = "peripheral_suspend",
	[DRD_STATE_HOST_IDLE] = "host_idle",
	[DRD_STATE_HOST] = "host",
};

static const char *dwc3_drd_state_string(enum dwc3_drd_state state)
{
	if (state < 0 || state >= ARRAY_SIZE(state_names))
		return "UNKNOWN";

	return state_names[state];
}

enum dwc3_id_state {
	DWC3_ID_GROUND = 0,
	DWC3_ID_FLOAT,
};

enum msm_usb_irq {
	HS_PHY_IRQ,
	PWR_EVNT_IRQ,
	DP_HS_PHY_IRQ,
	DM_HS_PHY_IRQ,
	SS_PHY_IRQ,
	DP_HS_PHY_IRQ_1,
	DM_HS_PHY_IRQ_1,
	SS_PHY_IRQ_1,
	USB_MAX_IRQ
};

enum icc_paths {
	USB_DDR,
	USB_IPA,
	DDR_USB,
	USB_MAX_PATH
};

enum bus_vote {
	BUS_VOTE_NONE,
	BUS_VOTE_NOMINAL,
	BUS_VOTE_SVS,
	BUS_VOTE_MIN,
	BUS_VOTE_MAX
};

static const char * const icc_path_names[] = {
	"usb-ddr", "usb-ipa", "ddr-usb",
};

static struct {
	u32 avg, peak;
} bus_vote_values[BUS_VOTE_MAX][3] = {
	/* usb_ddr avg/peak, usb_ipa avg/peak, apps_usb avg/peak */
	[BUS_VOTE_NONE]    = { {0, 0}, {0, 0}, {0, 0} },
	[BUS_VOTE_NOMINAL] = { {1000000, 1250000}, {0, 2400}, {0, 40000}, },
	[BUS_VOTE_SVS]     = { {240000, 700000}, {0, 2400}, {0, 40000}, },
	[BUS_VOTE_MIN]     = { {1, 1}, {1, 1}, {1, 1}, },
};

struct usb_irq_info {
	const char	*name;
	unsigned long	irq_type;
	bool		required;
};

static const struct usb_irq_info usb_irq_info[USB_MAX_IRQ] = {
	{ "hs_phy_irq",
	  IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQ_TYPE_LEVEL_HIGH |
		 IRQF_EARLY_RESUME,
	  false,
	},
	{ "pwr_event_irq",
	  IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQ_TYPE_LEVEL_HIGH |
		 IRQF_EARLY_RESUME,
	  true,
	},
	{ "dp_hs_phy_irq",
	  IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_EARLY_RESUME,
	  false,
	},
	{ "dm_hs_phy_irq",
	  IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_EARLY_RESUME,
	  false,
	},
	{ "ss_phy_irq",
	  IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQ_TYPE_LEVEL_HIGH |
		 IRQF_EARLY_RESUME,
	  false,
	},
	{ "dp_hs_phy_irq1",
	  IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_EARLY_RESUME,
	  false,
	},
	{ "dm_hs_phy_irq1",
	  IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_EARLY_RESUME,
	  false,
	},
	{ "ss_phy_irq1",
	  IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQ_TYPE_LEVEL_HIGH |
		 IRQF_EARLY_RESUME,
	  false,
	},
};

struct usb_irq {
	int irq;
	bool enable;
};

static const char * const gsi_op_strings[] = {
	"EP_CONFIG", "START_XFER", "STORE_DBL_INFO",
	"ENABLE_GSI", "UPDATE_XFER", "RING_DB",
	"END_XFER", "GET_CH_INFO", "GET_XFER_IDX", "PREPARE_TRBS",
	"FREE_TRBS", "SET_CLR_BLOCK_DBL", "CHECK_FOR_SUSP",
	"EP_DISABLE" };

static const char * const usb_role_strings[] = {
	"NONE",
	"HOST",
	"DEVICE"
};

struct dwc3_msm;

struct extcon_nb {
	struct extcon_dev	*edev;
	struct dwc3_msm		*mdwc;
	int			idx;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
};

/* Input bits to state machine (mdwc->inputs) */

#define ID			0
#define B_SESS_VLD		1
#define B_SUSPEND		2
#define WAIT_FOR_LPM		3

#define PM_QOS_SAMPLE_SEC	2
#define PM_QOS_THRESHOLD	400

struct dwc3_msm {
	struct device *dev;
	void __iomem *base;
	void __iomem *ahb2phy_base;
	struct platform_device	*dwc3;
	struct dma_iommu_mapping *iommu_map;
	const struct usb_ep_ops *original_ep_ops[DWC3_ENDPOINTS_NUM];
	struct list_head req_complete_list;
	struct clk		*xo_clk;
	struct clk		*core_clk;
	long			core_clk_rate;
	long			core_clk_rate_hs;
	struct clk		*iface_clk;
	struct clk		*sleep_clk;
	struct clk		*utmi_clk;
	unsigned int		utmi_clk_rate;
	struct clk		*utmi_clk_src;
	struct clk		*bus_aggr_clk;
	struct clk		*noc_aggr_clk;
	struct clk		*noc_aggr_north_clk;
	struct clk		*noc_aggr_south_clk;
	struct clk		*noc_sys_clk;
	struct clk		*cfg_ahb_clk;
	struct reset_control	*core_reset;
	struct regulator	*dwc3_gdsc;

	struct usb_phy		*hs_phy, *ss_phy;
	struct usb_phy		*hs_phy1, *ss_phy1;

	const struct dbm_reg_data *dbm_reg_table;
	int			dbm_num_eps;
	u8			dbm_ep_num_mapping[DBM_1_5_NUM_EP];
	bool			dbm_reset_ep_after_lpm;
	bool			dbm_is_1p4;

	/* VBUS regulator for host mode */
	struct regulator	*vbus_reg;
	int			vbus_retry_count;
	bool			resume_pending;
	atomic_t                pm_suspended;
	struct usb_irq		wakeup_irq[USB_MAX_IRQ];
	struct work_struct	resume_work;
	struct work_struct	restart_usb_work;
	bool			in_restart;
	struct workqueue_struct *dwc3_wq;
	struct workqueue_struct *sm_usb_wq;
	struct delayed_work	sm_work;
	unsigned long		inputs;
	unsigned int		max_power;
	enum dwc3_drd_state	drd_state;
	enum bus_vote		default_bus_vote;
	enum bus_vote		override_bus_vote;
	struct icc_path		*icc_paths[3];
	struct power_supply	*usb_psy;
	struct iio_channel	*chg_type;
	struct work_struct	vbus_draw_work;
	bool			in_host_mode;
	bool			in_device_mode;
	enum usb_device_speed	max_rh_port_speed;
	unsigned int		tx_fifo_size;
	bool			check_eud_state;
	bool			vbus_active;
	bool			eud_active;
	bool			suspend;
	bool			use_pdc_interrupts;
	enum dwc3_id_state	id_state;
	unsigned long		use_pwr_event_for_wakeup;
#define PWR_EVENT_SS_WAKEUP		BIT(0)
#define PWR_EVENT_HS_WAKEUP		BIT(1)

	unsigned long		lpm_flags;
#define MDWC3_SS_PHY_SUSPEND		BIT(0)
#define MDWC3_ASYNC_IRQ_WAKE_CAPABILITY	BIT(1)
#define MDWC3_POWER_COLLAPSE		BIT(2)
#define MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP BIT(3)

	struct extcon_nb	*extcon;
	int			ext_idx;
	struct notifier_block	host_nb;

	atomic_t                in_p3;
	unsigned int		lpm_to_suspend_delay;
	u32			num_gsi_event_buffers;
	struct dwc3_event_buffer **gsi_ev_buff;
	int pm_qos_latency;
	struct pm_qos_request pm_qos_req_dma;
	struct delayed_work perf_vote_work;
	struct delayed_work sdp_check;
	struct mutex suspend_resume_mutex;

	enum usb_device_speed override_usb_speed;
	enum charger_detection_type apsd_source;
	u32			*gsi_reg;
	int			gsi_reg_offset_cnt;

	struct notifier_block	dpdm_nb;
	struct regulator	*dpdm_reg;

	u64			dummy_gsi_db;
	dma_addr_t		dummy_gsi_db_dma;

	struct usb_role_switch *role_switch;
	bool			ss_release_called;
	int			orientation_override;

	struct device_node	*ss_redriver_node;
	bool			dual_port;

	bool			perf_mode;
};

#define USB_HSPHY_3P3_VOL_MIN		3050000 /* uV */
#define USB_HSPHY_3P3_VOL_MAX		3300000 /* uV */
#define USB_HSPHY_3P3_HPM_LOAD		16000	/* uA */

#define USB_HSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_HSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_HSPHY_1P8_HPM_LOAD		19000	/* uA */

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */

static void dwc3_pwr_event_handler(struct dwc3_msm *mdwc);
static int dwc3_msm_gadget_vbus_draw(struct dwc3_msm *mdwc, unsigned int mA);
static void dwc3_msm_notify_event(struct dwc3 *dwc,
		enum dwc3_notify_event event, unsigned int value);

/**
 *
 * Read register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg(void __iomem *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	return val;
}

/**
 * Read register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg_field(void __iomem *base,
					  u32 offset,
					  const u32 mask)
{
	u32 shift = __ffs(mask);
	u32 val = ioread32(base + offset);

	val &= mask;		/* clear other bits */
	val >>= shift;
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg(void __iomem *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

/**
 * Write register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg_field(void __iomem *base, u32 offset,
					    const u32 mask, u32 val)
{
	u32 shift = __ffs(mask);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;		/* clear written bits */
	val = tmp | (val << shift);
	iowrite32(val, base + offset);

	/* Read back to make sure that previous write goes through */
	ioread32(base + offset);
}

/**
 * Write DBM register masked field with debug info.
 *
 * @dbm - DBM specific data
 * @reg - DBM register, used to look up the offset value
 * @ep - endpoint number
 * @mask - register bitmask.
 * @val - value to write.
 */
static inline void msm_dbm_write_ep_reg_field(struct dwc3_msm *mdwc,
					      enum dbm_reg reg, int ep,
					      const u32 mask, u32 val)
{
	u32 offset = DBM_REG_OFFSET + mdwc->dbm_reg_table[reg].offset +
			(mdwc->dbm_reg_table[reg].ep_mult * ep);
	dwc3_msm_write_reg_field(mdwc->base, offset, mask, val);
}

#define msm_dbm_write_reg_field(d, r, m, v) \
	msm_dbm_write_ep_reg_field(d, r, 0, m, v)

/**
 * Read DBM register with debug info.
 *
 * @dbm - DBM specific data
 * @reg - DBM register, used to look up the offset value
 * @ep - endpoint number
 *
 * @return u32
 */
static inline u32 msm_dbm_read_ep_reg(struct dwc3_msm *mdwc,
				      enum dbm_reg reg, int ep)
{
	u32 offset = DBM_REG_OFFSET + mdwc->dbm_reg_table[reg].offset +
			(mdwc->dbm_reg_table[reg].ep_mult * ep);
	return dwc3_msm_read_reg(mdwc->base, offset);
}

#define msm_dbm_read_reg(d, r) msm_dbm_read_ep_reg(d, r, 0)

/**
 * Write DBM register with debug info.
 *
 * @dbm - DBM specific data
 * @reg - DBM register, used to look up the offset value
 * @ep - endpoint number
 */
static inline void msm_dbm_write_ep_reg(struct dwc3_msm *mdwc, enum dbm_reg reg,
					int ep, u32 val)
{
	u32 offset = DBM_REG_OFFSET + mdwc->dbm_reg_table[reg].offset +
			(mdwc->dbm_reg_table[reg].ep_mult * ep);
	dwc3_msm_write_reg(mdwc->base, offset, val);
}

#define msm_dbm_write_reg(d, r, v) msm_dbm_write_ep_reg(d, r, 0, v)

/**
 * Return DBM EP number according to usb endpoint number.
 */
static int find_matching_dbm_ep(struct dwc3_msm *mdwc, u8 usb_ep)
{
	int i;

	for (i = 0; i < mdwc->dbm_num_eps; i++)
		if (mdwc->dbm_ep_num_mapping[i] == usb_ep)
			return i;

	dev_dbg(mdwc->dev, "%s: No DBM EP matches USB EP %d\n",
			__func__, usb_ep);
	return -ENODEV; /* Not found */
}

/**
 * Return number of configured DBM endpoints.
 */
static int dbm_get_num_of_eps_configured(struct dwc3_msm *mdwc)
{
	int i;
	int count = 0;

	for (i = 0; i < min(mdwc->dbm_num_eps, DBM_1_5_NUM_EP); i++)
		if (mdwc->dbm_ep_num_mapping[i])
			count++;

	return count;
}

static bool dwc3_msm_is_ss_rhport_connected(struct dwc3_msm *mdwc)
{
	int i, num_ports;
	u32 reg;

	reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
	num_ports = HCS_MAX_PORTS(reg);

	for (i = 0; i < num_ports; i++) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_PORTSC + i*0x10);
		if ((reg & PORT_CONNECT) && DEV_SUPERSPEED_ANY(reg))
			return true;
	}

	return false;
}

static bool dwc3_msm_is_host_superspeed(struct dwc3_msm *mdwc)
{
	int i, num_ports;
	u32 reg;
	bool is_host_ss = false;

	reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
	num_ports = HCS_MAX_PORTS(reg);

	/*
	 * For single port controller, PORTSC register 1 is for SS bus, hence it
	 * maps to the only SSPHY. But for dual port controllers, PORTSC
	 * registers 2 and 3 are for SS busses 0 and 1 respectively. Hence,
	 * PORTSC register 2 maps to SSPHY0 and 3 maps to SSPHY1. Handle the
	 * flag setting using the below logic which will also maintain single
	 * port core.
	 *       num_ports = 2 (or) 4 (depending on no. of ports)
	 *   ->num_ports/2 = 1 (or) 2
	 * So, for single port core, i % (num_ports/2) = 0, hence we will always
	 * be updating SSPHY0 flags.
	 * For dual port, i % (num_ports/2) = 0 for port 0 (or) 1 for port 1.
	 * Hence we update SSPHY0 for reg 0 and 2; and SSPHY1 for reg 1 and 3.
	 */
	for (i = 0; i < num_ports; i++) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_PORTSC + i*0x10);
		if ((reg & PORT_PE) && DEV_SUPERSPEED_ANY(reg)) {
			is_host_ss = true;
			if (i % (num_ports / 2))
				mdwc->ss_phy1->flags |= DEVICE_IN_SS_MODE;
			else
				mdwc->ss_phy->flags |= DEVICE_IN_SS_MODE;
		}
	}

	return is_host_ss;
}

static inline bool dwc3_msm_is_dev_superspeed(struct dwc3_msm *mdwc)
{
	u8 speed;

	speed = dwc3_msm_read_reg(mdwc->base, DWC3_DSTS) & DWC3_DSTS_CONNECTSPD;
	if ((speed == DWC3_DSTS_SUPERSPEED) ||
			(speed == DWC3_DSTS_SUPERSPEED_PLUS)) {
		mdwc->ss_phy->flags |= DEVICE_IN_SS_MODE;
		return true;
	}

	return false;
}

static inline bool dwc3_msm_is_superspeed(struct dwc3_msm *mdwc)
{
	if (mdwc->in_host_mode)
		return dwc3_msm_is_host_superspeed(mdwc);

	return dwc3_msm_is_dev_superspeed(mdwc);
}

static int dwc3_msm_dbm_disable_updxfer(struct dwc3 *dwc, u8 usb_ep)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	u32 data;
	int dbm_ep;

	dev_dbg(mdwc->dev, "%s\n", __func__);

	dbm_ep = find_matching_dbm_ep(mdwc, usb_ep);
	if (dbm_ep < 0)
		return dbm_ep;

	data = msm_dbm_read_reg(mdwc, DBM_DISABLE_UPDXFER);
	data |= (0x1 << dbm_ep);
	msm_dbm_write_reg(mdwc, DBM_DISABLE_UPDXFER, data);

	return 0;
}

#if IS_ENABLED(CONFIG_USB_DWC3_GADGET) || IS_ENABLED(CONFIG_USB_DWC3_DUAL_ROLE)
/**
 * Configure the DBM with the BAM's data fifo.
 * This function is called by the USB BAM Driver
 * upon initialization.
 *
 * @ep - pointer to usb endpoint.
 * @addr - address of data fifo.
 * @size - size of data fifo.
 *
 */
int msm_data_fifo_config(struct usb_ep *ep, unsigned long addr,
			 u32 size, u8 dbm_ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	u32 lo = lower_32_bits(addr);
	u32 hi = upper_32_bits(addr);

	dev_dbg(mdwc->dev, "%s\n", __func__);

	if (dbm_ep >= DBM_1_5_NUM_EP) {
		dev_err(mdwc->dev, "Invalid DBM EP num:%d\n", dbm_ep);
		return -EINVAL;
	}

	mdwc->dbm_ep_num_mapping[dbm_ep] = dep->number;

	if (!mdwc->dbm_is_1p4 || sizeof(addr) > sizeof(u32)) {
		msm_dbm_write_ep_reg(mdwc, DBM_DATA_FIFO_LSB, dbm_ep, lo);
		msm_dbm_write_ep_reg(mdwc, DBM_DATA_FIFO_MSB, dbm_ep, hi);
	} else {
		msm_dbm_write_ep_reg(mdwc, DBM_DATA_FIFO, dbm_ep, addr);
	}

	msm_dbm_write_ep_reg_field(mdwc, DBM_DATA_FIFO_SIZE, dbm_ep,
		DBM_DATA_FIFO_SIZE_MASK, size);

	return 0;
}
EXPORT_SYMBOL(msm_data_fifo_config);

static int dbm_ep_unconfig(struct dwc3_msm *mdwc, u8 usb_ep);

/**
 * Configure the DBM with the USB3 core event buffer.
 * This function is called by the SNPS UDC upon initialization.
 *
 * @addr - address of the event buffer.
 * @size - size of the event buffer.
 *
 */
static int dbm_event_buffer_config(struct dwc3_msm *mdwc, u32 addr_lo,
				   u32 addr_hi, int size)
{
	dev_dbg(mdwc->dev, "Configuring event buffer\n");

	if (size < 0) {
		dev_err(mdwc->dev, "Invalid size %d\n", size);
		return -EINVAL;
	}

	/* In case event buffer is already configured, Do nothing. */
	if (msm_dbm_read_reg(mdwc, DBM_GEVNTSIZ))
		return 0;

	if (!mdwc->dbm_is_1p4 || sizeof(phys_addr_t) > sizeof(u32)) {
		msm_dbm_write_reg(mdwc, DBM_GEVNTADR_LSB, addr_lo);
		msm_dbm_write_reg(mdwc, DBM_GEVNTADR_MSB, addr_hi);
	} else {
		msm_dbm_write_reg(mdwc, DBM_GEVNTADR, addr_lo);
	}

	msm_dbm_write_reg_field(mdwc, DBM_GEVNTSIZ, DBM_GEVNTSIZ_MASK, size);

	return 0;
}

/**
 * Cleanups for msm endpoint on request complete.
 *
 * Also call original request complete.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 *
 * @return int - 0 on success, negative on error.
 */
static void dwc3_msm_req_complete_func(struct usb_ep *ep,
				       struct usb_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_msm_req_complete *req_complete = NULL;

	/* Find original request complete function and remove it from list */
	list_for_each_entry(req_complete, &mdwc->req_complete_list, list_item) {
		if (req_complete->req == request)
			break;
	}
	if (!req_complete || req_complete->req != request) {
		dev_err(dep->dwc->dev, "%s: could not find the request\n",
					__func__);
		return;
	}
	list_del(&req_complete->list_item);

	/*
	 * Release another one TRB to the pool since DBM queue took 2 TRBs
	 * (normal and link), and the dwc3/gadget.c :: dwc3_gadget_giveback
	 * released only one.
	 */
	dep->trb_dequeue++;

	/* Unconfigure dbm ep */
	dbm_ep_unconfig(mdwc, dep->number);

	/*
	 * If this is the last endpoint we unconfigured, than reset also
	 * the event buffers; unless unconfiguring the ep due to lpm,
	 * in which case the event buffer only gets reset during the
	 * block reset.
	 */
	if (dbm_get_num_of_eps_configured(mdwc) == 0 &&
		!mdwc->dbm_reset_ep_after_lpm)
		dbm_event_buffer_config(mdwc, 0, 0, 0);

	/*
	 * Call original complete function, notice that dwc->lock is already
	 * taken by the caller of this function (dwc3_gadget_giveback()).
	 */
	request->complete = req_complete->orig_complete;
	if (request->complete)
		request->complete(ep, request);

	kfree(req_complete);
}

/**
 * Reset the DBM endpoint which is linked to the given USB endpoint.
 * This function is called by the function driver upon events
 * such as transfer aborting, USB re-enumeration and USB
 * disconnection.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - 0 on success, negative on error.
 */
int msm_dwc3_reset_dbm_ep(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	int dbm_ep;

	dbm_ep = find_matching_dbm_ep(mdwc, dep->number);
	if (dbm_ep < 0)
		return dbm_ep;

	dev_dbg(mdwc->dev, "Resetting endpoint %d, DBM ep %d\n", dep->number,
			dbm_ep);

	/* Reset the dbm endpoint */
	msm_dbm_write_reg_field(mdwc, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS_MASK & 1 << dbm_ep, 1);

	/*
	 * The necessary delay between asserting and deasserting the dbm ep
	 * reset is based on the number of active endpoints. If there is more
	 * than one endpoint, a 1 msec delay is required. Otherwise, a shorter
	 * delay will suffice.
	 */
	if (dbm_get_num_of_eps_configured(mdwc) > 1)
		usleep_range(1000, 1200);
	else
		udelay(10);

	msm_dbm_write_reg_field(mdwc, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS_MASK & 1 << dbm_ep, 0);

	return 0;
}
EXPORT_SYMBOL(msm_dwc3_reset_dbm_ep);

/**
 * Helper function.
 * See the header of the dwc3_msm_ep_queue function.
 *
 * @dwc3_ep - pointer to dwc3_ep instance.
 * @req - pointer to dwc3_request instance.
 *
 * @return int - 0 on success, negative on error.
 */
static int __dwc3_msm_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3_trb *trb;
	struct dwc3_trb *trb_link;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd;
	int ret = 0;

	/* We push the request to the dep->started_list list to indicate that
	 * this request is issued with start transfer. The request will be out
	 * from this list in 2 cases. The first is that the transfer will be
	 * completed (not if the transfer is endless using a circular TRBs with
	 * with link TRB). The second case is an option to do stop stransfer,
	 * this can be initiated by the function driver when calling dequeue.
	 */
	req->status = DWC3_REQUEST_STATUS_STARTED;
	list_add_tail(&req->list, &dep->started_list);

	/* First, prepare a normal TRB, point to the fake buffer */
	trb = &dep->trb_pool[dep->trb_enqueue];
	if (++dep->trb_enqueue == (DWC3_TRB_NUM - 1))
		dep->trb_enqueue = 0;
	memset(trb, 0, sizeof(*trb));

	req->trb = trb;
	req->num_trbs = 1;
	trb->bph = DBM_TRB_BIT | DBM_TRB_DMA | DBM_TRB_EP_NUM(dep->number);
	trb->size = DWC3_TRB_SIZE_LENGTH(req->request.length);
	trb->ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_HWO |
		DWC3_TRB_CTRL_CHN | (req->direction ? 0 : DWC3_TRB_CTRL_CSP);
	req->trb_dma = dwc3_trb_dma_offset(dep, trb);

	/* Second, prepare a Link TRB that points to the first TRB*/
	trb_link = &dep->trb_pool[dep->trb_enqueue];
	if (++dep->trb_enqueue == (DWC3_TRB_NUM - 1))
		dep->trb_enqueue = 0;
	memset(trb_link, 0, sizeof(*trb_link));

	trb_link->bpl = lower_32_bits(req->trb_dma);
	trb_link->bph = upper_32_bits(req->trb_dma) | DBM_TRB_BIT |
			DBM_TRB_DMA | DBM_TRB_EP_NUM(dep->number);
	trb_link->size = 0;
	trb_link->ctrl = DWC3_TRBCTL_LINK_TRB | DWC3_TRB_CTRL_HWO;

	/*
	 * Now start the transfer
	 */
	memset(&params, 0, sizeof(params));
	params.param0 = upper_32_bits(req->trb_dma); /* TDAddr High */
	params.param1 = lower_32_bits(req->trb_dma); /* DAddr Low */

	/* DBM requires IOC to be set */
	cmd = DWC3_DEPCMD_STARTTRANSFER | DWC3_DEPCMD_CMDIOC;
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0) {
		dev_dbg(dep->dwc->dev,
			"%s: failed to send STARTTRANSFER command\n",
			__func__);

		list_del(&req->list);
		return ret;
	}

	return ret;
}

/**
 * Queue a usb request to the DBM endpoint.
 * This function should be called after the endpoint
 * was enabled by the ep_enable.
 *
 * This function prepares special structure of TRBs which
 * is familiar with the DBM HW, so it will possible to use
 * this endpoint in DBM mode.
 *
 * The TRBs prepared by this function, is one normal TRB
 * which point to a fake buffer, followed by a link TRB
 * that points to the first TRB.
 *
 * The API of this function follow the regular API of
 * usb_ep_queue (see usb_ep_ops in include/linuk/usb/gadget.h).
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to usb_request instance.
 * @gfp_flags - possible flags.
 *
 * @return int - 0 on success, negative on error.
 */
static int dwc3_msm_ep_queue(struct usb_ep *ep,
			     struct usb_request *request, gfp_t gfp_flags)
{
	struct dwc3_request *req = to_dwc3_request(request);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_msm_req_complete *req_complete;
	unsigned long flags;
	int ret = 0, size;

	/*
	 * We must obtain the lock of the dwc3 core driver,
	 * including disabling interrupts, so we will be sure
	 * that we are the only ones that configure the HW device
	 * core and ensure that we queuing the request will finish
	 * as soon as possible so we will release back the lock.
	 */
	spin_lock_irqsave(&dwc->lock, flags);
	if (!dep->endpoint.desc) {
		dev_err(mdwc->dev,
			"%s: trying to queue request %pK to disabled ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (!mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] was unconfigured as msm endpoint\n",
			ep->name, dep->number);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (!request) {
		dev_err(mdwc->dev, "%s: request is NULL\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	/* HW restriction regarding TRB size (8KB) */
	if (req->request.length < 0x2000) {
		dev_err(mdwc->dev, "%s: Min TRB size is 8KB\n", __func__);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}

	if (dep->number == 0 || dep->number == 1) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK to ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (dep->trb_dequeue != dep->trb_enqueue
					|| !list_empty(&dep->pending_list)
					|| !list_empty(&dep->started_list)) {
		dev_err(mdwc->dev,
			"%s: trying to queue dbm request %pK tp ep %s\n",
			__func__, request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}
	dep->trb_dequeue = 0;
	dep->trb_enqueue = 0;

	/*
	 * Override req->complete function, but before doing that,
	 * store it's original pointer in the req_complete_list.
	 */
	req_complete = kzalloc(sizeof(*req_complete), gfp_flags);

	if (!req_complete) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ENOMEM;
	}

	req_complete->req = request;
	req_complete->orig_complete = request->complete;
	list_add_tail(&req_complete->list_item, &mdwc->req_complete_list);
	request->complete = dwc3_msm_req_complete_func;

	dev_vdbg(dwc->dev, "%s: queuing request %pK to ep %s length %d\n",
			__func__, request, ep->name, request->length);
	size = dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTSIZ(0));
	dbm_event_buffer_config(mdwc,
		dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTADRLO(0)),
		dwc3_msm_read_reg(mdwc->base, DWC3_GEVNTADRHI(0)),
		DWC3_GEVNTSIZ_SIZE(size));

	ret = __dwc3_msm_ep_queue(dep, req);
	if (ret < 0) {
		dev_err(mdwc->dev,
			"error %d after calling __dwc3_msm_ep_queue\n", ret);
		goto err;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	msm_dbm_write_reg_field(mdwc, DBM_GEN_CFG, DBM_EN_USB3,
			dwc3_msm_is_dev_superspeed(mdwc) ? 1 : 0);

	return 0;

err:
	spin_unlock_irqrestore(&dwc->lock, flags);
	kfree(req_complete);
	return ret;
}

/**
 * Returns XferRscIndex for the EP. This is stored at StartXfer GSI EP OP
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - XferRscIndex
 */
static inline int gsi_get_xfer_index(struct usb_ep *ep)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);

	return dep->resource_index;
}

/**
 * Fills up the GSI channel information needed in call to IPA driver
 * for GSI channel creation.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @ch_info - output parameter with requested channel info
 */
static void gsi_get_channel_info(struct usb_ep *ep,
			struct gsi_channel_info *ch_info)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	int last_trb_index = 0;
	struct dwc3	*dwc = dep->dwc;
	struct usb_gsi_request *request = ch_info->ch_req;

	/* Provide physical USB addresses for DEPCMD and GEVENTCNT registers */
	ch_info->depcmd_low_addr = (u32)(dwc->reg_phys +
				DWC3_DEP_BASE(dep->number) + DWC3_DEPCMD);

	ch_info->depcmd_hi_addr = 0;

	ch_info->xfer_ring_base_addr = dwc3_trb_dma_offset(dep,
							&dep->trb_pool[0]);
	/* Convert to multipled of 1KB */
	ch_info->const_buffer_size = request->buf_len/1024;

	/* IN direction */
	if (dep->direction) {
		/*
		 * Multiply by size of each TRB for xfer_ring_len in bytes.
		 * 2n + 2 TRBs as per GSI h/w requirement. n Xfer TRBs + 1
		 * extra Xfer TRB followed by n ZLP TRBs + 1 LINK TRB.
		 */
		ch_info->xfer_ring_len = (2 * request->num_bufs + 2) * 0x10;
		last_trb_index = 2 * request->num_bufs + 2;
	} else { /* OUT direction */
		/*
		 * Multiply by size of each TRB for xfer_ring_len in bytes.
		 * n + 1 TRBs as per GSI h/w requirement. n Xfer TRBs + 1
		 * LINK TRB.
		 */
		ch_info->xfer_ring_len = (request->num_bufs + 2) * 0x10;
		last_trb_index = request->num_bufs + 2;
	}

	/* Store last 16 bits of LINK TRB address as per GSI hw requirement */
	ch_info->last_trb_addr = (dwc3_trb_dma_offset(dep,
			&dep->trb_pool[last_trb_index - 1]) & 0x0000FFFF);
	ch_info->gevntcount_low_addr = (u32)(dwc->reg_phys +
			DWC3_GEVNTCOUNT(request->ep_intr_num));
	ch_info->gevntcount_hi_addr = 0;

	dev_dbg(dwc->dev,
	"depcmd_laddr=%x last_trb_addr=%x gevtcnt_laddr=%x gevtcnt_haddr=%x",
		ch_info->depcmd_low_addr, ch_info->last_trb_addr,
		ch_info->gevntcount_low_addr, ch_info->gevntcount_hi_addr);
}

/**
 * Perform StartXfer on GSI EP. Stores XferRscIndex.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 * @return int - 0 on success
 */
static int gsi_startxfer_for_ep(struct usb_ep *ep, struct usb_gsi_request *req)
{
	int ret;
	struct dwc3_gadget_ep_cmd_params params;
	u32				cmd;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;

	memset(&params, 0, sizeof(params));
	params.param0 = GSI_TRB_ADDR_BIT_53_MASK | GSI_TRB_ADDR_BIT_55_MASK;
	params.param0 |= upper_32_bits(dwc3_trb_dma_offset(dep,
						&dep->trb_pool[0])) & 0xffff;
	params.param0 |= (req->ep_intr_num << 16);
	params.param1 = lower_32_bits(dwc3_trb_dma_offset(dep,
						&dep->trb_pool[0]));
	cmd = DWC3_DEPCMD_STARTTRANSFER;
	cmd |= DWC3_DEPCMD_PARAM(0);
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);

	if (ret < 0)
		dev_dbg(dwc->dev, "Fail StrtXfr on GSI EP#%d\n", dep->number);
	dev_dbg(dwc->dev, "XferRsc = %x", dep->resource_index);
	return ret;
}

/**
 * Store Ring Base and Doorbell Address for GSI EP
 * for GSI channel creation.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - USB GSI request to get Doorbell address obtained from IPA driver
 */
static void gsi_store_ringbase_dbl_info(struct usb_ep *ep,
			struct usb_gsi_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	int n = request->ep_intr_num - 1;

	dwc3_msm_write_reg(mdwc->base, GSI_RING_BASE_ADDR_L(mdwc->gsi_reg, n),
		lower_32_bits(dwc3_trb_dma_offset(dep, &dep->trb_pool[0])));
	dwc3_msm_write_reg(mdwc->base, GSI_RING_BASE_ADDR_H(mdwc->gsi_reg, n),
		upper_32_bits(dwc3_trb_dma_offset(dep, &dep->trb_pool[0])));

	dev_dbg(mdwc->dev, "Ring Base Addr %d: %x (LSB) %x (MSB)\n", n,
		lower_32_bits(dwc3_trb_dma_offset(dep, &dep->trb_pool[0])),
		upper_32_bits(dwc3_trb_dma_offset(dep, &dep->trb_pool[0])));

	if (request->mapped_db_reg_phs_addr_lsb &&
			dwc->sysdev != request->dev) {
		dma_unmap_resource(request->dev,
			request->mapped_db_reg_phs_addr_lsb,
			PAGE_SIZE, DMA_BIDIRECTIONAL, 0);
		request->mapped_db_reg_phs_addr_lsb = 0;
	}

	if (!request->mapped_db_reg_phs_addr_lsb) {
		request->mapped_db_reg_phs_addr_lsb =
			dma_map_resource(dwc->sysdev,
				(phys_addr_t)request->db_reg_phs_addr_lsb,
				PAGE_SIZE, DMA_BIDIRECTIONAL, 0);
		request->dev = dwc->sysdev;
		if (dma_mapping_error(dwc->sysdev,
				request->mapped_db_reg_phs_addr_lsb))
			dev_err(mdwc->dev, "mapping error for db_reg_phs_addr_lsb\n");
	}

	dev_dbg(mdwc->dev, "ep:%s dbl_addr_lsb:%x mapped_dbl_addr_lsb:%llx\n",
		ep->name, request->db_reg_phs_addr_lsb,
		(unsigned long long)request->mapped_db_reg_phs_addr_lsb);

	dbg_log_string("ep:%s dbl_addr_lsb:%x mapped_addr:%llx\n",
		ep->name, request->db_reg_phs_addr_lsb,
		(unsigned long long)request->mapped_db_reg_phs_addr_lsb);

	/*
	 * Replace dummy doorbell address with real one as IPA connection
	 * is setup now and GSI must be ready to handle doorbell updates.
	 */
	dwc3_msm_write_reg(mdwc->base, GSI_DBL_ADDR_H(mdwc->gsi_reg, n),
		upper_32_bits(request->mapped_db_reg_phs_addr_lsb));

	dwc3_msm_write_reg(mdwc->base, GSI_DBL_ADDR_L(mdwc->gsi_reg, n),
		lower_32_bits(request->mapped_db_reg_phs_addr_lsb));

	dev_dbg(mdwc->dev, "GSI DB Addr %d: %pad\n", n,
		&request->mapped_db_reg_phs_addr_lsb);
}

/**
 * Rings Doorbell for GSI Channel
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. This is used to pass in the
 * address of the GSI doorbell obtained from IPA driver
 */
static void gsi_ring_db(struct usb_ep *ep, struct usb_gsi_request *request)
{
	void __iomem *gsi_dbl_address_lsb;
	void __iomem *gsi_dbl_address_msb;
	dma_addr_t trb_dma;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3	*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	int num_trbs = (dep->direction) ? (2 * (request->num_bufs) + 2)
					: (request->num_bufs + 2);

	gsi_dbl_address_lsb = ioremap_nocache(request->db_reg_phs_addr_lsb,
				sizeof(u32));
	if (!gsi_dbl_address_lsb) {
		dev_err(mdwc->dev, "Failed to map GSI DBL address LSB 0x%x\n",
				request->db_reg_phs_addr_lsb);
		return;
	}

	gsi_dbl_address_msb = ioremap_nocache(request->db_reg_phs_addr_msb,
				sizeof(u32));
	if (!gsi_dbl_address_msb) {
		dev_err(mdwc->dev, "Failed to map GSI DBL address MSB 0x%x\n",
				request->db_reg_phs_addr_msb);
		iounmap(gsi_dbl_address_lsb);
		return;
	}

	trb_dma = dwc3_trb_dma_offset(dep, &dep->trb_pool[num_trbs-1]);
	dev_dbg(mdwc->dev, "Writing link TRB addr: %pad to %pK (lsb:%x msb:%x) for ep:%s\n",
		&trb_dma, gsi_dbl_address_lsb, request->db_reg_phs_addr_lsb,
		request->db_reg_phs_addr_msb, ep->name);

	dbg_log_string("ep:%s link TRB addr:%pad db_lsb:%pad db_msb:%pad\n",
		ep->name, &trb_dma, &request->db_reg_phs_addr_lsb,
		&request->db_reg_phs_addr_msb);

	writel_relaxed(lower_32_bits(trb_dma), gsi_dbl_address_lsb);
	writel_relaxed(upper_32_bits(trb_dma), gsi_dbl_address_msb);

	iounmap(gsi_dbl_address_lsb);
	iounmap(gsi_dbl_address_msb);
}

/**
 * Sets HWO bit for TRBs and performs UpdateXfer for OUT EP.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. Used to determine num of TRBs for OUT EP.
 *
 * @return int - 0 on success
 */
static int gsi_updatexfer_for_ep(struct usb_ep *ep,
					struct usb_gsi_request *request)
{
	int i;
	int ret;
	u32				cmd;
	int num_trbs = request->num_bufs + 1;
	struct dwc3_trb *trb;
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;

	for (i = 0; i < num_trbs - 1; i++) {
		trb = &dep->trb_pool[i];
		trb->ctrl |= DWC3_TRB_CTRL_HWO;
	}

	memset(&params, 0, sizeof(params));
	cmd = DWC3_DEPCMD_UPDATETRANSFER;
	cmd |= DWC3_DEPCMD_PARAM(dep->resource_index);
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0)
		dev_dbg(dwc->dev, "UpdateXfr fail on GSI EP#%d\n", dep->number);
	return ret;
}

#define TCM_BUF_SIZE	(16 * 1024)
#define TCM_BUF_NUM	8
#define TCM_MEM_REQ	(TCM_BUF_SIZE * TCM_BUF_NUM)

/* Using IOVA base address at end of USB IOVA address */
#define TCM_IOVA_BASE	0x9ffe0000
static void gsi_free_data_buffers(struct usb_ep *ep,
			struct usb_gsi_request *req)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;

	if (req->tcm_mem) {
		dbg_log_string("free tcm based buffer for ep:%s\n", dep->name);
		iommu_unmap(iommu_get_domain_for_dev(dwc->sysdev),
				TCM_IOVA_BASE, TCM_MEM_REQ);
		llcc_tcm_deactivate(req->tcm_mem);
		req->tcm_mem = NULL;
		req->dma = 0;
	} else {
		dma_free_coherent(dwc->sysdev,
			req->buf_len * req->num_bufs,
			req->buf_base_addr, req->dma);
	}
	req->buf_base_addr = NULL;
	sg_free_table(&req->sgt_data_buff);
}

static int gsi_allocate_data_buffers(struct usb_ep *ep,
			struct usb_gsi_request *req)
{
	size_t len = 0;
	int ret = 0;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct iommu_domain *usb_domain = NULL;

	/* Check need of using tcm for IN endpoint only */
	if (!(req->use_tcm_mem && dep->direction))
		goto normal_alloc;

	usb_domain = iommu_get_domain_for_dev(dwc->sysdev);
	if (usb_domain == NULL) {
		dev_err(dwc->dev, "No USB IOMMU domain\n");
		goto normal_alloc;
	}

	/* Validate USB IOVA range with TCM IOVA base */
	if (usb_domain->geometry.aperture_start <= TCM_IOVA_BASE &&
		usb_domain->geometry.aperture_end > TCM_IOVA_BASE) {
		dev_err(dwc->dev, "overlap IOVA: TCM:%x USB:(%x-%x)\n",
				TCM_IOVA_BASE,
				usb_domain->geometry.aperture_start,
				usb_domain->geometry.aperture_end);
		goto normal_alloc;
	}

	/* Check availability of TCM memory */
	req->tcm_mem = llcc_tcm_activate();
	if (IS_ERR_OR_NULL(req->tcm_mem)) {
		dev_err(dwc->dev, "can't use tcm_mem ep:%s err:%ld\n",
				dep->name, PTR_ERR(req->tcm_mem));
		req->tcm_mem = NULL;
		goto normal_alloc;
	}

	if (req->tcm_mem->mem_size < TCM_MEM_REQ) {
		dev_err(dwc->dev, "err:tcm_mem: req sz:(%d) > avail_sz(%d)\n",
				TCM_MEM_REQ, req->tcm_mem->mem_size);
		llcc_tcm_deactivate(req->tcm_mem);
		req->tcm_mem = NULL;
		goto normal_alloc;
	}

	len = TCM_MEM_REQ;
	ret = iommu_map(usb_domain, TCM_IOVA_BASE, req->tcm_mem->phys_addr, len,
			IOMMU_READ | IOMMU_WRITE | IOMMU_NOEXEC);
	if (ret) {
		dev_err(dwc->dev, "%s: can't map TCM mem, using DDR\n",
				dep->name);
		llcc_tcm_deactivate(req->tcm_mem);
		req->tcm_mem = NULL;
		goto normal_alloc;
	}

	req->buf_base_addr = (void __force *)req->tcm_mem->virt_addr;
	req->buf_len = TCM_BUF_SIZE;
	req->num_bufs = TCM_BUF_NUM;
	req->dma = TCM_IOVA_BASE;
	goto fill_sgtable;

normal_alloc:
	len = req->buf_len * req->num_bufs;
	req->buf_base_addr = dma_alloc_coherent(dwc->sysdev, len,
			&req->dma, GFP_KERNEL);
	if (!req->buf_base_addr)
		return -ENOMEM;

fill_sgtable:
	dma_get_sgtable(dwc->sysdev, &req->sgt_data_buff,
			req->buf_base_addr, req->dma, len);
	dbg_log_string("alloc buffer for ep:%s use_tcm_mem:%d mem_type:%s\n",
		dep->name, req->use_tcm_mem, req->tcm_mem ? "TCM" : "DDR");
	return 0;
}

/**
 * Allocates Buffers and TRBs. Configures TRBs for GSI EPs.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request.
 *
 * @return int - 0 on success
 */
static int gsi_prepare_trbs(struct usb_ep *ep, struct usb_gsi_request *req)
{
	int i = 0, ret;
	dma_addr_t buffer_addr;
	dma_addr_t trb0_dma;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3		*dwc = dep->dwc;
	struct dwc3_trb *trb;
	int num_trbs;
	struct scatterlist *sg;
	struct sg_table *sgt;

	ret = gsi_allocate_data_buffers(ep, req);
	if (ret) {
		dev_err(dep->dwc->dev, "failed to alloc TRB buffer for %s\n",
				dep->name);
		return ret;
	}

	buffer_addr = req->dma;
	/* Allocate and configure TRBs */
	num_trbs = (dep->direction) ? (2 * (req->num_bufs) + 2)
					: (req->num_bufs + 2);
	dep->trb_pool = dma_alloc_coherent(dwc->sysdev,
				num_trbs * sizeof(struct dwc3_trb),
				&dep->trb_pool_dma, GFP_KERNEL);
	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "failed to alloc trb dma pool for %s\n",
				dep->name);
		goto free_trb_buffer;
	}

	memset(dep->trb_pool, 0, num_trbs * sizeof(struct dwc3_trb));

	trb0_dma = dwc3_trb_dma_offset(dep, &dep->trb_pool[0]);

	dep->num_trbs = num_trbs;
	dma_get_sgtable(dwc->sysdev, &req->sgt_trb_xfer_ring, dep->trb_pool,
		dep->trb_pool_dma, num_trbs * sizeof(struct dwc3_trb));

	sgt = &req->sgt_trb_xfer_ring;
	dev_dbg(dwc->dev, "%s(): trb_pool:%pK trb_pool_dma:%lx\n",
		__func__, dep->trb_pool, (unsigned long)dep->trb_pool_dma);

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		dev_dbg(dwc->dev,
			"%i: page_link:%lx offset:%x length:%x address:%lx\n",
			i, sg->page_link, sg->offset, sg->length,
			(unsigned long)sg->dma_address);

	/* IN direction */
	if (dep->direction) {
		trb = &dep->trb_pool[0];
		/* Set up first n+1 TRBs for ZLPs */
		for (i = 0; i < req->num_bufs + 1; i++, trb++)
			trb->ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_IOC;

		/* Setup next n TRBs pointing to valid buffers */
		for (; i < num_trbs - 1; i++, trb++) {
			trb->bpl = lower_32_bits(buffer_addr);
			trb->bph = upper_32_bits(buffer_addr);
			trb->ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_IOC;
			buffer_addr += req->buf_len;
		}

		/* Set up the Link TRB at the end */
		trb->bpl = lower_32_bits(trb0_dma);
		trb->bph = upper_32_bits(trb0_dma) & 0xffff;
		trb->bph |= (1 << 23) | (1 << 21) | (req->ep_intr_num << 16);
		trb->ctrl = DWC3_TRBCTL_LINK_TRB | DWC3_TRB_CTRL_HWO;
	} else { /* OUT direction */
		/* Start ring with LINK TRB pointing to second entry */
		trb = &dep->trb_pool[0];
		trb->bpl = lower_32_bits(trb0_dma + sizeof(*trb));
		trb->bph = upper_32_bits(trb0_dma + sizeof(*trb));
		trb->ctrl = DWC3_TRBCTL_LINK_TRB;

		/* Setup next n-1 TRBs pointing to valid buffers */
		for (i = 1, trb++; i < num_trbs - 1; i++, trb++) {
			trb->bpl = lower_32_bits(buffer_addr);
			trb->bph = upper_32_bits(buffer_addr);
			trb->size = req->buf_len;
			buffer_addr += req->buf_len;
			trb->ctrl = DWC3_TRBCTL_NORMAL | DWC3_TRB_CTRL_IOC
				| DWC3_TRB_CTRL_CSP | DWC3_TRB_CTRL_ISP_IMI;
		}

		/* Set up the Link TRB at the end */
		trb->bpl = lower_32_bits(trb0_dma);
		trb->bph = upper_32_bits(trb0_dma) & 0xffff;
		trb->bph |= (1 << 23) | (1 << 21) | (req->ep_intr_num << 16);
		trb->ctrl = DWC3_TRBCTL_LINK_TRB | DWC3_TRB_CTRL_HWO;
	}

	dev_dbg(dwc->dev, "%s: Initialized TRB Ring for %s\n",
					__func__, dep->name);

	return 0;

free_trb_buffer:
	gsi_free_data_buffers(ep, req);
	return -ENOMEM;
}

/**
 * Frees TRBs and buffers for GSI EPs.
 *
 * @usb_ep - pointer to usb_ep instance.
 *
 */
static void gsi_free_trbs(struct usb_ep *ep, struct usb_gsi_request *req)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;

	if (!dep->gsi)
		return;

	/*  Free TRBs and TRB pool for EP */
	if (dep->trb_pool_dma) {
		dma_free_coherent(dwc->sysdev,
			dep->num_trbs * sizeof(struct dwc3_trb),
			dep->trb_pool,
			dep->trb_pool_dma);
		dep->trb_pool = NULL;
		dep->trb_pool_dma = 0;
	}
	sg_free_table(&req->sgt_trb_xfer_ring);

	/* free TRB buffers */
	gsi_free_data_buffers(ep, req);
}
/**
 * Configures GSI EPs. For GSI EPs we need to set interrupter numbers.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request.
 */
static void gsi_configure_ep(struct usb_ep *ep, struct usb_gsi_request *request)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3		*dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_gadget_ep_cmd_params params;
	const struct usb_endpoint_descriptor *desc = ep->desc;
	const struct usb_ss_ep_comp_descriptor *comp_desc = ep->comp_desc;
	int n = request->ep_intr_num - 1;
	u32 reg;
	int ret;

	/* setup dummy doorbell as IPA connection isn't setup yet */
	dwc3_msm_write_reg(mdwc->base, GSI_DBL_ADDR_H(mdwc->gsi_reg, n),
			upper_32_bits(mdwc->dummy_gsi_db_dma));
	dwc3_msm_write_reg(mdwc->base, GSI_DBL_ADDR_L(mdwc->gsi_reg, n),
			lower_32_bits(mdwc->dummy_gsi_db_dma));
	dev_dbg(mdwc->dev, "Dummy DB Addr %pK: %llx\n",
		&mdwc->dummy_gsi_db, mdwc->dummy_gsi_db_dma);

	memset(&params, 0x00, sizeof(params));

	/* Configure GSI EP */
	params.param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* Burst size is only needed in SuperSpeed mode */
	if (dwc->gadget.speed >= USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst - 1;

		params.param0 |= DWC3_DEPCFG_BURST_SIZE(burst);
	}

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params.param1 |= DWC3_DEPCFG_STREAM_CAPABLE
					| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	/* Set EP number */
	params.param1 |= DWC3_DEPCFG_EP_NUMBER(dep->number);

	/* Set interrupter number for GSI endpoints */
	params.param1 |= DWC3_DEPCFG_INT_NUM(request->ep_intr_num);

	/* Enable XferInProgress and XferComplete Interrupts */
	params.param1 |= DWC3_DEPCFG_XFER_COMPLETE_EN;
	params.param1 |= DWC3_DEPCFG_XFER_IN_PROGRESS_EN;
	params.param1 |= DWC3_DEPCFG_FIFO_ERROR_EN;
	/*
	 * We must use the lower 16 TX FIFOs even though
	 * HW might have more
	 */
	/* Remove FIFO Number for GSI EP*/
	if (dep->direction)
		params.param0 |= DWC3_DEPCFG_FIFO_NUMBER(dep->number >> 1);

	params.param0 |= DWC3_DEPCFG_ACTION_INIT;

	dev_dbg(mdwc->dev, "Set EP config to params = %x %x %x, for %s\n",
	params.param0, params.param1, params.param2, dep->name);

	dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETEPCONFIG, &params);

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		ret = dwc3_gadget_resize_tx_fifos(dwc, dep);
		if (ret)
			return;

		dep->endpoint.desc = desc;
		dep->endpoint.comp_desc = comp_desc;
		dep->type = usb_endpoint_type(desc);
		dep->flags |= DWC3_EP_ENABLED;
		reg = dwc3_msm_read_reg(mdwc->base, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_msm_write_reg(mdwc->base, DWC3_DALEPENA, reg);
	}

}

/**
 * Enables USB wrapper for GSI
 *
 * @usb_ep - pointer to usb_ep instance.
 */
static void gsi_enable(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dwc3_msm_write_reg_field(mdwc->base, GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
		GSI_CLK_EN_MASK, 1);
	dwc3_msm_write_reg_field(mdwc->base, GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
		GSI_RESTART_DBL_PNTR_MASK, 1);
	dwc3_msm_write_reg_field(mdwc->base, GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
		GSI_RESTART_DBL_PNTR_MASK, 0);
	dev_dbg(mdwc->dev, "%s: Enable GSI\n", __func__);
	dwc3_msm_write_reg_field(mdwc->base, GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
		GSI_EN_MASK, 1);
}

/**
 * Block or allow doorbell towards GSI
 *
 * @usb_ep - pointer to usb_ep instance.
 * @request - pointer to GSI request. In this case num_bufs is used as a bool
 * to set or clear the doorbell bit
 */
static void gsi_set_clear_dbell(struct usb_ep *ep,
					bool block_db)
{

	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	dbg_log_string("block_db(%d)", block_db);
	dwc3_msm_write_reg_field(mdwc->base, GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
		BLOCK_GSI_WR_GO_MASK, block_db);
}

/**
 * Performs necessary checks before stopping GSI channels
 *
 * @usb_ep - pointer to usb_ep instance to access DWC3 regs
 */
static bool gsi_check_ready_to_suspend(struct dwc3_msm *mdwc)
{
	u32	timeout = 1500;

	while (dwc3_msm_read_reg_field(mdwc->base,
		GSI_IF_STS(mdwc->gsi_reg), GSI_WR_CTRL_STATE_MASK)) {
		if (!timeout--) {
			dev_err(mdwc->dev,
			"Unable to suspend GSI ch. WR_CTRL_STATE != 0\n");
			return false;
		}
	}

	return true;
}

static inline const char *gsi_op_to_string(unsigned int op)
{
	if (op < ARRAY_SIZE(gsi_op_strings))
		return gsi_op_strings[op];

	return "Invalid";
}

/**
 * Performs GSI operations or GSI EP related operations.
 *
 * @usb_ep - pointer to usb_ep instance.
 * @op_data - pointer to opcode related data.
 * @op - GSI related or GSI EP related op code.
 *
 * @return int - 0 on success, negative on error.
 * Also returns XferRscIdx for GSI_EP_OP_GET_XFER_IDX.
 */
int usb_gsi_ep_op(struct usb_ep *ep, void *op_data, enum gsi_ep_op op)
{
	u32 ret = 0;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_gsi_request *request;
	struct gsi_channel_info *ch_info;
	bool block_db;
	unsigned long flags;

	dbg_log_string("%s(%d):%s", ep->name, dep->number >> 1,
			gsi_op_to_string(op));

	switch (op) {
	case GSI_EP_OP_PREPARE_TRBS:
		if (!dwc->pullups_connected) {
			dbg_log_string("No Pullup\n");
			return -ESHUTDOWN;
		}

		request = (struct usb_gsi_request *)op_data;
		ret = gsi_prepare_trbs(ep, request);
		break;
	case GSI_EP_OP_FREE_TRBS:
		request = (struct usb_gsi_request *)op_data;
		gsi_free_trbs(ep, request);
		break;
	case GSI_EP_OP_CONFIG:
		if (!dwc->pullups_connected) {
			dbg_log_string("No Pullup\n");
			return -ESHUTDOWN;
		}

		request = (struct usb_gsi_request *)op_data;
		spin_lock_irqsave(&dwc->lock, flags);
		gsi_configure_ep(ep, request);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_STARTXFER:
		if (!dwc->pullups_connected) {
			dbg_log_string("No Pullup\n");
			return -ESHUTDOWN;
		}

		request = (struct usb_gsi_request *)op_data;
		spin_lock_irqsave(&dwc->lock, flags);
		ret = gsi_startxfer_for_ep(ep, request);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_GET_XFER_IDX:
		ret = gsi_get_xfer_index(ep);
		break;
	case GSI_EP_OP_STORE_DBL_INFO:
		request = (struct usb_gsi_request *)op_data;
		gsi_store_ringbase_dbl_info(ep, request);
		break;
	case GSI_EP_OP_ENABLE_GSI:
		if (!dwc->pullups_connected) {
			dbg_log_string("No Pullup\n");
			return -ESHUTDOWN;
		}

		gsi_enable(ep);
		break;
	case GSI_EP_OP_GET_CH_INFO:
		ch_info = (struct gsi_channel_info *)op_data;
		gsi_get_channel_info(ep, ch_info);
		break;
	case GSI_EP_OP_RING_DB:
		if (!dwc->pullups_connected) {
			dbg_log_string("No Pullup\n");
			return -ESHUTDOWN;
		}

		request = (struct usb_gsi_request *)op_data;
		gsi_ring_db(ep, request);
		break;
	case GSI_EP_OP_UPDATEXFER:
		request = (struct usb_gsi_request *)op_data;
		spin_lock_irqsave(&dwc->lock, flags);
		ret = gsi_updatexfer_for_ep(ep, request);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_ENDXFER:
		spin_lock_irqsave(&dwc->lock, flags);
		dwc3_stop_active_transfer(dep, true, false);
		spin_unlock_irqrestore(&dwc->lock, flags);
		break;
	case GSI_EP_OP_SET_CLR_BLOCK_DBL:
		block_db = *((bool *)op_data);
		spin_lock_irqsave(&dwc->lock, flags);
		if (!dwc->pullups_connected && !block_db) {
			dbg_log_string("No Pullup\n");
			spin_unlock_irqrestore(&dwc->lock, flags);
			return -ESHUTDOWN;
		}

		spin_unlock_irqrestore(&dwc->lock, flags);
		gsi_set_clear_dbell(ep, block_db);
		break;
	case GSI_EP_OP_CHECK_FOR_SUSPEND:
		ret = gsi_check_ready_to_suspend(mdwc);
		break;
	case GSI_EP_OP_DISABLE:
		ret = ep->ops->disable(ep);
		break;
	default:
		dev_err(mdwc->dev, "%s: Invalid opcode GSI EP\n", __func__);
	}

	return ret;
}
EXPORT_SYMBOL(usb_gsi_ep_op);

/* Return true if host is supporting remote wakeup functionality. */
bool usb_get_remote_wakeup_status(struct usb_gadget *gadget)
{
	struct dwc3 *dwc = gadget_to_dwc(gadget);
	unsigned long flags;
	bool rw = false;

	spin_lock_irqsave(&dwc->lock, flags);
	rw = dwc->is_remote_wakeup_enabled ? true : false;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return rw;
}
EXPORT_SYMBOL(usb_get_remote_wakeup_status);

/**
 * Configure a USB DBM ep to work in BAM mode.
 *
 * @usb_ep - USB physical EP number.
 * @producer - producer/consumer.
 * @disable_wb - disable write back to system memory.
 * @internal_mem - use internal USB memory for data fifo.
 * @ioc - enable interrupt on completion.
 *
 * @return int - DBM ep number.
 */
static int dbm_ep_config(struct dwc3_msm *mdwc, u8 usb_ep, u8 bam_pipe,
		bool producer, bool disable_wb, bool internal_mem, bool ioc)
{
	int dbm_ep;
	u32 ep_cfg;
	u32 data;

	dev_dbg(mdwc->dev, "Configuring DBM ep\n");

	dbm_ep = find_matching_dbm_ep(mdwc, usb_ep);
	if (dbm_ep < 0)
		return dbm_ep;

	/* Due to HW issue, EP 7 can be set as IN EP only */
	if (!mdwc->dbm_is_1p4 && dbm_ep == 7 && producer) {
		pr_err("last DBM EP can't be OUT EP\n");
		return -ENODEV;
	}

	/* Set ioc bit for dbm_ep if needed */
	msm_dbm_write_reg_field(mdwc, DBM_DBG_CNFG,
		DBM_ENABLE_IOC_MASK & 1 << dbm_ep, ioc ? 1 : 0);

	ep_cfg = (producer ? DBM_PRODUCER : 0) |
		(disable_wb ? DBM_DISABLE_WB : 0) |
		(internal_mem ? DBM_INT_RAM_ACC : 0);

	msm_dbm_write_ep_reg_field(mdwc, DBM_EP_CFG, dbm_ep,
		DBM_PRODUCER | DBM_DISABLE_WB | DBM_INT_RAM_ACC, ep_cfg >> 8);

	msm_dbm_write_ep_reg_field(mdwc, DBM_EP_CFG, dbm_ep, USB3_EPNUM,
		usb_ep);

	if (mdwc->dbm_is_1p4) {
		msm_dbm_write_ep_reg_field(mdwc, DBM_EP_CFG, dbm_ep,
				DBM_BAM_PIPE_NUM, bam_pipe);
		msm_dbm_write_reg_field(mdwc, DBM_PIPE_CFG, 0x000000ff, 0xe4);
	}

	msm_dbm_write_ep_reg_field(mdwc, DBM_EP_CFG, dbm_ep, DBM_EN_EP, 1);

	data = msm_dbm_read_reg(mdwc, DBM_DISABLE_UPDXFER);
	data &= ~(0x1 << dbm_ep);
	msm_dbm_write_reg(mdwc, DBM_DISABLE_UPDXFER, data);

	return dbm_ep;
}

/**
 * usb_ep_autoconfig_by_name - Used to pick the endpoint by name. eg gsi-epin1
 * @gadget: The device to which the endpoint must belong.
 * @desc: Endpoint descriptor, with endpoint direction and transfer mode
 *	initialized.
 * @ep_name: EP name that is to be searched.
 *
 */
struct usb_ep *usb_ep_autoconfig_by_name(
			struct usb_gadget		*gadget,
			struct usb_endpoint_descriptor	*desc,
			const char			*ep_name
)
{
	struct usb_ep *ep;
	struct dwc3_ep *dep;
	bool ep_found = false;

	if (!ep_name || !strlen(ep_name))
		goto err;

	list_for_each_entry(ep, &gadget->ep_list, ep_list)
		if (strncmp(ep->name, ep_name, strlen(ep_name)) == 0 &&
				!ep->driver_data) {
			ep_found = true;
			break;
		}

	if (ep_found) {
		dep = to_dwc3_ep(ep);
		desc->bEndpointAddress &= USB_DIR_IN;
		desc->bEndpointAddress |= dep->number >> 1;
		ep->address = desc->bEndpointAddress;
		pr_debug("Allocating ep address:%x\n", ep->address);
		ep->desc = NULL;
		ep->comp_desc = NULL;
		return ep;
	}

err:
	pr_err("%s:error finding ep %s\n", __func__, ep_name);
	return NULL;
}
EXPORT_SYMBOL(usb_ep_autoconfig_by_name);

/**
 * Configure MSM endpoint.
 * This function do specific configurations
 * to an endpoint which need specific implementaion
 * in the MSM architecture.
 *
 * This function should be called by usb function/class
 * layer which need a support from the specific MSM HW
 * which wrap the USB3 core. (like GSI or DBM specific endpoints)
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negetive on error.
 */
int msm_ep_config(struct usb_ep *ep, struct usb_request *request, u32 bam_opts)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *new_ep_ops;
	int ret = 0;
	unsigned long flags;


	spin_lock_irqsave(&dwc->lock, flags);
	/* Save original ep ops for future restore*/
	if (mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] already configured as msm endpoint\n",
			ep->name, dep->number);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}
	mdwc->original_ep_ops[dep->number] = ep->ops;

	/* Set new usb ops as we like */
	new_ep_ops = kzalloc(sizeof(struct usb_ep_ops), GFP_ATOMIC);
	if (!new_ep_ops) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ENOMEM;
	}

	(*new_ep_ops) = (*ep->ops);
	new_ep_ops->queue = dwc3_msm_ep_queue;
	ep->ops = new_ep_ops;

	if (!request || dep->gsi) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return 0;
	}

	/*
	 * Configure the DBM endpoint if required.
	 */
	ret = dbm_ep_config(mdwc, dep->number,
			    bam_opts & MSM_PIPE_ID_MASK,
			    bam_opts & MSM_PRODUCER,
			    bam_opts & MSM_DISABLE_WB,
			    bam_opts & MSM_INTERNAL_MEM,
			    bam_opts & MSM_ETD_IOC);
	if (ret < 0) {
		dev_err(mdwc->dev,
			"error %d after calling dbm_ep_config\n", ret);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return ret;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(msm_ep_config);

static int dbm_ep_unconfig(struct dwc3_msm *mdwc, u8 usb_ep)
{
	int dbm_ep;
	u32 data;

	dev_dbg(mdwc->dev, "Unconfiguring DB ep\n");

	dbm_ep = find_matching_dbm_ep(mdwc, usb_ep);
	if (dbm_ep < 0)
		return dbm_ep;

	mdwc->dbm_ep_num_mapping[dbm_ep] = 0;

	data = msm_dbm_read_ep_reg(mdwc, DBM_EP_CFG, dbm_ep);
	data &= (~0x1);
	msm_dbm_write_ep_reg(mdwc, DBM_EP_CFG, dbm_ep, data);

	/*
	 * ep_soft_reset is not required during disconnect as pipe reset on
	 * next connect will take care of the same.
	 */
	return 0;
}

/**
 * Un-configure MSM endpoint.
 * Tear down configurations done in the
 * dwc3_msm_ep_config function.
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negative on error.
 */
int msm_ep_unconfig(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct usb_ep_ops *old_ep_ops;
	unsigned long flags;

	spin_lock_irqsave(&dwc->lock, flags);
	/* Restore original ep ops */
	if (!mdwc->original_ep_ops[dep->number]) {
		dev_err(mdwc->dev,
			"ep [%s,%d] was not configured as msm endpoint\n",
			ep->name, dep->number);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}
	old_ep_ops = (struct usb_ep_ops	*)ep->ops;
	ep->ops = mdwc->original_ep_ops[dep->number];
	mdwc->original_ep_ops[dep->number] = NULL;
	kfree(old_ep_ops);

	/* If this is a GSI endpoint, we're done */
	if (dep->gsi) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		return 0;
	}

	if (dep->trb_dequeue == dep->trb_enqueue
					&& list_empty(&dep->pending_list)
					&& list_empty(&dep->started_list)) {
		dev_dbg(mdwc->dev,
			"%s: request is not queued, disable DBM ep for ep %s\n",
			__func__, ep->name);
		/* Unconfigure dbm ep */
		dbm_ep_unconfig(mdwc, dep->number);

		/*
		 * If this is the last endpoint we unconfigured, than reset also
		 * the event buffers; unless unconfiguring the ep due to lpm,
		 * in which case the event buffer only gets reset during the
		 * block reset.
		 */
		if (dbm_get_num_of_eps_configured(mdwc) == 0 &&
				!mdwc->dbm_reset_ep_after_lpm)
			dbm_event_buffer_config(mdwc, 0, 0, 0);
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(msm_ep_unconfig);

void msm_ep_set_endless(struct usb_ep *ep, bool set_clear)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);

	dep->endless = !!set_clear;
}
EXPORT_SYMBOL(msm_ep_set_endless);

#endif /* (CONFIG_USB_DWC3_GADGET) || (CONFIG_USB_DWC3_DUAL_ROLE) */

static void dwc3_resume_work(struct work_struct *w);

static void dwc3_restart_usb_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
						restart_usb_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned int timeout = 50;

	dev_dbg(mdwc->dev, "%s\n", __func__);

	if (atomic_read(&dwc->in_lpm) || dwc->dr_mode != USB_DR_MODE_OTG) {
		dev_dbg(mdwc->dev, "%s failed!!!\n", __func__);
		return;
	}

	/* guard against concurrent VBUS handling */
	mdwc->in_restart = true;

	if (!mdwc->vbus_active) {
		dev_dbg(mdwc->dev, "%s bailing out in disconnect\n", __func__);
		dwc->err_evt_seen = false;
		mdwc->in_restart = false;
		return;
	}

	dbg_event(0xFF, "RestartUSB", 0);
	/* Reset active USB connection */
	dwc3_resume_work(&mdwc->resume_work);

	/* Make sure disconnect is processed before sending connect */
	while (--timeout && !pm_runtime_suspended(mdwc->dev))
		msleep(20);

	if (!timeout) {
		dev_dbg(mdwc->dev,
			"Not in LPM after disconnect, forcing suspend...\n");
		dbg_event(0xFF, "ReStart:RT SUSP",
			atomic_read(&mdwc->dev->power.usage_count));
		pm_runtime_suspend(mdwc->dev);
	}

	mdwc->in_restart = false;
	/* Force reconnect only if cable is still connected */
	if (mdwc->vbus_active)
		dwc3_resume_work(&mdwc->resume_work);

	dwc->err_evt_seen = false;
	flush_delayed_work(&mdwc->sm_work);

	/* see comments in dwc3_msm_suspend */
	if(!mdwc->vbus_active)
		pm_relax(mdwc->dev);
}

/*
 * Check whether the DWC3 requires resetting the ep
 * after going to Low Power Mode (lpm)
 */
bool msm_dwc3_reset_ep_after_lpm(struct usb_gadget *gadget)
{
	struct dwc3 *dwc = container_of(gadget, struct dwc3, gadget);
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);

	return mdwc->dbm_reset_ep_after_lpm;
}
EXPORT_SYMBOL(msm_dwc3_reset_ep_after_lpm);

/*
 * Config Global Distributed Switch Controller (GDSC)
 * to support controller power collapse
 */
static int dwc3_msm_config_gdsc(struct dwc3_msm *mdwc, int on)
{
	int ret;

	if (IS_ERR_OR_NULL(mdwc->dwc3_gdsc))
		return -EPERM;

	if (on) {
		ret = regulator_enable(mdwc->dwc3_gdsc);
		if (ret) {
			dev_err(mdwc->dev, "unable to enable usb3 gdsc\n");
			return ret;
		}

		qcom_clk_set_flags(mdwc->core_clk, CLKFLAG_RETAIN_MEM);
	} else {
		qcom_clk_set_flags(mdwc->core_clk, CLKFLAG_NORETAIN_MEM);
		ret = regulator_disable(mdwc->dwc3_gdsc);
		if (ret) {
			dev_err(mdwc->dev, "unable to disable usb3 gdsc\n");
			return ret;
		}
	}

	return ret;
}

static int dwc3_msm_link_clk_reset(struct dwc3_msm *mdwc, bool assert)
{
	int ret = 0;

	if (assert) {
		disable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);
		/* Using asynchronous block reset to the hardware */
		dev_dbg(mdwc->dev, "block_reset ASSERT\n");
		clk_disable_unprepare(mdwc->utmi_clk);
		clk_disable_unprepare(mdwc->sleep_clk);
		clk_disable_unprepare(mdwc->core_clk);
		clk_disable_unprepare(mdwc->iface_clk);
		ret = reset_control_assert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "dwc3 core_reset assert failed\n");
	} else {
		dev_dbg(mdwc->dev, "block_reset DEASSERT\n");
		ret = reset_control_deassert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "dwc3 core_reset deassert failed\n");
		ndelay(200);
		clk_prepare_enable(mdwc->iface_clk);
		clk_prepare_enable(mdwc->core_clk);
		clk_prepare_enable(mdwc->sleep_clk);
		clk_prepare_enable(mdwc->utmi_clk);
		enable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);
	}

	return ret;
}

static void dwc3_msm_vbus_draw_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
			vbus_draw_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dwc3_msm_gadget_vbus_draw(mdwc, dwc->vbus_draw);
}

static void dwc3_gsi_event_buf_alloc(struct dwc3 *dwc)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_event_buffer *evt;
	int i;

	if (!mdwc->num_gsi_event_buffers)
		return;

	mdwc->gsi_ev_buff = devm_kzalloc(dwc->dev,
		sizeof(*dwc->ev_buf) * mdwc->num_gsi_event_buffers,
		GFP_KERNEL);
	if (!mdwc->gsi_ev_buff) {
		dev_err(dwc->dev, "can't allocate gsi_ev_buff\n");
		return;
	}

	for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {

		evt = devm_kzalloc(dwc->dev, sizeof(*evt), GFP_KERNEL);
		if (!evt)
			return;
		evt->dwc	= dwc;
		evt->length	= DWC3_EVENT_BUFFERS_SIZE;
		evt->buf	= dma_alloc_coherent(dwc->sysdev,
					DWC3_EVENT_BUFFERS_SIZE,
					&evt->dma, GFP_KERNEL);
		if (!evt->buf) {
			dev_err(dwc->dev,
				"can't allocate gsi_evt_buf(%d)\n", i);
			return;
		}
		mdwc->gsi_ev_buff[i] = evt;
	}
	/*
	 * Set-up dummy buffer to use as doorbell while IPA GSI
	 * connection is in progress.
	 */
	mdwc->dummy_gsi_db_dma = dma_map_single(dwc->sysdev,
					&mdwc->dummy_gsi_db,
					sizeof(mdwc->dummy_gsi_db),
					DMA_FROM_DEVICE);

	if (dma_mapping_error(dwc->sysdev, mdwc->dummy_gsi_db_dma)) {
		dev_err(dwc->dev, "failed to map dummy doorbell buffer\n");
		mdwc->dummy_gsi_db_dma = (dma_addr_t)NULL;
	}
}

static void dwc3_msm_notify_event(struct dwc3 *dwc,
		enum dwc3_notify_event event, unsigned int value)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dwc->dev->parent);
	struct dwc3_event_buffer *evt;
	u32 reg;
	int i;

	switch (event) {
	case DWC3_CONTROLLER_ERROR_EVENT:
		dev_info(mdwc->dev,
			"DWC3_CONTROLLER_ERROR_EVENT received, irq cnt %lu\n",
			atomic_read(&dwc->irq_cnt));

		dwc3_msm_write_reg(mdwc->base, DWC3_DEVTEN, 0x00);

		/* prevent core from generating interrupts until recovery */
		reg = dwc3_msm_read_reg(mdwc->base, DWC3_GCTL);
		reg |= DWC3_GCTL_CORESOFTRESET;
		dwc3_msm_write_reg(mdwc->base, DWC3_GCTL, reg);

		/*
		 * If core could not recover after MAX_ERROR_RECOVERY_TRIES
		 * skip the restart USB work and keep the core in softreset
		 * state
		 */
		if (dwc->retries_on_error < MAX_ERROR_RECOVERY_TRIES)
			schedule_work(&mdwc->restart_usb_work);
		break;
	case DWC3_CONTROLLER_CONNDONE_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_CONNDONE_EVENT received\n");
		/*
		 * Add power event if the dbm indicates coming out of L1 by
		 * interrupt
		 */
		if (!mdwc->dbm_is_1p4)
			dwc3_msm_write_reg_field(mdwc->base,
					PWR_EVNT_IRQ_MASK_REG,
					PWR_EVNT_LPM_OUT_L1_MASK, 1);

		atomic_set(&dwc->in_lpm, 0);
		break;
	case DWC3_CONTROLLER_NOTIFY_OTG_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_NOTIFY_OTG_EVENT received\n");
		if (dwc->enable_bus_suspend) {
			mdwc->suspend = dwc->b_suspend;
			queue_work(mdwc->dwc3_wq, &mdwc->resume_work);
		}
		break;
	case DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT received\n");
		schedule_work(&mdwc->vbus_draw_work);
		break;
	case DWC3_CONTROLLER_PULLUP:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_PULLUP received\n");
		redriver_gadget_pullup(mdwc->ss_redriver_node, value);
		break;
	case DWC3_GSI_EVT_BUF_ALLOC:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_ALLOC\n");
		dwc3_gsi_event_buf_alloc(dwc);
		break;
	case DWC3_GSI_EVT_BUF_SETUP:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_SETUP\n");
		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			if (!evt)
				break;

			dev_dbg(mdwc->dev, "Evt buf %pK dma %08llx length %d\n",
				evt->buf, (unsigned long long) evt->dma,
				evt->length);
			memset(evt->buf, 0, evt->length);
			evt->lpos = 0;
			/*
			 * Primary event buffer is programmed with registers
			 * DWC3_GEVNT*(0). Hence use DWC3_GEVNT*(i+1) to
			 * program USB GSI related event buffer with DWC3
			 * controller.
			 */
			dwc3_msm_write_reg(mdwc->base, DWC3_GEVNTADRLO((i+1)),
				lower_32_bits(evt->dma));
			dwc3_msm_write_reg(mdwc->base, DWC3_GEVNTADRHI((i+1)),
				(upper_32_bits(evt->dma) & 0xffff) |
				DWC3_GEVNTADRHI_EVNTADRHI_GSI_EN(
				DWC3_GEVENT_TYPE_GSI) |
				DWC3_GEVNTADRHI_EVNTADRHI_GSI_IDX((i+1)));
			dwc3_msm_write_reg(mdwc->base, DWC3_GEVNTSIZ((i+1)),
				DWC3_GEVNTCOUNT_EVNTINTRPTMASK |
				((evt->length) & 0xffff));
			dwc3_msm_write_reg(mdwc->base,
					DWC3_GEVNTCOUNT((i+1)), 0);
		}
		break;
	case DWC3_GSI_EVT_BUF_CLEANUP:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_CLEANUP\n");
		if (!mdwc->gsi_ev_buff)
			break;

		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			evt->lpos = 0;
			/*
			 * Primary event buffer is programmed with registers
			 * DWC3_GEVNT*(0). Hence use DWC3_GEVNT*(i+1) to
			 * program USB GSI related event buffer with DWC3
			 * controller.
			 */
			dwc3_msm_write_reg(mdwc->base,
					DWC3_GEVNTADRLO((i+1)), 0);
			dwc3_msm_write_reg(mdwc->base,
					DWC3_GEVNTADRHI((i+1)), 0);
			dwc3_msm_write_reg(mdwc->base, DWC3_GEVNTSIZ((i+1)),
					DWC3_GEVNTSIZ_INTMASK |
					DWC3_GEVNTSIZ_SIZE((i+1)));
			dwc3_msm_write_reg(mdwc->base,
					DWC3_GEVNTCOUNT((i+1)), 0);
		}
		break;
	case DWC3_GSI_EVT_BUF_FREE:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_FREE\n");
		if (!mdwc->gsi_ev_buff)
			break;

		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			evt = mdwc->gsi_ev_buff[i];
			if (evt)
				dma_free_coherent(dwc->sysdev, evt->length,
							evt->buf, evt->dma);
		}
		if (mdwc->dummy_gsi_db_dma) {
			dma_unmap_single(dwc->sysdev, mdwc->dummy_gsi_db_dma,
					 sizeof(mdwc->dummy_gsi_db),
					 DMA_FROM_DEVICE);
			mdwc->dummy_gsi_db_dma = (dma_addr_t)NULL;
		}
		break;
	case DWC3_GSI_EVT_BUF_CLEAR:
		dev_dbg(mdwc->dev, "DWC3_GSI_EVT_BUF_CLEAR\n");
		for (i = 0; i < mdwc->num_gsi_event_buffers; i++) {
			reg = dwc3_msm_read_reg(mdwc->base,
					DWC3_GEVNTCOUNT((i+1)));
			reg &= DWC3_GEVNTCOUNT_MASK;
			dwc3_msm_write_reg(mdwc->base,
					DWC3_GEVNTCOUNT((i+1)), reg);
			dbg_log_string("remaining EVNTCOUNT(%d)=%d", i+1, reg);
		}
		break;
	case DWC3_CONTROLLER_NOTIFY_DISABLE_UPDXFER:
		dwc3_msm_dbm_disable_updxfer(dwc, value);
		break;
	case DWC3_CONTROLLER_NOTIFY_CLEAR_DB:
		dev_dbg(mdwc->dev, "DWC3_CONTROLLER_NOTIFY_CLEAR_DB\n");
		if (mdwc->gsi_reg) {
			dwc3_msm_write_reg_field(mdwc->base,
				GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
				BLOCK_GSI_WR_GO_MASK, true);
			dwc3_msm_write_reg_field(mdwc->base,
				GSI_GENERAL_CFG_REG(mdwc->gsi_reg),
				GSI_EN_MASK, 0);
		}
		break;
	default:
		dev_dbg(mdwc->dev, "unknown dwc3 event\n");
		break;
	}
}

static void dwc3_msm_block_reset(struct dwc3_msm *mdwc, bool core_reset)
{
	int ret  = 0;

	if (core_reset) {
		ret = dwc3_msm_link_clk_reset(mdwc, 1);
		if (ret)
			return;

		usleep_range(1000, 1200);
		ret = dwc3_msm_link_clk_reset(mdwc, 0);
		if (ret)
			return;

		usleep_range(10000, 12000);
	}

	/* Reset the DBM */
	msm_dbm_write_reg_field(mdwc, DBM_SOFT_RESET, DBM_SFT_RST_MASK, 1);
	usleep_range(1000, 1200);
	msm_dbm_write_reg_field(mdwc, DBM_SOFT_RESET, DBM_SFT_RST_MASK, 0);

	/* enable DBM */
	dwc3_msm_write_reg_field(mdwc->base, QSCRATCH_GENERAL_CFG,
		DBM_EN_MASK, 0x1);
	if (!mdwc->dbm_is_1p4) {
		msm_dbm_write_reg(mdwc, DBM_DATA_FIFO_ADDR_EN, 0xFF);
		msm_dbm_write_reg(mdwc, DBM_DATA_FIFO_SIZE_EN, 0xFF);
	}
}

static void dwc3_en_sleep_mode(struct dwc3_msm *mdwc)
{
	u32 reg;

	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0));
	reg |= DWC3_GUSB2PHYCFG_ENBLSLPM;
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0), reg);

	if (mdwc->dual_port) {
		reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(1));
		reg |= DWC3_GUSB2PHYCFG_ENBLSLPM;
		dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(1), reg);
	}

	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUCTL1);
	reg |= DWC3_GUCTL1_L1_SUSP_THRLD_EN_FOR_HOST;
	dwc3_msm_write_reg(mdwc->base, DWC3_GUCTL1, reg);
}

static void dwc3_dis_sleep_mode(struct dwc3_msm *mdwc)
{
	u32 reg;

	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0));
	reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0), reg);

	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUCTL1);
	reg &= ~DWC3_GUCTL1_L1_SUSP_THRLD_EN_FOR_HOST;
	dwc3_msm_write_reg(mdwc->base, DWC3_GUCTL1, reg);
}

static void dwc3_msm_power_collapse_por(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	u32 val = 0, val1 = 0;
	int ret;

	/* Configure AHB2PHY for one wait state read/write */
	if (mdwc->ahb2phy_base) {
		clk_prepare_enable(mdwc->cfg_ahb_clk);
		val = readl_relaxed(mdwc->ahb2phy_base +
				PERIPH_SS_AHB2PHY_TOP_CFG);
		if (val != ONE_READ_WRITE_WAIT) {
			writel_relaxed(ONE_READ_WRITE_WAIT,
				mdwc->ahb2phy_base + PERIPH_SS_AHB2PHY_TOP_CFG);
			/* complete above write before configuring USB PHY. */
			mb();
		}
		clk_disable_unprepare(mdwc->cfg_ahb_clk);
	}

	/*
	 * Below sequence is used when controller is working without
	 * having ssphy and only USB high/full speed is supported.
	 */
	if (dwc->maximum_speed == USB_SPEED_HIGH ||
				dwc->maximum_speed == USB_SPEED_FULL) {
		dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
			dwc3_msm_read_reg(mdwc->base,
			QSCRATCH_GENERAL_CFG)
			| PIPE_UTMI_CLK_DIS);

		usleep_range(2, 5);


		dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
			dwc3_msm_read_reg(mdwc->base,
			QSCRATCH_GENERAL_CFG)
			| PIPE_UTMI_CLK_SEL
			| PIPE3_PHYSTATUS_SW);

		usleep_range(2, 5);

		dwc3_msm_write_reg(mdwc->base, QSCRATCH_GENERAL_CFG,
			dwc3_msm_read_reg(mdwc->base,
			QSCRATCH_GENERAL_CFG)
			& ~PIPE_UTMI_CLK_DIS);
	}

	dwc->tx_fifo_size = mdwc->tx_fifo_size;
	ret = dwc3_core_init(dwc);
	if (ret)
		dev_err(mdwc->dev, "%s: dwc3_core init failed (%d)\n",
							__func__, ret);

	/* Get initial P3 status and enable IN_P3 event */
	if (dwc3_is_usb31(dwc)) {
		val = dwc3_msm_read_reg_field(mdwc->base,
			DWC31_LINK_GDBGLTSSM(0),
			DWC3_GDBGLTSSM_LINKSTATE_MASK);
		if (mdwc->dual_port)
			val1 = dwc3_msm_read_reg_field(mdwc->base,
				DWC31_LINK_GDBGLTSSM(1),
				DWC3_GDBGLTSSM_LINKSTATE_MASK);
	} else {
		val = dwc3_msm_read_reg_field(mdwc->base,
			DWC3_GDBGLTSSM, DWC3_GDBGLTSSM_LINKSTATE_MASK);
	}

	if (!mdwc->dual_port)
		val1 = DWC3_LINK_STATE_U3;

	atomic_set(&mdwc->in_p3, (val == DWC3_LINK_STATE_U3) &&
					(val1 == DWC3_LINK_STATE_U3));

	if (!mdwc->dual_port)
		dwc3_msm_write_reg_field(mdwc->base, PWR_EVNT_IRQ_MASK_REG,
				PWR_EVNT_POWERDOWN_IN_P3_MASK, 1);

	/* Set the core in host mode if it was in host mode during pm_suspend */
	if (mdwc->in_host_mode) {
		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_HOST);
		if (!dwc->dis_enblslpm_quirk)
			dwc3_en_sleep_mode(mdwc);
	}

}

/*
 * Suspend SSPHY0/SSPHY1 based on the index. 'idx' argument will choose
 * appropriate PWR_EVT_IRQ_STAT and GUSBPIPECTL registers.
 *
 * The return value is a boolean flag denoting if SSPHY was suspended or not.
 */
static bool dwc3_msm_ssphy_autosuspend(struct dwc3_msm *mdwc, int idx)
{
	unsigned long timeout;
	u32 reg = 0, stat_reg;
	bool suspended = false;

	idx = !!idx;
	stat_reg = idx ? PWR_EVNT_IRQ_STAT_REG1 : PWR_EVNT_IRQ_STAT_REG;

	/* Clear previous P3 events */
	dwc3_msm_write_reg(mdwc->base, stat_reg,
		PWR_EVNT_POWERDOWN_IN_P3_MASK | PWR_EVNT_POWERDOWN_OUT_P3_MASK);

	/* Prepare SSPHY for suspend */
	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB3PIPECTL(idx));
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(idx),
					reg | DWC3_GUSB3PIPECTL_SUSPHY);

	/* Wait for SSPHY to go into P3 */
	timeout = jiffies + msecs_to_jiffies(5);
	while (!time_after(jiffies, timeout)) {
		reg = dwc3_msm_read_reg(mdwc->base, stat_reg);
		if (reg & PWR_EVNT_POWERDOWN_IN_P3_MASK) {
			suspended = true;
			break;
		}
	}

	/* Clear P3 event bit */
	dwc3_msm_write_reg(mdwc->base, stat_reg, PWR_EVNT_POWERDOWN_IN_P3_MASK);

	return suspended;
}

static int dwc3_msm_prepare_suspend(struct dwc3_msm *mdwc, bool ignore_p3_state)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned long timeout;
	u32 reg = 0;
	bool ssphy0_sus = false, ssphy1_sus = false;

	/* Allow SSPHY(s) to go to P3 state if SSPHY autosuspend is disabled */
	if (dwc->dis_u3_susphy_quirk) {
		/* Clear in_p3 and allow SSPHY suspend explicitly */
		atomic_set(&mdwc->in_p3, 0);

		ssphy0_sus = dwc3_msm_ssphy_autosuspend(mdwc, 0);
		if (mdwc->ss_phy1)
			ssphy1_sus = dwc3_msm_ssphy_autosuspend(mdwc, 1);

		if (!mdwc->dual_port)
			atomic_set(&mdwc->in_p3, ssphy0_sus);
		else
			atomic_set(&mdwc->in_p3, ssphy0_sus & ssphy1_sus);
	}

	/* SSPHY(s) should be in P3 to detect port events */
	if (!ignore_p3_state && ((mdwc->in_host_mode || mdwc->in_device_mode)
		&& (dwc3_msm_is_superspeed(mdwc) || dwc->dis_u3_susphy_quirk) &&
							!mdwc->in_restart)) {
		if (!atomic_read(&mdwc->in_p3)) {
			dev_err(mdwc->dev, "Not in P3,aborting LPM sequence\n");
			return -EBUSY;
		}
	}

	/* Clear previous L2 events */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG,
		PWR_EVNT_LPM_IN_L2_MASK | PWR_EVNT_LPM_OUT_L2_MASK);

	/* Prepare HSPHY for suspend */
	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0));
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0),
		reg | DWC3_GUSB2PHYCFG_ENBLSLPM | DWC3_GUSB2PHYCFG_SUSPHY);

	/* Wait for PHY to go into L2 */
	timeout = jiffies + msecs_to_jiffies(5);
	while (!time_after(jiffies, timeout)) {
		reg = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG);
		if (reg & PWR_EVNT_LPM_IN_L2_MASK)
			break;
	}
	if (!(reg & PWR_EVNT_LPM_IN_L2_MASK))
		dev_err(mdwc->dev, "could not transition HS PHY to L2\n");

	/* Clear L2 event bit */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG,
		PWR_EVNT_LPM_IN_L2_MASK);

	/* Handling for dual port core */
	if (!mdwc->dual_port)
		return 0;

	/* Clear previous L2 events */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG1,
		PWR_EVNT_LPM_IN_L2_MASK | PWR_EVNT_LPM_OUT_L2_MASK);

	/* Prepare HSPHY for suspend */
	reg = dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(1));
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(1),
		reg | DWC3_GUSB2PHYCFG_ENBLSLPM | DWC3_GUSB2PHYCFG_SUSPHY);

	/* Wait for PHY to go into L2 */
	timeout = jiffies + msecs_to_jiffies(5);
	while (!time_after(jiffies, timeout)) {
		reg = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG1);
		if (reg & PWR_EVNT_LPM_IN_L2_MASK)
			break;
	}
	if (!(reg & PWR_EVNT_LPM_IN_L2_MASK))
		dev_err(mdwc->dev, "could not transition HS PHY1 to L2\n");

	/* Clear L2 event bit */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG1,
		PWR_EVNT_LPM_IN_L2_MASK);

	return 0;
}

/* Generic functions to set/clear PHY flags */
static void dwc3_msm_set_hsphy_flags(struct dwc3_msm *mdwc, int set)
{
	mdwc->hs_phy->flags |= set;
	if (mdwc->hs_phy1)
		mdwc->hs_phy1->flags |= set;
}

static void dwc3_msm_clear_hsphy_flags(struct dwc3_msm *mdwc, int clear)
{
	mdwc->hs_phy->flags &= ~clear;
	if (mdwc->hs_phy1)
		mdwc->hs_phy1->flags &= ~clear;
}

static void dwc3_msm_set_ssphy_flags(struct dwc3_msm *mdwc, int set)
{
	mdwc->ss_phy->flags |= set;
	if (mdwc->ss_phy1)
		mdwc->ss_phy1->flags |= set;
}

static void dwc3_msm_clear_ssphy_flags(struct dwc3_msm *mdwc, int clear)
{
	mdwc->ss_phy->flags &= ~clear;
	if (mdwc->ss_phy1)
		mdwc->ss_phy1->flags &= ~clear;
}

static void dwc3_set_phy_speed_flags(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int i, num_ports;
	u32 reg;

	dwc3_msm_clear_hsphy_flags(mdwc, PHY_HSFS_MODE | PHY_LS_MODE);

	/*
	 * For single port controller, there are 2 PORTSC registers, one for
	 * HS bus and other for SS bus, hence both mapping to the only HSPHY.
	 * But for dual port controllers, there are 4 PORTSC registers: 0 and 1
	 * for HS busses 0 and 1 respectively; and registers 2 and 3 for SS
	 * busses 0 and 1 respectively. Hence, PORTSC registers 0 and 2 map to
	 * HSPHY 0 and registers 1 and 3 map to HSPHY1. Handle the flag setting
	 * using the below logic which will also maintain single port core.
	 *       num_ports = 2 (or) 4 (depending on no. of ports)
	 *   ->num_ports/2 = 1 (or) 2
	 * So, for single port core, i % (num_ports/2) = 0, hence we will always
	 * be updating HSPHY0 flags.
	 * For dual port, i % (num_ports/2) = 0 for port 0 (or) 1 for port 1.
	 * Hence we update HSPHY0 for reg 0 and 2; and HSPHY1 for reg 1 and 3.
	 */
	if (mdwc->in_host_mode) {
		reg = dwc3_msm_read_reg(mdwc->base, USB3_HCSPARAMS1);
		num_ports = HCS_MAX_PORTS(reg);
		for (i = 0; i < num_ports; i++) {
			reg = dwc3_msm_read_reg(mdwc->base,
					USB3_PORTSC + i*0x10);
			if (reg & PORT_CONNECT) {
				if (DEV_HIGHSPEED(reg) || DEV_FULLSPEED(reg)) {
					if (i % (num_ports / 2))
						mdwc->hs_phy1->flags |=
								PHY_HSFS_MODE;
					else
						mdwc->hs_phy->flags |=
								PHY_HSFS_MODE;
				} else if (DEV_LOWSPEED(reg)) {
					if (i % (num_ports / 2))
						mdwc->hs_phy1->flags |=
								PHY_LS_MODE;
					else
						mdwc->hs_phy->flags |=
								PHY_LS_MODE;
				}
			}
		}
	} else {
		if (dwc->gadget.speed == USB_SPEED_HIGH ||
			dwc->gadget.speed == USB_SPEED_FULL)
			mdwc->hs_phy->flags |= PHY_HSFS_MODE;
		else if (dwc->gadget.speed == USB_SPEED_LOW)
			mdwc->hs_phy->flags |= PHY_LS_MODE;
	}
}

static void dwc3_set_ssphy_orientation_flag(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	union extcon_property_value val;
	struct extcon_dev *edev = NULL;
	unsigned int extcon_id;
	int ret;

	dwc3_msm_clear_ssphy_flags(mdwc, PHY_LANE_A | PHY_LANE_B);

	if (mdwc->orientation_override) {
		mdwc->ss_phy->flags |= mdwc->orientation_override;
	} else if (mdwc->ss_redriver_node) {
		ret = redriver_orientation_get(mdwc->ss_redriver_node);
		if (ret == 0)
			mdwc->ss_phy->flags |= PHY_LANE_A;
		else
			mdwc->ss_phy->flags |= PHY_LANE_B;
	} else {
		if (mdwc->extcon && mdwc->vbus_active && !mdwc->in_restart) {
			extcon_id = EXTCON_USB;
			edev = mdwc->extcon[mdwc->ext_idx].edev;
		} else if (mdwc->extcon && mdwc->id_state == DWC3_ID_GROUND) {
			extcon_id = EXTCON_USB_HOST;
			edev = mdwc->extcon[mdwc->ext_idx].edev;
		}

		if (edev && extcon_get_state(edev, extcon_id)) {
			ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_TYPEC_POLARITY, &val);
			if (ret == 0)
				mdwc->ss_phy->flags |= val.intval ?
						PHY_LANE_B : PHY_LANE_A;
		}
	}

	dbg_event(0xFF, "ss_flag", mdwc->ss_phy->flags);
}

static void msm_dwc3_perf_vote_update(struct dwc3_msm *mdwc,
						bool perf_mode);

static void configure_usb_wakeup_interrupt(struct dwc3_msm *mdwc,
	struct usb_irq *uirq, unsigned int polarity, bool enable)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (uirq && enable && !uirq->enable) {
		dbg_event(0xFF, "PDC_IRQ_EN", uirq->irq);
		dbg_event(0xFF, "PDC_IRQ_POL", polarity);
		/* clear any pending interrupt */
		irq_set_irqchip_state(uirq->irq, IRQCHIP_STATE_PENDING, 0);
		irq_set_irq_type(uirq->irq, polarity);
		enable_irq_wake(uirq->irq);
		enable_irq(uirq->irq);
		uirq->enable = true;
	}

	if (uirq && !enable && uirq->enable) {
		dbg_event(0xFF, "PDC_IRQ_DIS", uirq->irq);
		disable_irq_wake(uirq->irq);
		disable_irq_nosync(uirq->irq);
		uirq->enable = false;
	}
}

static void configure_usb_wakeup_interrupts(struct dwc3_msm *mdwc, bool enable)
{
	if (!enable)
		goto disable_usb_irq;

	if (mdwc->hs_phy->flags & PHY_LS_MODE) {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_FALLING, enable);
	} else if (mdwc->hs_phy->flags & PHY_HSFS_MODE) {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_FALLING, enable);
	} else {
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_RISING, true);
		configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ],
			IRQ_TYPE_EDGE_RISING, true);
	}

	if (mdwc->dual_port) {
		if (mdwc->hs_phy1->flags & PHY_LS_MODE) {
			configure_usb_wakeup_interrupt(mdwc,
				&mdwc->wakeup_irq[DM_HS_PHY_IRQ_1],
				IRQ_TYPE_EDGE_FALLING, enable);
		} else if (mdwc->hs_phy1->flags & PHY_HSFS_MODE) {
			configure_usb_wakeup_interrupt(mdwc,
				&mdwc->wakeup_irq[DP_HS_PHY_IRQ_1],
				IRQ_TYPE_EDGE_FALLING, enable);
		} else {
			configure_usb_wakeup_interrupt(mdwc,
				&mdwc->wakeup_irq[DP_HS_PHY_IRQ_1],
				IRQ_TYPE_EDGE_RISING, true);
			configure_usb_wakeup_interrupt(mdwc,
				&mdwc->wakeup_irq[DM_HS_PHY_IRQ_1],
				IRQ_TYPE_EDGE_RISING, true);
		}
	}

	configure_usb_wakeup_interrupt(mdwc,
		&mdwc->wakeup_irq[SS_PHY_IRQ],
		IRQF_TRIGGER_HIGH | IRQ_TYPE_LEVEL_HIGH, enable);
	configure_usb_wakeup_interrupt(mdwc,
		&mdwc->wakeup_irq[SS_PHY_IRQ_1],
		IRQF_TRIGGER_HIGH | IRQ_TYPE_LEVEL_HIGH, enable);
	return;

disable_usb_irq:
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ_1], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ_1], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[SS_PHY_IRQ_1], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DP_HS_PHY_IRQ], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[DM_HS_PHY_IRQ], 0, enable);
	configure_usb_wakeup_interrupt(mdwc,
			&mdwc->wakeup_irq[SS_PHY_IRQ], 0, enable);
}

static void configure_nonpdc_usb_interrupt(struct dwc3_msm *mdwc,
		struct usb_irq *uirq, bool enable)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (uirq && enable && !uirq->enable) {
		dbg_event(0xFF, "IRQ_EN", uirq->irq);
		enable_irq_wake(uirq->irq);
		enable_irq(uirq->irq);
		uirq->enable = true;
	}

	if (uirq && !enable && uirq->enable) {
		dbg_event(0xFF, "IRQ_DIS", uirq->irq);
		disable_irq_wake(uirq->irq);
		disable_irq_nosync(uirq->irq);
		uirq->enable = false;
	}
}

static void dwc3_msm_set_pwr_events(struct dwc3_msm *mdwc, bool on)
{
	u32 irq_mask, irq_stat;

	irq_stat = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG);

	/* clear pending interrupts */
	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG, irq_stat);

	irq_mask = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_MASK_REG);

	if (on) {
		/*
		 * In case of platforms which use mpm interrupts, in case where
		 * suspend happens with a hs/fs/ls device connected in host mode.
		 * DP/DM falling edge will be monitored, but gic doesn't have
		 * capability to detect falling edge. So program power event irq
		 * to notify exit from lpm in such case.
		 */
		if (mdwc->use_pwr_event_for_wakeup & PWR_EVENT_HS_WAKEUP)
			irq_mask |= PWR_EVNT_LPM_OUT_L2_MASK;
		if ((mdwc->use_pwr_event_for_wakeup & PWR_EVENT_SS_WAKEUP)
					&& !(mdwc->lpm_flags & MDWC3_SS_PHY_SUSPEND))
			irq_mask |= (PWR_EVNT_POWERDOWN_OUT_P3_MASK |
						PWR_EVNT_LPM_OUT_RX_ELECIDLE_IRQ_MASK);
	} else {
		if (mdwc->use_pwr_event_for_wakeup & PWR_EVENT_HS_WAKEUP)
			irq_mask &= ~PWR_EVNT_LPM_OUT_L2_MASK;
		if ((mdwc->use_pwr_event_for_wakeup & PWR_EVENT_SS_WAKEUP)
					&& !(mdwc->lpm_flags & MDWC3_SS_PHY_SUSPEND))
			irq_mask &= ~(PWR_EVNT_POWERDOWN_OUT_P3_MASK |
						PWR_EVNT_LPM_OUT_RX_ELECIDLE_IRQ_MASK);
	}

	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_MASK_REG, irq_mask);
}

static int dwc3_msm_update_bus_bw(struct dwc3_msm *mdwc, enum bus_vote bv)
{
	int i, ret = 0;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned int bv_index = mdwc->override_bus_vote ?: bv;

	dbg_event(0xFF, "bus_vote_start", bv);

	/* On some platforms SVS does not have separate vote.
	 * Vote for nominal if svs usecase does not exist.
	 * If the request is to set the bus_vote to _NONE,
	 * set it to _NONE irrespective of the requested vote
	 * from userspace.
	 */
	if (bv_index >= BUS_VOTE_MAX)
		bv_index = BUS_VOTE_MAX - 1;
	else if (bv_index < BUS_VOTE_NONE)
		bv_index = BUS_VOTE_NONE;

	for (i = 0; i < ARRAY_SIZE(mdwc->icc_paths); i++) {
		ret = icc_set_bw(mdwc->icc_paths[i],
				bus_vote_values[bv_index][i].avg,
				bus_vote_values[bv_index][i].peak);
		if (ret)
			dev_err(mdwc->dev, "bus bw voting path:%s bv:%d failed %d\n",
					icc_path_names[i], bv_index, ret);
	}

	dbg_event(0xFF, "bus_vote_end", bv_index);

	return ret;
}

static int dwc3_msm_suspend(struct dwc3_msm *mdwc, bool force_power_collapse,
					bool enable_wakeup)
{
	int ret;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct dwc3_event_buffer *evt;
	struct usb_irq *uirq;
	bool can_suspend_ssphy, no_active_ss;

	mutex_lock(&mdwc->suspend_resume_mutex);
	if (atomic_read(&dwc->in_lpm)) {
		dev_dbg(mdwc->dev, "%s: Already suspended\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return 0;
	}

	cancel_delayed_work_sync(&mdwc->perf_vote_work);
	msm_dwc3_perf_vote_update(mdwc, false);

	if (!mdwc->in_host_mode) {
		evt = dwc->ev_buf;
		if ((evt->flags & DWC3_EVENT_PENDING)) {
			dev_dbg(mdwc->dev,
				"%s: %d device events pending, abort suspend\n",
				__func__, evt->count / 4);
			mutex_unlock(&mdwc->suspend_resume_mutex);
			return -EBUSY;
		}
	}

	if (!mdwc->vbus_active && dwc->dr_mode == USB_DR_MODE_OTG &&
		mdwc->drd_state == DRD_STATE_PERIPHERAL) {
		/*
		 * In some cases, the pm_runtime_suspend may be called by
		 * usb_bam when there is pending lpm flag. However, if this is
		 * done when cable was disconnected and otg state has not
		 * yet changed to IDLE, then it means OTG state machine
		 * is running and we race against it. So cancel LPM for now,
		 * and OTG state machine will go for LPM later, after completing
		 * transition to IDLE state.
		 */
		dev_dbg(mdwc->dev,
			"%s: cable disconnected while not in idle otg state\n",
			__func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return -EBUSY;
	}

	/*
	 * Check if device is not in CONFIGURED state
	 * then check controller state of L2 and break
	 * LPM sequence. Check this for device bus suspend case.
	 */
	if ((dwc->dr_mode == USB_DR_MODE_OTG &&
			mdwc->drd_state == DRD_STATE_PERIPHERAL_SUSPEND) &&
			(dwc->gadget.state != USB_STATE_CONFIGURED)) {
		pr_err("%s(): Trying to go in LPM with state:%d\n",
					__func__, dwc->gadget.state);
		pr_err("%s(): LPM is not performed.\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return -EBUSY;
	}

	ret = dwc3_msm_prepare_suspend(mdwc, force_power_collapse);
	if (ret) {
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return ret;
	}

	/* Disable core irq */
	if (dwc->irq)
		disable_irq(dwc->irq);

	if (work_busy(&dwc->bh_work))
		dbg_event(0xFF, "pend evt", 0);

	/* disable power event irq, hs and ss phy irq is used as wake up src */
	disable_irq_nosync(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	dwc3_set_phy_speed_flags(mdwc);
	/* Suspend HS PHY */
	usb_phy_set_suspend(mdwc->hs_phy, 1);
	usb_phy_set_suspend(mdwc->hs_phy1, 1);

	/*
	 * Synopsys Superspeed PHY does not support ss_phy_irq, so to detect
	 * any wakeup events in host mode PHY cannot be suspended.
	 * This Superspeed PHY can be suspended only in the following cases:
	 *	1. The core is not in host mode
	 *	2. A Highspeed device is connected but not a Superspeed device
	 */
	no_active_ss = (!mdwc->in_host_mode) || (mdwc->in_host_mode &&
		((mdwc->hs_phy->flags & (PHY_HSFS_MODE | PHY_LS_MODE)) &&
			!dwc3_msm_is_superspeed(mdwc)));
	can_suspend_ssphy = dwc->maximum_speed >= USB_SPEED_SUPER &&
			(!(mdwc->use_pwr_event_for_wakeup & PWR_EVENT_SS_WAKEUP) || no_active_ss ||
			 !enable_wakeup);
	/* Suspend SS PHY */
	if (can_suspend_ssphy) {
		if (mdwc->in_host_mode) {
			u32 reg = dwc3_msm_read_reg(mdwc->base,
					DWC3_GUSB3PIPECTL(0));

			reg |= DWC3_GUSB3PIPECTL_DISRXDETU3;
			dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(0),
					reg);

			if (mdwc->dual_port) {
				reg = dwc3_msm_read_reg(mdwc->base,
						DWC3_GUSB3PIPECTL(1));
				reg |= DWC3_GUSB3PIPECTL_DISRXDETU3;
				dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(1),
						reg);
			}
		}
		/* indicate phy about SS mode */
		dwc3_msm_is_superspeed(mdwc);

		usb_phy_set_suspend(mdwc->ss_phy, 1);
		usb_phy_set_suspend(mdwc->ss_phy1, 1);
		mdwc->lpm_flags |= MDWC3_SS_PHY_SUSPEND;
	} else if (mdwc->use_pwr_event_for_wakeup & PWR_EVENT_SS_WAKEUP) {
		mdwc->lpm_flags |= MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP;
	}

	/*
	 * When operating in HS host mode, check if pwr event IRQ is
	 * required for wakeup.
	 */
	if (mdwc->in_host_mode && (mdwc->use_pwr_event_for_wakeup
						& PWR_EVENT_HS_WAKEUP))
		mdwc->lpm_flags |= MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP;

	if (mdwc->lpm_flags & MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP)
		dwc3_msm_set_pwr_events(mdwc, true);

	/* make sure above writes are completed before turning off clocks */
	wmb();

	/* Disable clocks */
	clk_disable_unprepare(mdwc->bus_aggr_clk);
	clk_disable_unprepare(mdwc->utmi_clk);

	clk_set_rate(mdwc->core_clk, 19200000);
	clk_disable_unprepare(mdwc->core_clk);
	clk_disable_unprepare(mdwc->noc_aggr_clk);
	clk_disable_unprepare(mdwc->noc_aggr_north_clk);
	clk_disable_unprepare(mdwc->noc_aggr_south_clk);
	clk_disable_unprepare(mdwc->noc_sys_clk);
	/*
	 * Disable iface_clk only after core_clk as core_clk has FSM
	 * depedency on iface_clk. Hence iface_clk should be turned off
	 * after core_clk is turned off.
	 */
	clk_disable_unprepare(mdwc->iface_clk);
	/* USB PHY no more requires TCXO */
	clk_disable_unprepare(mdwc->xo_clk);

	/* Perform controller power collapse */
	if (!(mdwc->in_host_mode || mdwc->in_device_mode) ||
	      mdwc->in_restart || force_power_collapse) {
		mdwc->lpm_flags |= MDWC3_POWER_COLLAPSE;
		dev_dbg(mdwc->dev, "%s: power collapse\n", __func__);
		dwc3_msm_config_gdsc(mdwc, 0);
		clk_disable_unprepare(mdwc->sleep_clk);
	}

	dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_NONE);

	/*
	 * if in_restart is marked as true from restart work do not release the wakeup
	 * active source as it can lead the device to enter system suspend (if usb is
	 * the last holding the wakeup active source) ; if actual cable disconnect happens
	 * while in_restart is true wakeup active source will be released from restart work.
	 */
	if (!mdwc->in_restart) {
		/*
		 * release wakeup source with timeout to defer system suspend to
		 * handle case where on USB cable disconnect, SUSPEND and DISCONNECT
		 * event is received.
		 */
		if (mdwc->lpm_to_suspend_delay) {
			dev_dbg(mdwc->dev, "defer suspend with %d(msecs)\n",
						mdwc->lpm_to_suspend_delay);
			pm_wakeup_event(mdwc->dev, mdwc->lpm_to_suspend_delay);
		} else {
			pm_relax(mdwc->dev);
		}
	}

	atomic_set(&dwc->in_lpm, 1);

	/*
	 * with the core in power collapse, we dont require wakeup
	 * using HS_PHY_IRQ or SS_PHY_IRQ. Hence enable wakeup only in
	 * case of host bus suspend and device bus suspend. Also in
	 * case of platforms with mpm interrupts and snps phy, enable
	 * dpse hsphy irq and dmse hsphy irq as done for pdc interrupts.
	 */
	if (!(mdwc->lpm_flags & MDWC3_POWER_COLLAPSE) && enable_wakeup) {
		if (mdwc->use_pdc_interrupts || !mdwc->wakeup_irq[HS_PHY_IRQ].irq) {
			configure_usb_wakeup_interrupts(mdwc, true);
		} else {
			uirq = &mdwc->wakeup_irq[HS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, true);
			uirq = &mdwc->wakeup_irq[SS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, true);
		}
		mdwc->lpm_flags |= MDWC3_ASYNC_IRQ_WAKE_CAPABILITY;
	}

	if (mdwc->lpm_flags & MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP)
		enable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	dev_info(mdwc->dev, "DWC3 in low power mode\n");
	dbg_event(0xFF, "Ctl Sus", atomic_read(&dwc->in_lpm));

	/* kick_sm if it is waiting for lpm sequence to finish */
	if (test_and_clear_bit(WAIT_FOR_LPM, &mdwc->inputs))
		queue_delayed_work(mdwc->sm_usb_wq, &mdwc->sm_work, 0);

	mutex_unlock(&mdwc->suspend_resume_mutex);

	return 0;
}

static int dwc3_msm_resume(struct dwc3_msm *mdwc)
{
	int ret;
	long core_clk_rate;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct usb_irq *uirq;

	dev_dbg(mdwc->dev, "%s: exiting lpm\n", __func__);

	/*
	 * If h/w exited LPM without any events, ensure
	 * h/w is reset before processing any new events.
	 */
	if (!mdwc->vbus_active && mdwc->id_state)
		set_bit(WAIT_FOR_LPM, &mdwc->inputs);

	mutex_lock(&mdwc->suspend_resume_mutex);
	if (!atomic_read(&dwc->in_lpm)) {
		dev_dbg(mdwc->dev, "%s: Already resumed\n", __func__);
		mutex_unlock(&mdwc->suspend_resume_mutex);
		return 0;
	}

	pm_stay_awake(mdwc->dev);

	if (mdwc->in_host_mode && mdwc->max_rh_port_speed == USB_SPEED_HIGH)
		dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_SVS);
	else
		dwc3_msm_update_bus_bw(mdwc, mdwc->default_bus_vote);

	/* Vote for TCXO while waking up USB HSPHY */
	ret = clk_prepare_enable(mdwc->xo_clk);
	if (ret)
		dev_err(mdwc->dev, "%s failed to vote TCXO buffer%d\n",
						__func__, ret);

	/* Restore controller power collapse */
	if (mdwc->lpm_flags & MDWC3_POWER_COLLAPSE) {
		dev_dbg(mdwc->dev, "%s: exit power collapse\n", __func__);
		dwc3_msm_config_gdsc(mdwc, 1);
		ret = reset_control_assert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "%s:core_reset assert failed\n",
					__func__);
		/* HW requires a short delay for reset to take place properly */
		usleep_range(1000, 1200);
		ret = reset_control_deassert(mdwc->core_reset);
		if (ret)
			dev_err(mdwc->dev, "%s:core_reset deassert failed\n",
					__func__);
		clk_prepare_enable(mdwc->sleep_clk);
	}

	/*
	 * Enable clocks
	 * Turned ON iface_clk before core_clk due to FSM depedency.
	 */
	clk_prepare_enable(mdwc->iface_clk);
	clk_prepare_enable(mdwc->noc_aggr_clk);
	clk_prepare_enable(mdwc->noc_aggr_north_clk);
	clk_prepare_enable(mdwc->noc_aggr_south_clk);
	clk_prepare_enable(mdwc->noc_sys_clk);

	core_clk_rate = mdwc->core_clk_rate;
	if (mdwc->in_host_mode && mdwc->max_rh_port_speed == USB_SPEED_HIGH) {
		core_clk_rate = mdwc->core_clk_rate_hs;
		dev_dbg(mdwc->dev, "%s: set hs core clk rate %ld\n", __func__,
			core_clk_rate);
	}

	clk_set_rate(mdwc->core_clk, core_clk_rate);
	clk_prepare_enable(mdwc->core_clk);

	clk_prepare_enable(mdwc->utmi_clk);
	clk_prepare_enable(mdwc->bus_aggr_clk);

	/*
	 * Disable any wakeup events that were enabled if pwr_event_irq
	 * is used as wakeup interrupt.
	 */
	if (mdwc->lpm_flags & MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP) {
		disable_irq_nosync(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);
		dwc3_msm_set_pwr_events(mdwc, false);
		mdwc->lpm_flags &= ~MDWC3_USE_PWR_EVENT_IRQ_FOR_WAKEUP;
	}

	/* Resume SS PHY */
	if (dwc->maximum_speed >= USB_SPEED_SUPER &&
			mdwc->lpm_flags & MDWC3_SS_PHY_SUSPEND) {
		dwc3_set_ssphy_orientation_flag(mdwc);
		usb_phy_set_suspend(mdwc->ss_phy1, 0);
		usb_phy_set_suspend(mdwc->ss_phy, 0);
		dwc3_msm_clear_ssphy_flags(mdwc, DEVICE_IN_SS_MODE);
		mdwc->lpm_flags &= ~MDWC3_SS_PHY_SUSPEND;

		if (mdwc->in_host_mode) {
			u32 reg = dwc3_msm_read_reg(mdwc->base,
					DWC3_GUSB3PIPECTL(0));

			reg &= ~DWC3_GUSB3PIPECTL_DISRXDETU3;
			dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(0),
					reg);

			if (mdwc->dual_port) {
				reg = dwc3_msm_read_reg(mdwc->base,
						DWC3_GUSB3PIPECTL(1));
				reg &= ~DWC3_GUSB3PIPECTL_DISRXDETU3;
				dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(1),
						reg);
			}
		}
	}

	dwc3_msm_clear_hsphy_flags(mdwc, PHY_HSFS_MODE | PHY_LS_MODE);
	/* Resume HS PHY */
	usb_phy_set_suspend(mdwc->hs_phy1, 0);
	usb_phy_set_suspend(mdwc->hs_phy, 0);

	/* Recover from controller power collapse */
	if (mdwc->lpm_flags & MDWC3_POWER_COLLAPSE) {
		dev_dbg(mdwc->dev, "%s: exit power collapse\n", __func__);

		dwc3_msm_power_collapse_por(mdwc);

		mdwc->lpm_flags &= ~MDWC3_POWER_COLLAPSE;
	}

	atomic_set(&dwc->in_lpm, 0);

	/* enable power evt irq for IN P3 detection */
	enable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	/* Disable HSPHY auto suspend */
	dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(0),
		dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(0)) &
				~DWC3_GUSB2PHYCFG_SUSPHY);

	if (mdwc->dual_port)
		dwc3_msm_write_reg(mdwc->base, DWC3_GUSB2PHYCFG(1),
			dwc3_msm_read_reg(mdwc->base, DWC3_GUSB2PHYCFG(1)) &
					~DWC3_GUSB2PHYCFG_SUSPHY);

	if (dwc->dis_u3_susphy_quirk) {
		dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(0),
			dwc3_msm_read_reg(mdwc->base, DWC3_GUSB3PIPECTL(0)) &
					~DWC3_GUSB3PIPECTL_SUSPHY);
		if (mdwc->dual_port)
			dwc3_msm_write_reg(mdwc->base, DWC3_GUSB3PIPECTL(1),
			   dwc3_msm_read_reg(mdwc->base, DWC3_GUSB3PIPECTL(1)) &
						~DWC3_GUSB3PIPECTL_SUSPHY);
	}

	/* Disable wakeup capable for HS_PHY IRQ & SS_PHY_IRQ if enabled */
	if (mdwc->lpm_flags & MDWC3_ASYNC_IRQ_WAKE_CAPABILITY) {
		if (mdwc->use_pdc_interrupts || !mdwc->wakeup_irq[HS_PHY_IRQ].irq) {
			configure_usb_wakeup_interrupts(mdwc, false);
		} else {
			uirq = &mdwc->wakeup_irq[HS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, false);
			uirq = &mdwc->wakeup_irq[SS_PHY_IRQ];
			configure_nonpdc_usb_interrupt(mdwc, uirq, false);
		}
		mdwc->lpm_flags &= ~MDWC3_ASYNC_IRQ_WAKE_CAPABILITY;
	}

	dev_info(mdwc->dev, "DWC3 exited from low power mode\n");

	/* Enable core irq */
	if (dwc->irq)
		enable_irq(dwc->irq);

	/*
	 * Handle other power events that could not have been handled during
	 * Low Power Mode
	 */
	if (!mdwc->dual_port)
		dwc3_pwr_event_handler(mdwc);

	if (pm_qos_request_active(&mdwc->pm_qos_req_dma))
		schedule_delayed_work(&mdwc->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));

	dbg_event(0xFF, "Ctl Res", atomic_read(&dwc->in_lpm));
	mutex_unlock(&mdwc->suspend_resume_mutex);

	return 0;
}

/**
 * dwc3_ext_event_notify - callback to handle events from external transceiver
 *
 * Returns 0 on success
 */
static void dwc3_ext_event_notify(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	/* Flush processing any pending events before handling new ones */
	flush_delayed_work(&mdwc->sm_work);

	dbg_log_string("enter: mdwc->inputs:%x hs_phy_flags:%x\n",
				mdwc->inputs, mdwc->hs_phy->flags);
	if (mdwc->id_state == DWC3_ID_FLOAT) {
		dbg_log_string("XCVR: ID set\n");
		set_bit(ID, &mdwc->inputs);
	} else {
		dbg_log_string("XCVR: ID clear\n");
		clear_bit(ID, &mdwc->inputs);
	}

	if (mdwc->vbus_active && !mdwc->in_restart) {
		if (mdwc->hs_phy->flags & EUD_SPOOF_DISCONNECT) {
			dbg_log_string("XCVR: BSV clear\n");
			clear_bit(B_SESS_VLD, &mdwc->inputs);
		} else {
			dbg_log_string("XCVR: BSV set\n");
			set_bit(B_SESS_VLD, &mdwc->inputs);
		}
	} else {
		dbg_log_string("XCVR: BSV clear\n");
		clear_bit(B_SESS_VLD, &mdwc->inputs);
	}

	if (mdwc->suspend) {
		dbg_log_string("XCVR: SUSP set\n");
		set_bit(B_SUSPEND, &mdwc->inputs);
	} else {
		dbg_log_string("XCVR: SUSP clear\n");
		clear_bit(B_SUSPEND, &mdwc->inputs);
	}

	if (mdwc->check_eud_state) {
		mdwc->hs_phy->flags &=
			~(EUD_SPOOF_CONNECT | EUD_SPOOF_DISCONNECT);
		dbg_log_string("eud: state:%d active:%d hs_phy_flags:0x%x\n",
			mdwc->check_eud_state, mdwc->eud_active,
			mdwc->hs_phy->flags);
		if (mdwc->eud_active) {
			mdwc->hs_phy->flags |= EUD_SPOOF_CONNECT;
			dbg_log_string("EUD: XCVR: BSV set\n");
			set_bit(B_SESS_VLD, &mdwc->inputs);
		} else {
			mdwc->hs_phy->flags |= EUD_SPOOF_DISCONNECT;
			dbg_log_string("EUD: XCVR: BSV clear\n");
			clear_bit(B_SESS_VLD, &mdwc->inputs);
		}

		mdwc->check_eud_state = false;
	}


	dbg_log_string("eud: state:%d active:%d hs_phy_flags:0x%x\n",
		mdwc->check_eud_state, mdwc->eud_active, mdwc->hs_phy->flags);

	/* handle case of USB cable disconnect after USB spoof disconnect */
	if (!mdwc->vbus_active &&
			(mdwc->hs_phy->flags & EUD_SPOOF_DISCONNECT)) {
		mdwc->hs_phy->flags &= ~EUD_SPOOF_DISCONNECT;
		mdwc->hs_phy->flags |= PHY_SUS_OVERRIDE;
		usb_phy_set_suspend(mdwc->hs_phy, 1);
		mdwc->hs_phy->flags &= ~PHY_SUS_OVERRIDE;
		return;
	}

	dbg_log_string("exit: mdwc->inputs:%x\n", mdwc->inputs);
	queue_delayed_work(mdwc->sm_usb_wq, &mdwc->sm_work, 0);
}

static void dwc3_resume_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm, resume_work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	union extcon_property_value val;
	unsigned int extcon_id;
	struct extcon_dev *edev = NULL;
	const char *edev_name;
	char *eud_str;
	int ret = 0;

	dev_dbg(mdwc->dev, "%s: dwc3 resume work\n", __func__);
	dbg_log_string("resume_work: ext_idx:%d\n", mdwc->ext_idx);
	if (mdwc->extcon && mdwc->vbus_active && !mdwc->in_restart) {
		extcon_id = EXTCON_USB;
		edev = mdwc->extcon[mdwc->ext_idx].edev;
	} else if (mdwc->extcon && mdwc->id_state == DWC3_ID_GROUND) {
		extcon_id = EXTCON_USB_HOST;
		edev = mdwc->extcon[mdwc->ext_idx].edev;
	}

	if (edev) {
		edev_name = extcon_get_edev_name(edev);
		dbg_log_string("edev:%s\n", edev_name);
		/* Skip querying speed and cc_state for EUD edev */
		eud_str = strnstr(edev_name, "eud", strlen(edev_name));
		if (eud_str)
			goto skip_update;
	}

	dwc->maximum_speed = dwc->max_hw_supp_speed;

	if (edev && extcon_get_state(edev, extcon_id)) {
		ret = extcon_get_property(edev, extcon_id,
				EXTCON_PROP_USB_SS, &val);

		if (!ret && val.intval == 0)
			dwc->maximum_speed = USB_SPEED_HIGH;
	}

	if (dwc->maximum_speed >= USB_SPEED_SUPER)
		dwc3_set_ssphy_orientation_flag(mdwc);

skip_update:
	dbg_log_string("max_speed:%d hw_supp_speed:%d override_speed:%d",
		dwc->maximum_speed, dwc->max_hw_supp_speed,
		mdwc->override_usb_speed);
	if (mdwc->override_usb_speed &&
			mdwc->override_usb_speed <= dwc->maximum_speed) {
		dwc->maximum_speed = mdwc->override_usb_speed;
		dwc->gadget.max_speed = dwc->maximum_speed;
	}

	dbg_event(0xFF, "speed", dwc->maximum_speed);

	/*
	 * Skip scheduling sm work if no work is pending. When boot-up
	 * with USB cable connected, usb state m/c is skipped to avoid
	 * any changes to dp/dm lines. As PM supsend and resume can
	 * happen while charger is connected, scheduling sm work during
	 * pm resume will reset the controller and phy which might impact
	 * dp/dm lines (and charging voltage).
	 */
	if (mdwc->drd_state == DRD_STATE_UNDEFINED &&
		!edev && !mdwc->resume_pending)
		return;
	/*
	 * exit LPM first to meet resume timeline from device side.
	 * resume_pending flag would prevent calling
	 * dwc3_msm_resume() in case we are here due to system
	 * wide resume without usb cable connected. This flag is set
	 * only in case of power event irq in lpm.
	 */
	if (mdwc->resume_pending) {
		dwc3_msm_resume(mdwc);
		mdwc->resume_pending = false;
	}

	if (atomic_read(&mdwc->pm_suspended)) {
		dbg_event(0xFF, "RWrk PMSus", 0);
		/* let pm resume kick in resume work later */
		return;
	}
	dwc3_ext_event_notify(mdwc);
}

static void dwc3_pwr_event_handler(struct dwc3_msm *mdwc)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	u32 irq_stat, irq_clear = 0;

	irq_stat = dwc3_msm_read_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG);
	dev_dbg(mdwc->dev, "%s irq_stat=%X\n", __func__, irq_stat);

	/* Check for P3 events */
	if ((irq_stat & PWR_EVNT_POWERDOWN_OUT_P3_MASK) &&
			(irq_stat & PWR_EVNT_POWERDOWN_IN_P3_MASK)) {
		u32 ls;

		/* Can't tell if entered or exit P3, so check LINKSTATE */
		if (dwc3_is_usb31(dwc))
			ls = dwc3_msm_read_reg_field(mdwc->base,
				DWC31_LINK_GDBGLTSSM(0),
				DWC3_GDBGLTSSM_LINKSTATE_MASK);
		else
			ls = dwc3_msm_read_reg_field(mdwc->base,
				DWC3_GDBGLTSSM, DWC3_GDBGLTSSM_LINKSTATE_MASK);
		dev_dbg(mdwc->dev, "%s link state = 0x%04x\n", __func__, ls);
		atomic_set(&mdwc->in_p3, ls == DWC3_LINK_STATE_U3);

		irq_stat &= ~(PWR_EVNT_POWERDOWN_OUT_P3_MASK |
				PWR_EVNT_POWERDOWN_IN_P3_MASK);
		irq_clear |= (PWR_EVNT_POWERDOWN_OUT_P3_MASK |
				PWR_EVNT_POWERDOWN_IN_P3_MASK);
	} else if (irq_stat & PWR_EVNT_POWERDOWN_OUT_P3_MASK) {
		atomic_set(&mdwc->in_p3, 0);
		irq_stat &= ~PWR_EVNT_POWERDOWN_OUT_P3_MASK;
		irq_clear |= PWR_EVNT_POWERDOWN_OUT_P3_MASK;
	} else if (irq_stat & PWR_EVNT_POWERDOWN_IN_P3_MASK) {
		atomic_set(&mdwc->in_p3, 1);
		irq_stat &= ~PWR_EVNT_POWERDOWN_IN_P3_MASK;
		irq_clear |= PWR_EVNT_POWERDOWN_IN_P3_MASK;
	}

	/* Handle exit from L1 events */
	if (irq_stat & PWR_EVNT_LPM_OUT_L1_MASK) {
		dev_dbg(mdwc->dev, "%s: handling PWR_EVNT_LPM_OUT_L1_MASK\n",
				__func__);
		if (usb_gadget_wakeup(&dwc->gadget))
			dev_err(mdwc->dev, "%s failed to take dwc out of L1\n",
					__func__);
		irq_stat &= ~PWR_EVNT_LPM_OUT_L1_MASK;
		irq_clear |= PWR_EVNT_LPM_OUT_L1_MASK;
	}

	/* Handle exit from L2 events */
	if (irq_stat & PWR_EVNT_LPM_OUT_L2_MASK) {
		dev_dbg(mdwc->dev, "%s: handling PWR_EVNT_LPM_OUT_L2_MASK\n",
				__func__);
		irq_stat &= ~PWR_EVNT_LPM_OUT_L2_MASK;
		irq_clear |= PWR_EVNT_LPM_OUT_L2_MASK;
	}
	/* Unhandled events */
	if (irq_stat)
		dev_dbg(mdwc->dev, "%s: unexpected PWR_EVNT, irq_stat=%X\n",
			__func__, irq_stat);

	dwc3_msm_write_reg(mdwc->base, PWR_EVNT_IRQ_STAT_REG, irq_clear);
}

static irqreturn_t msm_dwc3_pwr_irq_thread(int irq, void *_mdwc)
{
	struct dwc3_msm *mdwc = _mdwc;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(mdwc->dev, "%s\n", __func__);

	if (atomic_read(&dwc->in_lpm))
		dwc3_resume_work(&mdwc->resume_work);
	else
		dwc3_pwr_event_handler(mdwc);

	dbg_event(0xFF, "PWR IRQ", atomic_read(&dwc->in_lpm));
	return IRQ_HANDLED;
}

static irqreturn_t msm_dwc3_pwr_irq(int irq, void *data)
{
	struct dwc3_msm *mdwc = data;
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dwc->t_pwr_evt_irq = ktime_get();
	dev_dbg(mdwc->dev, "%s received\n", __func__);

	if (mdwc->drd_state == DRD_STATE_PERIPHERAL_SUSPEND) {
		dev_info(mdwc->dev, "USB Resume start\n");
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
		place_marker("M - USB device resume started");
#endif
	}

	/*
	 * When in Low Power Mode, can't read PWR_EVNT_IRQ_STAT_REG to acertain
	 * which interrupts have been triggered, as the clocks are disabled.
	 * Resume controller by waking up pwr event irq thread.After re-enabling
	 * clocks, dwc3_msm_resume will call dwc3_pwr_event_handler to handle
	 * all other power events.
	 */
	if (atomic_read(&dwc->in_lpm)) {
		/* set this to call dwc3_msm_resume() */
		mdwc->resume_pending = true;
		return IRQ_WAKE_THREAD;
	}

	dwc3_pwr_event_handler(mdwc);
	return IRQ_HANDLED;
}

static void dwc3_otg_sm_work(struct work_struct *w);
static int get_chg_type(struct dwc3_msm *mdwc);

static int dwc3_msm_get_clk_gdsc(struct dwc3_msm *mdwc)
{
	int ret;

	mdwc->dwc3_gdsc = devm_regulator_get(mdwc->dev, "USB3_GDSC");
	if (IS_ERR(mdwc->dwc3_gdsc)) {
		if (PTR_ERR(mdwc->dwc3_gdsc) == -EPROBE_DEFER)
			return PTR_ERR(mdwc->dwc3_gdsc);
		mdwc->dwc3_gdsc = NULL;
	}

	mdwc->xo_clk = devm_clk_get(mdwc->dev, "xo");
	if (IS_ERR(mdwc->xo_clk))
		mdwc->xo_clk = NULL;
	clk_set_rate(mdwc->xo_clk, 19200000);

	mdwc->iface_clk = devm_clk_get(mdwc->dev, "iface_clk");
	if (IS_ERR(mdwc->iface_clk)) {
		dev_err(mdwc->dev, "failed to get iface_clk\n");
		ret = PTR_ERR(mdwc->iface_clk);
		return ret;
	}

	/*
	 * DWC3 Core requires its CORE CLK (aka master / bus clk) to
	 * run at 125Mhz in SSUSB mode and >60MHZ for HSUSB mode.
	 * On newer platform it can run at 150MHz as well.
	 */
	mdwc->core_clk = devm_clk_get(mdwc->dev, "core_clk");
	if (IS_ERR(mdwc->core_clk)) {
		dev_err(mdwc->dev, "failed to get core_clk\n");
		ret = PTR_ERR(mdwc->core_clk);
		return ret;
	}

	mdwc->core_reset = devm_reset_control_get(mdwc->dev, "core_reset");
	if (IS_ERR(mdwc->core_reset)) {
		dev_err(mdwc->dev, "failed to get core_reset\n");
		return PTR_ERR(mdwc->core_reset);
	}

	if (of_property_read_u32(mdwc->dev->of_node, "qcom,core-clk-rate",
				(u32 *)&mdwc->core_clk_rate)) {
		dev_err(mdwc->dev, "USB core-clk-rate is not present\n");
		return -EINVAL;
	}

	mdwc->core_clk_rate = clk_round_rate(mdwc->core_clk,
							mdwc->core_clk_rate);
	dev_dbg(mdwc->dev, "USB core frequency = %ld\n",
						mdwc->core_clk_rate);
	ret = clk_set_rate(mdwc->core_clk, mdwc->core_clk_rate);
	if (ret)
		dev_err(mdwc->dev, "fail to set core_clk freq:%d\n", ret);

	if (of_property_read_u32(mdwc->dev->of_node, "qcom,core-clk-rate-hs",
				(u32 *)&mdwc->core_clk_rate_hs)) {
		dev_dbg(mdwc->dev, "USB core-clk-rate-hs is not present\n");
		mdwc->core_clk_rate_hs = mdwc->core_clk_rate;
	}

	mdwc->sleep_clk = devm_clk_get(mdwc->dev, "sleep_clk");
	if (IS_ERR(mdwc->sleep_clk)) {
		dev_err(mdwc->dev, "failed to get sleep_clk\n");
		ret = PTR_ERR(mdwc->sleep_clk);
		return ret;
	}

	clk_set_rate(mdwc->sleep_clk, 32000);
	mdwc->utmi_clk_rate = 19200000;
	mdwc->utmi_clk = devm_clk_get(mdwc->dev, "utmi_clk");
	if (IS_ERR(mdwc->utmi_clk)) {
		dev_err(mdwc->dev, "failed to get utmi_clk\n");
		ret = PTR_ERR(mdwc->utmi_clk);
		return ret;
	}

	clk_set_rate(mdwc->utmi_clk, mdwc->utmi_clk_rate);
	mdwc->bus_aggr_clk = devm_clk_get(mdwc->dev, "bus_aggr_clk");
	if (IS_ERR(mdwc->bus_aggr_clk))
		mdwc->bus_aggr_clk = NULL;

	mdwc->noc_aggr_clk = devm_clk_get(mdwc->dev, "noc_aggr_clk");
	if (IS_ERR(mdwc->noc_aggr_clk))
		mdwc->noc_aggr_clk = NULL;

	mdwc->noc_aggr_north_clk = devm_clk_get(mdwc->dev,
						"noc_aggr_north_clk");
	if (IS_ERR(mdwc->noc_aggr_north_clk))
		mdwc->noc_aggr_north_clk = NULL;

	mdwc->noc_aggr_south_clk = devm_clk_get(mdwc->dev,
						"noc_aggr_south_clk");
	if (IS_ERR(mdwc->noc_aggr_south_clk))
		mdwc->noc_aggr_south_clk = NULL;

	mdwc->noc_sys_clk = devm_clk_get(mdwc->dev, "noc_sys_clk");
	if (IS_ERR(mdwc->noc_sys_clk))
		mdwc->noc_sys_clk = NULL;

	if (of_property_match_string(mdwc->dev->of_node,
				"clock-names", "cfg_ahb_clk") >= 0) {
		mdwc->cfg_ahb_clk = devm_clk_get(mdwc->dev, "cfg_ahb_clk");
		if (IS_ERR(mdwc->cfg_ahb_clk)) {
			ret = PTR_ERR(mdwc->cfg_ahb_clk);
			mdwc->cfg_ahb_clk = NULL;
			if (ret != -EPROBE_DEFER)
				dev_err(mdwc->dev,
					"failed to get cfg_ahb_clk ret %d\n",
					ret);
			return ret;
		}
	}

	return 0;
}

static int dwc3_msm_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3 *dwc;
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb, id_nb);
	struct dwc3_msm *mdwc = enb->mdwc;
	enum dwc3_id_state id;

	if (!edev || !mdwc)
		return NOTIFY_DONE;

	dwc = platform_get_drvdata(mdwc->dwc3);

	dbg_event(0xFF, "extcon idx", enb->idx);

	id = event ? DWC3_ID_GROUND : DWC3_ID_FLOAT;

	if (mdwc->id_state == id)
		return NOTIFY_DONE;

	mdwc->ext_idx = enb->idx;

	dev_dbg(mdwc->dev, "host:%ld (id:%d) event received\n", event, id);

	mdwc->id_state = id;
	dbg_event(0xFF, "id_state", mdwc->id_state);
	queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return NOTIFY_DONE;
}

static void check_for_sdp_connection(struct work_struct *w)
{
	struct dwc3_msm *mdwc =
		container_of(w, struct dwc3_msm, sdp_check.work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (!mdwc->vbus_active)
		return;

	/* floating D+/D- lines detected */
	if (dwc->gadget.state < USB_STATE_DEFAULT &&
		dwc3_gadget_get_link_state(dwc) != DWC3_LINK_STATE_CMPLY) {
		mdwc->vbus_active = false;
		dbg_event(0xFF, "Q RW SPD CHK", mdwc->vbus_active);
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);
	}
}

#define DP_PULSE_WIDTH_MSEC 200

static int dwc3_msm_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3 *dwc;
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb, vbus_nb);
	struct dwc3_msm *mdwc = enb->mdwc;
	char *eud_str;
	const char *edev_name;
	bool is_cdp;

	if (!edev || !mdwc)
		return NOTIFY_DONE;

	dwc = platform_get_drvdata(mdwc->dwc3);

	dbg_event(0xFF, "extcon idx", enb->idx);
	dev_dbg(mdwc->dev, "vbus:%ld event received\n", event);
	edev_name = extcon_get_edev_name(edev);
	dbg_log_string("edev:%s\n", edev_name);

	/* detect USB spoof disconnect/connect notification with EUD device */
	eud_str = strnstr(edev_name, "eud", strlen(edev_name));
	if (eud_str) {
		if (mdwc->eud_active == event)
			return NOTIFY_DONE;
		mdwc->eud_active = event;
		mdwc->check_eud_state = true;
	} else {
		if (mdwc->vbus_active == event)
			return NOTIFY_DONE;
		mdwc->vbus_active = event;
	}

	/*
	 * In case of ADSP based charger detection driving a pulse on
	 * DP to ensure proper CDP detection will be taken care by
	 * ADSP.
	 */
	is_cdp = ((mdwc->apsd_source == IIO) &&
		(get_chg_type(mdwc) == POWER_SUPPLY_TYPE_USB_CDP)) ||
		((mdwc->apsd_source == PSY) &&
		(get_chg_type(mdwc) == POWER_SUPPLY_USB_TYPE_CDP));

	/*
	 * Drive a pulse on DP to ensure proper CDP detection
	 * and only when the vbus connect event is a valid one.
	 */
	if (is_cdp && mdwc->vbus_active && !mdwc->check_eud_state) {
		dev_dbg(mdwc->dev, "Connected to CDP, pull DP up\n");
		mdwc->hs_phy->charger_detect(mdwc->hs_phy);
	}

	mdwc->ext_idx = enb->idx;
	if (dwc->dr_mode == USB_DR_MODE_OTG && !mdwc->in_restart)
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return NOTIFY_DONE;
}

static int dwc3_msm_extcon_register(struct dwc3_msm *mdwc)
{
	struct device_node *node = mdwc->dev->of_node;
	struct extcon_dev *edev;
	int idx, extcon_cnt, ret = 0;
	bool check_vbus_state, check_id_state, phandle_found = false;

	extcon_cnt = of_count_phandle_with_args(node, "extcon", NULL);
	if (extcon_cnt < 0) {
		dev_err(mdwc->dev, "of_count_phandle_with_args failed\n");
		return -ENODEV;
	}

	mdwc->extcon = devm_kcalloc(mdwc->dev, extcon_cnt,
					sizeof(*mdwc->extcon), GFP_KERNEL);
	if (!mdwc->extcon)
		return -ENOMEM;

	for (idx = 0; idx < extcon_cnt; idx++) {
		edev = extcon_get_edev_by_phandle(mdwc->dev, idx);
		if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV)
			return PTR_ERR(edev);

		if (IS_ERR_OR_NULL(edev))
			continue;

		check_vbus_state = check_id_state = true;
		phandle_found = true;

		mdwc->extcon[idx].mdwc = mdwc;
		mdwc->extcon[idx].edev = edev;
		mdwc->extcon[idx].idx = idx;

		mdwc->extcon[idx].vbus_nb.notifier_call =
						dwc3_msm_vbus_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB,
						&mdwc->extcon[idx].vbus_nb);
		if (ret < 0)
			check_vbus_state = false;

		mdwc->extcon[idx].id_nb.notifier_call = dwc3_msm_id_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
						&mdwc->extcon[idx].id_nb);
		if (ret < 0)
			check_id_state = false;

		/* Update initial VBUS/ID state */
		if (check_vbus_state && extcon_get_state(edev, EXTCON_USB))
			dwc3_msm_vbus_notifier(&mdwc->extcon[idx].vbus_nb,
						true, edev);
		else  if (check_id_state &&
				extcon_get_state(edev, EXTCON_USB_HOST))
			dwc3_msm_id_notifier(&mdwc->extcon[idx].id_nb,
						true, edev);
	}

	if (!phandle_found) {
		dev_err(mdwc->dev, "no extcon device found\n");
		return -ENODEV;
	}

	return 0;
}

static inline const char *usb_role_string(enum usb_role role)
{
	if (role < ARRAY_SIZE(usb_role_strings))
		return usb_role_strings[role];

	return "Invalid";
}

static enum usb_role dwc3_msm_usb_get_role(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	enum usb_role role;

	if (mdwc->vbus_active)
		role = USB_ROLE_DEVICE;
	else if (mdwc->id_state == DWC3_ID_GROUND)
		role = USB_ROLE_HOST;
	else
		role = USB_ROLE_NONE;

	dbg_log_string("get_role:%s\n", usb_role_string(role));
	return role;
}

static int dwc3_msm_usb_set_role(struct device *dev, enum usb_role role)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	enum usb_role cur_role = USB_ROLE_NONE;

	cur_role = dwc3_msm_usb_get_role(dev);

	switch (role) {
	case USB_ROLE_HOST:
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_GROUND;
		break;

	case USB_ROLE_DEVICE:
		mdwc->vbus_active = true;
		mdwc->id_state = DWC3_ID_FLOAT;
		break;

	case USB_ROLE_NONE:
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_FLOAT;
		break;
	}

	dbg_log_string("cur_role:%s new_role:%s\n", usb_role_string(cur_role),
						usb_role_string(role));

	/*
	 * For boot up without USB cable connected case, don't check
	 * previous role value to allow resetting USB controller and
	 * PHYs.
	 */
	if (mdwc->drd_state != DRD_STATE_UNDEFINED && cur_role == role) {
		dbg_log_string("no USB role change");
		return 0;
	}

	if (mdwc->ss_release_called) {
		flush_delayed_work(&mdwc->sm_work);
		dwc->maximum_speed = USB_SPEED_HIGH;
		if (role == USB_ROLE_NONE) {
			dwc->maximum_speed = USB_SPEED_UNKNOWN;
			mdwc->ss_release_called = false;
		}
	}

	dwc3_ext_event_notify(mdwc);
	return 0;
}

static struct usb_role_switch_desc role_desc = {
	.set = dwc3_msm_usb_set_role,
	.get = dwc3_msm_usb_get_role,
	.allow_userspace_control = true,
};

static ssize_t orientation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (mdwc->orientation_override == PHY_LANE_A)
		return scnprintf(buf, PAGE_SIZE, "A\n");
	if (mdwc->orientation_override == PHY_LANE_B)
		return scnprintf(buf, PAGE_SIZE, "B\n");

	return scnprintf(buf, PAGE_SIZE, "none\n");
}

static ssize_t orientation_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "A"))
		mdwc->orientation_override = PHY_LANE_A;
	else if (sysfs_streq(buf, "B"))
		mdwc->orientation_override = PHY_LANE_B;
	else
		mdwc->orientation_override = 0;

	return count;
}

static DEVICE_ATTR_RW(orientation);

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (mdwc->vbus_active)
		return scnprintf(buf, PAGE_SIZE, "peripheral\n");
	if (mdwc->id_state == DWC3_ID_GROUND)
		return scnprintf(buf, PAGE_SIZE, "host\n");

	return scnprintf(buf, PAGE_SIZE, "none\n");
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (sysfs_streq(buf, "peripheral")) {
		if (dwc->dr_mode == USB_DR_MODE_HOST) {
			dev_err(dev, "Core supports host mode only.\n");
			return -EINVAL;
		}

		mdwc->vbus_active = true;
		mdwc->id_state = DWC3_ID_FLOAT;
	} else if (sysfs_streq(buf, "host")) {
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_GROUND;
	} else {
		mdwc->vbus_active = false;
		mdwc->id_state = DWC3_ID_FLOAT;
	}

	dwc3_ext_event_notify(mdwc);

	return count;
}

static DEVICE_ATTR_RW(mode);
static void msm_dwc3_perf_vote_work(struct work_struct *w);

/* This node only shows max speed supported dwc3 and it should be
 * same as what is reported in udc/core.c max_speed node. For current
 * operating gadget speed, query current_speed node which is implemented
 * by udc/core.c
 */
static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			usb_speed_string(dwc->maximum_speed));
}

static ssize_t speed_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	enum usb_device_speed req_speed = USB_SPEED_UNKNOWN;

	/* DEVSPD can only have values SS(0x4), HS(0x0) and FS(0x1).
	 * per 3.20a data book. Allow only these settings. Note that,
	 * xhci does not support full-speed only mode.
	 */
	if (sysfs_streq(buf, "full"))
		req_speed = USB_SPEED_FULL;
	else if (sysfs_streq(buf, "high"))
		req_speed = USB_SPEED_HIGH;
	else if (sysfs_streq(buf, "super"))
		req_speed = USB_SPEED_SUPER;
	else if (sysfs_streq(buf, "ssp"))
		req_speed = USB_SPEED_SUPER_PLUS;
	else
		return -EINVAL;

	/* restart usb only works for device mode. Perform manual cable
	 * plug in/out for host mode restart.
	 */
	if (req_speed != dwc->maximum_speed &&
			req_speed <= dwc->max_hw_supp_speed) {
		mdwc->override_usb_speed = req_speed;
		schedule_work(&mdwc->restart_usb_work);
	} else if (req_speed >= dwc->max_hw_supp_speed) {
		mdwc->override_usb_speed = 0;
	}

	return count;
}
static DEVICE_ATTR_RW(speed);

static ssize_t bus_vote_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);

	if (mdwc->override_bus_vote == BUS_VOTE_MIN)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			"Fixed bus vote: min");
	else if (mdwc->override_bus_vote == BUS_VOTE_MAX)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			"Fixed bus vote: max");
	else
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			"Do not have fixed bus vote");
}

static ssize_t bus_vote_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	bool bv_fixed = false;
	enum bus_vote bv;

	if (sysfs_streq(buf, "min")) {
		bv_fixed = true;
		mdwc->override_bus_vote = BUS_VOTE_MIN;
	} else if (sysfs_streq(buf, "max")) {
		bv_fixed = true;
		mdwc->override_bus_vote = BUS_VOTE_MAX;
	} else if (sysfs_streq(buf, "cancel")) {
		bv_fixed = false;
		mdwc->override_bus_vote = BUS_VOTE_NONE;
	} else {
		dev_err(dev, "min/max/cancel only.\n");
		return -EINVAL;
	}

	/* Update bus vote value only when not suspend */
	if (!atomic_read(&dwc->in_lpm)) {
		if (bv_fixed)
			bv = mdwc->override_bus_vote;
		else if (mdwc->in_host_mode
			&& (mdwc->max_rh_port_speed == USB_SPEED_HIGH))
			bv = BUS_VOTE_SVS;
		else
			bv = mdwc->default_bus_vote;

		dwc3_msm_update_bus_bw(mdwc, bv);
	}

	return count;
}
static DEVICE_ATTR_RW(bus_vote);

static int dwc3_msm_interconnect_vote_populate(struct dwc3_msm *mdwc)
{
	int ret_nom = 0, i = 0, j = 0, count = 0;
	int ret_svs = 0, ret = 0;
	u32 *vv_nom, *vv_svs;

	count = of_property_count_strings(mdwc->dev->of_node,
						"interconnect-names");
	if (count < 0) {
		dev_err(mdwc->dev, "No interconnects found.\n");
		return -EINVAL;
	}

	/* 2 signifies the two types of values avg & peak */
	vv_nom = kzalloc(count * 2 * sizeof(*vv_nom), GFP_KERNEL);
	if (!vv_nom)
		return -ENOMEM;

	vv_svs = kzalloc(count * 2 * sizeof(*vv_svs), GFP_KERNEL);
	if (!vv_svs)
		return -ENOMEM;

	/* of_property_read_u32_array returns 0 on success */
	ret_nom = of_property_read_u32_array(mdwc->dev->of_node,
				"qcom,interconnect-values-nom",
					vv_nom, count * 2);
	if (ret_nom) {
		dev_err(mdwc->dev, "Nominal values not found.\n");
		ret = ret_nom;
		goto icc_err;
	}

	ret_svs = of_property_read_u32_array(mdwc->dev->of_node,
				"qcom,interconnect-values-svs",
					vv_svs, count * 2);
	if (ret_svs) {
		dev_err(mdwc->dev, "Svs values not found.\n");
		ret = ret_svs;
		goto icc_err;
	}

	for (i = USB_DDR; i < count && i < USB_MAX_PATH; i++) {
		/* Updating votes NOMINAL */
		bus_vote_values[BUS_VOTE_NOMINAL][i].avg
						= vv_nom[j];
		bus_vote_values[BUS_VOTE_NOMINAL][i].peak
						= vv_nom[j+1];
		/* Updating votes SVS */
		bus_vote_values[BUS_VOTE_SVS][i].avg
						= vv_svs[j];
		bus_vote_values[BUS_VOTE_SVS][i].peak
						= vv_svs[j+1];
		j += 2;
	}
icc_err:
	/* freeing the temporary resource */
	kfree(vv_nom);
	kfree(vv_svs);

	return ret;
}

static int dwc_dpdm_cb(struct notifier_block *nb, unsigned long evt, void *p)
{
	struct dwc3_msm *mdwc = container_of(nb, struct dwc3_msm, dpdm_nb);

	switch (evt) {
	case REGULATOR_EVENT_ENABLE:
		dev_dbg(mdwc->dev, "%s: enable state:%s\n", __func__,
				dwc3_drd_state_string(mdwc->drd_state));
		break;
	case REGULATOR_EVENT_DISABLE:
		dev_dbg(mdwc->dev, "%s: disable state:%s\n", __func__,
				dwc3_drd_state_string(mdwc->drd_state));
		if (mdwc->drd_state == DRD_STATE_UNDEFINED)
			queue_delayed_work(mdwc->sm_usb_wq, &mdwc->sm_work, 0);
		break;
	default:
		dev_dbg(mdwc->dev, "%s: unknown event state:%s\n", __func__,
				dwc3_drd_state_string(mdwc->drd_state));
		break;
	}

	return NOTIFY_OK;
}

static void dwc3_init_dbm(struct dwc3_msm *mdwc)
{
	const char *dbm_ver;
	int ret;

	ret = of_property_read_string(mdwc->dev->of_node, "qcom,dbm-version",
			&dbm_ver);
	if (!ret && !strcmp(dbm_ver, "1.4")) {
		mdwc->dbm_reg_table = dbm_1_4_regtable;
		mdwc->dbm_num_eps = DBM_1_4_NUM_EP;
		mdwc->dbm_is_1p4 = true;
	} else {
		/* default to v1.5 register layout */
		mdwc->dbm_reg_table = dbm_1_5_regtable;
		mdwc->dbm_num_eps = DBM_1_5_NUM_EP;
	}

	mdwc->dbm_reset_ep_after_lpm = of_property_read_bool(mdwc->dev->of_node,
			"qcom,reset-ep-after-lpm-resume");
}

static void dwc3_start_stop_host(struct dwc3_msm *mdwc, bool start)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (start) {
		dbg_log_string("start host mode");
		mdwc->id_state = DWC3_ID_GROUND;
		mdwc->vbus_active = false;
	} else {
		dbg_log_string("stop_host_mode started");
		mdwc->id_state = DWC3_ID_FLOAT;
		mdwc->vbus_active = false;
	}

	dwc3_ext_event_notify(mdwc);
	dbg_event(0xFF, "flush_work", 0);
	flush_work(&mdwc->resume_work);
	drain_workqueue(mdwc->sm_usb_wq);
	if (start)
		dbg_log_string("host mode started");
	else
		dbg_log_string("stop_host_mode completed");
}

static void dwc3_start_stop_device(struct dwc3_msm *mdwc, bool start)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	if (start) {
		dbg_log_string("start device mode");
		mdwc->id_state = DWC3_ID_FLOAT;
		mdwc->vbus_active = true;
	} else {
		dbg_log_string("stop device mode");
		mdwc->id_state = DWC3_ID_FLOAT;
		mdwc->vbus_active = false;
	}

	dwc3_ext_event_notify(mdwc);
	dbg_event(0xFF, "flush_work", 0);
	flush_work(&mdwc->resume_work);
	drain_workqueue(mdwc->sm_usb_wq);
	if (start)
		dbg_log_string("device mode restarted");
	else
		dbg_log_string("stop_device_mode completed");
}

int dwc3_msm_release_ss_lane(struct device *dev, bool usb_dp_concurrent_mode)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = NULL;

	if (mdwc == NULL) {
		dev_err(dev, "dwc3-msm is not initialized yet.\n");
		return -EAGAIN;
	}

	dwc = platform_get_drvdata(mdwc->dwc3);
	if (dwc == NULL) {
		dev_err(dev, "dwc3 controller is not initialized yet.\n");
		return -EAGAIN;
	}

	/*
	 * If the MPA connected is multi_func capable set the flag assuming
	 * that USB and DP is operating in concurrent mode and bail out early.
	 */
	if (usb_dp_concurrent_mode) {
		mdwc->ss_phy->flags |= PHY_USB_DP_CONCURRENT_MODE;
		dbg_event(0xFF, "USB_DP_CONCURRENT_MODE", 1);
		return 0;
	}

	dbg_event(0xFF, "ss_lane_release", 0);
	/* flush any pending work */
	flush_work(&mdwc->resume_work);
	drain_workqueue(mdwc->sm_usb_wq);

	redriver_release_usb_lanes(mdwc->ss_redriver_node);

	mdwc->ss_release_called = true;
	if (mdwc->id_state == DWC3_ID_GROUND) {
		/* stop USB host mode */
		dwc3_start_stop_host(mdwc, false);
		/* restart USB host mode into high speed */
		dwc->maximum_speed = USB_SPEED_HIGH;
		dwc3_start_stop_host(mdwc, true);
	} else if (mdwc->vbus_active) {
		/* stop USB device mode */
		dwc3_start_stop_device(mdwc, false);
		/* restart USB device mode into high speed */
		dwc->maximum_speed = USB_SPEED_HIGH;
		dwc3_start_stop_device(mdwc, true);
	} else {
		dbg_log_string("USB is not active.\n");
		dwc->maximum_speed = USB_SPEED_HIGH;
	}

	return 0;
}
EXPORT_SYMBOL(dwc3_msm_release_ss_lane);

static int dwc3_msm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node, *dwc3_node;
	struct device	*dev = &pdev->dev;
	struct dwc3_msm *mdwc;
	struct dwc3	*dwc;
	struct resource *res;
	int ret = 0, size = 0, i;
	u32 val;

	mdwc = devm_kzalloc(&pdev->dev, sizeof(*mdwc), GFP_KERNEL);
	if (!mdwc)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdwc);
	mdwc->dev = &pdev->dev;

	INIT_LIST_HEAD(&mdwc->req_complete_list);
	INIT_WORK(&mdwc->resume_work, dwc3_resume_work);
	INIT_WORK(&mdwc->restart_usb_work, dwc3_restart_usb_work);
	INIT_WORK(&mdwc->vbus_draw_work, dwc3_msm_vbus_draw_work);
	INIT_DELAYED_WORK(&mdwc->sm_work, dwc3_otg_sm_work);
	INIT_DELAYED_WORK(&mdwc->perf_vote_work, msm_dwc3_perf_vote_work);
	INIT_DELAYED_WORK(&mdwc->sdp_check, check_for_sdp_connection);

	mdwc->dwc3_wq = alloc_ordered_workqueue("dwc3_wq", 0);
	if (!mdwc->dwc3_wq) {
		pr_err("%s: Unable to create workqueue dwc3_wq\n", __func__);
		return -ENOMEM;
	}

	/*
	 * Create an ordered freezable workqueue for sm_work so that it gets
	 * scheduled only after pm_resume has happened completely. This helps
	 * in avoiding race conditions between xhci_plat_resume and
	 * xhci_runtime_resume and also between hcd disconnect and xhci_resume.
	 */
	mdwc->sm_usb_wq = alloc_ordered_workqueue("k_sm_usb", WQ_FREEZABLE);
	if (!mdwc->sm_usb_wq) {
		destroy_workqueue(mdwc->dwc3_wq);
		return -ENOMEM;
	}

	/* Get all clks and gdsc reference */
	ret = dwc3_msm_get_clk_gdsc(mdwc);
	if (ret) {
		dev_err(&pdev->dev, "error getting clock or gdsc.\n");
		goto err;
	}

	mdwc->id_state = DWC3_ID_FLOAT;
	set_bit(ID, &mdwc->inputs);

	mdwc->dual_port = of_property_read_bool(node, "qcom,dual-port");

	ret = of_property_read_u32(node, "qcom,lpm-to-suspend-delay-ms",
				&mdwc->lpm_to_suspend_delay);
	if (ret) {
		dev_dbg(&pdev->dev, "setting lpm_to_suspend_delay to zero.\n");
		mdwc->lpm_to_suspend_delay = 0;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core_base");
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err;
	}

	mdwc->base = devm_ioremap_nocache(&pdev->dev, res->start,
			resource_size(res));
	if (!mdwc->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"ahb2phy_base");
	if (res) {
		mdwc->ahb2phy_base = devm_ioremap_nocache(&pdev->dev,
					res->start, resource_size(res));
		if (IS_ERR_OR_NULL(mdwc->ahb2phy_base)) {
			dev_err(dev, "couldn't find ahb2phy_base addr.\n");
			mdwc->ahb2phy_base = NULL;
		} else {
			/*
			 * On some targets cfg_ahb_clk depends upon usb gdsc
			 * regulator. If cfg_ahb_clk is enabled without
			 * turning on usb gdsc regulator clk is stuck off.
			 */
			dwc3_msm_config_gdsc(mdwc, 1);
			clk_prepare_enable(mdwc->cfg_ahb_clk);
			/* Configure AHB2PHY for one wait state read/write*/
			val = readl_relaxed(mdwc->ahb2phy_base +
					PERIPH_SS_AHB2PHY_TOP_CFG);
			if (val != ONE_READ_WRITE_WAIT) {
				writel_relaxed(ONE_READ_WRITE_WAIT,
					mdwc->ahb2phy_base +
					PERIPH_SS_AHB2PHY_TOP_CFG);
				/* complete above write before using USB PHY */
				mb();
			}
			clk_disable_unprepare(mdwc->cfg_ahb_clk);
			dwc3_msm_config_gdsc(mdwc, 0);
		}
	}

	if (of_property_read_u32(node, "qcom,dwc-usb3-msm-tx-fifo-size",
				 &mdwc->tx_fifo_size))
		dev_err(&pdev->dev,
			"unable to read platform data tx fifo size\n");

	ret = of_property_read_u32(node, "qcom,num-gsi-evt-buffs",
				&mdwc->num_gsi_event_buffers);

	if (mdwc->num_gsi_event_buffers) {
		of_get_property(node, "qcom,gsi-reg-offset", &size);
		if (size) {
			mdwc->gsi_reg = devm_kzalloc(dev, size, GFP_KERNEL);
			if (!mdwc->gsi_reg)
				return -ENOMEM;

			mdwc->gsi_reg_offset_cnt =
					(size / sizeof(*mdwc->gsi_reg));
			if (mdwc->gsi_reg_offset_cnt != GSI_REG_MAX) {
				dev_err(dev, "invalid reg offset count\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,gsi-reg-offset", mdwc->gsi_reg,
				mdwc->gsi_reg_offset_cnt);
		} else {
			dev_err(dev, "err provide qcom,gsi-reg-offset\n");
			return -EINVAL;
		}
	}

	mdwc->use_pdc_interrupts = of_property_read_bool(node,
				"qcom,use-pdc-interrupts");
	dwc3_set_notifier(&dwc3_msm_notify_event);

	if (of_property_read_bool(node, "qcom,iommu-best-fit-algo"))
		iommu_dma_enable_best_fit_algo(dev);

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))) {
		dev_err(&pdev->dev, "setting DMA mask to 64 failed.\n");
		if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32))) {
			dev_err(&pdev->dev, "setting DMA mask to 32 failed.\n");
			ret = -EOPNOTSUPP;
			goto err;
		}
	}

	/* Assumes dwc3 is the first DT child of dwc3-msm */
	dwc3_node = of_get_next_available_child(node, NULL);
	if (!dwc3_node) {
		dev_err(&pdev->dev, "failed to find dwc3 child\n");
		ret = -ENODEV;
		goto err;
	}

	ret = of_platform_populate(node, NULL, NULL, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev,
				"failed to add create dwc3 core\n");
		of_node_put(dwc3_node);
		goto err;
	}

	mdwc->dwc3 = of_find_device_by_node(dwc3_node);
	of_node_put(dwc3_node);
	if (!mdwc->dwc3) {
		dev_err(&pdev->dev, "failed to get dwc3 platform device\n");
		goto put_dwc3;
	}

	mdwc->hs_phy = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 0);
	if (IS_ERR(mdwc->hs_phy)) {
		dev_err(&pdev->dev, "unable to get hsphy device\n");
		ret = PTR_ERR(mdwc->hs_phy);
		goto put_dwc3;
	}
	mdwc->ss_phy = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 1);
	if (IS_ERR(mdwc->ss_phy)) {
		dev_err(&pdev->dev, "unable to get ssphy device\n");
		ret = PTR_ERR(mdwc->ss_phy);
		goto put_dwc3;
	}

	if (mdwc->dual_port) {
		mdwc->hs_phy1 = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 2);
		if (IS_ERR(mdwc->hs_phy1)) {
			dev_err(&pdev->dev, "unable to get hsphy1 device\n");
			ret = PTR_ERR(mdwc->hs_phy1);
			goto put_dwc3;
		}

		mdwc->ss_phy1 = devm_usb_get_phy_by_phandle(&mdwc->dwc3->dev,
							"usb-phy", 3);
		if (IS_ERR(mdwc->ss_phy1)) {
			dev_err(&pdev->dev, "unable to get ssphy1 device\n");
			ret = PTR_ERR(mdwc->ss_phy1);
			goto put_dwc3;
		}
	}

	ret = dwc3_msm_interconnect_vote_populate(mdwc);
	if (ret)
		dev_err(&pdev->dev, "Dynamic voting failed\n");

	/* use default as nominal bus voting */
	mdwc->default_bus_vote = BUS_VOTE_NOMINAL;
	ret = of_property_read_u32(node, "qcom,default-bus-vote",
			&mdwc->default_bus_vote);

	if (mdwc->default_bus_vote >= BUS_VOTE_MAX)
		mdwc->default_bus_vote = BUS_VOTE_MAX - 1;
	else if (mdwc->default_bus_vote < BUS_VOTE_NONE)
		mdwc->default_bus_vote = BUS_VOTE_NONE;

	for (i = 0; i < ARRAY_SIZE(mdwc->icc_paths); i++) {
		mdwc->icc_paths[i] = of_icc_get(&pdev->dev, icc_path_names[i]);
		if (IS_ERR(mdwc->icc_paths[i]))
			mdwc->icc_paths[i] = NULL;
	}

	dwc = platform_get_drvdata(mdwc->dwc3);
	if (!dwc) {
		dev_err(&pdev->dev, "Failed to get dwc3 device\n");
		goto put_dwc3;
	}

	for (i = 0; i < USB_MAX_IRQ; i++) {
		mdwc->wakeup_irq[i].irq = platform_get_irq_byname(pdev,
					usb_irq_info[i].name);
		/* pwr_evnt_irq is mandatory for cores allowing SSPHY suspend */
		if (mdwc->wakeup_irq[i].irq < 0) {
			if (!dwc->dis_u3_susphy_quirk &&
					usb_irq_info[i].required) {
				dev_err(&pdev->dev, "get_irq for %s failed\n\n",
						usb_irq_info[i].name);
				ret = -EINVAL;
				goto put_dwc3;
			}
			mdwc->wakeup_irq[i].irq = 0;
		} else {
			irq_set_status_flags(mdwc->wakeup_irq[i].irq,
						IRQ_NOAUTOEN);

			ret = devm_request_threaded_irq(&pdev->dev,
					mdwc->wakeup_irq[i].irq,
					msm_dwc3_pwr_irq,
					msm_dwc3_pwr_irq_thread,
					usb_irq_info[i].irq_type,
					usb_irq_info[i].name, mdwc);
			if (ret) {
				dev_err(&pdev->dev, "irq req %s failed: %d\n\n",
						usb_irq_info[i].name, ret);
				goto put_dwc3;
			}
		}
	}

	dwc3_init_dbm(mdwc);

	/* Add power event if the dbm indicates coming out of L1 by interrupt */
	if (!mdwc->dbm_is_1p4) {
		/* pwr_evnt_irq is mandatory for cores allowing SSPHY suspend */
		if (!dwc->dis_u3_susphy_quirk &&
				!mdwc->wakeup_irq[PWR_EVNT_IRQ].irq) {
			dev_err(&pdev->dev,
				"need pwr_event_irq exiting L1\n");
			ret = -EINVAL;
			goto put_dwc3;
		}
	}

	if (of_property_read_bool(node, "qcom,ignore-wakeup-src-in-hostmode")) {
		dwc->ignore_wakeup_src_in_hostmode = true;
		dev_dbg(mdwc->dev, "%s: Allow system suspend irrespective of runtime suspend\n",
								__func__);
	}

	/*
	 * On platforms with SS PHY that do not support ss_phy_irq for wakeup
	 * events, use pwr_event_irq for wakeup events in superspeed mode.
	 */
	if (dwc->maximum_speed >= USB_SPEED_SUPER
			&& !mdwc->wakeup_irq[SS_PHY_IRQ].irq)
		mdwc->use_pwr_event_for_wakeup |= PWR_EVENT_SS_WAKEUP;

	/*
	 * On platforms with mpm interrupts and snps phy, when operating in
	 * HS host mode use power event irq for wakeup events as GIC is not
	 * capable to detect falling edge of dp/dm hsphy irq.
	 */
	if (!mdwc->use_pdc_interrupts && !mdwc->wakeup_irq[HS_PHY_IRQ].irq)
		mdwc->use_pwr_event_for_wakeup |= PWR_EVENT_HS_WAKEUP;

	/*
	 * Clocks and regulators will not be turned on until the first time
	 * runtime PM resume is called. This is to allow for booting up with
	 * charger already connected so as not to disturb PHY line states.
	 */
	mdwc->lpm_flags = MDWC3_POWER_COLLAPSE | MDWC3_SS_PHY_SUSPEND;
	atomic_set(&dwc->in_lpm, 1);
	pm_runtime_set_autosuspend_delay(mdwc->dev, 1000);
	pm_runtime_use_autosuspend(mdwc->dev);
	/* Skip creating device wakeup node if remote wakeup is not a requirement*/
	if (!dwc->ignore_wakeup_src_in_hostmode)
		device_init_wakeup(mdwc->dev, 1);

	if (of_property_read_bool(node, "qcom,disable-dev-mode-pm"))
		pm_runtime_get_noresume(mdwc->dev);

	ret = of_property_read_u32(node, "qcom,pm-qos-latency",
				&mdwc->pm_qos_latency);
	if (ret) {
		dev_dbg(&pdev->dev, "setting pm-qos-latency to zero.\n");
		mdwc->pm_qos_latency = 0;
	}

	if (mdwc->dual_port && dwc->dr_mode != USB_DR_MODE_HOST) {
		dev_err(&pdev->dev, "Dual port not allowed for DRD core\n");
		goto put_dwc3;
	}

	dwc->dual_port = mdwc->dual_port;

	mutex_init(&mdwc->suspend_resume_mutex);

	mdwc->ss_redriver_node = of_parse_phandle(node, "ssusb_redriver", 0);

	if (of_property_read_bool(node, "usb-role-switch")) {
		role_desc.fwnode = dev_fwnode(&pdev->dev);
		mdwc->role_switch = usb_role_switch_register(mdwc->dev,
								&role_desc);
		if (IS_ERR(mdwc->role_switch)) {
			ret = PTR_ERR(mdwc->role_switch);
			goto put_dwc3;
		}
	}

	/* Check charger detection type to obtain charger type */
	if (of_get_property(mdwc->dev->of_node, "io-channel-names", NULL))
		mdwc->apsd_source = IIO;
	else if (of_get_property(mdwc->dev->of_node, "usb-role-switch", NULL))
		mdwc->apsd_source = REMOTE_PROC;
	else
		mdwc->apsd_source = PSY;

	if (of_property_read_bool(node, "extcon")) {
		ret = dwc3_msm_extcon_register(mdwc);
		if (ret)
			goto put_dwc3;

		/*
		 * dpdm regulator will be turned on to perform apsd
		 * (automatic power source detection). dpdm regulator is
		 * used to float (or high-z) dp/dm lines. Do not reset
		 * controller/phy if regulator is turned on.
		 * if dpdm is not present controller can be reset
		 * as this controller may not be used for charger detection.
		 */
		mdwc->dpdm_reg = devm_regulator_get_optional(&pdev->dev,
				"dpdm");
		if (IS_ERR(mdwc->dpdm_reg)) {
			dev_dbg(mdwc->dev, "assume cable is not connected\n");
			mdwc->dpdm_reg = NULL;
		}

		if (!mdwc->vbus_active && mdwc->dpdm_reg &&
				regulator_is_enabled(mdwc->dpdm_reg)) {
			mdwc->dpdm_nb.notifier_call = dwc_dpdm_cb;
			regulator_register_notifier(mdwc->dpdm_reg,
					&mdwc->dpdm_nb);
		} else {
			if (!mdwc->role_switch)
				queue_delayed_work(mdwc->sm_usb_wq,
							&mdwc->sm_work, 0);
		}
	}

	if (!mdwc->role_switch && !mdwc->extcon) {
		switch (dwc->dr_mode) {
		case USB_DR_MODE_OTG:
			if (of_property_read_bool(node,
						"qcom,default-mode-host")) {
				dev_dbg(mdwc->dev, "%s: start host mode\n",
								__func__);
				mdwc->id_state = DWC3_ID_GROUND;
			} else if (of_property_read_bool(node,
						"qcom,default-mode-none")) {
				dev_dbg(mdwc->dev, "%s: stay in none mode\n",
								__func__);
			} else {
				dev_dbg(mdwc->dev, "%s: start peripheral mode\n",
								__func__);
				mdwc->vbus_active = true;
			}
			break;
		case USB_DR_MODE_HOST:
			mdwc->id_state = DWC3_ID_GROUND;
			break;
		case USB_DR_MODE_PERIPHERAL:
			/* fall through */
		default:
			mdwc->vbus_active = true;
			dwc->vbus_active = true;
			break;
		}

		dwc3_ext_event_notify(mdwc);
	}

	device_create_file(&pdev->dev, &dev_attr_orientation);
	device_create_file(&pdev->dev, &dev_attr_mode);
	device_create_file(&pdev->dev, &dev_attr_speed);
	device_create_file(&pdev->dev, &dev_attr_bus_vote);

	return 0;

put_dwc3:
	usb_role_switch_unregister(mdwc->role_switch);
	of_node_put(mdwc->ss_redriver_node);
	platform_device_put(mdwc->dwc3);
	for (i = 0; i < ARRAY_SIZE(mdwc->icc_paths); i++)
		icc_put(mdwc->icc_paths[i]);

	of_platform_depopulate(&pdev->dev);
err:
	destroy_workqueue(mdwc->sm_usb_wq);
	destroy_workqueue(mdwc->dwc3_wq);
	return ret;
}

static int dwc3_msm_remove(struct platform_device *pdev)
{
	struct dwc3_msm	*mdwc = platform_get_drvdata(pdev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int i, ret_pm;

	usb_role_switch_unregister(mdwc->role_switch);
	of_node_put(mdwc->ss_redriver_node);
	device_remove_file(&pdev->dev, &dev_attr_mode);
	device_remove_file(&pdev->dev, &dev_attr_speed);
	device_remove_file(&pdev->dev, &dev_attr_bus_vote);

	if (mdwc->dpdm_nb.notifier_call) {
		regulator_unregister_notifier(mdwc->dpdm_reg, &mdwc->dpdm_nb);
		mdwc->dpdm_nb.notifier_call = NULL;
	}

	if (mdwc->usb_psy)
		power_supply_put(mdwc->usb_psy);

	/*
	 * In case of system suspend, pm_runtime_get_sync fails.
	 * Hence turn ON the clocks manually.
	 */
	ret_pm = pm_runtime_get_sync(mdwc->dev);
	dbg_event(0xFF, "Remov gsyn", ret_pm);
	if (ret_pm < 0) {
		dev_err(mdwc->dev,
			"pm_runtime_get_sync failed with %d\n", ret_pm);
		clk_prepare_enable(mdwc->noc_aggr_north_clk);
		clk_prepare_enable(mdwc->noc_aggr_south_clk);
		clk_prepare_enable(mdwc->noc_sys_clk);
		clk_prepare_enable(mdwc->noc_aggr_clk);
		clk_prepare_enable(mdwc->utmi_clk);
		clk_prepare_enable(mdwc->core_clk);
		clk_prepare_enable(mdwc->iface_clk);
		clk_prepare_enable(mdwc->sleep_clk);
		clk_prepare_enable(mdwc->bus_aggr_clk);
		clk_prepare_enable(mdwc->xo_clk);
	}

	cancel_delayed_work_sync(&mdwc->perf_vote_work);
	cancel_delayed_work_sync(&mdwc->sm_work);

	dwc3_msm_clear_hsphy_flags(mdwc, PHY_HOST_MODE);
	dbg_event(0xFF, "Remov put", 0);
	platform_device_put(mdwc->dwc3);
	of_platform_depopulate(&pdev->dev);

	pm_runtime_disable(mdwc->dev);
	pm_runtime_barrier(mdwc->dev);
	pm_runtime_put_sync(mdwc->dev);
	pm_runtime_set_suspended(mdwc->dev);
	device_wakeup_disable(mdwc->dev);

	for (i = 0; i < ARRAY_SIZE(mdwc->icc_paths); i++)
		icc_put(mdwc->icc_paths[i]);

	if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
		regulator_disable(mdwc->vbus_reg);

	if (mdwc->wakeup_irq[HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[DP_HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[DP_HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[DM_HS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[DM_HS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[SS_PHY_IRQ].irq)
		disable_irq(mdwc->wakeup_irq[SS_PHY_IRQ].irq);
	if (mdwc->wakeup_irq[DP_HS_PHY_IRQ_1].irq)
		disable_irq(mdwc->wakeup_irq[DP_HS_PHY_IRQ_1].irq);
	if (mdwc->wakeup_irq[DM_HS_PHY_IRQ_1].irq)
		disable_irq(mdwc->wakeup_irq[DM_HS_PHY_IRQ_1].irq);
	if (mdwc->wakeup_irq[SS_PHY_IRQ_1].irq)
		disable_irq(mdwc->wakeup_irq[SS_PHY_IRQ_1].irq);
	disable_irq(mdwc->wakeup_irq[PWR_EVNT_IRQ].irq);

	clk_disable_unprepare(mdwc->utmi_clk);
	clk_set_rate(mdwc->core_clk, 19200000);
	clk_disable_unprepare(mdwc->core_clk);
	clk_disable_unprepare(mdwc->iface_clk);
	clk_disable_unprepare(mdwc->sleep_clk);
	clk_disable_unprepare(mdwc->xo_clk);

	dwc3_msm_config_gdsc(mdwc, 0);

	destroy_workqueue(mdwc->sm_usb_wq);
	destroy_workqueue(mdwc->dwc3_wq);

	return 0;
}

static int dwc3_msm_host_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct dwc3_msm *mdwc = container_of(nb, struct dwc3_msm, host_nb);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	struct usb_device *udev = ptr;

	if (event != USB_DEVICE_ADD && event != USB_DEVICE_REMOVE)
		return NOTIFY_DONE;

	/*
	 * STAR: 9001378493: SSPHY1 going in and out of P3 when HS transfers
	 * being done on port 0. We do not need the below workaround of
	 * corresponding SSPHY powerdown for multiport controller. Instead, we
	 * will keep the SSPHY autosuspend disabled when USB is in resumed
	 * state. Since dual port controller is present only on automotive
	 * platforms, power leakage is not a concern. Also, as a part of USB
	 * suspend sequence, we will enable autosuspend for SSPHYs to go to P3.
	 */
	if (dwc->dis_u3_susphy_quirk)
		return NOTIFY_DONE;

	/*
	 * For direct-attach devices, new udev is direct child of root hub
	 * i.e. dwc -> xhci -> root_hub -> udev
	 * root_hub's udev->parent==NULL, so traverse struct device hierarchy
	 */
	if (udev->parent && !udev->parent->parent &&
			udev->dev.parent->parent == &dwc->xhci->dev) {
		if (event == USB_DEVICE_ADD && udev->actconfig) {
			if (!dwc3_msm_is_ss_rhport_connected(mdwc)) {
				/*
				 * Core clock rate can be reduced only if root
				 * hub SS port is not enabled/connected.
				 */
				clk_set_rate(mdwc->core_clk,
				mdwc->core_clk_rate_hs);
				dev_dbg(mdwc->dev,
					"set hs core clk rate %ld\n",
					mdwc->core_clk_rate_hs);
				mdwc->max_rh_port_speed = USB_SPEED_HIGH;
				dwc3_msm_update_bus_bw(mdwc, BUS_VOTE_SVS);
			} else {
				mdwc->max_rh_port_speed = USB_SPEED_SUPER;
			}
		} else {
			/* set rate back to default core clk rate */
			clk_set_rate(mdwc->core_clk, mdwc->core_clk_rate);
			dev_dbg(mdwc->dev, "set core clk rate %ld\n",
				mdwc->core_clk_rate);
			mdwc->max_rh_port_speed = USB_SPEED_UNKNOWN;
			dwc3_msm_update_bus_bw(mdwc, mdwc->default_bus_vote);
		}
	}

	return NOTIFY_DONE;
}

static void msm_dwc3_perf_vote_update(struct dwc3_msm *mdwc, bool perf_mode)
{
	int latency = mdwc->pm_qos_latency;

	if ((mdwc->perf_mode == perf_mode) || !latency)
		return;

	if (perf_mode)
		pm_qos_update_request(&mdwc->pm_qos_req_dma, latency);
	else
		pm_qos_update_request(&mdwc->pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);

	mdwc->perf_mode = perf_mode;
	pr_debug("%s: latency updated to: %d\n", __func__,
			perf_mode ? latency : PM_QOS_DEFAULT_VALUE);
}

static void msm_dwc3_perf_vote_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm,
						perf_vote_work.work);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	unsigned int irq_cnt = atomic_xchg(&dwc->irq_cnt, 0);
	bool in_perf_mode = false;

	if (irq_cnt >= PM_QOS_THRESHOLD)
		in_perf_mode = true;

	pr_debug("%s: in_perf_mode:%u, interrupts in last sample:%lu\n",
		 __func__, in_perf_mode, irq_cnt);

	msm_dwc3_perf_vote_update(mdwc, in_perf_mode);
	schedule_delayed_work(&mdwc->perf_vote_work,
			msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
}

#define VBUS_REG_CHECK_DELAY	(msecs_to_jiffies(1000))

/**
 * dwc3_otg_start_host -  helper function for starting/stopping the host
 * controller driver.
 *
 * @mdwc: Pointer to the dwc3_msm structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_host(struct dwc3_msm *mdwc, int on)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);
	int ret = 0;

	/*
	 * The vbus_reg pointer could have multiple values
	 * NULL: regulator_get() hasn't been called, or was previously deferred
	 * IS_ERR: regulator could not be obtained, so skip using it
	 * Valid pointer otherwise
	 */
	if (!mdwc->vbus_reg) {
		mdwc->vbus_reg = devm_regulator_get_optional(mdwc->dev,
					"vbus_dwc3");
		if (IS_ERR(mdwc->vbus_reg) &&
				PTR_ERR(mdwc->vbus_reg) == -EPROBE_DEFER) {
			/* regulators may not be ready, so retry again later */
			mdwc->vbus_reg = NULL;
			return -EPROBE_DEFER;
		}
	}

	if (on) {
		dev_dbg(mdwc->dev, "%s: turn on host\n", __func__);
		dwc3_msm_set_hsphy_flags(mdwc, PHY_HOST_MODE);
		dbg_event(0xFF, "hs_phy_flag:%x", mdwc->hs_phy->flags);
		pm_runtime_get_sync(mdwc->dev);
		dbg_event(0xFF, "StrtHost gync",
			atomic_read(&mdwc->dev->power.usage_count));
		redriver_notify_connect(mdwc->ss_redriver_node);
		if (dwc->maximum_speed >= USB_SPEED_SUPER) {
			dwc3_msm_set_ssphy_flags(mdwc, PHY_HOST_MODE);
			usb_phy_notify_connect(mdwc->ss_phy,
						USB_SPEED_SUPER);
			usb_phy_notify_connect(mdwc->ss_phy1,
						USB_SPEED_SUPER);
		}

		usb_phy_notify_connect(mdwc->hs_phy, USB_SPEED_HIGH);
		usb_phy_notify_connect(mdwc->hs_phy1, USB_SPEED_HIGH);
		if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
			ret = regulator_enable(mdwc->vbus_reg);
		if (ret) {
			dev_err(mdwc->dev, "unable to enable vbus_reg\n");
			dwc3_msm_clear_hsphy_flags(mdwc, PHY_HOST_MODE);
			dwc3_msm_clear_ssphy_flags(mdwc, PHY_HOST_MODE);
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "vregerr psync",
				atomic_read(&mdwc->dev->power.usage_count));
			return ret;
		}


		mdwc->host_nb.notifier_call = dwc3_msm_host_notifier;
#ifdef CONFIG_USB
		usb_register_notify(&mdwc->host_nb);
#endif

		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_HOST);
		if (!dwc->dis_enblslpm_quirk)
			dwc3_en_sleep_mode(mdwc);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(mdwc->dev,
				"%s: failed to add XHCI pdev ret=%d\n",
				__func__, ret);
			if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
				regulator_disable(mdwc->vbus_reg);

			dwc3_msm_clear_hsphy_flags(mdwc, PHY_HOST_MODE);
			dwc3_msm_clear_ssphy_flags(mdwc, PHY_HOST_MODE);
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "pdeverr psync",
				atomic_read(&mdwc->dev->power.usage_count));
			usb_unregister_notify(&mdwc->host_nb);
			return ret;
		}

		mdwc->in_host_mode = true;
		if (!dwc->dis_u3_susphy_quirk) {
			dwc3_msm_write_reg_field(mdwc->base, DWC3_GUSB3PIPECTL(0),
					DWC3_GUSB3PIPECTL_SUSPHY, 1);
			if (mdwc->dual_port) {
				dwc3_msm_write_reg_field(mdwc->base, DWC3_GUSB3PIPECTL(1),
						DWC3_GUSB3PIPECTL_SUSPHY, 1);
			}
		}

		/* Reduce the U3 exit handshake timer from 8us to approximately
		 * 300ns to avoid lfps handshake interoperability issues
		 */
		if (dwc->revision == DWC3_USB31_REVISION_170A) {
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN2_U3_EXIT_RSP_RX_CLK_MASK, 6);
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN1_U3_EXIT_RSP_RX_CLK_MASK, 5);
			dev_dbg(mdwc->dev, "link0 LU3:%08x\n",
				dwc3_msm_read_reg(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0)));

			if (mdwc->dual_port) {
				dwc3_msm_write_reg_field(mdwc->base,
					       DWC31_LINK_LU3LFPSRXTIM(1),
					       GEN2_U3_EXIT_RSP_RX_CLK_MASK, 6);
				dwc3_msm_write_reg_field(mdwc->base,
					       DWC31_LINK_LU3LFPSRXTIM(1),
					       GEN1_U3_EXIT_RSP_RX_CLK_MASK, 5);
				dev_dbg(mdwc->dev, "link1 LU3:%08x\n",
					dwc3_msm_read_reg(mdwc->base,
					       DWC31_LINK_LU3LFPSRXTIM(1)));
			}
		}

		/* xHCI should have incremented child count as necessary */
		dbg_event(0xFF, "StrtHost psync",
			atomic_read(&mdwc->dev->power.usage_count));
		pm_runtime_mark_last_busy(mdwc->dev);
		pm_runtime_put_sync_autosuspend(mdwc->dev);
		pm_qos_add_request(&mdwc->pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		/* start in perf mode for better performance initially */
		msm_dwc3_perf_vote_update(mdwc, true);
		schedule_delayed_work(&mdwc->perf_vote_work,
				msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	} else {
		dev_dbg(mdwc->dev, "%s: turn off host\n", __func__);

		if (!IS_ERR_OR_NULL(mdwc->vbus_reg))
			ret = regulator_disable(mdwc->vbus_reg);
		if (ret) {
			dev_err(mdwc->dev, "unable to disable vbus_reg\n");
			return ret;
		}

		cancel_delayed_work_sync(&mdwc->perf_vote_work);
		msm_dwc3_perf_vote_update(mdwc, false);
		pm_qos_remove_request(&mdwc->pm_qos_req_dma);

		pm_runtime_get_sync(mdwc->dev);
		dbg_event(0xFF, "StopHost gsync",
			atomic_read(&mdwc->dev->power.usage_count));
		usb_phy_notify_disconnect(mdwc->hs_phy1, USB_SPEED_HIGH);
		usb_phy_notify_disconnect(mdwc->hs_phy, USB_SPEED_HIGH);
		if (dwc->maximum_speed >= USB_SPEED_SUPER) {
			usb_phy_notify_disconnect(mdwc->ss_phy1,
					USB_SPEED_SUPER);
			usb_phy_notify_disconnect(mdwc->ss_phy,
					USB_SPEED_SUPER);
			mdwc->ss_phy->flags &= ~PHY_USB_DP_CONCURRENT_MODE;
		}
		redriver_notify_disconnect(mdwc->ss_redriver_node);

		dwc3_msm_clear_ssphy_flags(mdwc, PHY_HOST_MODE);
		dwc3_msm_clear_hsphy_flags(mdwc, PHY_HOST_MODE);
		dwc3_host_exit(dwc);
#ifdef CONFIG_USB
		usb_unregister_notify(&mdwc->host_nb);
#endif

		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		if (!dwc->dis_u3_susphy_quirk) {
			dwc3_msm_write_reg_field(mdwc->base, DWC3_GUSB3PIPECTL(0),
					DWC3_GUSB3PIPECTL_SUSPHY, 0);
			if (mdwc->dual_port) {
				dwc3_msm_write_reg_field(mdwc->base, DWC3_GUSB3PIPECTL(1),
						DWC3_GUSB3PIPECTL_SUSPHY, 0);
			}
		}
		mdwc->in_host_mode = false;

		/* wait for LPM, to ensure h/w is reset after stop_host */
		set_bit(WAIT_FOR_LPM, &mdwc->inputs);

		pm_runtime_put_sync_suspend(mdwc->dev);
		dbg_event(0xFF, "StopHost psync",
			atomic_read(&mdwc->dev->power.usage_count));
	}

	return 0;
}

static void dwc3_override_vbus_status(struct dwc3_msm *mdwc, bool vbus_present)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	/* Update OTG VBUS Valid from HSPHY to controller */
	dwc3_msm_write_reg_field(mdwc->base, HS_PHY_CTRL_REG,
			UTMI_OTG_VBUS_VALID, !!vbus_present);

	/* Update only if Super Speed is supported */
	if (dwc->maximum_speed >= USB_SPEED_SUPER) {
		/* Update VBUS Valid from SSPHY to controller */
		dwc3_msm_write_reg_field(mdwc->base, SS_PHY_CTRL_REG,
			LANE0_PWR_PRESENT, !!vbus_present);
	}
}

/**
 * dwc3_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @mdwc: Pointer to the dwc3_msm structure.
 * @on:   Turn ON/OFF the gadget.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int dwc3_otg_start_peripheral(struct dwc3_msm *mdwc, int on)
{
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	pm_runtime_get_sync(mdwc->dev);
	dbg_event(0xFF, "StrtGdgt gsync",
		atomic_read(&mdwc->dev->power.usage_count));

	if (on) {
		dev_dbg(mdwc->dev, "%s: turn on gadget %s\n",
					__func__, dwc->gadget.name);

		dwc3_override_vbus_status(mdwc, true);
		redriver_notify_connect(mdwc->ss_redriver_node);
		usb_phy_notify_connect(mdwc->hs_phy, USB_SPEED_HIGH);
		usb_phy_notify_connect(mdwc->ss_phy, USB_SPEED_SUPER);

		/*
		 * Core reset is not required during start peripheral. Only
		 * DBM reset is required, hence perform only DBM reset here.
		 */
		dwc3_msm_block_reset(mdwc, false);
		dwc3_set_prtcap(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		dwc3_dis_sleep_mode(mdwc);
		mdwc->in_device_mode = true;

		/* Reduce the U3 exit handshake timer from 8us to approximately
		 * 300ns to avoid lfps handshake interoperability issues
		 */
		if (dwc->revision == DWC3_USB31_REVISION_170A) {
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN2_U3_EXIT_RSP_RX_CLK_MASK, 6);
			dwc3_msm_write_reg_field(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0),
					GEN1_U3_EXIT_RSP_RX_CLK_MASK, 5);
			dev_dbg(mdwc->dev, "LU3:%08x\n",
				dwc3_msm_read_reg(mdwc->base,
					DWC31_LINK_LU3LFPSRXTIM(0)));
		}

		usb_gadget_vbus_connect(&dwc->gadget);
		pm_qos_add_request(&mdwc->pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		/* start in perf mode for better performance initially */
		msm_dwc3_perf_vote_update(mdwc, true);
		schedule_delayed_work(&mdwc->perf_vote_work,
				msecs_to_jiffies(1000 * PM_QOS_SAMPLE_SEC));
	} else {
		dev_dbg(mdwc->dev, "%s: turn off gadget %s\n",
					__func__, dwc->gadget.name);
		cancel_delayed_work_sync(&mdwc->perf_vote_work);
		msm_dwc3_perf_vote_update(mdwc, false);
		pm_qos_remove_request(&mdwc->pm_qos_req_dma);

		mdwc->in_device_mode = false;
		usb_gadget_vbus_disconnect(&dwc->gadget);
		/*
		 * Clearing err_evt_seen after disconnect ensures that interrupts
		 * are ignored if err_evt_seen is set
		 */
		dwc->err_evt_seen = false;
		usb_phy_notify_disconnect(mdwc->hs_phy, USB_SPEED_HIGH);
		usb_phy_notify_disconnect(mdwc->ss_phy, USB_SPEED_SUPER);
		redriver_notify_disconnect(mdwc->ss_redriver_node);
		mdwc->ss_phy->flags &= ~PHY_USB_DP_CONCURRENT_MODE;
		dwc3_override_vbus_status(mdwc, false);
		dwc3_msm_write_reg_field(mdwc->base, DWC3_GUSB3PIPECTL(0),
				DWC3_GUSB3PIPECTL_SUSPHY, 0);

		/* wait for LPM, to ensure h/w is reset after stop_peripheral */
		set_bit(WAIT_FOR_LPM, &mdwc->inputs);
	}

	pm_runtime_put_sync(mdwc->dev);
	dbg_event(0xFF, "StopGdgt psync",
		atomic_read(&mdwc->dev->power.usage_count));

	return 0;
}

static int get_chg_type(struct dwc3_msm *mdwc)
{
	int ret, value = 0;
	union power_supply_propval pval = {0};

	switch (mdwc->apsd_source) {
	case IIO:
		if (!mdwc->chg_type) {
			mdwc->chg_type = devm_iio_channel_get(mdwc->dev,
						"chg_type");
			if (IS_ERR_OR_NULL(mdwc->chg_type)) {
				dev_dbg(mdwc->dev,
					"unable to get iio channel\n");
				mdwc->chg_type = NULL;
				return -ENODEV;
			}
		}

		ret = iio_read_channel_processed(mdwc->chg_type, &value);
		if (ret < 0) {
			dev_err(mdwc->dev, "failed to get charger type\n");
			return ret;
		}
		break;
	case PSY:
		if (!mdwc->usb_psy) {
			mdwc->usb_psy = power_supply_get_by_name("usb");
			if (!mdwc->usb_psy) {
				dev_err(mdwc->dev, "Could not get usb psy\n");
				return -ENODEV;
			}
		}

		power_supply_get_property(mdwc->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &pval);
		value = pval.intval;
		break;
	default:
		return -EINVAL;
	}

	return value;
}

static int dwc3_msm_gadget_vbus_draw(struct dwc3_msm *mdwc, unsigned int mA)
{
	union power_supply_propval pval = {0};
	int ret, chg_type;

	if (!mdwc->usb_psy && of_property_read_bool(mdwc->dev->of_node,
					"qcom,usb-charger")) {
		mdwc->usb_psy = power_supply_get_by_name("usb");
		if (!mdwc->usb_psy) {
			dev_err(mdwc->dev, "Could not get usb psy\n");
			return -ENODEV;
		}
	}

	if (!mdwc->usb_psy)
		return 0;

	chg_type = get_chg_type(mdwc);
	if (chg_type == QTI_POWER_SUPPLY_TYPE_USB_FLOAT) {
		/*
		 * Do not notify charger driver for any current and
		 * bail out if suspend happened with float cable
		 * connected
		 */
		if (mA == 2)
			return 0;

		if (!mA)
			pval.intval = -ETIMEDOUT;
		else
			pval.intval = 1000 * mA;
		goto set_prop;
	}

	/* Do not set current multiple times */
	if (mdwc->max_power == mA)
		return 0;

	/*
	 * Set the valid current only when the device
	 * is connected to a Standard Downstream Port.
	 * For ADSP based charger detection set current
	 * for all charger types. For psy based charger
	 * detection power_supply_usb_type enum is
	 * returned from pmic while for iio based charger
	 * detection power_supply_type enum is returned.
	 */
	if (mdwc->apsd_source == PSY && chg_type != POWER_SUPPLY_USB_TYPE_SDP)
		return 0;

	if (mdwc->apsd_source == IIO && chg_type != POWER_SUPPLY_TYPE_USB)
		return 0;

	/* Set max current limit in uA */
	pval.intval = 1000 * mA;

set_prop:
	dev_info(mdwc->dev, "Avail curr from USB = %u\n", mA);
	ret = power_supply_set_property(mdwc->usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &pval);
	if (ret) {
		dev_dbg(mdwc->dev, "power supply error when setting property\n");
		return ret;
	}

	mdwc->max_power = mA;
	return 0;
}

/**
 * dwc3_otg_sm_work - workqueue function.
 *
 * @w: Pointer to the dwc3 otg workqueue
 *
 * NOTE: After any change in drd_state, we must reschdule the state machine.
 */
static void dwc3_otg_sm_work(struct work_struct *w)
{
	struct dwc3_msm *mdwc = container_of(w, struct dwc3_msm, sm_work.work);
	struct dwc3 *dwc = NULL;
	bool work = false;
	int ret = 0;
	unsigned long delay = 0;
	const char *state;

	if (mdwc->dwc3)
		dwc = platform_get_drvdata(mdwc->dwc3);

	if (!dwc) {
		dev_err(mdwc->dev, "dwc is NULL.\n");
		return;
	}

	state = dwc3_drd_state_string(mdwc->drd_state);
	dev_dbg(mdwc->dev, "%s state\n", state);
	dbg_event(0xFF, state, 0);

	/* Check OTG state */
	switch (mdwc->drd_state) {
	case DRD_STATE_UNDEFINED:
		if (mdwc->dpdm_nb.notifier_call) {
			regulator_unregister_notifier(mdwc->dpdm_reg,
					&mdwc->dpdm_nb);
			mdwc->dpdm_nb.notifier_call = NULL;
		}

		/* put controller and phy in suspend if no cable connected */
		if (test_bit(ID, &mdwc->inputs) &&
				!test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dbg_event(0xFF, "undef_id_!bsv", 0);
			dwc3_msm_resume(mdwc);
			pm_runtime_set_active(mdwc->dev);
			pm_runtime_enable(mdwc->dev);
			pm_runtime_get_noresume(mdwc->dev);
			pm_runtime_put_sync(mdwc->dev);
			dbg_event(0xFF, "Undef NoUSB",
				atomic_read(&mdwc->dev->power.usage_count));
			mdwc->drd_state = DRD_STATE_IDLE;
			break;
		}

		dbg_event(0xFF, "Exit UNDEF", 0);
		mdwc->drd_state = DRD_STATE_IDLE;
		pm_runtime_set_suspended(mdwc->dev);
		pm_runtime_enable(mdwc->dev);
		/* fall-through */
	case DRD_STATE_IDLE:
		if (test_bit(WAIT_FOR_LPM, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "still not in lpm, wait.\n");
			break;
		}

		if (!test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "!id\n");
			mdwc->drd_state = DRD_STATE_HOST_IDLE;
			work = true;
		} else if (test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "b_sess_vld\n");
			if (get_chg_type(mdwc) == QTI_POWER_SUPPLY_TYPE_USB_FLOAT)
				queue_delayed_work(mdwc->dwc3_wq,
						&mdwc->sdp_check,
				msecs_to_jiffies(SDP_CONNECTION_CHECK_TIME));
			/*
			 * Increment pm usage count upon cable connect. Count
			 * is decremented in DRD_STATE_PERIPHERAL state on
			 * cable disconnect or in bus suspend.
			 */
			pm_runtime_get_sync(mdwc->dev);
			dbg_event(0xFF, "BIDLE gsync",
				atomic_read(&mdwc->dev->power.usage_count));
			dwc3_otg_start_peripheral(mdwc, 1);
			mdwc->drd_state = DRD_STATE_PERIPHERAL;
			work = true;
		} else {
			dwc3_msm_gadget_vbus_draw(mdwc, 0);
			dev_dbg(mdwc->dev, "Cable disconnected\n");
		}
		break;

	case DRD_STATE_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &mdwc->inputs) ||
				!test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "!id || !bsv\n");
			mdwc->drd_state = DRD_STATE_IDLE;
			cancel_delayed_work_sync(&mdwc->sdp_check);
			dwc3_otg_start_peripheral(mdwc, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * DRD_STATE_IDLE state
			 */
			pm_runtime_put_sync_suspend(mdwc->dev);
			dbg_event(0xFF, "!BSV psync",
				atomic_read(&mdwc->dev->power.usage_count));
			work = true;
		} else if (test_bit(B_SUSPEND, &mdwc->inputs) &&
			test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BPER bsv && susp\n");
			mdwc->drd_state = DRD_STATE_PERIPHERAL_SUSPEND;
			/*
			 * Decrement pm usage count upon bus suspend.
			 * Count was incremented either upon cable
			 * connect in DRD_STATE_IDLE or host
			 * initiated resume after bus suspend in
			 * DRD_STATE_PERIPHERAL_SUSPEND state
			 */
			pm_runtime_mark_last_busy(mdwc->dev);
			pm_runtime_put_autosuspend(mdwc->dev);
			dbg_event(0xFF, "SUSP put",
				atomic_read(&mdwc->dev->power.usage_count));
		}
		break;

	case DRD_STATE_PERIPHERAL_SUSPEND:
		if (!test_bit(B_SESS_VLD, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BSUSP: !bsv\n");
			mdwc->drd_state = DRD_STATE_IDLE;
			cancel_delayed_work_sync(&mdwc->sdp_check);
			dwc3_otg_start_peripheral(mdwc, 0);
		} else if (!test_bit(B_SUSPEND, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "BSUSP !susp\n");
			mdwc->drd_state = DRD_STATE_PERIPHERAL;
			/*
			 * Increment pm usage count upon host
			 * initiated resume. Count was decremented
			 * upon bus suspend in
			 * DRD_STATE_PERIPHERAL state.
			 */
			pm_runtime_get_sync(mdwc->dev);
			dbg_event(0xFF, "!SUSP gsync",
				atomic_read(&mdwc->dev->power.usage_count));
		}
		break;

	case DRD_STATE_HOST_IDLE:
		/* Switch to A-Device*/
		if (test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "id\n");
			mdwc->drd_state = DRD_STATE_IDLE;
			mdwc->vbus_retry_count = 0;
			work = true;
		} else {
			mdwc->drd_state = DRD_STATE_HOST;

			ret = dwc3_otg_start_host(mdwc, 1);
			if ((ret == -EPROBE_DEFER) &&
						mdwc->vbus_retry_count < 3) {
				/*
				 * Get regulator failed as regulator driver is
				 * not up yet. Will try to start host after 1sec
				 */
				mdwc->drd_state = DRD_STATE_HOST_IDLE;
				dev_dbg(mdwc->dev, "Unable to get vbus regulator. Retrying...\n");
				delay = VBUS_REG_CHECK_DELAY;
				work = true;
				mdwc->vbus_retry_count++;
			} else if (ret) {
				dev_err(mdwc->dev, "unable to start host\n");
				mdwc->drd_state = DRD_STATE_HOST_IDLE;
				goto ret;
			}
		}
		break;

	case DRD_STATE_HOST:
		if (test_bit(ID, &mdwc->inputs)) {
			dev_dbg(mdwc->dev, "id\n");
			dwc3_otg_start_host(mdwc, 0);
			mdwc->drd_state = DRD_STATE_IDLE;
			mdwc->vbus_retry_count = 0;
			work = true;
		} else {
			dev_dbg(mdwc->dev, "still in a_host state. Resuming root hub.\n");
			dbg_event(0xFF, "XHCIResume", 0);
			if (dwc)
				pm_runtime_resume(&dwc->xhci->dev);
		}
		break;

	default:
		dev_err(mdwc->dev, "%s: invalid otg-state\n", __func__);

	}

	if (work)
		queue_delayed_work(mdwc->sm_usb_wq, &mdwc->sm_work, delay);

ret:
	return;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_msm_pm_suspend(struct device *dev)
{
	int ret = 0;
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM suspend\n");
	dbg_event(0xFF, "PM Sus", 0);

	/*
	 * Check if pm_suspend can proceed irrespective of runtimePM state of
	 * host.
	 */
	if (!dwc->ignore_wakeup_src_in_hostmode || !mdwc->in_host_mode) {
		if (!atomic_read(&dwc->in_lpm)) {
			dev_err(mdwc->dev, "Abort PM suspend!! (USB is outside LPM)\n");
			return -EBUSY;
		}

		atomic_set(&mdwc->pm_suspended, 1);

		return 0;
	}

	/* Wakeup not required for automotive/telematics platform host mode */
	ret = dwc3_msm_suspend(mdwc, false, device_may_wakeup(dev));
	if (!ret)
		atomic_set(&mdwc->pm_suspended, 1);

	return ret;
}

static int dwc3_msm_pm_resume(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM resume\n");
	dbg_event(0xFF, "PM Res", 0);

	atomic_set(&mdwc->pm_suspended, 0);

	if (atomic_read(&dwc->in_lpm) &&
			mdwc->drd_state == DRD_STATE_PERIPHERAL_SUSPEND) {
		dev_info(mdwc->dev, "USB Resume start\n");
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
		place_marker("M - USB device resume started");
#endif
	}

	if (!mdwc->in_host_mode) {
		/* kick in otg state machine */
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

		return 0;
	}

	/* Resume dwc to avoid unclocked access by xhci_plat_resume */
	if (pm_runtime_status_suspended(dev))
		pm_runtime_resume(dev);
	else
		dwc3_msm_resume(mdwc);

	/* kick in otg state machine */
	queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return 0;
}

static int dwc3_msm_pm_freeze(struct device *dev)
{
	int ret = 0;
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM freeze\n");
	dbg_event(0xFF, "PM Freeze", 0);

	/*
	 * Check if pm_freeze can proceed irrespective of runtimePM state of
	 * host.
	 */
	if (!dwc->ignore_wakeup_src_in_hostmode || !mdwc->in_host_mode) {
		if (!atomic_read(&dwc->in_lpm)) {
			dev_err(mdwc->dev, "Abort PM freeze!! (USB is outside LPM)\n");
			return -EBUSY;
		}

		atomic_set(&mdwc->pm_suspended, 1);

		return 0;
	}

	/*
	 * PHYs also need to be power collapsed, so call notify_disconnect
	 * before suspend to ensure it.
	 */
	usb_phy_notify_disconnect(mdwc->hs_phy1, USB_SPEED_HIGH);
	usb_phy_notify_disconnect(mdwc->hs_phy, USB_SPEED_HIGH);
	dwc3_msm_clear_hsphy_flags(mdwc, PHY_HOST_MODE);
	usb_phy_notify_disconnect(mdwc->ss_phy1, USB_SPEED_SUPER);
	usb_phy_notify_disconnect(mdwc->ss_phy, USB_SPEED_SUPER);
	dwc3_msm_clear_ssphy_flags(mdwc, PHY_HOST_MODE);

	/*
	 * Power collapse the core. Hence call dwc3_msm_suspend with
	 * 'force_power_collapse' set to 'true'.
	 * Wakeup not required for automotive/telematics platform host mode.
	 */
	ret = dwc3_msm_suspend(mdwc, true, false);
	if (!ret)
		atomic_set(&mdwc->pm_suspended, 1);

	return ret;
}

static int dwc3_msm_pm_restore(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "dwc3-msm PM restore\n");
	dbg_event(0xFF, "PM Restore", 0);

	atomic_set(&mdwc->pm_suspended, 0);

	if (!mdwc->in_host_mode) {
		/* kick in otg state machine */
		queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

		return 0;
	}

	/* Resume dwc to avoid unclocked access by xhci_plat_resume */
	dwc3_msm_resume(mdwc);

	/* Restore PHY flags if hibernated in host mode */
	dwc3_msm_set_hsphy_flags(mdwc, PHY_HOST_MODE);
	usb_phy_notify_connect(mdwc->hs_phy, USB_SPEED_HIGH);
	usb_phy_notify_connect(mdwc->hs_phy1, USB_SPEED_HIGH);

	if (dwc->maximum_speed >= USB_SPEED_SUPER) {
		dwc3_msm_set_ssphy_flags(mdwc, PHY_HOST_MODE);
		usb_phy_notify_connect(mdwc->ss_phy,
					USB_SPEED_SUPER);
		usb_phy_notify_connect(mdwc->ss_phy1,
					USB_SPEED_SUPER);
	}

	/* kick in otg state machine */
	queue_work(mdwc->dwc3_wq, &mdwc->resume_work);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int dwc3_msm_runtime_idle(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime idle\n");
	dbg_event(0xFF, "RT Idle", 0);

	return 0;
}

static int dwc3_msm_runtime_suspend(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime suspend\n");
	dbg_event(0xFF, "RT Sus", 0);

	return dwc3_msm_suspend(mdwc, false, true);
}

static int dwc3_msm_runtime_resume(struct device *dev)
{
	struct dwc3_msm *mdwc = dev_get_drvdata(dev);
	struct dwc3 *dwc = platform_get_drvdata(mdwc->dwc3);

	dev_dbg(dev, "DWC3-msm runtime resume\n");
	dbg_event(0xFF, "RT Res", 0);

	return dwc3_msm_resume(mdwc);
}
#endif

static const struct dev_pm_ops dwc3_msm_dev_pm_ops = {
	.suspend	= dwc3_msm_pm_suspend,
	.resume		= dwc3_msm_pm_resume,
	.freeze		= dwc3_msm_pm_freeze,
	.thaw		= dwc3_msm_pm_restore,
	.poweroff	= dwc3_msm_pm_suspend,
	.restore	= dwc3_msm_pm_restore,
	SET_RUNTIME_PM_OPS(dwc3_msm_runtime_suspend, dwc3_msm_runtime_resume,
				dwc3_msm_runtime_idle)
};

static const struct of_device_id of_dwc3_matach[] = {
	{
		.compatible = "qcom,dwc-usb3-msm",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_matach);

static struct platform_driver dwc3_msm_driver = {
	.probe		= dwc3_msm_probe,
	.remove		= dwc3_msm_remove,
	.driver		= {
		.name	= "msm-dwc3",
		.pm	= &dwc3_msm_dev_pm_ops,
		.of_match_table	= of_dwc3_matach,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 MSM Glue Layer");
MODULE_SOFTDEP("pre: phy-generic phy-msm-snps-hs phy-msm-ssusb-qmp eud");

static int dwc3_msm_init(void)
{
	return platform_driver_register(&dwc3_msm_driver);
}
module_init(dwc3_msm_init);

static void __exit dwc3_msm_exit(void)
{
	platform_driver_unregister(&dwc3_msm_driver);
}
module_exit(dwc3_msm_exit);
