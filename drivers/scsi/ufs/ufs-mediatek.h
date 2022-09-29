/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_H
#define _UFS_MEDIATEK_H

#include <linux/bitops.h>
#include <linux/pm_qos.h>
#include <linux/of_device.h>

#include "ufs.h"
#include "ufshci.h"
#include "ufshcd.h"

#ifdef CONFIG_UFSFEATURE
#include "ufsfeature.h"
#endif

/*
 * Vendor specific UFSHCI Registers
 */
#define REG_UFS_XOUFS_CTRL          0x140
#define REG_UFS_REFCLK_CTRL         0x144
#define REG_UFS_EXTREG              0x2100
#define REG_UFS_MPHYCTRL            0x2200
#define REG_UFS_MTK_IP_VER          0x2240
#define REG_UFS_REJECT_MON          0x22AC
#define REG_UFS_AH8E_MON            0x22B0
#define REG_UFS_AH8X_MON            0x22B4
#define REG_UFS_DEBUG_SEL           0x22C0
#define REG_UFS_PROBE               0x22C8
#define REG_UFS_DEBUG_SEL_B0        0x22D0
#define REG_UFS_DEBUG_SEL_B1        0x22D4
#define REG_UFS_DEBUG_SEL_B2        0x22D8
#define REG_UFS_DEBUG_SEL_B3        0x22DC

/*
 * Details of UIC Errors
 */
static const u8 *ufs_uic_err_str[] = {
	"PHY Adapter Layer",
	"Data Link Layer",
	"Network Link Layer",
	"Transport Link Layer",
	"DME"
};

static const u8 *ufs_uic_pa_err_str[] = {
	"PHY error on Lane 0",
	"PHY error on Lane 1",
	"PHY error on Lane 2",
	"PHY error on Lane 3",
	"Generic PHY Adapter Error. This should be the LINERESET indication"
};

static const u8 *ufs_uic_dl_err_str[] = {
	"NAC_RECEIVED",
	"TCx_REPLAY_TIMER_EXPIRED",
	"AFCx_REQUEST_TIMER_EXPIRED",
	"FCx_PROTECTION_TIMER_EXPIRED",
	"CRC_ERROR",
	"RX_BUFFER_OVERFLOW",
	"MAX_FRAME_LENGTH_EXCEEDED",
	"WRONG_SEQUENCE_NUMBER",
	"AFC_FRAME_SYNTAX_ERROR",
	"NAC_FRAME_SYNTAX_ERROR",
	"EOF_SYNTAX_ERROR",
	"FRAME_SYNTAX_ERROR",
	"BAD_CTRL_SYMBOL_TYPE",
	"PA_INIT_ERROR (FATAL ERROR)",
	"PA_ERROR_IND_RECEIVED",
	"PA_INIT (3.0 FATAL ERROR)"
};

/*
 * Ref-clk control
 *
 * Values for register REG_UFS_REFCLK_CTRL
 */
#define REFCLK_RELEASE              0x0
#define REFCLK_REQUEST              BIT(0)
#define REFCLK_ACK                  BIT(1)

#define REFCLK_REQ_TIMEOUT_US       3000

/*
 * Other attributes
 */
#define VS_DEBUGCLOCKENABLE         0xD0A1
#define VS_SAVEPOWERCONTROL         0xD0A6
#define VS_UNIPROPOWERDOWNCONTROL   0xD0A8

/*
 * Vendor specific link state
 */
enum {
	VS_LINK_DISABLED            = 0,
	VS_LINK_DOWN                = 1,
	VS_LINK_UP                  = 2,
	VS_LINK_HIBERN8             = 3,
	VS_LINK_LOST                = 4,
	VS_LINK_CFG                 = 5,
};

/*
 * Vendor specific host controller state
 */
enum {
	VS_HCE_RESET                = 0,
	VS_HCE_BASE                 = 1,
	VS_HCE_OOCPR_WAIT           = 2,
	VS_HCE_DME_RESET            = 3,
	VS_HCE_MIDDLE               = 4,
	VS_HCE_DME_ENABLE           = 5,
	VS_HCE_DEFAULTS             = 6,
	VS_HIB_IDLEEN               = 7,
	VS_HIB_ENTER                = 8,
	VS_HIB_ENTER_CONF           = 9,
	VS_HIB_MIDDLE               = 10,
	VS_HIB_WAITTIMER            = 11,
	VS_HIB_EXIT_CONF            = 12,
	VS_HIB_EXIT                 = 13,
};

/*
 * VS_DEBUGCLOCKENABLE
 */
enum {
	TX_SYMBOL_CLK_REQ_FORCE = 5,
};

/*
 * VS_SAVEPOWERCONTROL
 */
enum {
	RX_SYMBOL_CLK_GATE_EN   = 0,
	SYS_CLK_GATE_EN         = 2,
	TX_CLK_GATE_EN          = 3,
};

/*
 * Host capability
 */
enum ufs_mtk_host_caps {
	UFS_MTK_CAP_BOOST_CRYPT_ENGINE         = 1 << 0,
	UFS_MTK_CAP_VA09_PWR_CTRL              = 1 << 1,
	UFS_MTK_CAP_DISABLE_AH8                = 1 << 2,
	UFS_MTK_CAP_BROKEN_VCC                 = 1 << 3,

	/* Override UFS_MTK_CAP_BROKEN_VCC's behavior to
	 * allow vccqx upstream to enter LPM
	 */
	UFS_MTK_CAP_FORCE_VSx_LPM              = 1 << 5,
	UFS_MTK_CAP_PMC_VIA_FASTAUTO	       = 1 << 6,
};

struct ufs_mtk_crypt_cfg {
	struct regulator *reg_vcore;
	struct clk *clk_crypt_perf;
	struct clk *clk_crypt_mux;
	struct clk *clk_crypt_lp;
	int vcore_volt;
};

struct ufs_mtk_clk {
	struct ufs_clk_info *ufs_sel_clki; // mux
	struct ufs_clk_info *ufs_sel_max_clki; // max src
	struct ufs_clk_info *ufs_sel_min_clki; // min src
};

struct ufs_mtk_hw_ver {
	u8 step;
	u8 minor;
	u8 major;
};

#if IS_ENABLED(CONFIG_UFS_MEDIATEK_INTERNAL)
struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};
#endif

struct ufs_mtk_host {
	struct phy *mphy;
	struct regulator *reg_va09;
	struct reset_control *hci_reset;
	struct reset_control *unipro_reset;
	struct reset_control *crypto_reset;
	struct ufs_hba *hba;
	struct ufs_mtk_crypt_cfg *crypt;
	struct ufs_mtk_hw_ver hw_ver;
	enum ufs_mtk_host_caps caps;
	bool mphy_powered_on;
	bool unipro_lpm;
	bool ref_clk_enabled;
	bool clk_scale_up;
	u16 ref_clk_ungating_wait_us;
	u16 ref_clk_gating_wait_us;
	u32 ip_ver;
	struct ufs_mtk_clk mclk;
	bool pm_qos_init;
	struct pm_qos_request pm_qos_req;
	bool qos_allowed;
	bool qos_enabled;
	bool boot_device;

	struct completion luns_added;

	struct semaphore rpmb_sem;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature ufsf;
#endif
};

#define UFSHCD_MAX_TAG	256

enum {
	REG_UFS_MCQCAP			= 0x0c,
	REG_UFS_MCQCFG			= 0x28,

	REG_UFS_MMIO_OPT_CTRL_0 = 0x160,
	REG_UFS_MMIO_SQ_IS		= 0x190,
	REG_UFS_MMIO_SQ_IE		= 0x194,
	REG_UFS_MMIO_CQ_IS		= 0x1A0,
	REG_UFS_MMIO_CQ_IE		= 0x1A4,
	REG_UFS_MMIO_VER_ID 	= 0x1B0,
	REG_UFS_MCQ_BASE		= 0x320,
	REG_UFS_SQ_ATTR 		= (REG_UFS_MCQ_BASE + 0x0),
	REG_UFS_SQ_LBA			= (REG_UFS_MCQ_BASE + 0x4),
	REG_UFS_SQ_UBA			= (REG_UFS_MCQ_BASE + 0x8),
	REG_UFS_SQ_HEAD 		= (REG_UFS_MCQ_BASE + 0xc),
	REG_UFS_SQ_TAIL 		= (REG_UFS_MCQ_BASE + 0x10),
	REG_UFS_CQ_ATTR 		= (REG_UFS_MCQ_BASE + 0x18),
	REG_UFS_CQ_LBA			= (REG_UFS_MCQ_BASE + 0x1c),
	REG_UFS_CQ_UBA			= (REG_UFS_MCQ_BASE + 0x20),
	REG_UFS_CQ_HEAD 		= (REG_UFS_MCQ_BASE + 0x24),
	REG_UFS_CQ_TAIL 		= (REG_UFS_MCQ_BASE + 0x28),
};

/* MCQCAP 08h */
#define MAX_Q	GENMASK(4, 0)

/* REG_UFS_MMIO_OPT_CTRL_0 160h */
#define EHS_EN				0x1
#define PFM_IMPV			0x2
#define MCQ_MULTI_INTR_EN	0x4
#define MCQ_CMB_INTR_EN		0x8
#define MCQ_AH8				0x10

#define SQ_INT 						0x80000
#define CQ_INT 						0x100000
#define UFSHCD_ENABLE_INTRS_MCQ	(SQ_INT |\
					 CQ_INT |\
					 UTP_TASK_REQ_COMPL |\
					 UFSHCD_ERROR_MASK)

#define UFSHCD_ENABLE_INTRS_MCQ_SEPARATE \
					 (UTP_TASK_REQ_COMPL | UFSHCD_ERROR_MASK)

#define MCQ_INTR_EN_MSK		(MCQ_MULTI_INTR_EN | MCQ_CMB_INTR_EN)

struct utp_cq_entry {
	__le64 UCD_base;
	__le32 dword_2;
	__le32 dword_3;
	__le32 dword_4;
	__le32 dword_5; //reserved
	__le32 dword_6; //reserved
	__le32 dword_7; //reserved
};

#define UCD_BASE_ADD_MASK	UFS_MASK(0x1ffffffffffffff,7)	//bit7~63
#define UCD_BASE_SQID_MASK	UFS_MASK(0x1f, 0)	//[4:0]
#define UTRD_OCS_MASK		UFS_MASK(0xff, 0)

union utp_q_entry {
	struct utp_transfer_req_desc	sq;
	struct utp_cq_entry	cq;
};

#define SQE_SIZE sizeof(struct utp_transfer_req_desc)
#define CQE_SIZE sizeof(struct utp_cq_entry)
#define SQE_NUM_1K (1024 / SQE_SIZE)
#define CQE_NUM_1K (1024 / CQE_SIZE)

/* MCQCFG */
#define MCQCFG_ARB_SCHEME	(0x3<<1)
	#define MCQCFG_ARB_SP	(0x0<<1)
	#define MCQCFG_ARB_RRP	(0x1<<1)
#define MCQCFG_TYPE			(0x1<<0)

#define Q_SPACING	(0x30)
#define MCQ_ADDR(base, index)	((base) + (Q_SPACING * (index)))
#define Q_ENABLE	(0x1<<31)

#define UFSHCD_MAX_Q_NR	8
#define UFSHCD_MCQ_PRI_THRESHOD	5

enum {
	MCQ_Q_TYPE_SQ	= 0,
	MCQ_Q_TYPE_CQ	= 1,
};

struct ufs_queue {
	u8 q_enable;   //0: disable, 1: enable
	u8 qid;			//0... N-1
	u8 q_type;		//SQ/CQ
	u16 q_depth;
	u8 priority;

	union utp_q_entry *q_base_addr;	//content list
	union utp_q_entry *head;	//u16 head;
	union utp_q_entry *tail;	//u16 tail;			//SW
	union utp_q_entry *tail_written;	//u16 tail_written;	//SW

	spinlock_t q_lock;

	dma_addr_t q_dma_addr;
};

struct ufs_sw_queue {
	u16 depth;
	u16 head;
	u16 tail;
	int tag_data[UFSHCD_MAX_TAG];
};
struct ufs_queue_config {
	struct ufs_queue *sq;
	struct ufs_queue *cq;
	u8 sq_nr;
	u8 cq_nr;
	u8 sq_cq_map[UFSHCD_MAX_Q_NR];          //sq to cq mapping, ex. sq_cq_mapping[3] = 2 means sq[3] mapping to cq[2]
	u32 sent_cmd_count[UFSHCD_MAX_Q_NR];
};

struct ufs_mcq_intr_info {
	struct ufs_hba *hba;
	u32 intr;
	u8 qid;
};

#define BITMAP_TAGS_LEN BITS_TO_LONGS(UFSHCD_MAX_TAG)

struct ufs_hba_private {
	//struct ufs_hba *hba;
	u32 max_q;
	u32 mcq_cap;
	u32 mcq_nr_hw_queue;
	u32 mcq_nr_q_depth;
	u32 mcq_nr_intr;
	struct ufs_mcq_intr_info mcq_intr_info[UFSHCD_MAX_Q_NR];

	struct utp_transfer_req_desc *usel_base_addr;
	struct utp_cq_entry *ucel_base_addr;
	dma_addr_t usel_dma_addr;
	dma_addr_t ucel_dma_addr;
	dma_addr_t sq_dma_addr;
	dma_addr_t cq_dma_addr;

	struct ufs_queue_config mcq_q_cfg;

	unsigned long outstanding_mcq_reqs[BITMAP_TAGS_LEN];

	bool is_mcq_enabled;
};

int ufs_mtk_mcq_alloc_priv(struct ufs_hba *hba);
void ufs_mtk_mcq_host_dts(struct ufs_hba *hba);
void ufs_mtk_mcq_get_irq(struct platform_device *pdev);
void ufs_mtk_mcq_request_irq(struct ufs_hba *hba);
void ufs_mtk_mcq_set_irq_affinity(struct ufs_hba *hba);
int ufs_mtk_mcq_memory_alloc(struct ufs_hba *hba);
int ufs_mtk_mcq_install_tracepoints(void);

/*
 *  IOCTL opcode for ufs queries has the following opcode after
 *  SCSI_IOCTL_GET_PCI
 */
#define UFS_IOCTL_QUERY			0x5388

/**
 * struct ufs_ioctl_query_data - used to transfer data to and from user via
 * ioctl
 * @opcode: type of data to query (descriptor/attribute/flag)
 * @idn: id of the data structure
 * @buf_size: number of allocated bytes/data size on return
 * @buffer: data location
 *
 * Received: buffer and buf_size (available space for transferred data)
 * Submitted: opcode, idn, length, buf_size
 */
struct ufs_ioctl_query_data {
	/*
	 * User should select one of the opcode defined in "enum query_opcode".
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 * Note that only UPIU_QUERY_OPCODE_READ_DESC,
	 * UPIU_QUERY_OPCODE_READ_ATTR & UPIU_QUERY_OPCODE_READ_FLAG are
	 * supported as of now. All other query_opcode would be considered
	 * invalid.
	 * As of now only read query operations are supported.
	 */
	__u32 opcode;
	/*
	 * User should select one of the idn from "enum flag_idn" or "enum
	 * attr_idn" or "enum desc_idn" based on whether opcode above is
	 * attribute, flag or descriptor.
	 * Please check include/uapi/scsi/ufs/ufs.h for the definition of it.
	 */
	__u8 idn;
	/*
	 * User should specify the size of the buffer (buffer[0] below) where
	 * it wants to read the query data (attribute/flag/descriptor).
	 * As we might end up reading less data then what is specified in
	 * buf_size. So we are updating buf_size to what exactly we have read.
	 */
	__u16 buf_size;
	/*
	 * placeholder for the start of the data buffer where kernel will copy
	 * the query data (attribute/flag/descriptor) read from the UFS device
	 * Note:
	 * For Read/Write Attribute you will have to allocate 4 bytes
	 * For Read/Write Flag you will have to allocate 1 byte
	 */
	__u8 buffer[0];
};

enum {
	BOOTDEV_SDMMC = 1,
	BOOTDEV_UFS   = 2
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

#if IS_ENABLED(CONFIG_RPMB)
struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void);
#endif

#if defined(CONFIG_UFSFEATURE)
static inline struct ufsf_feature *ufs_mtk_get_ufsf(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return &host->ufsf;
}
#endif

static inline const void *ufs_mtk_get_boot_property(struct device_node *np,
	const char *name, int *lenp)
{
	struct device_node *boot_node = NULL;

	boot_node = of_parse_phandle(np, "bootmode", 0);
	if (!boot_node)
		return NULL;
	return of_get_property(boot_node, name, lenp);
}

#endif /* !_UFS_MEDIATEK_H */
