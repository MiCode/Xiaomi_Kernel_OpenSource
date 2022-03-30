// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "vcp_ipi.h"
#include "vcp_ipi_table.h"
#include "vcp.h"

#define IPI_NO_USE 0xFF

struct vcp_ipi_desc mpool[VCP_NR_IPI - IPI_MPOOL - 1];
struct vcp_ipi_wrapper vcp_ipi_legacy_id[1];


struct vcp_ipi_legacy_pkt {
	unsigned int id;
	unsigned int len;
	void *data;
};

/*
 * API let apps unregister an ipi handler
 * @param id:	   IPI ID
 */
enum vcp_ipi_status vcp_ipi_unregistration(enum vcp_ipi_id id)
{
	int ret;

	if (id >= VCP_NR_IPI || id == IPI_MPOOL)
		return VCP_IPI_ERROR;

	if (id > IPI_MPOOL) {
		/* if ipi id belongs to mbox pool, remove handler only */
		mpool[id - IPI_MPOOL - 1].handler = NULL;
		return VCP_IPI_DONE;
	}

	if (vcp_ipi_legacy_id[id].in_id_0 != IPI_NO_USE) {
		ret = mtk_ipi_unregister(&vcp_ipidev,
					vcp_ipi_legacy_id[id].in_id_0);

		if (ret != IPI_ACTION_DONE)
			return VCP_IPI_ERROR;
	}

	if (vcp_ipi_legacy_id[id].in_id_1 != IPI_NO_USE) {
		ret = mtk_ipi_unregister(&vcp_ipidev,
					vcp_ipi_legacy_id[id].in_id_1);

		if (ret != IPI_ACTION_DONE)
			return VCP_IPI_ERROR;
	}

	return VCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(vcp_ipi_unregistration);

/*
 * API for apps to send an IPI to vcp
 * @param id:   IPI ID
 * @param buf:  the pointer of data
 * @param len:  data length
 * @param wait: If true, wait (atomically) until data have been gotten by Host
 * @param len:  data length
 */
enum vcp_ipi_status vcp_ipi_send(enum vcp_ipi_id id, void *buf,
	unsigned int  len, unsigned int wait, enum vcp_core_id vcp_id)
{
	/* declare pkt with a mbox maximum size for re-structing data */
	char pkt[256];
	int ret = VCP_IPI_ERROR, tmp_id;
	void *ptr;

	if (id >= VCP_NR_IPI || id == IPI_MPOOL)
		return VCP_IPI_ERROR;

	if (id > IPI_MPOOL)
		tmp_id = IPI_MPOOL;
	else
		tmp_id = id;

	if (is_vcp_ready(vcp_id) == 0) {
		pr_notice("[VCP] %s: %s not ready\n", __func__, core_ids[vcp_id]);
		return VCP_IPI_NOT_READY;
	}

	if (len > (vcp_ipi_legacy_id[tmp_id].out_size - 2) * MBOX_SLOT_SIZE) {
		pr_notice("%s: len overflow\n", __func__);
		return VCP_IPI_ERROR;
	}

	/* variation length will only support chre and sensor for reducing slot
	 * and cpu time cost by memcpy.
	 */
	memcpy((void *)pkt, (void *)&id, sizeof(uint32_t));
	memcpy((void *)(pkt + 4), (void *)&len, sizeof(uint32_t));
	memcpy((void *)(pkt + 8), buf, len);
	ptr = pkt;

	if (vcp_ipi_legacy_id[tmp_id].out_id_0 != IPI_NO_USE
	    && vcp_id == (enum vcp_core_id)VCP_CORE0_ID) {
		ret =
		   mtk_ipi_send(&vcp_ipidev, vcp_ipi_legacy_id[tmp_id].out_id_0,
				0, ptr, vcp_ipi_legacy_id[tmp_id].out_size,
				wait * VCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return VCP_IPI_DONE;
		if (ret == IPI_PIN_BUSY)
			return VCP_IPI_BUSY;
	}

	if (vcp_ipi_legacy_id[tmp_id].out_id_1 != IPI_NO_USE
	    && vcp_id == (enum vcp_core_id)VCP_CORE1_ID) {
		ret =
		   mtk_ipi_send(&vcp_ipidev, vcp_ipi_legacy_id[tmp_id].out_id_1,
				0, ptr, vcp_ipi_legacy_id[tmp_id].out_size,
				wait * VCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return VCP_IPI_DONE;
		if (ret == IPI_PIN_BUSY)
			return VCP_IPI_BUSY;
	}

	return VCP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(vcp_ipi_send);

void mbox_setup_pin_table(unsigned int mbox)
{
	int i, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;

	for (i = 0; i < vcp_mboxdev.send_count; i++) {
		if (mbox == vcp_mbox_pin_send[i].mbox) {
			vcp_mbox_pin_send[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_send[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_send[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_send[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_send[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_send[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < vcp_mboxdev.recv_count; i++) {
		if (mbox == vcp_mbox_pin_recv[i].mbox) {
			vcp_mbox_pin_recv[i].offset = last_ofs + last_slot;
			vcp_mbox_pin_recv[i].pin_index = last_idx + last_sz;
			last_idx = vcp_mbox_pin_recv[i].pin_index;
			if (vcp_mbox_info[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
					   vcp_mbox_pin_recv[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = vcp_mbox_pin_recv[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < vcp_mbox_pin_recv[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
	   (last_ofs + last_slot) > (vcp_mbox_info[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

/*
 * API to dump wakeup reason for sensor ipi in detailed
 */
void mt_print_vcp_ipi_id(unsigned int mbox)
{
	unsigned int irq_status, i;
	struct vcp_ipi_info {
		unsigned int id;
		unsigned int len;
		uint8_t info[4];
	} buf;

	irq_status = mtk_mbox_read_recv_irq(&vcp_mboxdev, mbox);
	for (i = 0; i < vcp_mboxdev.recv_count; i++) {
		if (vcp_mbox_pin_recv[i].mbox == mbox) {
			if (irq_status & 1 << vcp_mbox_pin_recv[i].pin_index) {
				if (vcp_mbox_pin_recv[i].chan_id
						== IPI_IN_VCP_MPOOL_0) {
					/* only read 1st, 2nd, 3rd slot for
					 * sensor id, len, and header info
					 */
					mtk_mbox_read(&vcp_mboxdev, mbox,
						    vcp_mbox_pin_recv[i].offset,
						    &buf, MBOX_SLOT_SIZE * 3);
					pr_info("[VCP] ipi id/type/action/event/reserve = %u/%u/%u/%u/%u\n",
						buf.id, buf.info[0],
						buf.info[1], buf.info[2],
						buf.info[3]);
				} else {
					pr_info("[VCP] mbox%u, ipi id %u\n",
						mbox,
						vcp_mbox_pin_recv[i].chan_id);
				}
			}
		}
	}
}

