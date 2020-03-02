/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "scp_ipi.h"

#define SCP_IPI_LEGACY_OUT_VAR(id) ((1 << id) & SCP_OUT_WRAPPER_VAR)
#define SCP_IPI_LEGACY_IN_VAR(id) ((1 << id) & SCP_IN_WRAPPER_VAR)


static struct scp_ipi_wrapper scp_ipi_legacy_id[] = SCP_IPI_LEGACY_GROUP;
struct scp_ipi_legacy_pkt {
	unsigned int len;
	void *data;
};

/*
 * This is a handler for handling legacy ipi callback function
 */
static void scp_legacy_handler(int id, void *prdata, void *data,
			       unsigned int len)
{
	void (*handler)(int id, void *data, unsigned int len);
	struct scp_ipi_legacy_pkt pkt;

	/* variation length will only support chre, chrex and sensor for
	 * reducing slot and cpu time cost by memcpy.
	 */
	if (SCP_IPI_LEGACY_IN_VAR(id)) {
		pkt.len = *(unsigned int *)data;
		pkt.data = (void *)(data + 4);
	} else {
		pkt.len = len;
		pkt.data = data;
	}

	handler = prdata;
	handler(id, pkt.data, pkt.len);
}

/*
 * API let apps can register an ipi handler to receive IPI
 * @param id:	   IPI ID
 * @param handler:  IPI handler
 * @param name:	 IPI name
 */
enum scp_ipi_status scp_ipi_registration(enum ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name)
{
	int ret;

	if (id >= SCP_NR_IPI)
		return SCP_IPI_ERROR;

	if (ipi_handler == NULL)
		return SCP_IPI_ERROR;


	if (scp_ipi_legacy_id[id].in_id_0 != 0) {
		ret =
		    mtk_ipi_register(&scp_ipidev, scp_ipi_legacy_id[id].in_id_0,
				    (void *)scp_legacy_handler, ipi_handler,
				    &scp_ipi_legacy_id[id].msg);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	if (scp_ipi_legacy_id[id].in_id_1 != 0) {
		ret =
		    mtk_ipi_register(&scp_ipidev, scp_ipi_legacy_id[id].in_id_1,
				    (void *)scp_legacy_handler, ipi_handler,
				    &scp_ipi_legacy_id[id].msg);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_ipi_registration);

/*
 * API for apps to send an IPI to scp
 * @param id:   IPI ID
 * @param buf:  the pointer of data
 * @param len:  data length
 * @param wait: If true, wait (atomically) until data have been gotten by Host
 * @param len:  data length
 */
enum scp_ipi_status scp_ipi_send(enum ipi_id id, void *buf,
	unsigned int  len, unsigned int wait, enum scp_core_id scp_id)
{
	int ret = 0;
	char pkt[scp_ipi_legacy_id[id].out_size * MBOX_SLOT_SIZE];
	void *ptr;
	unsigned int mbox_len;

	if (id >= SCP_NR_IPI)
		return SCP_IPI_ERROR;

	if (SCP_IPI_LEGACY_OUT_VAR(id))
		mbox_len =
			(scp_ipi_legacy_id[id].out_size - 1) * MBOX_SLOT_SIZE;
	else
		mbox_len = scp_ipi_legacy_id[id].out_size * MBOX_SLOT_SIZE;

	if (len > mbox_len) {
		pr_err("%s: len overflow\n", __func__);
		return SCP_IPI_ERROR;
	}

	/* variation length will only support chre and sensor for reducing slot
	 * and cpu time cost by memcpy.
	 */
	if (SCP_IPI_LEGACY_OUT_VAR(id)) {
		memcpy((void *)pkt, (void *)&len, sizeof(uint32_t));
		memcpy((void *)(pkt + 4), buf, len);
		ptr = pkt;
	} else
		ptr = buf;


	if (scp_ipi_legacy_id[id].out_id_0 != 0) {
		ret = mtk_ipi_send(&scp_ipidev, scp_ipi_legacy_id[id].out_id_0,
				   0, ptr, scp_ipi_legacy_id[id].out_size,
				   wait * SCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return SCP_IPI_DONE;
	}

	if (scp_ipi_legacy_id[id].out_id_1 != 0) {
		ret = mtk_ipi_send(&scp_ipidev, scp_ipi_legacy_id[id].out_id_1,
				   0, ptr, scp_ipi_legacy_id[id].out_size,
				   wait * SCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return SCP_IPI_DONE;
	}

	return SCP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(scp_ipi_send);

