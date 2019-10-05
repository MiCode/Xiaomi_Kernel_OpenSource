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

#define IPI_NO_USE 0xFF

static char msg_legacy_ipi_mpool_0[PIN_IN_SIZE_SCP_MPOOL * MBOX_SLOT_SIZE];
static char msg_legacy_ipi_mpool_1[PIN_IN_SIZE_SCP_MPOOL * MBOX_SLOT_SIZE];

#define SCP_IPI_LEGACY_GROUP				  \
{							  \
	{	.out_id_0 = IPI_OUT_SCP_MPOOL_0,	  \
		.out_id_1 = IPI_OUT_SCP_MPOOL_1,	  \
		.in_id_0 = IPI_IN_SCP_MPOOL_0,		  \
		.in_id_1 = IPI_IN_SCP_MPOOL_1,		  \
		.out_size = PIN_OUT_SIZE_SCP_MPOOL,	  \
		.in_size = PIN_IN_SIZE_SCP_MPOOL,	  \
		.msg_0 = msg_legacy_ipi_mpool_0,	  \
		.msg_1 = msg_legacy_ipi_mpool_1,	  \
	},						  \
}

struct scp_ipi_desc mpool[SCP_NR_IPI - IPI_MPOOL - 1];
struct scp_ipi_wrapper scp_ipi_legacy_id[] = SCP_IPI_LEGACY_GROUP;


struct scp_ipi_legacy_pkt {
	unsigned int id;
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
	pkt.id = *(unsigned int *)data;
	pkt.len = *(unsigned int *)(data + 4);
	pkt.data = (void *)(data + 8);

	if (pkt.id > IPI_MPOOL)
		handler = mpool[pkt.id - IPI_MPOOL - 1].handler;
	else
		handler = prdata;
	if (handler)
		handler(pkt.id, pkt.data, pkt.len);
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
	int ret = SCP_IPI_ERROR;

	if (id >= SCP_NR_IPI || id == IPI_MPOOL)
		return SCP_IPI_ERROR;

	if (ipi_handler == NULL)
		return SCP_IPI_ERROR;

	if (id > IPI_MPOOL) {
		mpool[id - IPI_MPOOL - 1].handler = ipi_handler;
		return SCP_IPI_DONE;
	}

	if (scp_ipi_legacy_id[id].in_id_0 != IPI_NO_USE) {
		ret =
		    mtk_ipi_register(&scp_ipidev, scp_ipi_legacy_id[id].in_id_0,
				    (void *)scp_legacy_handler, ipi_handler,
				    scp_ipi_legacy_id[id].msg_0);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	if (scp_ipi_legacy_id[id].in_id_1 != IPI_NO_USE) {
		ret =
		    mtk_ipi_register(&scp_ipidev, scp_ipi_legacy_id[id].in_id_1,
				    (void *)scp_legacy_handler, ipi_handler,
				    scp_ipi_legacy_id[id].msg_1);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:	   IPI ID
 */
enum scp_ipi_status scp_ipi_unregistration(enum ipi_id id)
{
	int ret;

	if (id >= SCP_NR_IPI || id == IPI_MPOOL)
		return SCP_IPI_ERROR;

	if (id > IPI_MPOOL) {
		/* if ipi id belongs to mbox pool, remove handler only */
		mpool[id - IPI_MPOOL - 1].handler = NULL;
		return SCP_IPI_DONE;
	}

	if (scp_ipi_legacy_id[id].in_id_0 != IPI_NO_USE) {
		ret = mtk_ipi_unregister(&scp_ipidev,
					scp_ipi_legacy_id[id].in_id_0);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	if (scp_ipi_legacy_id[id].in_id_1 != IPI_NO_USE) {
		ret = mtk_ipi_unregister(&scp_ipidev,
					scp_ipi_legacy_id[id].in_id_1);

		if (ret != IPI_ACTION_DONE)
			return SCP_IPI_ERROR;
	}

	return SCP_IPI_DONE;
}
EXPORT_SYMBOL_GPL(scp_ipi_unregistration);

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
	/* declare pkt with a mbox maximum size for re-structing data */
	char pkt[256];
	int ret = SCP_IPI_ERROR, tmp_id;
	void *ptr;

	if (id >= SCP_NR_IPI || id == IPI_MPOOL)
		return SCP_IPI_ERROR;

	if (id > IPI_MPOOL)
		tmp_id = IPI_MPOOL;
	else
		tmp_id = id;

	if (len > (scp_ipi_legacy_id[tmp_id].out_size - 2) * MBOX_SLOT_SIZE) {
		pr_err("%s: len overflow\n", __func__);
		return SCP_IPI_ERROR;
	}

	/* variation length will only support chre and sensor for reducing slot
	 * and cpu time cost by memcpy.
	 */
	memcpy((void *)pkt, (void *)&id, sizeof(uint32_t));
	memcpy((void *)(pkt + 4), (void *)&len, sizeof(uint32_t));
	memcpy((void *)(pkt + 8), buf, len);
	ptr = pkt;

	if (scp_ipi_legacy_id[tmp_id].out_id_0 != IPI_NO_USE
	    && scp_id == SCP_CORE0_ID) {
		ret =
		   mtk_ipi_send(&scp_ipidev, scp_ipi_legacy_id[tmp_id].out_id_0,
				0, ptr, scp_ipi_legacy_id[tmp_id].out_size,
				wait * SCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return SCP_IPI_DONE;
	}

	if (scp_ipi_legacy_id[tmp_id].out_id_1 != IPI_NO_USE
	    && scp_id == SCP_CORE1_ID) {
		ret =
		   mtk_ipi_send(&scp_ipidev, scp_ipi_legacy_id[tmp_id].out_id_1,
				0, ptr, scp_ipi_legacy_id[tmp_id].out_size,
				wait * SCP_IPI_LEGACY_WAIT);

		if (ret == IPI_ACTION_DONE)
			return SCP_IPI_DONE;
	}

	return SCP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(scp_ipi_send);

enum scp_ipi_status scp_legacy_ipi_init(void)
{
	int ret = 0;

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_MPOOL_0,
			      (void *)scp_legacy_handler, 0,
			      msg_legacy_ipi_mpool_0);

	if (ret != IPI_ACTION_DONE)
		return SCP_IPI_ERROR;

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_MPOOL_1,
			      (void *)scp_legacy_handler, 0,
			      msg_legacy_ipi_mpool_1);

	if (ret != IPI_ACTION_DONE)
		return SCP_IPI_ERROR;

	return SCP_IPI_DONE;
}
