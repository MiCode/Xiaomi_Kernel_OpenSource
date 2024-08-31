#ifndef PCIE_HDMA_H__
#define PCIE_HDMA_H__

#include <linux/spinlock.h>
#include <ispv4_pcie.h>
#include <linux/completion.h>
#include "media/ispv4_defs.h"

#define ISPV4_LL_BLOCK_CNT		8
#define ISPV4_HDMA_BUF_SIZE		0x100000
#define ISPV4_HDMA_EP_BUF_ADDR		0x80000000
#define PCIE_HDMA_MSI_NUM		22
#define PCIE_HDMA_CHAN_NUM		2
#define PCIE_HDMA_MAX_XFER_ELMNT_CNT	(16 + 1) /* one extra for link elemnt */
#define PCIE_HDMA_MAX_CHAN_NAME		16
#define PCIE_HDMA_DEF_TIMEOUT_MS	2000
#define ISPV4_PCIE_PERFORMANCE		1

/*
 * HDMA registers
 * ch: channel index
 * dir: 0: HDMA_TO_DEVICE, 1: HDMA_FROM_DEVICE
 */
#define HDMA_CHAN_GAP   0x800
#define HDMA_RW_GAP     0x400
#define HDMA_READ 1
#define HDMA_WRITE 1

#define HDMA_CH_DIR_BASE(ch, dir)       ((HDMA_CHAN_GAP * ch) + (HDMA_RW_GAP * dir))
#define HDMA_EN(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x0)
#define HDMA_DOORBELL(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x4)
#define HDMA_ELEM_PF(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x8)
#define HDMA_LLP_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x10)
#define HDMA_LLP_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x14)
#define HDMA_CYCLE(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x18)
#define HDMA_XFERSIZE(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x1c)
#define HDMA_SAR_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x20)
#define HDMA_SAR_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x24)
#define HDMA_DAR_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x28)
#define HDMA_DAR_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x2c)
#define HDMA_WATERMARK_EN(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x30)
#define HDMA_CONTROL1(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x34)
#define HDMA_FUNC_NUM(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x38)
#define HDMA_QOS(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x3c)
#define HDMA_STATUS(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x80)
#define HDMA_INT_STATUS(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x84)
#define HDMA_INT_SETUP(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x88)
#define HDMA_INT_LAIE   BIT(6)
#define HDMA_INT_RAIE   BIT(5)
#define HDMA_INT_LSIE   BIT(4)
#define HDMA_INT_RSIE   BIT(3)
#define HDMA_INT_ABRT_MSK       BIT(2)
#define HDMA_INT_WATERMARK_MSK  BIT(1)
#define HDMA_INT_STP_MSK        BIT(0)
#define HDMA_INT_CLEAR(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x8c)
#define HDMA_MSI_STOP_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x90)
#define HDMA_MSI_STOP_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x94)
#define HDMA_MSI_WATERMARK_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x98)
#define HDMA_MSI_WATERMARK_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0x9c)
#define HDMA_MSI_ABORT_LOW(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0xa0)
#define HDMA_MSI_ABORT_HIGH(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0xa4)
#define HDMA_MSI_MSGD(ch, dir)   (HDMA_CH_DIR_BASE(ch, dir) + 0xa8)

#define HDMA_CHAN_EN    BIT(0)

#define HDMA_LINK_ELMNT_CB      BIT(0)
#define HDMA_LINK_ELMNT_TCB     BIT(1)
#define HDMA_LINK_ELMNT_LLP     BIT(2)
#define HDMA_LINK_ELMNT_LWIE    BIT(3)
#define HDMA_LINK_ELMNT_RWIE    BIT(4)

#define HDMA_DB_START   BIT(0)
#define HDMA_DB_STOP    BIT(1)

#define HDMA_CYCLE_CYCLE_STATE  BIT(1)
#define HDMA_CYCLE_CYCLE_BIT    BIT(0)

#define HDMA_CONTROL1_LLEN BIT(0)

#define HDMA_STATUS_MASK        0x3
#define HDMA_STATUS_RUNNING     0x1
#define HDMA_STATUS_ABORTED     0x2
#define HDMA_STATUS_STOPPED     0x3

#define HDMA_INT_STATUS_STOP    BIT(0)
#define HDMA_INT_STATUS_WATERMARK       BIT(1)
#define HDMA_INT_STATUS_ABORT   BIT(2)
#define HDMA_INT_STATUS_ERR_OFF 3
#define HDMA_INT_STATUS_ERR_MASK (0xF << HDMA_INT_STATUS_ERR_OFF)
#define HDMA_INT_STATUS_ERR_LL_CPL_UR   0x1
#define HDMA_INT_STATUS_ERR_LL_CPL_CA   0x2
#define HDMA_INT_STATUS_ERR_LL_CPL_EP   0x3
#define HDMA_INT_STATUS_ERR_DATA_CPL_TIMEOUT    0x8
#define HDMA_INT_STATUS_ERR_DATA_CPL_UR 0x9
#define HDMA_INT_STATUS_ERR_DATA_CPL_CA 0xA
#define HDMA_INT_STATUS_ERR_DATA_CP_EP  0xB
#define HDMA_INT_STATUS_ERR_DATA_MWR    0xC

enum pcie_hdma_xfer_mode {
	HDMA_LL_MODE = 0,
	HDMA_SINGLE_BLK_MODE,
};

#ifdef ISPV4_PCIE_PERFORMANCE
enum pcie_hdma_wait_type {
	POLLING = 0,
	INTERRUPT = 1,
};
#endif /* ISPV4_PCIE_PERFORMANCE */

struct pcie_hdma_ll_data_elmnt {
	uint32_t cfg;
	uint32_t tx_size;
	uint32_t sar_low;
	uint32_t sar_high;
	uint32_t dar_low;
	uint32_t dar_high;
};

struct pcie_hdma_ll_link_elmnt {
	uint32_t cfg;
	uint32_t reserved_0;
	uint32_t llp_low;
	uint32_t llp_high;
	uint32_t reserved_1;
	uint32_t reserved_2;
};

struct pcie_hdma_xfer_cfg {
	/* one extra for link elemnt */
	struct pcie_hdma_ll_data_elmnt ll_elmnt[PCIE_HDMA_MAX_XFER_ELMNT_CNT + 1];
	uint32_t ll_elmnt_cnt;
#ifdef ISPV4_PCIE_PERFORMANCE
	uint64_t total_len;
#endif
};

struct pcie_hdma_region {
	phys_addr_t paddr;
	void __iomem *vaddr;
	size_t sz;
};

struct pcie_hdma_mm_single_blk {
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t size;
};

struct pcie_hdma_mm_blocks {
	uint32_t blk_cnt;
	struct pcie_hdma_mm_single_blk blks[PCIE_HDMA_MAX_XFER_ELMNT_CNT];
};

struct pcie_hdma_chan_ctrl {
	uint8_t id;
	bool inuse;
	struct completion comp;
	spinlock_t chan_lock;
	enum pcie_hdma_dir dir;
	enum pcie_hdma_xfer_mode xfer_mode;
	struct pcie_hdma_xfer_cfg xfer_cfg;
	uint32_t timeout_ms;
	uint32_t int_status;
#ifdef ISPV4_PCIE_PERFORMANCE
	enum pcie_hdma_wait_type wait_type;
	uint64_t start_ns;
	uint64_t end_ns;
#endif /* ISPV4_PCIE_PERFORMANCE */
};

struct pcie_hdma {
	struct pci_dev			*pci_dev;
	struct ispv4_data		pdata;
	struct pcie_hdma_chan_ctrl	r_chans[PCIE_HDMA_CHAN_NUM];
	struct pcie_hdma_chan_ctrl	w_chans[PCIE_HDMA_CHAN_NUM];
	struct task_struct		*hdma_thread;
	atomic_t			hdma_wakeup;
	spinlock_t			lock;		/* Only for legacy */
	void __iomem			*base;
	dma_addr_t			dma;
	void				*va;
};

struct pcie_hdma_chan_ctrl *ispv4_hdma_request_chan(struct pcie_hdma *hdma, enum pcie_hdma_dir dir);
void ispv4_hdma_release_chan(struct pcie_hdma_chan_ctrl *chan_ctrl);
int32_t ispv4_hdma_xfer_add_block(struct pcie_hdma_chan_ctrl *chan_ctrl,
				  uint32_t src_addr, uint32_t dst_addr, uint32_t size);
int32_t ispv4_hdma_xfer_add_ll_blocks(struct pcie_hdma_chan_ctrl *chan_ctrl,
				      struct pcie_hdma_mm_blocks *blocks);
int32_t ispv4_pcie_hdma_start_and_wait_end(struct pcie_hdma *hdma,
					   struct pcie_hdma_chan_ctrl *chan_ctrl);
int ispv4_hdma_single_transfer(struct pcie_hdma *hdma, enum pcie_hdma_dir dir);
int ispv4_hdma_ll_transfer(struct pcie_hdma *hdma, enum pcie_hdma_dir dir);
//void ispv4_pcie_hdma_init(struct pcie_hdma *hdma);
struct pcie_hdma *ispv4_pcie_hdma_init(struct ispv4_data *priv);
int ispv4_hdma_single_trans(struct pcie_hdma *hdma, enum pcie_hdma_dir dir,
			    void* data, int len, u32 ep_addr);
#endif  /* PCIE_HDMA_H__ */
