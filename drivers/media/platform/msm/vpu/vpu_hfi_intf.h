/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __H_VPU_HFI_INTF_H__
#define __H_VPU_HFI_INTF_H__


struct hfi_queue_table_header {
	u32 qtbl_version;
	u32 qtbl_size;
	u32 qtbl_qhdr0_offset;
	u32 qtbl_qhdr_size;
	u32 qtbl_num_q;
	u32 qtbl_num_active_q;
};

/* qhdr_status */
#define QHDR_STATUS_INACTIVE		0
#define QHDR_STATUS_ACTIVE		1

/* qhdr_type bitmask */
#define QHDR_TYPE_TX_MODE_SHIFT		24
#define QHDR_TYPE_RX_MODE_SHIFT		16
#define Q_TYPE_INT_ONE			0
#define Q_TYPE_INT_MANY			1
#define Q_TYPE_POLL			2
#define QHDR_TYPE_PRIORITY_SHIFT	8
#define QHDR_TYPE_DIR_TX		0
#define QHDR_TYPE_DIR_RX		1

/* total number of queues */
#define VPU_MAX_TXQ_NUM			4
#define VPU_MAX_RXQ_NUM			4
#define VPU_MAX_QUEUE_NUM		(VPU_MAX_TXQ_NUM + VPU_MAX_RXQ_NUM)

/* qhdr_q_size in bytes */
#define VPU_SYS_QUEUE_SIZE		SZ_16K
#define VPU_SESSION_QUEUE_SIZE		SZ_32K
#define VPU_LOGGING_QUEUE_SIZE		SZ_64K

#define TX_Q_IDX_TO_Q_ID(idx)		(VPU_SYSTEM_CMD_QUEUE_ID + (idx * 2))
#define RX_Q_IDX_TO_Q_ID(idx)		(VPU_SYSTEM_MSG_QUEUE_ID + (idx * 2))

struct hfi_queue_header {
	u32 qhdr_status;
	u32 qhdr_start_addr;
	u32 qhdr_type;
	u32 qhdr_q_size;
	u32 qhdr_pkt_size;
	u32 qhdr_pkt_drop_cnt;
	u32 qhdr_rx_wm;
	u32 qhdr_tx_wm;
	u32 qhdr_rx_req;
	u32 qhdr_tx_req;
	u32 qhdr_rx_irq_status;	/* not used */
	u32 qhdr_tx_irq_status;	/* not used */
	u32 qhdr_read_idx;
	u32 qhdr_write_idx;
};

static inline void vpu_hfi_init_qhdr(struct hfi_queue_header *qhdr, bool tx,
					u32 offset, u32 size)
{
	memset(qhdr, 0, sizeof(*qhdr));

	if (tx)
		qhdr->qhdr_type = QHDR_TYPE_DIR_TX |
				(Q_TYPE_POLL << QHDR_TYPE_TX_MODE_SHIFT) |
				(Q_TYPE_POLL << QHDR_TYPE_RX_MODE_SHIFT);
	else
		qhdr->qhdr_type = QHDR_TYPE_DIR_RX |
				(Q_TYPE_POLL << QHDR_TYPE_TX_MODE_SHIFT) |
				(Q_TYPE_POLL << QHDR_TYPE_RX_MODE_SHIFT);

	qhdr->qhdr_rx_wm = 1;
	qhdr->qhdr_tx_wm = 1;

	qhdr->qhdr_rx_req = 1; /* queue is empty intially */

	/* the queue array */
	qhdr->qhdr_start_addr = offset;
	qhdr->qhdr_q_size = size;

	/* the indices */
	qhdr->qhdr_read_idx = 0;
	qhdr->qhdr_write_idx = 0;

	/* make the queue active */
	qhdr->qhdr_status = QHDR_STATUS_INACTIVE;
}

static inline void vpu_hfi_deinit_qhdr(struct hfi_queue_header *qhdr)
{
}

static inline void vpu_hfi_enable_qhdr(struct hfi_queue_header *qhdr)
{
	qhdr->qhdr_status = QHDR_STATUS_ACTIVE;
}

static inline void vpu_hfi_disable_qhdr(struct hfi_queue_header *qhdr)
{
	qhdr->qhdr_status = QHDR_STATUS_INACTIVE;
}

static inline bool vpu_hfi_q_empty(struct hfi_queue_header *qhdr)
{
	return (qhdr->qhdr_read_idx == qhdr->qhdr_write_idx);
}

static inline u32 vpu_hfi_q_size(int q_id)
{
	/* q_id defined in vpu_hfi.h*/
	switch (q_id) {
	case VPU_SYSTEM_CMD_QUEUE_ID:
	case VPU_SYSTEM_MSG_QUEUE_ID:
		return VPU_SYS_QUEUE_SIZE;
		break;
	case VPU_SESSION_CMD_QUEUE_0_ID:
	case VPU_SESSION_MSG_QUEUE_0_ID:
	case VPU_SESSION_CMD_QUEUE_1_ID:
	case VPU_SESSION_MSG_QUEUE_1_ID:
		return VPU_SESSION_QUEUE_SIZE;
		break;
	case VPU_SYSTEM_LOG_QUEUE_ID:
		return VPU_LOGGING_QUEUE_SIZE;
		break;
	default:
		break;
	}
	return 0;
}

/*
 * VPU CSR register offsets
 */

/* fw -> apps sgi interrupt */
#define VPU_CSR_APPS_SGI_STS			0x050
#define VPU_CSR_APPS_SGI_CLR			0x054
/* apps->fw sgi interrupts */
#define VPU_CSR_FW_SGI_EN_SET			0x084
#define VPU_CSR_FW_SGI_EN_CLR			0x088
#define VPU_CSR_FW_SGI_FORCELEVEL		0x08c
#define VPU_CSR_FW_SGI_STS			0x090
#define VPU_CSR_FW_SGI_CLR			0x094
#define VPU_CSR_FW_SGI_TRIG			0x098
/* scratch registers */
#define VPU_CSR_SW_SCRATCH0_STS			0x160	/* vpu status */
#define VPU_CSR_SW_SCRATCH1_QTBL_INFO		0x164	/* qtable ready */
#define VPU_CSR_SW_SCRATCH2_QTBL_ADDR		0x168	/* qtable phy addr */
#define VPU_CSR_SW_SCRATCH3			0x16c	/* unused */
#define VPU_CSR_SW_SCRATCH4_IPC_POLLING		0x170	/* IPC polling reg */
#define VPU_HW_VERSION				0x1A0
#define VPU_CSR_LAST_REG			VPU_HW_VERSION

static inline void raw_hfi_qtbl_paddr_set(void __iomem *regbase, u32 phyaddr)
{
	/* lower 32 bit of qtable phy address */
	writel_relaxed(phyaddr, regbase + VPU_CSR_SW_SCRATCH2_QTBL_ADDR);

	/* barrier then make the qtbl infor ready */
	wmb();
	writel_relaxed(1, regbase + VPU_CSR_SW_SCRATCH1_QTBL_INFO);
}

static inline void raw_hfi_int_enable(void __iomem *regbase)
{
	/* use edge interrrupt */
	writel_relaxed(0, regbase + VPU_CSR_FW_SGI_FORCELEVEL);

	/* barrier, then enable sgi interrupt */
	wmb();
	writel_relaxed(1, regbase + VPU_CSR_FW_SGI_EN_SET);
}

static inline void raw_hfi_int_disable(void __iomem *regbase)
{
	/* disable sgi interrupt */
	wmb();
	writel_relaxed(1, regbase + VPU_CSR_FW_SGI_EN_CLR);

	/* make sure this go through */
	mb();
}

static inline void raw_hfi_int_ack(void __iomem *regbase)
{
	/* clear sgi interrupt */
	wmb();
	writel_relaxed(1, regbase + VPU_CSR_APPS_SGI_CLR);

	/* make sure this go through */
	mb();
}

static inline void raw_hfi_int_fire(void __iomem *regbase)
{
	/* barrier, then trigger interrupt */
	wmb();
	writel_relaxed(1, regbase + VPU_CSR_FW_SGI_TRIG);

	/* no need for barrier after */
}

static inline void raw_hfi_reg_write(void __iomem *addr, u32 val)
{
	writel_relaxed(val, addr);
	wmb();
}

static inline u32 raw_hfi_reg_read(void __iomem *addr)
{
	u32 val;

	val = readl_relaxed(addr);
	rmb();

	return val;
}

static inline u32 raw_hfi_status_read(void __iomem *regbase)
{
	return raw_hfi_reg_read(regbase + VPU_CSR_SW_SCRATCH0_STS);
}

static inline bool raw_hfi_fw_ready(void __iomem *regbase)
{
	u32 val = raw_hfi_status_read(regbase);

	return (val & 0x1) ? true : false;
}

static inline bool raw_hfi_fw_halted(void __iomem *regbase)
{
	u32 val = raw_hfi_status_read(regbase);

	return (val & 0x2) ? true : false;
}

#endif /* __H_VPU_HFI_INTF_H__ */
