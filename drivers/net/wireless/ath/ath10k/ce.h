/*
 * Copyright (c) 2005-2011 Atheros Communications Inc.
 * Copyright (c) 2011-2013 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _CE_H_
#define _CE_H_

#include "hif.h"

/* Maximum number of Copy Engine's supported */
#define CE_COUNT_MAX 12
#define CE_HTT_H2T_MSG_SRC_NENTRIES 8192

/* Descriptor rings must be aligned to this boundary */
#define CE_DESC_RING_ALIGN	8
#define CE_SEND_FLAG_GATHER	0x00010000

/*
 * Copy Engine support: low-level Target-side Copy Engine API.
 * This is a hardware access layer used by code that understands
 * how to use copy engines.
 */

struct ath10k_ce_pipe;

#define CE_DESC_FLAGS_GATHER         (1 << 0)
#define CE_DESC_FLAGS_BYTE_SWAP      (1 << 1)
#define CE_WCN3990_DESC_FLAGS_GATHER BIT(31)

#define CE_DESC_FLAGS_GET_MASK		0x1F
#define CE_DESC_37BIT_ADDR_MASK		0x1FFFFFFFFF

/* Following desc flags are used in QCA99X0 */
#define CE_DESC_FLAGS_HOST_INT_DIS	(1 << 2)
#define CE_DESC_FLAGS_TGT_INT_DIS	(1 << 3)

#define CE_DESC_FLAGS_META_DATA_MASK ar->hw_values->ce_desc_meta_data_mask
#define CE_DESC_FLAGS_META_DATA_LSB  ar->hw_values->ce_desc_meta_data_lsb

#ifndef CONFIG_ATH10K_SNOC
struct ce_desc {
	__le32 addr;
	__le16 nbytes;
	__le16 flags; /* %CE_DESC_FLAGS_ */
};
#else
struct ce_desc {
	__le64 addr;
	u16 nbytes; /* length in register map */
	u16 flags; /* fw_metadata_high */
	u32 toeplitz_hash_result;
};
#endif

struct ath10k_ce_ring {
	/* Number of entries in this ring; must be power of 2 */
	unsigned int nentries;
	unsigned int nentries_mask;

	/*
	 * For dest ring, this is the next index to be processed
	 * by software after it was/is received into.
	 *
	 * For src ring, this is the last descriptor that was sent
	 * and completion processed by software.
	 *
	 * Regardless of src or dest ring, this is an invariant
	 * (modulo ring size):
	 *     write index >= read index >= sw_index
	 */
	unsigned int sw_index;
	/* cached copy */
	unsigned int write_index;
	/*
	 * For src ring, this is the next index not yet processed by HW.
	 * This is a cached copy of the real HW index (read index), used
	 * for avoiding reading the HW index register more often than
	 * necessary.
	 * This extends the invariant:
	 *     write index >= read index >= hw_index >= sw_index
	 *
	 * For dest ring, this is currently unused.
	 */
	/* cached copy */
	unsigned int hw_index;

	/* Start of DMA-coherent area reserved for descriptors */
	/* Host address space */
	void *base_addr_owner_space_unaligned;
	/* CE address space */
	u32 base_addr_ce_space_unaligned;

	/*
	 * Actual start of descriptors.
	 * Aligned to descriptor-size boundary.
	 * Points into reserved DMA-coherent area, above.
	 */
	/* Host address space */
	void *base_addr_owner_space;

	/* CE address space */
	u32 base_addr_ce_space;

	char *shadow_base_unaligned;
	struct ce_desc *shadow_base;

	/* keep last */
	void *per_transfer_context[0];
};

struct ath10k_ce_pipe {
	struct ath10k *ar;
	unsigned int id;

	unsigned int attr_flags;

	u32 ctrl_addr;

	void (*send_cb)(struct ath10k_ce_pipe *);
	void (*recv_cb)(struct ath10k_ce_pipe *);

	unsigned int src_sz_max;
	struct ath10k_ce_ring *src_ring;
	struct ath10k_ce_ring *dest_ring;
};

/* Copy Engine settable attributes */
struct ce_attr;

#define SHADOW_VALUE0       (ar->shadow_reg_value->shadow_reg_value_0)
#define SHADOW_VALUE1       (ar->shadow_reg_value->shadow_reg_value_1)
#define SHADOW_VALUE2       (ar->shadow_reg_value->shadow_reg_value_2)
#define SHADOW_VALUE3       (ar->shadow_reg_value->shadow_reg_value_3)
#define SHADOW_VALUE4       (ar->shadow_reg_value->shadow_reg_value_4)
#define SHADOW_VALUE5       (ar->shadow_reg_value->shadow_reg_value_5)
#define SHADOW_VALUE6       (ar->shadow_reg_value->shadow_reg_value_6)
#define SHADOW_VALUE7       (ar->shadow_reg_value->shadow_reg_value_7)
#define SHADOW_VALUE8       (ar->shadow_reg_value->shadow_reg_value_8)
#define SHADOW_VALUE9       (ar->shadow_reg_value->shadow_reg_value_9)
#define SHADOW_VALUE10      (ar->shadow_reg_value->shadow_reg_value_10)
#define SHADOW_VALUE11      (ar->shadow_reg_value->shadow_reg_value_11)
#define SHADOW_VALUE12      (ar->shadow_reg_value->shadow_reg_value_12)
#define SHADOW_VALUE13      (ar->shadow_reg_value->shadow_reg_value_13)
#define SHADOW_VALUE14      (ar->shadow_reg_value->shadow_reg_value_14)
#define SHADOW_VALUE15      (ar->shadow_reg_value->shadow_reg_value_15)
#define SHADOW_VALUE16      (ar->shadow_reg_value->shadow_reg_value_16)
#define SHADOW_VALUE17      (ar->shadow_reg_value->shadow_reg_value_17)
#define SHADOW_VALUE18      (ar->shadow_reg_value->shadow_reg_value_18)
#define SHADOW_VALUE19      (ar->shadow_reg_value->shadow_reg_value_19)
#define SHADOW_VALUE20      (ar->shadow_reg_value->shadow_reg_value_20)
#define SHADOW_VALUE21      (ar->shadow_reg_value->shadow_reg_value_21)
#define SHADOW_VALUE22      (ar->shadow_reg_value->shadow_reg_value_22)
#define SHADOW_VALUE23      (ar->shadow_reg_value->shadow_reg_value_23)
#define SHADOW_ADDRESS0     (ar->shadow_reg_address->shadow_reg_address_0)
#define SHADOW_ADDRESS1     (ar->shadow_reg_address->shadow_reg_address_1)
#define SHADOW_ADDRESS2     (ar->shadow_reg_address->shadow_reg_address_2)
#define SHADOW_ADDRESS3     (ar->shadow_reg_address->shadow_reg_address_3)
#define SHADOW_ADDRESS4     (ar->shadow_reg_address->shadow_reg_address_4)
#define SHADOW_ADDRESS5     (ar->shadow_reg_address->shadow_reg_address_5)
#define SHADOW_ADDRESS6     (ar->shadow_reg_address->shadow_reg_address_6)
#define SHADOW_ADDRESS7     (ar->shadow_reg_address->shadow_reg_address_7)
#define SHADOW_ADDRESS8     (ar->shadow_reg_address->shadow_reg_address_8)
#define SHADOW_ADDRESS9     (ar->shadow_reg_address->shadow_reg_address_9)
#define SHADOW_ADDRESS10    (ar->shadow_reg_address->shadow_reg_address_10)
#define SHADOW_ADDRESS11    (ar->shadow_reg_address->shadow_reg_address_11)
#define SHADOW_ADDRESS12    (ar->shadow_reg_address->shadow_reg_address_12)
#define SHADOW_ADDRESS13    (ar->shadow_reg_address->shadow_reg_address_13)
#define SHADOW_ADDRESS14    (ar->shadow_reg_address->shadow_reg_address_14)
#define SHADOW_ADDRESS15    (ar->shadow_reg_address->shadow_reg_address_15)
#define SHADOW_ADDRESS16    (ar->shadow_reg_address->shadow_reg_address_16)
#define SHADOW_ADDRESS17    (ar->shadow_reg_address->shadow_reg_address_17)
#define SHADOW_ADDRESS18    (ar->shadow_reg_address->shadow_reg_address_18)
#define SHADOW_ADDRESS19    (ar->shadow_reg_address->shadow_reg_address_19)
#define SHADOW_ADDRESS20    (ar->shadow_reg_address->shadow_reg_address_20)
#define SHADOW_ADDRESS21    (ar->shadow_reg_address->shadow_reg_address_21)
#define SHADOW_ADDRESS22    (ar->shadow_reg_address->shadow_reg_address_22)
#define SHADOW_ADDRESS23    (ar->shadow_reg_address->shadow_reg_address_23)

#define SHADOW_ADDRESS(i) (SHADOW_ADDRESS0 + \
			   i * (SHADOW_ADDRESS1 - SHADOW_ADDRESS0))

u32 shadow_sr_wr_ind_addr(struct ath10k *ar, u32 ctrl_addr);
u32 shadow_dst_wr_ind_addr(struct ath10k *ar, u32 ctrl_addr);

struct ath10k_bus_ops {
	u32 (*read32)(struct ath10k *ar, u32 offset);
	void (*write32)(struct ath10k *ar, u32 offset, u32 value);
	int (*get_num_banks)(struct ath10k *ar);
};

static inline struct bus_opaque *ath10k_bus_priv(struct ath10k *ar)
{
	return (struct bus_opaque *)ar->drv_priv;
}

struct bus_opaque {
	/* protects CE info */
	spinlock_t ce_lock;
	const struct ath10k_bus_ops *bus_ops;
	struct ath10k_ce_pipe ce_states[CE_COUNT_MAX];
};

/*==================Send====================*/

/* ath10k_ce_send flags */
#define CE_SEND_FLAG_BYTE_SWAP 1

/*
 * Queue a source buffer to be sent to an anonymous destination buffer.
 *   ce         - which copy engine to use
 *   buffer          - address of buffer
 *   nbytes          - number of bytes to send
 *   transfer_id     - arbitrary ID; reflected to destination
 *   flags           - CE_SEND_FLAG_* values
 * Returns 0 on success; otherwise an error status.
 *
 * Note: If no flags are specified, use CE's default data swap mode.
 *
 * Implementation note: pushes 1 buffer to Source ring
 */
int ath10k_ce_send(struct ath10k_ce_pipe *ce_state,
		   void *per_transfer_send_context,
		   dma_addr_t buffer,
		   unsigned int nbytes,
		   /* 14 bits */
		   unsigned int transfer_id,
		   unsigned int flags);

int ath10k_ce_send_nolock(struct ath10k_ce_pipe *ce_state,
			  void *per_transfer_context,
			  dma_addr_t buffer,
			  unsigned int nbytes,
			  unsigned int transfer_id,
			  unsigned int flags);

void __ath10k_ce_send_revert(struct ath10k_ce_pipe *pipe);

int ath10k_ce_num_free_src_entries(struct ath10k_ce_pipe *pipe);

/*==================Recv=======================*/

int __ath10k_ce_rx_num_free_bufs(struct ath10k_ce_pipe *pipe);
int __ath10k_ce_rx_post_buf(struct ath10k_ce_pipe *pipe, void *ctx,
			    dma_addr_t paddr);
int ath10k_ce_rx_post_buf(struct ath10k_ce_pipe *pipe, void *ctx,
			  dma_addr_t paddr);
void ath10k_ce_rx_update_write_idx(struct ath10k_ce_pipe *pipe, u32 nentries);

/* recv flags */
/* Data is byte-swapped */
#define CE_RECV_FLAG_SWAPPED	1

/*
 * Supply data for the next completed unprocessed receive descriptor.
 * Pops buffer from Dest ring.
 */
int ath10k_ce_completed_recv_next(struct ath10k_ce_pipe *ce_state,
				  void **per_transfer_contextp,
				  unsigned int *nbytesp);
/*
 * Supply data for the next completed unprocessed send descriptor.
 * Pops 1 completed send buffer from Source ring.
 */
int ath10k_ce_completed_send_next(struct ath10k_ce_pipe *ce_state,
				  void **per_transfer_contextp);

int ath10k_ce_completed_send_next_nolock(struct ath10k_ce_pipe *ce_state,
					 void **per_transfer_contextp);

/*==================CE Engine Initialization=======================*/

int ath10k_ce_init_pipe(struct ath10k *ar, unsigned int ce_id,
			const struct ce_attr *attr);
void ath10k_ce_deinit_pipe(struct ath10k *ar, unsigned int ce_id);
int ath10k_ce_alloc_pipe(struct ath10k *ar, int ce_id,
			 const struct ce_attr *attr);
void ath10k_ce_free_pipe(struct ath10k *ar, int ce_id);

/*==================CE Engine Shutdown=======================*/
/*
 * Support clean shutdown by allowing the caller to revoke
 * receive buffers.  Target DMA must be stopped before using
 * this API.
 */
int ath10k_ce_revoke_recv_next(struct ath10k_ce_pipe *ce_state,
			       void **per_transfer_contextp,
			       u32 *bufferp);

int ath10k_ce_completed_recv_next_nolock(struct ath10k_ce_pipe *ce_state,
					 void **per_transfer_contextp,
					 unsigned int *nbytesp);

/*
 * Support clean shutdown by allowing the caller to cancel
 * pending sends.  Target DMA must be stopped before using
 * this API.
 */
int ath10k_ce_cancel_send_next(struct ath10k_ce_pipe *ce_state,
			       void **per_transfer_contextp,
			       u32 *bufferp,
			       unsigned int *nbytesp,
			       unsigned int *transfer_idp);

/*==================CE Interrupt Handlers====================*/
void ath10k_ce_per_engine_service_any(struct ath10k *ar);
void ath10k_ce_per_engine_service(struct ath10k *ar, unsigned int ce_id);
int ath10k_ce_disable_interrupts(struct ath10k *ar);
void ath10k_ce_enable_interrupts(struct ath10k *ar);
void ath10k_ce_disable_per_ce_interrupts(struct ath10k *ar, unsigned int ce_id);
void ath10k_ce_enable_per_ce_interrupts(struct ath10k *ar, unsigned int ce_id);

/* ce_attr.flags values */
/* Use NonSnooping PCIe accesses? */
#define CE_ATTR_NO_SNOOP		1

/* Byte swap data words */
#define CE_ATTR_BYTE_SWAP_DATA		2

/* Swizzle descriptors? */
#define CE_ATTR_SWIZZLE_DESCRIPTORS	4

/* no interrupt on copy completion */
#define CE_ATTR_DIS_INTR		8

/* Attributes of an instance of a Copy Engine */
struct ce_attr {
	/* CE_ATTR_* values */
	unsigned int flags;

	/* #entries in source ring - Must be a power of 2 */
	unsigned int src_nentries;

	/*
	 * Max source send size for this CE.
	 * This is also the minimum size of a destination buffer.
	 */
	unsigned int src_sz_max;

	/* #entries in destination ring - Must be a power of 2 */
	unsigned int dest_nentries;

	void (*send_cb)(struct ath10k_ce_pipe *);
	void (*recv_cb)(struct ath10k_ce_pipe *);
};

#ifndef CONFIG_ATH10K_SNOC
#define CE_CMD_HALT_STATUS_MSB			3
#define CE_CMD_HALT_STATUS_LSB			3
#define CE_CMD_HALT_STATUS_MASK			0x00000008
#define CE_CMD_HALT_STATUS_GET(x) \
	(((x) & CE_CMD_HALT_STATUS_MASK) >> CE_CMD_HALT_STATUS_LSB)
#define CE_CMD_HALT_STATUS_SET(x) \
	(((0 | (x)) << CE_CMD_HALT_STATUS_LSB) & CE_CMD_HALT_STATUS_MASK)
#define CE_CMD_HALT_STATUS_RESET		0
#define CE_CMD_HALT_MSB				0
#define CE_CMD_HALT_MASK			0x00000001

#define HOST_IE_COPY_COMPLETE_MSB		0
#define HOST_IE_COPY_COMPLETE_LSB		0
#define HOST_IE_COPY_COMPLETE_MASK		0x00000001
#define HOST_IE_COPY_COMPLETE_GET(x) \
	(((x) & HOST_IE_COPY_COMPLETE_MASK) >> HOST_IE_COPY_COMPLETE_LSB)
#define HOST_IE_COPY_COMPLETE_SET(x) \
	(((0 | (x)) << HOST_IE_COPY_COMPLETE_LSB) & HOST_IE_COPY_COMPLETE_MASK)
#define HOST_IE_COPY_COMPLETE_RESET		0

#define HOST_IS_DST_RING_LOW_WATERMARK_MASK	0x00000010
#define HOST_IS_DST_RING_HIGH_WATERMARK_MASK	0x00000008
#define HOST_IS_SRC_RING_LOW_WATERMARK_MASK	0x00000004
#define HOST_IS_SRC_RING_HIGH_WATERMARK_MASK	0x00000002
#define HOST_IS_COPY_COMPLETE_MASK		0x00000001
#define HOST_IS_ADDRESS				0x0030

#define MISC_IS_AXI_ERR_MASK			0x00000400

#define MISC_IS_DST_ADDR_ERR_MASK		0x00000200
#define MISC_IS_SRC_LEN_ERR_MASK		0x00000100
#define MISC_IS_DST_MAX_LEN_VIO_MASK		0x00000080
#define MISC_IS_DST_RING_OVERFLOW_MASK		0x00000040
#define MISC_IS_SRC_RING_OVERFLOW_MASK		0x00000020

#define MISC_IS_ADDRESS				0x0038

#define SRC_WATERMARK_LOW_MSB			31
#define SRC_WATERMARK_LOW_LSB			16
#define SRC_WATERMARK_LOW_MASK			0xffff0000
#define SRC_WATERMARK_LOW_GET(x) \
	(((x) & SRC_WATERMARK_LOW_MASK) >> SRC_WATERMARK_LOW_LSB)
#define SRC_WATERMARK_LOW_SET(x) \
	(((0 | (x)) << SRC_WATERMARK_LOW_LSB) & SRC_WATERMARK_LOW_MASK)
#define SRC_WATERMARK_LOW_RESET			0
#define SRC_WATERMARK_HIGH_MSB			15
#define SRC_WATERMARK_HIGH_LSB			0
#define SRC_WATERMARK_HIGH_MASK			0x0000ffff
#define SRC_WATERMARK_HIGH_GET(x) \
	(((x) & SRC_WATERMARK_HIGH_MASK) >> SRC_WATERMARK_HIGH_LSB)
#define SRC_WATERMARK_HIGH_SET(x) \
	(((0 | (x)) << SRC_WATERMARK_HIGH_LSB) & SRC_WATERMARK_HIGH_MASK)
#define SRC_WATERMARK_HIGH_RESET		0
#define SRC_WATERMARK_ADDRESS			0x004c

#define DST_WATERMARK_LOW_LSB			16
#define DST_WATERMARK_LOW_MASK			0xffff0000
#define DST_WATERMARK_LOW_SET(x) \
	(((0 | (x)) << DST_WATERMARK_LOW_LSB) & DST_WATERMARK_LOW_MASK)
#define DST_WATERMARK_LOW_RESET			0
#define DST_WATERMARK_HIGH_MSB			15
#define DST_WATERMARK_HIGH_LSB			0
#define DST_WATERMARK_HIGH_MASK			0x0000ffff
#define DST_WATERMARK_HIGH_GET(x) \
	(((x) & DST_WATERMARK_HIGH_MASK) >> DST_WATERMARK_HIGH_LSB)
#define DST_WATERMARK_HIGH_SET(x) \
	(((0 | (x)) << DST_WATERMARK_HIGH_LSB) & DST_WATERMARK_HIGH_MASK)
#define DST_WATERMARK_HIGH_RESET		0
#define DST_WATERMARK_ADDRESS			0x0050

#else
#define WCN3990_CE0_SR_BA_LOW		(0x00240000)
#define WCN3990_CE1_SR_BA_LOW		(0x00241000)
#define WCN3990_CE2_SR_BA_LOW		(0x00242000)
#define WCN3990_CE3_SR_BA_LOW		(0x00243000)
#define WCN3990_CE4_SR_BA_LOW		(0x00244000)
#define WCN3990_CE5_SR_BA_LOW		(0x00245000)
#define WCN3990_CE6_SR_BA_LOW		(0x00246000)
#define WCN3990_CE7_SR_BA_LOW		(0x00247000)
#define WCN3990_CE8_SR_BA_LOW		(0x00248000)
#define WCN3990_CE9_SR_BA_LOW		(0x00249000)
#define WCN3990_CE10_SR_BA_LOW		(0x0024A000)
#define WCN3990_CE11_SR_BA_LOW		(0x0024B000)
#define WCN3990_CE0_DR_BA_LOW		(0x0024000C)
#define WNC3990_CE0_DR_SIZE		(0x00240014)
#define WCN3990_CE0_CE_CTRL1		(0x00240018)
#define WCN3990_CE0_HOST_IE		(0x0024002C)
#define WCN3990_CE0_HOST_IS		(0x00240030)
#define WCN3990_CE0_MISC_IE		(0x00240034)
#define WCN3990_CE0_MISC_IS		(0x00240038)
#define WCN3990_CE0_SRC_WR_INDEX	(0x0024003C)
#define WCN3990_CE0_CURRENT_SRRI	(0x00240044)
#define WCN3990_CE0_CURRENT_DRRI	(0x00240048)
#define WCN3990_CE0_SRC_WATERMARK	(0x0024004C)
#define WCN3990_CE0_DST_WATERMARK	(0x00240050)
#define WCN3990_CE0_SR_SIZE		(0x00240008)
#define HOST_IE_COPY_COMPLETE_MASK	(0x00000001)
#define WCN3990_CE_WRAPPER_HOST_INTERRUPT_SUMMARY	0x0024C000
#define WCN3990_CE_WRAPPER_INDEX_BASE_LOW		0x0024C004
#define WCN3990_CE_WRAPPER_INDEX_BASE_HIGH		0x0024C008
#define CE_CTRL1_IDX_UPD_EN				0x00080000

#define WCN3990_CE_WRAPPER_BASE_ADDRESS \
			WCN3990_CE_WRAPPER_HOST_INTERRUPT_SUMMARY
#define WCN3990_CE0_BASE_ADDRESS \
			WCN3990_CE0_SR_BA_LOW
#define WCN3990_CE1_BASE_ADDRESS \
			WCN3990_CE1_SR_BA_LOW
#define WCN3990_CE2_BASE_ADDRESS \
			WCN3990_CE2_SR_BA_LOW
#define WCN3990_CE3_BASE_ADDRESS \
			WCN3990_CE3_SR_BA_LOW
#define WCN3990_CE4_BASE_ADDRESS \
			WCN3990_CE4_SR_BA_LOW
#define WCN3990_CE5_BASE_ADDRESS \
			WCN3990_CE5_SR_BA_LOW
#define WCN3990_CE6_BASE_ADDRESS \
			WCN3990_CE6_SR_BA_LOW
#define WCN3990_CE7_BASE_ADDRESS \
			WCN3990_CE7_SR_BA_LOW
#define WCN3990_CE8_BASE_ADDRESS \
			WCN3990_CE8_SR_BA_LOW
#define WCN3990_CE9_BASE_ADDRESS \
			WCN3990_CE9_SR_BA_LOW
#define WCN3990_CE10_BASE_ADDRESS \
			WCN3990_CE10_SR_BA_LOW
#define WCN3990_CE11_BASE_ADDRESS \
			WCN3990_CE11_SR_BA_LOW

#define HOST_IS_DST_RING_LOW_WATERMARK_MASK	0x00000010
#define HOST_IS_DST_RING_HIGH_WATERMARK_MASK	0x00000008
#define HOST_IS_SRC_RING_LOW_WATERMARK_MASK	0x00000004
#define HOST_IS_SRC_RING_HIGH_WATERMARK_MASK	0x00000002
#define HOST_IS_COPY_COMPLETE_MASK		0x00000001
#define HOST_IS_ADDRESS		(WCN3990_CE0_HOST_IS \
				- WCN3990_CE0_BASE_ADDRESS)

#define MISC_IS_AXI_ERR_MASK			0x00000100
#define MISC_IS_DST_ADDR_ERR_MASK		0x00000200
#define MISC_IS_SRC_LEN_ERR_MASK		0x00000100
#define MISC_IS_DST_MAX_LEN_VIO_MASK		0x00000080
#define MISC_IS_DST_RING_OVERFLOW_MASK		0x00000040
#define MISC_IS_SRC_RING_OVERFLOW_MASK		0x00000020
#define MISC_IS_ADDRESS		(WCN3990_CE0_MISC_IS \
				- WCN3990_CE0_BASE_ADDRESS)

#define SRC_WATERMARK_LOW_MSB			0
#define SRC_WATERMARK_LOW_LSB			16

#define SRC_WATERMARK_LOW_MASK			0xffff0000
#define SRC_WATERMARK_LOW_GET(x) \
	(((x) & SRC_WATERMARK_LOW_MASK) >> SRC_WATERMARK_LOW_LSB)
#define SRC_WATERMARK_LOW_SET(x) \
	(((0 | (x)) << SRC_WATERMARK_LOW_LSB) & SRC_WATERMARK_LOW_MASK)

#define SRC_WATERMARK_LOW_RESET			0
#define SRC_WATERMARK_HIGH_MSB			15
#define SRC_WATERMARK_HIGH_LSB			0
#define SRC_WATERMARK_HIGH_MASK			0x0000ffff
#define SRC_WATERMARK_HIGH_GET(x) \
	(((x) & SRC_WATERMARK_HIGH_MASK) >> SRC_WATERMARK_HIGH_LSB)
#define SRC_WATERMARK_HIGH_SET(x) \
	(((0 | (x)) << SRC_WATERMARK_HIGH_LSB) & SRC_WATERMARK_HIGH_MASK)

#define SRC_WATERMARK_HIGH_RESET		0
#define SRC_WATERMARK_ADDRESS	(WCN3990_CE0_SRC_WATERMARK\
					- WCN3990_CE0_BASE_ADDRESS)

#define DST_WATERMARK_LOW_LSB			16
#define DST_WATERMARK_LOW_MASK			0xffff0000
#define DST_WATERMARK_LOW_SET(x) \
	(((0 | (x)) << DST_WATERMARK_LOW_LSB) & DST_WATERMARK_LOW_MASK)
#define DST_WATERMARK_LOW_RESET			0
#define DST_WATERMARK_HIGH_MSB			15
#define DST_WATERMARK_HIGH_LSB			0
#define DST_WATERMARK_HIGH_MASK			0x0000ffff
#define DST_WATERMARK_HIGH_GET(x) \
	(((x) & DST_WATERMARK_HIGH_MASK) >> DST_WATERMARK_HIGH_LSB)
#define DST_WATERMARK_HIGH_SET(x) \
	(((0 | (x)) << DST_WATERMARK_HIGH_LSB) & DST_WATERMARK_HIGH_MASK)
#define DST_WATERMARK_HIGH_RESET		0
#define DST_WATERMARK_ADDRESS	(WCN3990_CE0_DST_WATERMARK \
						- WCN3990_CE0_BASE_ADDRESS)

#define BITS0_TO_31(val) ((uint32_t)((uint64_t)(val)\
				     & (uint64_t)(0xFFFFFFFF)))
#define BITS32_TO_35(val) ((uint32_t)(((uint64_t)(val)\
				     & (uint64_t)(0xF00000000)) >> 32))
#endif

#define COPY_ENGINE_ID(COPY_ENGINE_BASE_ADDRESS) ((COPY_ENGINE_BASE_ADDRESS \
		- CE0_BASE_ADDRESS) / (CE1_BASE_ADDRESS - CE0_BASE_ADDRESS))

static inline u32 ath10k_ce_base_address(struct ath10k *ar, unsigned int ce_id)
{
	return CE0_BASE_ADDRESS + (CE1_BASE_ADDRESS - CE0_BASE_ADDRESS) * ce_id;
}

#define CE_WATERMARK_MASK (HOST_IS_SRC_RING_LOW_WATERMARK_MASK  | \
			   HOST_IS_SRC_RING_HIGH_WATERMARK_MASK | \
			   HOST_IS_DST_RING_LOW_WATERMARK_MASK  | \
			   HOST_IS_DST_RING_HIGH_WATERMARK_MASK)

#define CE_ERROR_MASK	(MISC_IS_AXI_ERR_MASK           | \
			 MISC_IS_DST_ADDR_ERR_MASK      | \
			 MISC_IS_SRC_LEN_ERR_MASK       | \
			 MISC_IS_DST_MAX_LEN_VIO_MASK   | \
			 MISC_IS_DST_RING_OVERFLOW_MASK | \
			 MISC_IS_SRC_RING_OVERFLOW_MASK)

#define CE_SRC_RING_TO_DESC(baddr, idx) \
	(&(((struct ce_desc *)baddr)[idx]))

#define CE_DEST_RING_TO_DESC(baddr, idx) \
	(&(((struct ce_desc *)baddr)[idx]))

/* Ring arithmetic (modulus number of entries in ring, which is a pwr of 2). */
#define CE_RING_DELTA(nentries_mask, fromidx, toidx) \
	(((int)(toidx) - (int)(fromidx)) & (nentries_mask))

#define CE_RING_IDX_INCR(nentries_mask, idx) (((idx) + 1) & (nentries_mask))
#define CE_RING_IDX_ADD(nentries_mask, idx, num) \
		(((idx) + (num)) & (nentries_mask))

#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB \
				ar->regs->ce_wrap_intr_sum_host_msi_lsb
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK \
				ar->regs->ce_wrap_intr_sum_host_msi_mask
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET(x) \
	(((x) & CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK) >> \
		CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB)
#define CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS			0x0000

#define CE_INTERRUPT_SUMMARY(ar, ar_opaque) \
	CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET( \
		ar_opaque->bus_ops->read32((ar), CE_WRAPPER_BASE_ADDRESS + \
		CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS))

#endif /* _CE_H_ */
