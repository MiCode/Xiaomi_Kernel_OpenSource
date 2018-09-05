/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/ipa_mhi.h>
#include <linux/ipa.h>
#include "../ipa_v3/ipa_i.h"
#include "../../gsi/gsi.h"
#include "../../gsi/gsi_reg.h"
#include "ipa_ut_framework.h"

#define IPA_MHI_TEST_NUM_CHANNELS		8
#define IPA_MHI_TEST_NUM_EVENT_RINGS		8
#define IPA_MHI_TEST_FIRST_CHANNEL_ID		100
#define IPA_MHI_TEST_FIRST_EVENT_RING_ID	100
#define IPA_MHI_TEST_LAST_CHANNEL_ID \
	(IPA_MHI_TEST_FIRST_CHANNEL_ID + IPA_MHI_TEST_NUM_CHANNELS - 1)
#define IPA_MHI_TEST_LAST_EVENT_RING_ID \
	(IPA_MHI_TEST_FIRST_EVENT_RING_ID + IPA_MHI_TEST_NUM_EVENT_RINGS - 1)
#define IPA_MHI_TEST_MAX_DATA_BUF_SIZE		1500
#define IPA_MHI_TEST_SEQ_TYPE_DMA		0x00000000

#define IPA_MHI_TEST_LOOP_NUM			5
#define IPA_MHI_RUN_TEST_UNIT_IN_LOOP(test_unit, rc, args...)		\
	do {								\
		int __i;						\
		for (__i = 0; __i < IPA_MHI_TEST_LOOP_NUM; __i++) {	\
			IPA_UT_LOG(#test_unit " START iter %d\n", __i);	\
			rc = test_unit(args);				\
			if (!rc)					\
				continue;				\
			IPA_UT_LOG(#test_unit " failed %d\n", rc);	\
			break;						\
		}							\
	} while (0)

/**
 * check for MSI interrupt for one or both channels:
 * OUT channel MSI my be missed as it
 * will be overwritten by the IN channel MSI
 */
#define IPA_MHI_TEST_CHECK_MSI_INTR(__both, __timeout)			\
	do {								\
		int i;							\
		for (i = 0; i < 20; i++) {				\
			if (*((u32 *)test_mhi_ctx->msi.base) ==		\
				(0x10000000 |				\
				(IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1))) { \
				__timeout = false;			\
				break;					\
			}						\
			if (__both && (*((u32 *)test_mhi_ctx->msi.base) == \
				(0x10000000 |				\
				(IPA_MHI_TEST_FIRST_EVENT_RING_ID)))) { \
				/* sleep to be sure IN MSI is generated */ \
				msleep(20);				\
				__timeout = false;			\
				break;					\
			}						\
			msleep(20);					\
		}							\
	} while (0)

static DECLARE_COMPLETION(mhi_test_ready_comp);
static DECLARE_COMPLETION(mhi_test_wakeup_comp);

/**
 * enum ipa_mhi_ring_elements_type - MHI ring elements types.
 */
enum ipa_mhi_ring_elements_type {
	IPA_MHI_RING_ELEMENT_NO_OP = 1,
	IPA_MHI_RING_ELEMENT_TRANSFER = 2
};

/**
 * enum ipa_mhi_channel_direction - MHI channel directions
 */
enum ipa_mhi_channel_direction {
	IPA_MHI_OUT_CHAHNNEL = 1,
	IPA_MHI_IN_CHAHNNEL = 2,
};

/**
 * struct ipa_mhi_channel_context_array - MHI Channel context array entry
 *
 * mapping is taken from MHI spec
 */
struct ipa_mhi_channel_context_array {
	u32	chstate:8;	/*0-7*/
	u32	brsmode:2;	/*8-9*/
	u32	pollcfg:6;	/*10-15*/
	u32	reserved:16;	/*16-31*/
	u32	chtype;		/*channel type (inbound/outbound)*/
	u32	erindex;	/*event ring index*/
	u64	rbase;		/*ring base address in the host addr spc*/
	u64	rlen;		/*ring length in bytes*/
	u64	rp;		/*read pointer in the host system addr spc*/
	u64	wp;		/*write pointer in the host system addr spc*/
} __packed;

/**
 * struct ipa_mhi_event_context_array - MGI event ring context array entry
 *
 * mapping is taken from MHI spec
 */
struct ipa_mhi_event_context_array {
	u16	intmodc;
	u16	intmodt;/* Interrupt moderation timer (in microseconds) */
	u32	ertype;
	u32	msivec;	/* MSI vector for interrupt (MSI data)*/
	u64	rbase;	/* ring base address in host address space*/
	u64	rlen;	/* ring length in bytes*/
	u64	rp;	/* read pointer in the host system address space*/
	u64	wp;	/* write pointer in the host system address space*/
} __packed;

/**
 *
 * struct ipa_mhi_mmio_register_set - MHI configuration registers,
 *	control registers, status registers, pointers to doorbell arrays,
 *	pointers to channel and event context arrays.
 *
 * The structure is defined in mhi spec (register names are taken from there).
 *	Only values accessed by HWP or test are documented
 */
struct ipa_mhi_mmio_register_set {
	u32	mhireglen;
	u32	reserved_08_04;
	u32	mhiver;
	u32	reserved_10_0c;
	struct mhicfg {
		u8		nch;
		u8		reserved_15_8;
		u8		ner;
		u8		reserved_31_23;
	} __packed mhicfg;

	u32	reserved_18_14;
	u32	chdboff;
	u32	reserved_20_1C;
	u32	erdboff;
	u32	reserved_28_24;
	u32	bhioff;
	u32	reserved_30_2C;
	u32	debugoff;
	u32	reserved_38_34;

	struct mhictrl {
		u32 rs : 1;
		u32 reset : 1;
		u32 reserved_7_2 : 6;
		u32 mhistate : 8;
		u32 reserved_31_16 : 16;
	} __packed mhictrl;

	u64	reserved_40_3c;
	u32	reserved_44_40;

	struct mhistatus {
		u32 ready : 1;
		u32 reserved_3_2 : 1;
		u32 syserr : 1;
		u32 reserved_7_3 : 5;
		u32 mhistate : 8;
		u32 reserved_31_16 : 16;
	} __packed mhistatus;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the handle for
	 *  the buffer of channel context array
	 */
	u32 reserved_50_4c;

	u32 mhierror;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the handle for
	 * the buffer of event ring context array
	 */
	u32 reserved_58_54;

	/**
	 * 64-bit pointer to the channel context array in the host memory space
	 *  host sets the pointer to the channel context array during
	 *  initialization.
	 */
	u64 ccabap;
	/**
	 * 64-bit pointer to the event context array in the host memory space
	 *  host sets the pointer to the event context array during
	 *  initialization
	 */
	u64 ecabap;
	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of virtual address
	 *  for the buffer of channel context array
	 */
	u64 crcbap;
	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of virtual address
	 *  for the buffer of event ring context array
	 */
	u64 crdb;

	u64	reserved_80_78;

	struct mhiaddr {
		/**
		 * Base address (64-bit) of the memory region in
		 *  the host address space where the MHI control
		 *  data structures are allocated by the host,
		 *  including channel context array, event context array,
		 *  and rings.
		 *  The device uses this information to set up its internal
		 *   address translation tables.
		 *  value must be aligned to 4 Kbytes.
		 */
		u64 mhicrtlbase;
		/**
		 * Upper limit address (64-bit) of the memory region in
		 *  the host address space where the MHI control
		 *  data structures are allocated by the host.
		 * The device uses this information to setup its internal
		 *  address translation tables.
		 * The most significant 32 bits of MHICTRLBASE and
		 * MHICTRLLIMIT registers must be equal.
		 */
		u64 mhictrllimit;
		u64 reserved_18_10;
		/**
		 * Base address (64-bit) of the memory region in
		 *  the host address space where the MHI data buffers
		 *  are allocated by the host.
		 * The device uses this information to setup its
		 *  internal address translation tables.
		 * value must be aligned to 4 Kbytes.
		 */
		u64 mhidatabase;
		/**
		 * Upper limit address (64-bit) of the memory region in
		 *  the host address space where the MHI data buffers
		 *  are allocated by the host.
		 * The device uses this information to setup its
		 *  internal address translation tables.
		 * The most significant 32 bits of MHIDATABASE and
		 *  MHIDATALIMIT registers must be equal.
		 */
		u64 mhidatalimit;
		u64 reserved_30_28;
	} __packed mhiaddr;

} __packed;

/**
 * struct ipa_mhi_event_ring_element - MHI Event ring element
 *
 * mapping is taken from MHI spec
 */
struct ipa_mhi_event_ring_element {
	/**
	 * pointer to ring element that generated event in
	 *  the host system address space
	 */
	u64	ptr;
	union {
		struct {
			u32	len : 24;
			u32	code : 8;
		} __packed bits;
		u32	dword;
	} __packed dword_8;
	u16	reserved;
	u8		type;
	u8		chid;
} __packed;

/**
 * struct ipa_mhi_transfer_ring_element - MHI Transfer ring element
 *
 * mapping is taken from MHI spec
 */
struct ipa_mhi_transfer_ring_element {
	u64	ptr; /*pointer to buffer in the host system address space*/
	u16	len; /*transaction length in bytes*/
	u16	reserved0;
	union {
		struct {
			u16		chain : 1;
			u16		reserved_7_1 : 7;
			u16		ieob : 1;
			u16		ieot : 1;
			u16		bei : 1;
			u16		reserved_15_11 : 5;
		} __packed bits;
		u16	word;
	} __packed word_C;
	u8		type;
	u8		reserved1;
} __packed;

/**
 * struct ipa_test_mhi_context - MHI test context
 */
struct ipa_test_mhi_context {
	void __iomem *gsi_mmio;
	struct ipa_mem_buffer msi;
	struct ipa_mem_buffer ch_ctx_array;
	struct ipa_mem_buffer ev_ctx_array;
	struct ipa_mem_buffer mmio_buf;
	struct ipa_mem_buffer xfer_ring_bufs[IPA_MHI_TEST_NUM_CHANNELS];
	struct ipa_mem_buffer ev_ring_bufs[IPA_MHI_TEST_NUM_EVENT_RINGS];
	struct ipa_mem_buffer in_buffer;
	struct ipa_mem_buffer out_buffer;
	u32 prod_hdl;
	u32 cons_hdl;
	u32 test_prod_hdl;
	phys_addr_t transport_phys_addr;
	unsigned long transport_size;
};

static struct ipa_test_mhi_context *test_mhi_ctx;

static void ipa_mhi_test_cb(void *priv,
	enum ipa_mhi_event_type event, unsigned long data)
{
	IPA_UT_DBG("Entry\n");

	if (event == IPA_MHI_EVENT_DATA_AVAILABLE)
		complete_all(&mhi_test_wakeup_comp);
	else if (event == IPA_MHI_EVENT_READY)
		complete_all(&mhi_test_ready_comp);
	else
		WARN_ON(1);
}

static void ipa_test_mhi_free_mmio_space(void)
{
	IPA_UT_DBG("Entry\n");

	if (!test_mhi_ctx)
		return;

	dma_free_coherent(ipa3_ctx->pdev, test_mhi_ctx->mmio_buf.size,
		test_mhi_ctx->mmio_buf.base,
		test_mhi_ctx->mmio_buf.phys_base);

	dma_free_coherent(ipa3_ctx->pdev, test_mhi_ctx->ev_ctx_array.size,
		test_mhi_ctx->ev_ctx_array.base,
		test_mhi_ctx->ev_ctx_array.phys_base);

	dma_free_coherent(ipa3_ctx->pdev, test_mhi_ctx->ch_ctx_array.size,
		test_mhi_ctx->ch_ctx_array.base,
		test_mhi_ctx->ch_ctx_array.phys_base);

	dma_free_coherent(ipa3_ctx->pdev, test_mhi_ctx->msi.size,
		test_mhi_ctx->msi.base, test_mhi_ctx->msi.phys_base);
}

static int ipa_test_mhi_alloc_mmio_space(void)
{
	int rc = 0;
	struct ipa_mem_buffer *msi;
	struct ipa_mem_buffer *ch_ctx_array;
	struct ipa_mem_buffer *ev_ctx_array;
	struct ipa_mem_buffer *mmio_buf;
	struct ipa_mhi_mmio_register_set *p_mmio;

	IPA_UT_DBG("Entry\n");

	msi = &test_mhi_ctx->msi;
	ch_ctx_array = &test_mhi_ctx->ch_ctx_array;
	ev_ctx_array = &test_mhi_ctx->ev_ctx_array;
	mmio_buf = &test_mhi_ctx->mmio_buf;

	/* Allocate MSI */
	msi->size = 4;
	msi->base = dma_alloc_coherent(ipa3_ctx->pdev, msi->size,
		&msi->phys_base, GFP_KERNEL);
	if (!msi->base) {
		IPA_UT_ERR("no mem for msi\n");
		return -ENOMEM;
	}

	IPA_UT_DBG("msi: base 0x%pK phys_addr 0x%pad size %d\n",
		msi->base, &msi->phys_base, msi->size);

	/* allocate buffer for channel context */
	ch_ctx_array->size = sizeof(struct ipa_mhi_channel_context_array) *
		IPA_MHI_TEST_NUM_CHANNELS;
	ch_ctx_array->base = dma_alloc_coherent(ipa3_ctx->pdev,
		ch_ctx_array->size, &ch_ctx_array->phys_base, GFP_KERNEL);
	if (!ch_ctx_array->base) {
		IPA_UT_ERR("no mem for ch ctx array\n");
		rc = -ENOMEM;
		goto fail_free_msi;
	}
	IPA_UT_DBG("channel ctx array: base 0x%pK phys_addr %pad size %d\n",
		ch_ctx_array->base, &ch_ctx_array->phys_base,
		ch_ctx_array->size);

	/* allocate buffer for event context */
	ev_ctx_array->size = sizeof(struct ipa_mhi_event_context_array) *
		IPA_MHI_TEST_NUM_EVENT_RINGS;
	ev_ctx_array->base = dma_alloc_coherent(ipa3_ctx->pdev,
		ev_ctx_array->size, &ev_ctx_array->phys_base, GFP_KERNEL);
	if (!ev_ctx_array->base) {
		IPA_UT_ERR("no mem for ev ctx array\n");
		rc = -ENOMEM;
		goto fail_free_ch_ctx_arr;
	}
	IPA_UT_DBG("event ctx array: base 0x%pK phys_addr %pad size %d\n",
		ev_ctx_array->base, &ev_ctx_array->phys_base,
		ev_ctx_array->size);

	/* allocate buffer for mmio */
	mmio_buf->size = sizeof(struct ipa_mhi_mmio_register_set);
	mmio_buf->base = dma_alloc_coherent(ipa3_ctx->pdev, mmio_buf->size,
		&mmio_buf->phys_base, GFP_KERNEL);
	if (!mmio_buf->base) {
		IPA_UT_ERR("no mem for mmio buf\n");
		rc = -ENOMEM;
		goto fail_free_ev_ctx_arr;
	}
	IPA_UT_DBG("mmio buffer: base 0x%pK phys_addr %pad size %d\n",
		mmio_buf->base, &mmio_buf->phys_base, mmio_buf->size);

	/* initlize table */
	p_mmio = (struct ipa_mhi_mmio_register_set *)mmio_buf->base;

	/**
	 * 64-bit pointer to the channel context array in the host memory space;
	 * Host sets the pointer to the channel context array
	 * during initialization.
	 */
	p_mmio->ccabap = (u32)ch_ctx_array->phys_base -
		(IPA_MHI_TEST_FIRST_CHANNEL_ID *
		sizeof(struct ipa_mhi_channel_context_array));
	IPA_UT_DBG("pMmio->ccabap 0x%llx\n", p_mmio->ccabap);

	/**
	 * 64-bit pointer to the event context array in the host memory space;
	 * Host sets the pointer to the event context array
	 * during initialization
	 */
	p_mmio->ecabap = (u32)ev_ctx_array->phys_base -
		(IPA_MHI_TEST_FIRST_EVENT_RING_ID *
		sizeof(struct ipa_mhi_event_context_array));
	IPA_UT_DBG("pMmio->ecabap 0x%llx\n", p_mmio->ecabap);

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of
	 *  virtual address for the buffer of channel context array
	 */
	p_mmio->crcbap = (unsigned long)ch_ctx_array->base;

	/**
	 * Register is not accessed by HWP.
	 * In test register carries the pointer of
	 *  virtual address for the buffer of channel context array
	 */
	p_mmio->crdb = (unsigned long)ev_ctx_array->base;

	/* test is running only on device. no need to translate addresses */
	p_mmio->mhiaddr.mhicrtlbase = 0x04;
	p_mmio->mhiaddr.mhictrllimit = 0xFFFFFFFF;
	p_mmio->mhiaddr.mhidatabase = 0x04;
	p_mmio->mhiaddr.mhidatalimit = 0xFFFFFFFF;

	return rc;

fail_free_ev_ctx_arr:
	dma_free_coherent(ipa3_ctx->pdev, ev_ctx_array->size,
		ev_ctx_array->base, ev_ctx_array->phys_base);
	ev_ctx_array->base = NULL;
fail_free_ch_ctx_arr:
	dma_free_coherent(ipa3_ctx->pdev, ch_ctx_array->size,
		ch_ctx_array->base, ch_ctx_array->phys_base);
	ch_ctx_array->base = NULL;
fail_free_msi:
	dma_free_coherent(ipa3_ctx->pdev, msi->size, msi->base,
		msi->phys_base);
	msi->base = NULL;
	return rc;
}

static void ipa_mhi_test_destroy_channel_context(
	struct ipa_mem_buffer transfer_ring_bufs[],
	struct ipa_mem_buffer event_ring_bufs[],
	u8 channel_id,
	u8 event_ring_id)
{
	u32 ev_ring_idx;
	u32 ch_idx;

	IPA_UT_DBG("Entry\n");

	if ((channel_id < IPA_MHI_TEST_FIRST_CHANNEL_ID) ||
		(channel_id > IPA_MHI_TEST_LAST_CHANNEL_ID)) {
		IPA_UT_ERR("channal_id invalid %d\n", channel_id);
		return;
	}

	if ((event_ring_id < IPA_MHI_TEST_FIRST_EVENT_RING_ID) ||
		(event_ring_id > IPA_MHI_TEST_LAST_EVENT_RING_ID)) {
		IPA_UT_ERR("event_ring_id invalid %d\n", event_ring_id);
		return;
	}

	ch_idx = channel_id - IPA_MHI_TEST_FIRST_CHANNEL_ID;
	ev_ring_idx = event_ring_id - IPA_MHI_TEST_FIRST_EVENT_RING_ID;

	if (transfer_ring_bufs[ch_idx].base) {
		dma_free_coherent(ipa3_ctx->pdev,
			transfer_ring_bufs[ch_idx].size,
			transfer_ring_bufs[ch_idx].base,
			transfer_ring_bufs[ch_idx].phys_base);
		transfer_ring_bufs[ch_idx].base = NULL;
	}

	if (event_ring_bufs[ev_ring_idx].base) {
		dma_free_coherent(ipa3_ctx->pdev,
			event_ring_bufs[ev_ring_idx].size,
			event_ring_bufs[ev_ring_idx].base,
			event_ring_bufs[ev_ring_idx].phys_base);
		event_ring_bufs[ev_ring_idx].base = NULL;
	}
}

static int ipa_mhi_test_config_channel_context(
	struct ipa_mem_buffer *mmio,
	struct ipa_mem_buffer transfer_ring_bufs[],
	struct ipa_mem_buffer event_ring_bufs[],
	u8 channel_id,
	u8 event_ring_id,
	u16 transfer_ring_size,
	u16 event_ring_size,
	u8 ch_type)
{
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_channels;
	struct ipa_mhi_event_context_array *p_events;
	u32 ev_ring_idx;
	u32 ch_idx;

	IPA_UT_DBG("Entry\n");

	if ((channel_id < IPA_MHI_TEST_FIRST_CHANNEL_ID) ||
		(channel_id > IPA_MHI_TEST_LAST_CHANNEL_ID)) {
		IPA_UT_DBG("channal_id invalid %d\n", channel_id);
		return -EFAULT;
	}

	if ((event_ring_id < IPA_MHI_TEST_FIRST_EVENT_RING_ID) ||
		(event_ring_id > IPA_MHI_TEST_LAST_EVENT_RING_ID)) {
		IPA_UT_DBG("event_ring_id invalid %d\n", event_ring_id);
		return -EFAULT;
	}

	p_mmio = (struct ipa_mhi_mmio_register_set *)mmio->base;
	p_channels =
		(struct ipa_mhi_channel_context_array *)
		((unsigned long)p_mmio->crcbap);
	p_events = (struct ipa_mhi_event_context_array *)
		((unsigned long)p_mmio->crdb);

	IPA_UT_DBG("p_mmio: %pK p_channels: %pK p_events: %pK\n",
		p_mmio, p_channels, p_events);

	ch_idx = channel_id - IPA_MHI_TEST_FIRST_CHANNEL_ID;
	ev_ring_idx = event_ring_id - IPA_MHI_TEST_FIRST_EVENT_RING_ID;

	IPA_UT_DBG("ch_idx: %u ev_ring_idx: %u\n", ch_idx, ev_ring_idx);
	if (transfer_ring_bufs[ch_idx].base) {
		IPA_UT_ERR("ChannelId %d is already allocated\n", channel_id);
		return -EFAULT;
	}

	/* allocate and init event ring if needed */
	if (!event_ring_bufs[ev_ring_idx].base) {
		IPA_UT_LOG("Configuring event ring...\n");
		event_ring_bufs[ev_ring_idx].size =
			event_ring_size *
				sizeof(struct ipa_mhi_event_ring_element);
		event_ring_bufs[ev_ring_idx].base =
			dma_alloc_coherent(ipa3_ctx->pdev,
				event_ring_bufs[ev_ring_idx].size,
				&event_ring_bufs[ev_ring_idx].phys_base,
				GFP_KERNEL);
		if (!event_ring_bufs[ev_ring_idx].base) {
			IPA_UT_ERR("no mem for ev ring buf\n");
			return -ENOMEM;
		}
		p_events[ev_ring_idx].intmodc = 1;
		p_events[ev_ring_idx].intmodt = 0;
		p_events[ev_ring_idx].msivec = event_ring_id;
		p_events[ev_ring_idx].rbase =
			(u32)event_ring_bufs[ev_ring_idx].phys_base;
		p_events[ev_ring_idx].rlen =
			event_ring_bufs[ev_ring_idx].size;
		p_events[ev_ring_idx].rp =
			(u32)event_ring_bufs[ev_ring_idx].phys_base;
		p_events[ev_ring_idx].wp =
			(u32)event_ring_bufs[ev_ring_idx].phys_base +
			event_ring_bufs[ev_ring_idx].size - 16;
	} else {
		IPA_UT_LOG("Skip configuring event ring - already done\n");
	}

	transfer_ring_bufs[ch_idx].size =
		transfer_ring_size *
			sizeof(struct ipa_mhi_transfer_ring_element);
	transfer_ring_bufs[ch_idx].base =
		dma_alloc_coherent(ipa3_ctx->pdev,
			transfer_ring_bufs[ch_idx].size,
			&transfer_ring_bufs[ch_idx].phys_base,
			GFP_KERNEL);
	if (!transfer_ring_bufs[ch_idx].base) {
		IPA_UT_ERR("no mem for xfer ring buf\n");
		dma_free_coherent(ipa3_ctx->pdev,
			event_ring_bufs[ev_ring_idx].size,
			event_ring_bufs[ev_ring_idx].base,
			event_ring_bufs[ev_ring_idx].phys_base);
		event_ring_bufs[ev_ring_idx].base = NULL;
		return -ENOMEM;
	}

	p_channels[ch_idx].erindex = event_ring_id;
	p_channels[ch_idx].rbase = (u32)transfer_ring_bufs[ch_idx].phys_base;
	p_channels[ch_idx].rlen = transfer_ring_bufs[ch_idx].size;
	p_channels[ch_idx].rp = (u32)transfer_ring_bufs[ch_idx].phys_base;
	p_channels[ch_idx].wp = (u32)transfer_ring_bufs[ch_idx].phys_base;
	p_channels[ch_idx].chtype = ch_type;
	p_channels[ch_idx].brsmode = IPA_MHI_BURST_MODE_DEFAULT;
	p_channels[ch_idx].pollcfg = 0;

	return 0;
}

static void ipa_mhi_test_destroy_data_structures(void)
{
	IPA_UT_DBG("Entry\n");

	/* Destroy OUT data buffer */
	if (test_mhi_ctx->out_buffer.base) {
		dma_free_coherent(ipa3_ctx->pdev,
			test_mhi_ctx->out_buffer.size,
			test_mhi_ctx->out_buffer.base,
			test_mhi_ctx->out_buffer.phys_base);
		test_mhi_ctx->out_buffer.base = NULL;
	}

	/* Destroy IN data buffer */
	if (test_mhi_ctx->in_buffer.base) {
		dma_free_coherent(ipa3_ctx->pdev,
			test_mhi_ctx->in_buffer.size,
			test_mhi_ctx->in_buffer.base,
			test_mhi_ctx->in_buffer.phys_base);
		test_mhi_ctx->in_buffer.base = NULL;
	}

	/* Destroy IN channel ctx */
	ipa_mhi_test_destroy_channel_context(
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1);

	/* Destroy OUT channel ctx */
	ipa_mhi_test_destroy_channel_context(
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID);
}

static int ipa_mhi_test_setup_data_structures(void)
{
	int rc = 0;

	IPA_UT_DBG("Entry\n");

	/* Config OUT Channel Context */
	rc = ipa_mhi_test_config_channel_context(
		&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID,
		0x100,
		0x80,
		IPA_MHI_OUT_CHAHNNEL);
	if (rc) {
		IPA_UT_ERR("Fail to config OUT ch ctx - err %d", rc);
		return rc;
	}

	/* Config IN Channel Context */
	rc = ipa_mhi_test_config_channel_context(
		&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1,
		0x100,
		0x80,
		IPA_MHI_IN_CHAHNNEL);
	if (rc) {
		IPA_UT_ERR("Fail to config IN ch ctx - err %d", rc);
		goto fail_destroy_out_ch_ctx;
	}

	/* allocate IN data buffer */
	test_mhi_ctx->in_buffer.size = IPA_MHI_TEST_MAX_DATA_BUF_SIZE;
	test_mhi_ctx->in_buffer.base = dma_alloc_coherent(
		ipa3_ctx->pdev, test_mhi_ctx->in_buffer.size,
		&test_mhi_ctx->in_buffer.phys_base, GFP_KERNEL);
	if (!test_mhi_ctx->in_buffer.base) {
		IPA_UT_ERR("no mem for In data buffer\n");
		rc = -ENOMEM;
		goto fail_destroy_in_ch_ctx;
	}
	memset(test_mhi_ctx->in_buffer.base, 0,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE);

	/* allocate OUT data buffer */
	test_mhi_ctx->out_buffer.size = IPA_MHI_TEST_MAX_DATA_BUF_SIZE;
	test_mhi_ctx->out_buffer.base = dma_alloc_coherent(
		ipa3_ctx->pdev, test_mhi_ctx->out_buffer.size,
		&test_mhi_ctx->out_buffer.phys_base, GFP_KERNEL);
	if (!test_mhi_ctx->out_buffer.base) {
		IPA_UT_ERR("no mem for Out data buffer\n");
		rc = -EFAULT;
		goto fail_destroy_in_data_buf;
	}
	memset(test_mhi_ctx->out_buffer.base, 0,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE);

	return 0;

fail_destroy_in_data_buf:
	dma_free_coherent(ipa3_ctx->pdev,
		test_mhi_ctx->in_buffer.size,
		test_mhi_ctx->in_buffer.base,
		test_mhi_ctx->in_buffer.phys_base);
	test_mhi_ctx->in_buffer.base = NULL;
fail_destroy_in_ch_ctx:
	ipa_mhi_test_destroy_channel_context(
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1);
fail_destroy_out_ch_ctx:
	ipa_mhi_test_destroy_channel_context(
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID);
	return 0;
}

/**
 * ipa_test_mhi_suite_setup() - Suite setup function
 */
static int ipa_test_mhi_suite_setup(void **ppriv)
{
	int rc = 0;
	struct ipa_sys_connect_params sys_in;

	IPA_UT_DBG("Start Setup\n");

	if (!ipa3_ctx) {
		IPA_UT_ERR("No IPA ctx\n");
		return -EINVAL;
	}

	test_mhi_ctx = kzalloc(sizeof(struct ipa_test_mhi_context),
		GFP_KERNEL);
	if (!test_mhi_ctx) {
		IPA_UT_ERR("failed allocated ctx\n");
		return -ENOMEM;
	}

	rc = ipa3_get_transport_info(&test_mhi_ctx->transport_phys_addr,
				     &test_mhi_ctx->transport_size);
	if (rc != 0) {
		IPA_UT_ERR("ipa3_get_transport_info() failed\n");
		rc = -EFAULT;
		goto fail_free_ctx;
	}

	test_mhi_ctx->gsi_mmio =
	    ioremap_nocache(test_mhi_ctx->transport_phys_addr,
			    test_mhi_ctx->transport_size);
	if (!test_mhi_ctx->gsi_mmio) {
		IPA_UT_ERR("failed to remap GSI HW size=%lu\n",
			   test_mhi_ctx->transport_size);
		rc = -EFAULT;
		goto fail_free_ctx;
	}

	rc = ipa_test_mhi_alloc_mmio_space();
	if (rc) {
		IPA_UT_ERR("failed to alloc mmio space");
		goto fail_iounmap;
	}

	rc = ipa_mhi_test_setup_data_structures();
	if (rc) {
		IPA_UT_ERR("failed to setup data structures");
		goto fail_free_mmio_spc;
	}

	/* connect PROD pipe for remote wakeup */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_TEST_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_MHI_CONS;
	if (ipa_setup_sys_pipe(&sys_in, &test_mhi_ctx->test_prod_hdl)) {
		IPA_UT_ERR("setup sys pipe failed.\n");
		goto fail_destroy_data_structures;
	}

	*ppriv = test_mhi_ctx;
	return 0;

fail_destroy_data_structures:
	ipa_mhi_test_destroy_data_structures();
fail_free_mmio_spc:
	ipa_test_mhi_free_mmio_space();
fail_iounmap:
	iounmap(test_mhi_ctx->gsi_mmio);
fail_free_ctx:
	kfree(test_mhi_ctx);
	test_mhi_ctx = NULL;
	return rc;
}

/**
 * ipa_test_mhi_suite_teardown() - Suite teardown function
 */
static int ipa_test_mhi_suite_teardown(void *priv)
{
	IPA_UT_DBG("Start Teardown\n");

	if (!test_mhi_ctx)
		return  0;

	ipa_teardown_sys_pipe(test_mhi_ctx->test_prod_hdl);
	ipa_mhi_test_destroy_data_structures();
	ipa_test_mhi_free_mmio_space();
	iounmap(test_mhi_ctx->gsi_mmio);
	kfree(test_mhi_ctx);
	test_mhi_ctx = NULL;

	return 0;
}

/**
 * ipa_mhi_test_initialize_driver() - MHI init and possibly start and connect
 *
 * To be run during tests
 * 1. MHI init (Ready state)
 * 2. Conditional MHI start and connect (M0 state)
 */
static int ipa_mhi_test_initialize_driver(bool skip_start_and_conn)
{
	int rc = 0;
	struct ipa_mhi_init_params init_params;
	struct ipa_mhi_start_params start_params;
	struct ipa_mhi_connect_params prod_params;
	struct ipa_mhi_connect_params cons_params;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	u64 phys_addr;

	IPA_UT_LOG("Entry\n");

	p_mmio = test_mhi_ctx->mmio_buf.base;

	/* start IPA MHI */
	memset(&init_params, 0, sizeof(init_params));
	init_params.msi.addr_low = test_mhi_ctx->msi.phys_base;
	init_params.msi.data = 0x10000000;
	init_params.msi.mask = ~0x10000000;
	/* MMIO not needed for GSI */
	init_params.first_ch_idx = IPA_MHI_TEST_FIRST_CHANNEL_ID;
	init_params.first_er_idx = IPA_MHI_TEST_FIRST_EVENT_RING_ID;
	init_params.assert_bit40 = false;
	init_params.notify = ipa_mhi_test_cb;
	init_params.priv = NULL;
	init_params.test_mode = true;

	rc = ipa_mhi_init(&init_params);
	if (rc) {
		IPA_UT_LOG("ipa_mhi_init failed %d\n", rc);
		return rc;
	}

	IPA_UT_LOG("Wait async ready event\n");
	if (wait_for_completion_timeout(&mhi_test_ready_comp, 10 * HZ) == 0) {
		IPA_UT_LOG("timeout waiting for READY event");
		IPA_UT_TEST_FAIL_REPORT("failed waiting for state ready");
		return -ETIME;
	}

	if (!skip_start_and_conn) {
		memset(&start_params, 0, sizeof(start_params));
		start_params.channel_context_array_addr = p_mmio->ccabap;
		start_params.event_context_array_addr = p_mmio->ecabap;

		IPA_UT_LOG("BEFORE mhi_start\n");
		rc = ipa_mhi_start(&start_params);
		if (rc) {
			IPA_UT_LOG("mhi_start failed %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("fail start mhi");
			return rc;
		}
		IPA_UT_LOG("AFTER mhi_start\n");

		phys_addr = p_mmio->ccabap + (IPA_MHI_TEST_FIRST_CHANNEL_ID *
			sizeof(struct ipa_mhi_channel_context_array));
		p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
			(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
		IPA_UT_LOG("ch: %d base: 0x%pK phys_addr 0x%llx chstate: %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID,
			p_ch_ctx_array, phys_addr,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));

		memset(&prod_params, 0, sizeof(prod_params));
		prod_params.sys.client = IPA_CLIENT_MHI_PROD;
		prod_params.sys.ipa_ep_cfg.mode.mode = IPA_DMA;
		prod_params.sys.ipa_ep_cfg.mode.dst = IPA_CLIENT_MHI_CONS;
		prod_params.sys.ipa_ep_cfg.seq.seq_type =
			IPA_MHI_TEST_SEQ_TYPE_DMA;
		prod_params.sys.ipa_ep_cfg.seq.set_dynamic = true;
		prod_params.channel_id = IPA_MHI_TEST_FIRST_CHANNEL_ID;
		IPA_UT_LOG("BEFORE connect_pipe (PROD): client:%d ch_id:%u\n",
			prod_params.sys.client, prod_params.channel_id);
		rc = ipa_mhi_connect_pipe(&prod_params,
			&test_mhi_ctx->prod_hdl);
		if (rc) {
			IPA_UT_LOG("mhi_connect_pipe failed %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("fail connect PROD pipe");
			return rc;
		}

		if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
			IPA_UT_LOG("MHI_PROD: chstate is not RUN chstate:%s\n",
				ipa_mhi_get_state_str(
				p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("PROD pipe state is not run");
			return -EFAULT;
		}

		phys_addr = p_mmio->ccabap +
			((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
			sizeof(struct ipa_mhi_channel_context_array));
		p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
			(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
		IPA_UT_LOG("ch: %d base: 0x%pK phys_addr 0x%llx chstate: %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
			p_ch_ctx_array, phys_addr,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));

		memset(&cons_params, 0, sizeof(cons_params));
		cons_params.sys.client = IPA_CLIENT_MHI_CONS;
		cons_params.sys.skip_ep_cfg = true;
		cons_params.channel_id = IPA_MHI_TEST_FIRST_CHANNEL_ID + 1;
		IPA_UT_LOG("BEFORE connect_pipe (CONS): client:%d ch_id:%u\n",
			cons_params.sys.client, cons_params.channel_id);
		rc = ipa_mhi_connect_pipe(&cons_params,
			&test_mhi_ctx->cons_hdl);
		if (rc) {
			IPA_UT_LOG("mhi_connect_pipe failed %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("fail connect CONS pipe");
			return rc;
		}

		if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
			IPA_UT_LOG("MHI_CONS: chstate is not RUN chstate:%s\n",
				ipa_mhi_get_state_str(
				p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("CONS pipe state is not run");
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * To be run during test
 * 1. MHI destroy
 * 2. re-configure the channels
 */
static int ipa_mhi_test_destroy(struct ipa_test_mhi_context *ctx)
{
	struct ipa_mhi_mmio_register_set *p_mmio;
	u64 phys_addr;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	int rc;

	IPA_UT_LOG("Entry\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("Input err invalid ctx\n");
		return -EINVAL;
	}

	p_mmio = ctx->mmio_buf.base;

	phys_addr = p_mmio->ccabap +
		((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = ctx->ch_ctx_array.base +
		(phys_addr - ctx->ch_ctx_array.phys_base);
	IPA_UT_LOG("channel id %d (CONS): chstate %s\n",
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		ipa_mhi_get_state_str(p_ch_ctx_array->chstate));

	phys_addr = p_mmio->ccabap +
		((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = ctx->ch_ctx_array.base +
		(phys_addr - ctx->ch_ctx_array.phys_base);
	IPA_UT_LOG("channel id %d (PROD): chstate %s\n",
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		ipa_mhi_get_state_str(p_ch_ctx_array->chstate));

	IPA_UT_LOG("MHI Destroy\n");
	ipa_mhi_destroy();
	IPA_UT_LOG("Post MHI Destroy\n");

	ctx->prod_hdl = 0;
	ctx->cons_hdl = 0;

	dma_free_coherent(ipa3_ctx->pdev, ctx->xfer_ring_bufs[1].size,
		ctx->xfer_ring_bufs[1].base, ctx->xfer_ring_bufs[1].phys_base);
	ctx->xfer_ring_bufs[1].base = NULL;

	IPA_UT_LOG("config channel context for channel %d (MHI CONS)\n",
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1);
	rc = ipa_mhi_test_config_channel_context(
		&ctx->mmio_buf,
		ctx->xfer_ring_bufs,
		ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1,
		0x100,
		0x80,
		IPA_MHI_IN_CHAHNNEL);
	if (rc) {
		IPA_UT_LOG("config channel context failed %d, channel %d\n",
			rc, IPA_MHI_TEST_FIRST_CHANNEL_ID + 1);
		IPA_UT_TEST_FAIL_REPORT("fail config CONS channel ctx");
		return -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, ctx->xfer_ring_bufs[0].size,
		ctx->xfer_ring_bufs[0].base, ctx->xfer_ring_bufs[0].phys_base);
	ctx->xfer_ring_bufs[0].base = NULL;

	IPA_UT_LOG("config channel context for channel %d (MHI PROD)\n",
		IPA_MHI_TEST_FIRST_CHANNEL_ID);
	rc = ipa_mhi_test_config_channel_context(
		&ctx->mmio_buf,
		ctx->xfer_ring_bufs,
		ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID,
		0x100,
		0x80,
		IPA_MHI_OUT_CHAHNNEL);
	if (rc) {
		IPA_UT_LOG("config channel context failed %d, channel %d\n",
			rc, IPA_MHI_TEST_FIRST_CHANNEL_ID);
		IPA_UT_TEST_FAIL_REPORT("fail config PROD channel ctx");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 * 1. Destroy
 * 2. Initialize (to Ready or M0 states)
 */
static int ipa_mhi_test_reset(struct ipa_test_mhi_context *ctx,
	bool skip_start_and_conn)
{
	int rc;

	IPA_UT_LOG("Entry\n");

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy fail");
		return rc;
	}

	rc = ipa_mhi_test_initialize_driver(skip_start_and_conn);
	if (rc) {
		IPA_UT_LOG("driver init failed skip_start_and_con=%d rc=%d\n",
			skip_start_and_conn, rc);
		IPA_UT_TEST_FAIL_REPORT("init fail");
		return rc;
	}

	return 0;
}

/**
 * To be run during test
 *	1. disconnect cons channel
 *	2. config cons channel
 *	3. disconnect prod channel
 *	4. config prod channel
 *	5. connect prod
 *	6. connect cons
 */
static int ipa_mhi_test_channel_reset(void)
{
	int rc;
	struct ipa_mhi_connect_params prod_params;
	struct ipa_mhi_connect_params cons_params;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	u64 phys_addr;

	p_mmio = test_mhi_ctx->mmio_buf.base;

	IPA_UT_LOG("Before pipe disconnect (CONS) client hdl=%u=\n",
		test_mhi_ctx->cons_hdl);
	rc = ipa_mhi_disconnect_pipe(test_mhi_ctx->cons_hdl);
	if (rc) {
		IPA_UT_LOG("disconnect_pipe failed (CONS) %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("CONS pipe disconnect fail");
		return -EFAULT;
	}
	test_mhi_ctx->cons_hdl = 0;

	phys_addr = p_mmio->ccabap +
		((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		IPA_UT_LOG("chstate is not disabled! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("CONS pipe state is not disabled");
		return -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev,
		test_mhi_ctx->xfer_ring_bufs[1].size,
		test_mhi_ctx->xfer_ring_bufs[1].base,
		test_mhi_ctx->xfer_ring_bufs[1].phys_base);
	test_mhi_ctx->xfer_ring_bufs[1].base = NULL;
	rc = ipa_mhi_test_config_channel_context(
		&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID + 1,
		0x100,
		0x80,
		IPA_MHI_IN_CHAHNNEL);
	if (rc) {
		IPA_UT_LOG("config_channel_context IN failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail config CONS channel context");
		return -EFAULT;
	}
	IPA_UT_LOG("Before pipe disconnect (CONS) client hdl=%u=\n",
		test_mhi_ctx->prod_hdl);
	rc = ipa_mhi_disconnect_pipe(test_mhi_ctx->prod_hdl);
	if (rc) {
		IPA_UT_LOG("disconnect_pipe failed (PROD) %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("PROD pipe disconnect fail");
		return -EFAULT;
	}
	test_mhi_ctx->prod_hdl = 0;

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		IPA_UT_LOG("chstate is not disabled! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("PROD pipe state is not disabled");
		return -EFAULT;
	}

	dma_free_coherent(ipa3_ctx->pdev, test_mhi_ctx->xfer_ring_bufs[0].size,
		test_mhi_ctx->xfer_ring_bufs[0].base,
		test_mhi_ctx->xfer_ring_bufs[0].phys_base);
	test_mhi_ctx->xfer_ring_bufs[0].base = NULL;
	rc = ipa_mhi_test_config_channel_context(
		&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		IPA_MHI_TEST_FIRST_EVENT_RING_ID,
		0x100,
		0x80,
		IPA_MHI_OUT_CHAHNNEL);
	if (rc) {
		IPA_UT_LOG("config_channel_context OUT failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("PROD pipe state is not disabled");
		return -EFAULT;
	}

	memset(&prod_params, 0, sizeof(prod_params));
	prod_params.sys.client = IPA_CLIENT_MHI_PROD;
	prod_params.sys.ipa_ep_cfg.mode.mode = IPA_DMA;
	prod_params.sys.ipa_ep_cfg.mode.dst = IPA_CLIENT_MHI_CONS;
	prod_params.sys.ipa_ep_cfg.seq.seq_type = IPA_MHI_TEST_SEQ_TYPE_DMA;
	prod_params.sys.ipa_ep_cfg.seq.set_dynamic = true;
	prod_params.channel_id = IPA_MHI_TEST_FIRST_CHANNEL_ID;
	IPA_UT_LOG("BEFORE connect PROD\n");
	rc = ipa_mhi_connect_pipe(&prod_params, &test_mhi_ctx->prod_hdl);
	if (rc) {
		IPA_UT_LOG("connect_pipe failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail connect PROD pipe");
		return rc;
	}

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
		IPA_UT_LOG("chstate is not run! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("PROD pipe state is not run");
		return -EFAULT;
	}

	memset(&cons_params, 0, sizeof(cons_params));
	cons_params.sys.client = IPA_CLIENT_MHI_CONS;
	cons_params.sys.skip_ep_cfg = true;
	cons_params.channel_id = IPA_MHI_TEST_FIRST_CHANNEL_ID + 1;
	IPA_UT_LOG("BEFORE connect CONS\n");
	rc = ipa_mhi_connect_pipe(&cons_params, &test_mhi_ctx->cons_hdl);
	if (rc) {
		IPA_UT_LOG("ipa_mhi_connect_pipe failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail connect CONS pipe");
		return rc;
	}

	phys_addr = p_mmio->ccabap +
		((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
		IPA_UT_LOG("chstate is not run! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("CONS pipe state is not run");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 * Send data
 */
static int ipa_mhi_test_q_transfer_re(struct ipa_mem_buffer *mmio,
	struct ipa_mem_buffer xfer_ring_bufs[],
	struct ipa_mem_buffer ev_ring_bufs[],
	u8 channel_id,
	struct ipa_mem_buffer buf_array[],
	int buf_array_size,
	bool ieob,
	bool ieot,
	bool bei,
	bool trigger_db)
{
	struct ipa_mhi_transfer_ring_element *curr_re;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_channels;
	struct ipa_mhi_event_context_array *p_events;
	u32 channel_idx;
	u32 event_ring_index;
	u32 wp_ofst;
	u32 rp_ofst;
	u32 next_wp_ofst;
	int i;
	u32 num_of_ed_to_queue;
	u32 avail_ev;

	IPA_UT_LOG("Entry\n");

	p_mmio = (struct ipa_mhi_mmio_register_set *)mmio->base;
	p_channels = (struct ipa_mhi_channel_context_array *)
		((unsigned long)p_mmio->crcbap);
	p_events = (struct ipa_mhi_event_context_array *)
		((unsigned long)p_mmio->crdb);

	if (ieob)
		num_of_ed_to_queue = buf_array_size;
	else
		num_of_ed_to_queue = ieot ? 1 : 0;

	if (channel_id >=
		(IPA_MHI_TEST_FIRST_CHANNEL_ID + IPA_MHI_TEST_NUM_CHANNELS) ||
		channel_id < IPA_MHI_TEST_FIRST_CHANNEL_ID) {
		IPA_UT_LOG("Invalid Channel ID %d\n", channel_id);
		return -EFAULT;
	}

	channel_idx = channel_id - IPA_MHI_TEST_FIRST_CHANNEL_ID;

	if (!xfer_ring_bufs[channel_idx].base) {
		IPA_UT_LOG("Channel is not allocated\n");
		return -EFAULT;
	}
	if (p_channels[channel_idx].brsmode == IPA_MHI_BURST_MODE_DEFAULT ||
	    p_channels[channel_idx].brsmode == IPA_MHI_BURST_MODE_ENABLE)
		num_of_ed_to_queue += 1; /* for OOB/DB mode event */

	/* First queue EDs */
	event_ring_index = p_channels[channel_idx].erindex -
		IPA_MHI_TEST_FIRST_EVENT_RING_ID;

	wp_ofst = (u32)(p_events[event_ring_index].wp -
		p_events[event_ring_index].rbase);
	rp_ofst = (u32)(p_events[event_ring_index].rp -
		p_events[event_ring_index].rbase);

	if (p_events[event_ring_index].rlen & 0xFFFFFFFF00000000) {
		IPA_UT_LOG("invalid ev rlen %llu\n",
			p_events[event_ring_index].rlen);
		return -EFAULT;
	}

	if (wp_ofst > rp_ofst) {
		avail_ev = (wp_ofst - rp_ofst) /
			sizeof(struct ipa_mhi_event_ring_element);
	} else {
		avail_ev = (u32)p_events[event_ring_index].rlen -
			(rp_ofst - wp_ofst);
		avail_ev /= sizeof(struct ipa_mhi_event_ring_element);
	}

	IPA_UT_LOG("wp_ofst=0x%x rp_ofst=0x%x rlen=%llu avail_ev=%u\n",
		wp_ofst, rp_ofst, p_events[event_ring_index].rlen, avail_ev);

	if (num_of_ed_to_queue > ((u32)p_events[event_ring_index].rlen /
		sizeof(struct ipa_mhi_event_ring_element))) {
		IPA_UT_LOG("event ring too small for %u credits\n",
			num_of_ed_to_queue);
		return -EFAULT;
	}

	if (num_of_ed_to_queue > avail_ev) {
		IPA_UT_LOG("Need to add event credits (needed=%u)\n",
			num_of_ed_to_queue - avail_ev);

		next_wp_ofst = (wp_ofst + (num_of_ed_to_queue - avail_ev) *
			sizeof(struct ipa_mhi_event_ring_element)) %
			(u32)p_events[event_ring_index].rlen;

		/* set next WP */
		p_events[event_ring_index].wp =
			(u32)p_events[event_ring_index].rbase + next_wp_ofst;

		/* write value to event ring doorbell */
		IPA_UT_LOG("DB to event 0x%llx: base %pa ofst 0x%x\n",
			p_events[event_ring_index].wp,
			&(test_mhi_ctx->transport_phys_addr),
			GSI_EE_n_EV_CH_k_DOORBELL_0_OFFS(
			event_ring_index + ipa3_ctx->mhi_evid_limits[0], 0));
		iowrite32(p_events[event_ring_index].wp,
			test_mhi_ctx->gsi_mmio +
			GSI_EE_n_EV_CH_k_DOORBELL_0_OFFS(
			event_ring_index + ipa3_ctx->mhi_evid_limits[0], 0));
	}

	for (i = 0; i < buf_array_size; i++) {
		/* calculate virtual pointer for current WP and RP */
		wp_ofst = (u32)(p_channels[channel_idx].wp -
			p_channels[channel_idx].rbase);
		rp_ofst = (u32)(p_channels[channel_idx].rp -
			p_channels[channel_idx].rbase);
		(void)rp_ofst;
		curr_re = (struct ipa_mhi_transfer_ring_element *)
			((unsigned long)xfer_ring_bufs[channel_idx].base +
			wp_ofst);
		if (p_channels[channel_idx].rlen & 0xFFFFFFFF00000000) {
			IPA_UT_LOG("invalid ch rlen %llu\n",
				p_channels[channel_idx].rlen);
			return -EFAULT;
		}
		next_wp_ofst = (wp_ofst +
			sizeof(struct ipa_mhi_transfer_ring_element)) %
			(u32)p_channels[channel_idx].rlen;

		/* write current RE */
		curr_re->type = IPA_MHI_RING_ELEMENT_TRANSFER;
		curr_re->len = (u16)buf_array[i].size;
		curr_re->ptr = (u32)buf_array[i].phys_base;
		curr_re->word_C.bits.bei = bei;
		curr_re->word_C.bits.ieob = ieob;
		curr_re->word_C.bits.ieot = ieot;

		/* set next WP */
		p_channels[channel_idx].wp =
			p_channels[channel_idx].rbase + next_wp_ofst;

		if (i == (buf_array_size - 1)) {
			/* last buffer */
			curr_re->word_C.bits.chain = 0;
			if (trigger_db) {
				IPA_UT_LOG(
					"DB to channel 0x%llx: base %pa ofst 0x%x\n"
					, p_channels[channel_idx].wp
					, &(test_mhi_ctx->transport_phys_addr)
					, GSI_EE_n_GSI_CH_k_DOORBELL_0_OFFS(
						channel_idx, 0));
				iowrite32(p_channels[channel_idx].wp,
					test_mhi_ctx->gsi_mmio +
					GSI_EE_n_GSI_CH_k_DOORBELL_0_OFFS(
					channel_idx, 0));
			}
		} else {
			curr_re->word_C.bits.chain = 1;
		}
	}

	return 0;
}

/**
 * To be run during test
 * Send data in loopback (from In to OUT) and compare
 */
static int ipa_mhi_test_loopback_data_transfer(void)
{
	struct ipa_mem_buffer *p_mmio;
	int i;
	int rc;
	static int val;
	bool timeout = true;

	IPA_UT_LOG("Entry\n");

	p_mmio = &test_mhi_ctx->mmio_buf;

	/* invalidate spare register value (for msi) */
	memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

	val++;

	memset(test_mhi_ctx->in_buffer.base, 0,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	for (i = 0; i < IPA_MHI_TEST_MAX_DATA_BUF_SIZE; i++)
		memset(test_mhi_ctx->out_buffer.base + i, (val + i) & 0xFF, 1);

	/* queue RE for IN side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(p_mmio,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		&test_mhi_ctx->in_buffer,
		1,
		true,
		true,
		false,
		true);

	if (rc) {
		IPA_UT_LOG("q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail IN q xfer re");
		return rc;
	}

	/* queue REs for OUT side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(p_mmio,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		&test_mhi_ctx->out_buffer,
		1,
		true,
		true,
		false,
		true);

	if (rc) {
		IPA_UT_LOG("q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail OUT q xfer re");
		return rc;
	}

	IPA_MHI_TEST_CHECK_MSI_INTR(true, timeout);
	if (timeout) {
		IPA_UT_LOG("transfer timeout. MSI = 0x%x\n",
			*((u32 *)test_mhi_ctx->msi.base));
		IPA_UT_TEST_FAIL_REPORT("xfter timeout");
		return -EFAULT;
	}

	/* compare the two buffers */
	if (memcmp(test_mhi_ctx->in_buffer.base, test_mhi_ctx->out_buffer.base,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE)) {
		IPA_UT_LOG("buffer are not equal\n");
		IPA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 * Do suspend and check channel states to be suspend if should success
 */
static int ipa_mhi_test_suspend(bool force, bool should_success)
{
	int rc;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	u64 phys_addr;

	IPA_UT_LOG("Entry\n");

	rc = ipa_mhi_suspend(force);
	if (should_success && rc != 0) {
		IPA_UT_LOG("ipa_mhi_suspend failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend failed");
		return -EFAULT;
	}

	if (!should_success && rc != -EAGAIN) {
		IPA_UT_LOG("ipa_mhi_suspend did not return -EAGAIN fail %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("suspend succeeded unexpectedly");
		return -EFAULT;
	}

	p_mmio = test_mhi_ctx->mmio_buf.base;

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (should_success) {
		if (p_ch_ctx_array->chstate !=
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND) {
			IPA_UT_LOG("chstate is not suspend! ch %d chstate %s\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
				ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("channel state not suspend");
			return -EFAULT;
		}
		if (!force && p_ch_ctx_array->rp != p_ch_ctx_array->wp) {
			IPA_UT_LOG("rp not updated ch %d rp 0x%llx wp 0x%llx\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
				p_ch_ctx_array->rp, p_ch_ctx_array->wp);
			IPA_UT_TEST_FAIL_REPORT("rp was not updated");
			return -EFAULT;
		}
	} else {
		if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
			IPA_UT_LOG("chstate is not running! ch %d chstate %s\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
				ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("channel state not run");
			return -EFAULT;
		}
	}

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (should_success) {
		if (p_ch_ctx_array->chstate !=
			IPA_HW_MHI_CHANNEL_STATE_SUSPEND) {
			IPA_UT_LOG("chstate is not suspend! ch %d chstate %s\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID,
				ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("channel state not suspend");
			return -EFAULT;
		}
		if (!force && p_ch_ctx_array->rp != p_ch_ctx_array->wp) {
			IPA_UT_LOG("rp not updated ch %d rp 0x%llx wp 0x%llx\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID,
				p_ch_ctx_array->rp, p_ch_ctx_array->wp);
			IPA_UT_TEST_FAIL_REPORT("rp was not updated");
			return -EFAULT;
		}
	} else {
		if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
			IPA_UT_LOG("chstate is not running! ch %d chstate %s\n",
				IPA_MHI_TEST_FIRST_CHANNEL_ID,
				ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
			IPA_UT_TEST_FAIL_REPORT("channel state not run");
			return -EFAULT;
		}
	}

	return 0;
}

/**
 * To be run during test
 * Do resume and check channel state to be running
 */
static int ipa_test_mhi_resume(void)
{
	int rc;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	u64 phys_addr;

	rc = ipa_mhi_resume();
	if (rc) {
		IPA_UT_LOG("resume failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("resume failed");
		return -EFAULT;
	}

	p_mmio = test_mhi_ctx->mmio_buf.base;

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID + 1) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
		IPA_UT_LOG("chstate is not running! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("channel state not run");
		return -EFAULT;
	}

	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	if (p_ch_ctx_array->chstate != IPA_HW_MHI_CHANNEL_STATE_RUN) {
		IPA_UT_LOG("chstate is not running! ch %d chstate %s\n",
			IPA_MHI_TEST_FIRST_CHANNEL_ID,
			ipa_mhi_get_state_str(p_ch_ctx_array->chstate));
		IPA_UT_TEST_FAIL_REPORT("channel state not run");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 *	1. suspend
 *	2. queue RE for IN and OUT and send data
 *	3. should get MSI timeout due to suspend
 *	4. resume
 *	5. should get the MSIs now
 *	6. comapre the IN and OUT buffers
 */
static int ipa_mhi_test_suspend_resume(void)
{
	int rc;
	int i;
	bool timeout = true;

	IPA_UT_LOG("Entry\n");

	IPA_UT_LOG("BEFORE suspend\n");
	rc = ipa_mhi_test_suspend(false, true);
	if (rc) {
		IPA_UT_LOG("suspend failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend failed");
		return rc;
	}
	IPA_UT_LOG("AFTER suspend\n");

	/* invalidate spare register value (for msi) */
	memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

	memset(test_mhi_ctx->in_buffer.base, 0, IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	for (i = 0; i < IPA_MHI_TEST_MAX_DATA_BUF_SIZE; i++)
		memset(test_mhi_ctx->out_buffer.base + i, i & 0xFF, 1);

	/* queue RE for IN side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		&test_mhi_ctx->in_buffer,
		1,
		true,
		true,
		false,
		true);
	if (rc) {
		IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail IN q xfer re");
		return rc;
	}

	/* queue REs for OUT side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID,
		&test_mhi_ctx->out_buffer,
		1,
		true,
		true,
		false,
		true);

	if (rc) {
		IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail OUT q xfer re");
		return rc;
	}

	IPA_MHI_TEST_CHECK_MSI_INTR(true, timeout);
	if (!timeout) {
		IPA_UT_LOG("Error: transfer success on suspend\n");
		IPA_UT_TEST_FAIL_REPORT("xfer suceeded unexpectedly");
		return -EFAULT;
	}

	IPA_UT_LOG("BEFORE resume\n");
	rc = ipa_test_mhi_resume();
	if (rc) {
		IPA_UT_LOG("ipa_mhi_resume failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("resume fail");
		return rc;
	}
	IPA_UT_LOG("AFTER resume\n");

	IPA_MHI_TEST_CHECK_MSI_INTR(true, timeout);
	if (timeout) {
		IPA_UT_LOG("Error: transfer timeout\n");
		IPA_UT_TEST_FAIL_REPORT("xfer timeout");
		return -EFAULT;
	}

	/* compare the two buffers */
	if (memcmp(test_mhi_ctx->in_buffer.base,
		test_mhi_ctx->out_buffer.base,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE)) {
		IPA_UT_LOG("Error: buffers are not equal\n");
		IPA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 *	1. enable aggregation
 *	2. queue IN RE (ring element)
 *	3. allocate skb with data
 *	4. send it (this will create open aggr frame)
 */
static int ipa_mhi_test_create_aggr_open_frame(void)
{
	struct ipa_ep_cfg_aggr ep_aggr;
	struct sk_buff *skb;
	int rc;
	int i;
	u32 aggr_state_active;

	IPA_UT_LOG("Entry\n");

	memset(&ep_aggr, 0, sizeof(ep_aggr));
	ep_aggr.aggr_en = IPA_ENABLE_AGGR;
	ep_aggr.aggr = IPA_GENERIC;
	ep_aggr.aggr_pkt_limit = 2;

	rc = ipa3_cfg_ep_aggr(test_mhi_ctx->cons_hdl, &ep_aggr);
	if (rc) {
		IPA_UT_LOG("failed to configure aggr");
		IPA_UT_TEST_FAIL_REPORT("failed to configure aggr");
		return rc;
	}

	/* invalidate spare register value (for msi) */
	memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

	/* queue RE for IN side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		&test_mhi_ctx->in_buffer,
		1,
		true,
		true,
		false,
		true);
	if (rc) {
		IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail IN q xfer re");
		return rc;
	}

	skb = dev_alloc_skb(IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	if (!skb) {
		IPA_UT_LOG("non mem for skb\n");
		IPA_UT_TEST_FAIL_REPORT("fail alloc skb");
		return -ENOMEM;
	}
	skb_put(skb, IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	for (i = 0; i < IPA_MHI_TEST_MAX_DATA_BUF_SIZE; i++) {
		memset(skb->data + i, i & 0xFF, 1);
		memset(test_mhi_ctx->out_buffer.base + i, i & 0xFF, 1);
	}

	rc = ipa_tx_dp(IPA_CLIENT_TEST_PROD, skb, NULL);
	if (rc) {
		IPA_UT_LOG("ipa_tx_dp failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("ipa tx dp fail");
		return rc;
	}

	msleep(20);

	aggr_state_active = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
	IPA_UT_LOG("IPA_STATE_AGGR_ACTIVE  0x%x\n", aggr_state_active);
	if (aggr_state_active == 0) {
		IPA_UT_LOG("No aggregation frame open!\n");
		IPA_UT_TEST_FAIL_REPORT("No aggregation frame open");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 *	1. create open aggr by sending data
 *	2. suspend - if force it should succeed, otherwize it fails
 *	3. if force - wait for wakeup event - it should arrive
 *	4. if force - resume
 *	5. force close the aggr.
 *	6. wait for MSI - it should arrive
 *	7. compare IN and OUT buffers
 *	8. disable aggr.
 */
static int ipa_mhi_test_suspend_aggr_open(bool force)
{
	int rc;
	struct ipa_ep_cfg_aggr ep_aggr;
	bool timeout = true;

	IPA_UT_LOG("Entry\n");

	rc = ipa_mhi_test_create_aggr_open_frame();
	if (rc) {
		IPA_UT_LOG("failed create open aggr\n");
		IPA_UT_TEST_FAIL_REPORT("fail create open aggr");
		return rc;
	}

	if (force)
		reinit_completion(&mhi_test_wakeup_comp);

	IPA_UT_LOG("BEFORE suspend\n");
	/**
	 * if suspend force, then suspend should succeed.
	 * otherwize it should fail due to open aggr.
	 */
	rc = ipa_mhi_test_suspend(force, force);
	if (rc) {
		IPA_UT_LOG("suspend failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend fail");
		return rc;
	}
	IPA_UT_LOG("AFTER suspend\n");

	if (force) {
		if (!wait_for_completion_timeout(&mhi_test_wakeup_comp, HZ)) {
			IPA_UT_LOG("timeout waiting for wakeup event\n");
			IPA_UT_TEST_FAIL_REPORT("timeout waitinf wakeup event");
			return -ETIME;
		}

		IPA_UT_LOG("BEFORE resume\n");
		rc = ipa_test_mhi_resume();
		if (rc) {
			IPA_UT_LOG("resume failed %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("resume failed");
			return rc;
		}
		IPA_UT_LOG("AFTER resume\n");
	}

	ipahal_write_reg(IPA_AGGR_FORCE_CLOSE, (1 << test_mhi_ctx->cons_hdl));

	IPA_MHI_TEST_CHECK_MSI_INTR(false, timeout);
	if (timeout) {
		IPA_UT_LOG("fail: transfer not completed\n");
		IPA_UT_TEST_FAIL_REPORT("timeout on transferring data");
		return -EFAULT;
	}

	/* compare the two buffers */
	if (memcmp(test_mhi_ctx->in_buffer.base,
		test_mhi_ctx->out_buffer.base,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE)) {
		IPA_UT_LOG("fail: buffer are not equal\n");
		IPA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
		return -EFAULT;
	}

	memset(&ep_aggr, 0, sizeof(ep_aggr));
	rc = ipa3_cfg_ep_aggr(test_mhi_ctx->cons_hdl, &ep_aggr);
	if (rc) {
		IPA_UT_LOG("failed to configure aggr");
		IPA_UT_TEST_FAIL_REPORT("fail to disable aggr");
		return rc;
	}

	return 0;
}

/**
 * To be run during test
 *	1. suspend
 *	2. queue IN RE (ring element)
 *	3. allocate skb with data
 *	4. send it (this will create open aggr frame)
 *	5. wait for wakeup event - it should arrive
 *	6. resume
 *	7. wait for MSI - it should arrive
 *	8. compare IN and OUT buffers
 */
static int ipa_mhi_test_suspend_host_wakeup(void)
{
	int rc;
	int i;
	bool timeout = true;
	struct sk_buff *skb;

	reinit_completion(&mhi_test_wakeup_comp);

	IPA_UT_LOG("BEFORE suspend\n");
	rc = ipa_mhi_test_suspend(false, true);
	if (rc) {
		IPA_UT_LOG("suspend failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend fail");
		return rc;
	}
	IPA_UT_LOG("AFTER suspend\n");

	/* invalidate spare register value (for msi) */
	memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

	memset(test_mhi_ctx->in_buffer.base, 0, IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	/* queue RE for IN side and trigger doorbell*/
	rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		&test_mhi_ctx->in_buffer,
		1,
		true,
		true,
		false,
		true);

	if (rc) {
		IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail IN q xfer re");
		return rc;
	}

	skb = dev_alloc_skb(IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	if (!skb) {
		IPA_UT_LOG("non mem for skb\n");
		IPA_UT_TEST_FAIL_REPORT("no mem for skb");
		return -ENOMEM;
	}
	skb_put(skb, IPA_MHI_TEST_MAX_DATA_BUF_SIZE);
	for (i = 0; i < IPA_MHI_TEST_MAX_DATA_BUF_SIZE; i++) {
		memset(skb->data + i, i & 0xFF, 1);
		memset(test_mhi_ctx->out_buffer.base + i, i & 0xFF, 1);
	}

	rc = ipa_tx_dp(IPA_CLIENT_TEST_PROD, skb, NULL);
	if (rc) {
		IPA_UT_LOG("ipa_tx_dp failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("ipa tx dp fail");
		return rc;
	}

	if (wait_for_completion_timeout(&mhi_test_wakeup_comp,
		msecs_to_jiffies(3500)) == 0) {
		IPA_UT_LOG("timeout waiting for wakeup event\n");
		IPA_UT_TEST_FAIL_REPORT("timeout waiting for wakeup event");
		return -ETIME;
	}

	IPA_UT_LOG("BEFORE resume\n");
	rc = ipa_test_mhi_resume();
	if (rc) {
		IPA_UT_LOG("resume failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("resume fail");
		return rc;
	}
	IPA_UT_LOG("AFTER resume\n");

	/* check for MSI interrupt one channels */
	IPA_MHI_TEST_CHECK_MSI_INTR(false, timeout);
	if (timeout) {
		IPA_UT_LOG("fail: transfer timeout\n");
		IPA_UT_TEST_FAIL_REPORT("timeout on xfer");
		return -EFAULT;
	}

	/* compare the two buffers */
	if (memcmp(test_mhi_ctx->in_buffer.base,
		test_mhi_ctx->out_buffer.base,
		IPA_MHI_TEST_MAX_DATA_BUF_SIZE)) {
		IPA_UT_LOG("fail: buffer are not equal\n");
		IPA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
		return -EFAULT;
	}

	return 0;
}

/**
 * To be run during test
 *	1. queue OUT RE/buffer
 *	2. wait for MSI on OUT
 *	3. Do 1. and 2. till got MSI wait timeout (ch full / holb)
 */
static int ipa_mhi_test_create_full_channel(int *submitted_packets)
{
	int i;
	bool timeout = true;
	int rc;

	if (!submitted_packets) {
		IPA_UT_LOG("Input error\n");
		return -EINVAL;
	}

	*submitted_packets = 0;

	for (i = 0; i < IPA_MHI_TEST_MAX_DATA_BUF_SIZE; i++)
		memset(test_mhi_ctx->out_buffer.base + i, i & 0xFF, 1);

	do {
		/* invalidate spare register value (for msi) */
		memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

		IPA_UT_LOG("submitting OUT buffer\n");
		timeout = true;
		/* queue REs for OUT side and trigger doorbell */
		rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
			test_mhi_ctx->xfer_ring_bufs,
			test_mhi_ctx->ev_ring_bufs,
			IPA_MHI_TEST_FIRST_CHANNEL_ID,
			&test_mhi_ctx->out_buffer,
			1,
			true,
			true,
			false,
			true);
		if (rc) {
			IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n",
				rc);
			IPA_UT_TEST_FAIL_REPORT("fail OUT q re");
			return rc;
		}
		(*submitted_packets)++;

		IPA_UT_LOG("waiting for MSI\n");
		for (i = 0; i < 10; i++) {
			if (*((u32 *)test_mhi_ctx->msi.base) ==
				(0x10000000 |
				(IPA_MHI_TEST_FIRST_EVENT_RING_ID))) {
				IPA_UT_LOG("got MSI\n");
				timeout = false;
				break;
			}
			msleep(20);
		}
	} while (!timeout);

	return 0;
}

/**
 * To be run during test
 *	1. queue OUT RE/buffer
 *	2. wait for MSI on OUT
 *	3. Do 1. and 2. till got MSI wait timeout (ch full)
 *	4. suspend - it should fail with -EAGAIN - M1 is rejected
 *	5. foreach submitted pkt, do the next steps
 *	6. queue IN RE/buffer
 *	7. wait for MSI
 *	8. compare IN and OUT buffers
 */
static int ipa_mhi_test_suspend_full_channel(bool force)
{
	int rc;
	bool timeout;
	int submitted_packets = 0;

	rc = ipa_mhi_test_create_full_channel(&submitted_packets);
	if (rc) {
		IPA_UT_LOG("fail create full channel\n");
		IPA_UT_TEST_FAIL_REPORT("fail create full channel");
		return rc;
	}

	IPA_UT_LOG("BEFORE suspend\n");
	rc = ipa_mhi_test_suspend(force, false);
	if (rc) {
		IPA_UT_LOG("ipa_mhi_suspend did not returned -EAGAIN. rc %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("test suspend fail");
		return -EFAULT;
	}
	IPA_UT_LOG("AFTER suspend\n");

	while (submitted_packets) {
		memset(test_mhi_ctx->in_buffer.base, 0,
			IPA_MHI_TEST_MAX_DATA_BUF_SIZE);

		/* invalidate spare register value (for msi) */
		memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);

		timeout = true;
		/* queue RE for IN side and trigger doorbell */
		rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
			test_mhi_ctx->xfer_ring_bufs,
			test_mhi_ctx->ev_ring_bufs,
			IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
			&test_mhi_ctx->in_buffer,
			1,
			true,
			true,
			false,
			true);
		if (rc) {
			IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n",
				rc);
			IPA_UT_TEST_FAIL_REPORT("fail IN q re");
			return rc;
		}

		IPA_MHI_TEST_CHECK_MSI_INTR(true, timeout);
		if (timeout) {
			IPA_UT_LOG("transfer failed - timeout\n");
			IPA_UT_TEST_FAIL_REPORT("timeout on xfer");
			return -EFAULT;
		}

		/* compare the two buffers */
		if (memcmp(test_mhi_ctx->in_buffer.base,
			test_mhi_ctx->out_buffer.base,
			IPA_MHI_TEST_MAX_DATA_BUF_SIZE)) {
			IPA_UT_LOG("buffer are not equal\n");
			IPA_UT_TEST_FAIL_REPORT("non-equal buffers after xfer");
			return -EFAULT;
		}

		submitted_packets--;
	}

	return 0;
}

/**
 * To be called from test
 *	1. suspend
 *	2. reset to M0 state
 */
static int ipa_mhi_test_suspend_and_reset(struct ipa_test_mhi_context *ctx)
{
	int rc;

	IPA_UT_LOG("BEFORE suspend\n");
	rc = ipa_mhi_test_suspend(false, true);
	if (rc) {
		IPA_UT_LOG("suspend failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend fail");
		return rc;
	}
	IPA_UT_LOG("AFTER suspend\n");

	rc = ipa_mhi_test_reset(ctx, false);
	if (rc) {
		IPA_UT_LOG("reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("reset fail");
		return rc;
	}

	return 0;
}

/**
 * To be run during test
 *	1. manualy update wp
 *	2. suspend - should succeed
 *	3. restore wp value
 */
static int ipa_mhi_test_suspend_wp_update(void)
{
	int rc;
	struct ipa_mhi_mmio_register_set *p_mmio;
	struct ipa_mhi_channel_context_array *p_ch_ctx_array;
	u64 old_wp;
	u64 phys_addr;

	/* simulate a write by updating the wp */
	p_mmio = test_mhi_ctx->mmio_buf.base;
	phys_addr = p_mmio->ccabap + ((IPA_MHI_TEST_FIRST_CHANNEL_ID) *
		sizeof(struct ipa_mhi_channel_context_array));
	p_ch_ctx_array = test_mhi_ctx->ch_ctx_array.base +
		(phys_addr - test_mhi_ctx->ch_ctx_array.phys_base);
	old_wp = p_ch_ctx_array->wp;
	p_ch_ctx_array->wp += 16;

	IPA_UT_LOG("BEFORE suspend\n");
	rc = ipa_mhi_test_suspend(false, false);
	if (rc) {
		IPA_UT_LOG("suspend failed rc %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend fail");
		p_ch_ctx_array->wp = old_wp;
		return rc;
	}
	IPA_UT_LOG("AFTER suspend\n");

	p_ch_ctx_array->wp = old_wp;

	return 0;
}

/**
 * To be run during test
 *	1. create open aggr by sending data
 *	2. channel reset (disconnect/connet)
 *	3. validate no aggr. open after reset
 *	4. disable aggr.
 */
static int ipa_mhi_test_channel_reset_aggr_open(void)
{
	int rc;
	u32 aggr_state_active;
	struct ipa_ep_cfg_aggr ep_aggr;

	IPA_UT_LOG("Entry\n");

	rc = ipa_mhi_test_create_aggr_open_frame();
	if (rc) {
		IPA_UT_LOG("failed create open aggr rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail creare open aggr frame");
		return rc;
	}

	rc = ipa_mhi_test_channel_reset();
	if (rc) {
		IPA_UT_LOG("channel reset failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("channel reset fail");
		return rc;
	}

	aggr_state_active = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
	IPADBG("IPA_STATE_AGGR_ACTIVE 0x%x\n", aggr_state_active);
	if (aggr_state_active != 0) {
		IPA_UT_LOG("aggregation frame open after reset!\n");
		IPA_UT_LOG("IPA_STATE_AGGR_ACTIVE 0x%x\n", aggr_state_active);
		IPA_UT_TEST_FAIL_REPORT("open aggr after reset");
		return -EFAULT;
	}

	memset(&ep_aggr, 0, sizeof(ep_aggr));
	rc = ipa3_cfg_ep_aggr(test_mhi_ctx->cons_hdl, &ep_aggr);
	if (rc) {
		IPA_UT_LOG("failed to configure aggr");
		IPA_UT_TEST_FAIL_REPORT("fail to disable aggr");
		return rc;
	}

	return rc;
}

/**
 * To be run during test
 *	1. queue OUT RE/buffer
 *	2. wait for MSI on OUT
 *	3. Do 1. and 2. till got MSI wait timeout (ch full)
 *	4. channel reset
 *		disconnect and reconnect the prod and cons
 *	5. queue IN RE/buffer and ring DB
 *	6. wait for MSI - should get timeout as channels were reset
 *	7. reset again
 */
static int ipa_mhi_test_channel_reset_ipa_holb(void)
{
	int rc;
	int submitted_packets = 0;
	bool timeout;

	IPA_UT_LOG("Entry\n");

	rc = ipa_mhi_test_create_full_channel(&submitted_packets);
	if (rc) {
		IPA_UT_LOG("fail create full channel rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail create full channel");
		return rc;
	}

	rc = ipa_mhi_test_channel_reset();
	if (rc) {
		IPA_UT_LOG("channel reset failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("channel reset fail");
		return rc;
	}

	/* invalidate spare register value (for msi) */
	memset(test_mhi_ctx->msi.base, 0xFF, test_mhi_ctx->msi.size);
	timeout = true;
	/* queue RE for IN side and trigger doorbell */
	rc = ipa_mhi_test_q_transfer_re(&test_mhi_ctx->mmio_buf,
		test_mhi_ctx->xfer_ring_bufs,
		test_mhi_ctx->ev_ring_bufs,
		IPA_MHI_TEST_FIRST_CHANNEL_ID + 1,
		&test_mhi_ctx->in_buffer,
		1,
		true,
		true,
		false,
		true);

	if (rc) {
		IPA_UT_LOG("ipa_mhi_test_q_transfer_re failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail IN q re");
		return rc;
	}
	submitted_packets--;

	IPA_MHI_TEST_CHECK_MSI_INTR(true, timeout);
	if (!timeout) {
		IPA_UT_LOG("transfer succeed although we had reset\n");
		IPA_UT_TEST_FAIL_REPORT("xfer succeed although we had reset");
		return -EFAULT;
	}

	rc = ipa_mhi_test_channel_reset();
	if (rc) {
		IPA_UT_LOG("channel reset failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("channel reset fail");
		return rc;
	}

	return rc;
}


/**
 * TEST: mhi reset in READY state
 *	1. init to ready state (without start and connect)
 *	2. reset (destroy and re-init)
 *	2. destroy
 */
static int ipa_mhi_test_reset_ready_state(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(true);
	if (rc) {
		IPA_UT_LOG("init to Ready state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init to ready state");
		return rc;
	}

	rc = ipa_mhi_test_reset(ctx, true);
	if (rc) {
		IPA_UT_LOG("reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("reset (destroy/re-init) failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi reset in M0 state
 *	1. init to M0 state (with start and connect)
 *	2. reset (destroy and re-init)
 *	2. destroy
 */
static int ipa_mhi_test_reset_m0_state(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT
			("fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	rc = ipa_mhi_test_reset(ctx, false);
	if (rc) {
		IPA_UT_LOG("reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("reset (destroy/re-init) failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi in-loop reset in M0 state
 *	1. init to M0 state (with start and connect)
 *	2. reset (destroy and re-init) in-loop
 *	3. destroy
 */
static int ipa_mhi_test_inloop_reset_m0_state(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT
			("fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_reset, rc, ctx, false);
	if (rc) {
		IPA_UT_LOG("in-loop reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"reset (destroy/re-init) in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data with reset
 *	1. init to M0 state (with start and connect)
 *	2. reset (destroy and re-init)
 *	3. loopback data
 *	4. reset (destroy and re-init)
 *	5. loopback data again
 *	6. destroy
 */
static int ipa_mhi_test_loopback_data_with_reset(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	rc = ipa_mhi_test_reset(ctx, false);
	if (rc) {
		IPA_UT_LOG("reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("reset (destroy/re-init) failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_reset(ctx, false);
	if (rc) {
		IPA_UT_LOG("reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("reset (destroy/re-init) failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi reset in suspend state
 *	1. init to M0 state (with start and connect)
 *	2. suspend
 *	3. reset (destroy and re-init)
 *	4. destroy
 */
static int ipa_mhi_test_reset_on_suspend(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return -EFAULT;
	}

	rc = ipa_mhi_test_suspend_and_reset(ctx);
	if (rc) {
		IPA_UT_LOG("suspend and reset failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("suspend and then reset failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return -EFAULT;
	}

	return 0;
}

/**
 * TEST: mhi in-loop reset in suspend state
 *	1. init to M0 state (with start and connect)
 *	2. suspend
 *	3. reset (destroy and re-init)
 *	4. Do 2 and 3 in loop
 *	3. destroy
 */
static int ipa_mhi_test_inloop_reset_on_suspend(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_and_reset, rc, ctx);
	if (rc) {
		IPA_UT_LOG("in-loop reset in suspend failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to in-loop reset while suspend");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data with reset
 *	1. init to M0 state (with start and connect)
 *	2. suspend
 *	3. reset (destroy and re-init)
 *	4. loopback data
 *	5. suspend
 *	5. reset (destroy and re-init)
 *	6. destroy
 */
static int ipa_mhi_test_loopback_data_with_reset_on_suspend(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	rc = ipa_mhi_test_suspend_and_reset(ctx);
	if (rc) {
		IPA_UT_LOG("suspend and reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to suspend and then reset");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_suspend_and_reset(ctx);
	if (rc) {
		IPA_UT_LOG("suspend and reset failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to suspend and then reset");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop suspend/resume
 *	1. init to M0 state (with start and connect)
 *	2. in loop suspend/resume
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_suspend_resume(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_resume, rc);
	if (rc) {
		IPA_UT_LOG("suspend resume failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT("in loop suspend/resume failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop suspend/resume with aggr open
 *	1. init to M0 state (with start and connect)
 *	2. in loop suspend/resume with open aggr.
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_suspend_resume_aggr_open(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_aggr_open,
		rc, false);
	if (rc) {
		IPA_UT_LOG("suspend resume with aggr open failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop suspend/resume with open aggr failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop force suspend/resume with aggr open
 *	1. init to M0 state (with start and connect)
 *	2. in loop force suspend/resume with open aggr.
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_force_suspend_resume_aggr_open(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_aggr_open,
		rc, true);
	if (rc) {
		IPA_UT_LOG("force suspend resume with aggr open failed rc=%d",
			rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop force suspend/resume with open aggr failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop suspend/host wakeup resume
 *	1. init to M0 state (with start and connect)
 *	2. in loop suspend/resume with host wakeup
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_suspend_host_wakeup(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_host_wakeup, rc);
	if (rc) {
		IPA_UT_LOG("suspend host wakeup resume failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop suspend/resume with hsot wakeup failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop rejected suspend as full channel
 *	1. init to M0 state (with start and connect)
 *	2. in loop rejrected suspend
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_reject_suspend_full_channel(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_full_channel,
		rc, false);
	if (rc) {
		IPA_UT_LOG("full channel rejected suspend failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop rejected suspend due to full channel failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop rejected force suspend as full channel
 *	1. init to M0 state (with start and connect)
 *	2. in loop force rejected suspend
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_reject_force_suspend_full_channel(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_full_channel,
		rc, true);
	if (rc) {
		IPA_UT_LOG("full channel rejected force suspend failed rc=%d",
			rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop force rejected suspend as full ch failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop suspend after wp manual update
 *	1. init to M0 state (with start and connect)
 *	2. in loop suspend after wp update
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_suspend_resume_wp_update(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_suspend_wp_update, rc);
	if (rc) {
		IPA_UT_LOG("suspend after wp update failed rc=%d", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop suspend after wp update failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop channel reset (disconnect/connect)
 *	1. init to M0 state (with start and connect)
 *	2. in loop channel reset (disconnect/connect)
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_channel_reset(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_channel_reset, rc);
	if (rc) {
		IPA_UT_LOG("channel reset (disconnect/connect) failed rc=%d",
			rc);
		IPA_UT_TEST_FAIL_REPORT("in loop channel reset failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop channel reset (disconnect/connect)
 *	1. init to M0 state (with start and connect)
 *	2. in loop channel reset (disconnect/connect) with open aggr
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_channel_reset_aggr_open(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_channel_reset_aggr_open, rc);
	if (rc) {
		IPA_UT_LOG("channel reset (disconnect/connect) failed rc=%d",
			rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop channel reset with open aggr failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/**
 * TEST: mhi loopback data after in loop channel reset (disconnect/connect)
 *	1. init to M0 state (with start and connect)
 *	2. in loop channel reset (disconnect/connect) with channel in HOLB
 *	3. loopback data
 *	4. destroy
 */
static int ipa_mhi_test_in_loop_channel_reset_ipa_holb(void *priv)
{
	int rc;
	struct ipa_test_mhi_context *ctx = (struct ipa_test_mhi_context *)priv;

	IPA_UT_LOG("Test Start\n");

	if (unlikely(!ctx)) {
		IPA_UT_LOG("No context");
		return -EFAULT;
	}

	rc = ipa_mhi_test_initialize_driver(false);
	if (rc) {
		IPA_UT_LOG("init to M0 state failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT(
			"fail to init to M0 state (w/ start and connect)");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_channel_reset_ipa_holb, rc);
	if (rc) {
		IPA_UT_LOG("channel reset (disconnect/connect) failed rc=%d",
			rc);
		IPA_UT_TEST_FAIL_REPORT(
			"in loop channel reset with channel HOLB failed");
		return rc;
	}

	IPA_MHI_RUN_TEST_UNIT_IN_LOOP(ipa_mhi_test_loopback_data_transfer, rc);
	if (rc) {
		IPA_UT_LOG("data loopback failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("loopback data in loop failed");
		return rc;
	}

	rc = ipa_mhi_test_destroy(ctx);
	if (rc) {
		IPA_UT_LOG("destroy failed rc=%d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return rc;
	}

	return 0;
}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(mhi, "MHI for GSI",
	ipa_test_mhi_suite_setup, ipa_test_mhi_suite_teardown)
{
	IPA_UT_ADD_TEST(reset_ready_state,
		"reset test in Ready state",
		ipa_mhi_test_reset_ready_state,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(reset_m0_state,
		"reset test in M0 state",
		ipa_mhi_test_reset_m0_state,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(inloop_reset_m0_state,
		"several reset iterations in M0 state",
		ipa_mhi_test_inloop_reset_m0_state,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(loopback_data_with_reset_on_m0,
		"reset before and after loopback data in M0 state",
		ipa_mhi_test_loopback_data_with_reset,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(reset_on_suspend,
		"reset test in suspend state",
		ipa_mhi_test_reset_on_suspend,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(inloop_reset_on_suspend,
		"several reset iterations in suspend state",
		ipa_mhi_test_inloop_reset_on_suspend,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(loopback_data_with_reset_on_suspend,
		"reset before and after loopback data in suspend state",
		ipa_mhi_test_loopback_data_with_reset_on_suspend,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(suspend_resume,
		"several suspend/resume iterations",
		ipa_mhi_test_in_loop_suspend_resume,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(suspend_resume_with_open_aggr,
		"several suspend/resume iterations with open aggregation frame",
		ipa_mhi_test_in_loop_suspend_resume_aggr_open,
		true, IPA_HW_v3_0, IPA_HW_v3_5_1),
	IPA_UT_ADD_TEST(force_suspend_resume_with_open_aggr,
		"several force suspend/resume iterations with open aggregation frame",
		ipa_mhi_test_in_loop_force_suspend_resume_aggr_open,
		true, IPA_HW_v3_0, IPA_HW_v3_5_1),
	IPA_UT_ADD_TEST(suspend_resume_with_host_wakeup,
		"several suspend and host wakeup resume iterations",
		ipa_mhi_test_in_loop_suspend_host_wakeup,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(reject_suspend_channel_full,
		"several rejected suspend iterations due to full channel",
		ipa_mhi_test_in_loop_reject_suspend_full_channel,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(reject_force_suspend_channel_full,
		"several rejected force suspend iterations due to full channel",
		ipa_mhi_test_in_loop_reject_force_suspend_full_channel,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(suspend_resume_manual_wp_update,
		"several suspend/resume iterations with after simulating writing by wp manual update",
		ipa_mhi_test_in_loop_suspend_resume_wp_update,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(channel_reset,
		"several channel reset (disconnect/connect) iterations",
		ipa_mhi_test_in_loop_channel_reset,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(channel_reset_aggr_open,
		"several channel reset (disconnect/connect) iterations with open aggregation frame",
		ipa_mhi_test_in_loop_channel_reset_aggr_open,
		true, IPA_HW_v3_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(channel_reset_ipa_holb,
		"several channel reset (disconnect/connect) iterations with channel in HOLB state",
		ipa_mhi_test_in_loop_channel_reset_ipa_holb,
		true, IPA_HW_v3_0, IPA_HW_MAX),
} IPA_UT_DEFINE_SUITE_END(mhi);
