// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "reviser_reg.h"
#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_mem_mgt.h"
#include "reviser_hw.h"

//ONLY FOR DEBUGGING
#define _FORCE_SET_REMAP 0


//static unsigned long pgtable_dram[BITS_TO_LONGS(VLM_DRAM_BANK_MAX)];
static unsigned long table_tcm[BITS_TO_LONGS(TABLE_TCM_MAX)];
static unsigned long table_ctxID[BITS_TO_LONGS(TABLE_CTXID_MAX)];

static bool g_ctxid_empty;
static uint32_t g_tcm_free;
static struct vlm_pgtable g_vlm_pgtable[VLM_CTXT_CTX_ID_MAX];
static struct table_remap g_table_remap;

static int _reviser_set_vlm_pgtable(void *drvinfo,
		unsigned long ctxID, struct table_vlm *vlm_pgtable);
static int _reviser_clear_vlm_pgtable(void *drvinfo,
		unsigned long ctxID, struct table_tcm *tcm_pgtable);
static int _reviser_force_remap(void *drvinfo);

static int _reviser_force_remap(void *drvinfo)
{
	int ret = 0;
/* Debug ONLY */
#if _FORCE_SET_REMAP

	int valid = 0;
	int ctxid = 0;

	/* Set HW remap table */
	if (reviser_set_remap_table(drvinfo, 0, valid, ctxid, 0, 0)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 1, valid, ctxid, 0, 1)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 2, valid, ctxid, 2, 2)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 3, valid, ctxid, 3, 3)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 4, valid, ctxid, 0, 1)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 5, valid, ctxid, 1, 2)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 6, valid, ctxid, 2, 3)) {
		ret = -1;
		goto out;
	}
	if (reviser_set_remap_table(drvinfo, 7, valid, ctxid, 3, 0)) {
		ret = -1;
		goto out;
	}
out:
#endif

	return ret;
}
int reviser_table_init_ctxID(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_ctxid);

	bitmap_zero(table_ctxID, TABLE_CTXID_MAX);
	g_ctxid_empty = false;

	mutex_unlock(&reviser_device->mutex_ctxid);

	return 0;
}
int reviser_table_get_ctxID_sync(void *drvinfo, unsigned long *ctxID)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	while (1) {
		if (reviser_table_get_ctxID(drvinfo, ctxID)) {
			LOG_DEBUG("Wait for Getting ctxID\n");
			wait_event_interruptible(reviser_device->wait_ctxid,
					!g_ctxid_empty);
		} else {
			break;
		}
	}
	//LOG_DEBUG("Sync Get ctxID %lu\n", *ctxID);

	return 0;
}

int reviser_table_get_ctxID(void *drvinfo, unsigned long *ctxID)
{
	unsigned long fist_zero = 0;
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;


	mutex_lock(&reviser_device->mutex_ctxid);

	fist_zero = find_first_zero_bit(table_ctxID, TABLE_CTXID_MAX);
	if (fist_zero < TABLE_CTXID_MAX) {
		bitmap_set(table_ctxID, fist_zero, 1);

		*ctxID = fist_zero;
	} else {
		LOG_ERR("No free ctxID %lu\n", fist_zero);
		g_ctxid_empty = true;
		goto free_mutex;
	}
	LOG_DEBUG("[out] ctxID(%lu) table_ctxID(%08lx)\n",
			*ctxID, table_ctxID[0]);

	mutex_unlock(&reviser_device->mutex_ctxid);


	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_ctxid);
	return -1;
}

int reviser_table_free_ctxID(void *drvinfo, unsigned long ctxID)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (ctxID >= TABLE_CTXID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;


	mutex_lock(&reviser_device->mutex_ctxid);


	bitmap_clear(table_ctxID, ctxID, 1);
	g_ctxid_empty = false;
	//LOG_DEBUG("Clear table for ctxID %lu\n", ctxID);
	wake_up_interruptible(&reviser_device->wait_ctxid);

	LOG_DEBUG("[in] ctxID(%lu) [out] table_ctxID(%08lx)\n"
			, ctxID, table_ctxID[0]);

	mutex_unlock(&reviser_device->mutex_ctxid);

	return 0;

}
void reviser_table_print_ctxID(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t i;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}


	reviser_device = (struct reviser_dev_info *)drvinfo;
	mutex_lock(&reviser_device->mutex_ctxid);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " Contex Table\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < BITS_TO_LONGS(TABLE_CTXID_MAX); i++)
		LOG_CON(s, "%d: [%lx]\n", i, table_ctxID[i]);

	LOG_CON(s, "=============================\n");

	mutex_unlock(&reviser_device->mutex_ctxid);
}


int reviser_table_init_tcm(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_tcm);

	if (TABLE_TCM_MAX != 0)
		bitmap_zero(table_tcm, TABLE_TCM_MAX);
	g_tcm_free = TABLE_TCM_MAX;

	mutex_unlock(&reviser_device->mutex_tcm);

	return 0;
}


int reviser_table_get_tcm_sync(void *drvinfo,
		uint32_t page_num, struct table_tcm *pg_table)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (page_num > VLM_TCM_BANK_MAX) {
		LOG_ERR("invalid page_num %d\n", page_num);
		return -1;
	}
	if (pg_table == NULL) {
		LOG_ERR("invalid pg_table\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	while (1) {
		if (reviser_table_get_tcm(drvinfo, page_num, pg_table)) {
			LOG_DEBUG("Wait for Getting tcm\n");
			wait_event_interruptible(reviser_device->wait_tcm,
					g_tcm_free >= page_num);
		} else {
			break;
		}
	}
	//LOG_DEBUG("Sync Get page_num %u\n", pg_table->page_num);
	//LOG_DEBUG("Sync Get table_tcm %lx\n", pg_table->table_tcm[0]);
	return 0;
}

int reviser_table_get_tcm(void *drvinfo,
		uint32_t page_num, struct table_tcm *tcm_pgtable)
{
	unsigned long fist_zero = 0;
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t i;
	unsigned long setbits = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;



	mutex_lock(&reviser_device->mutex_tcm);

//	LOG_DEBUG("page_num %u tcm_pgtable %lx\n",
//			page_num, tcm_pgtable->table_tcm[0]);
	if (g_tcm_free == 0) {
		LOG_DEBUG("No free TCM (%u/%u)\n",
				page_num, g_tcm_free);
		tcm_pgtable->page_num = 0;
		goto free_mutex;
	}

	setbits = bitmap_weight(table_tcm, TABLE_TCM_MAX);
	//LOG_DEBUG("setbits %lu\n", setbits);
	if (TABLE_TCM_MAX - setbits < page_num) {
		LOG_DEBUG("No free page (%u/%lu)\n",
				page_num, TABLE_TCM_MAX - setbits);
		tcm_pgtable->page_num = 0;
		goto free_mutex;
	}

	for (i = 0; i < page_num; i++) {
		fist_zero = find_first_zero_bit(table_tcm, TABLE_TCM_MAX);
		if (fist_zero < TABLE_TCM_MAX) {
			bitmap_set(table_tcm, fist_zero, 1);
			bitmap_set(tcm_pgtable->table_tcm, fist_zero, 1);
			g_tcm_free--;
			tcm_pgtable->page_num++;
			//LOG_DEBUG("page_num %lu\n", i);
			//LOG_DEBUG("tcm table %lx\n", table_tcm[0]);
			//LOG_DEBUG("tcm_pgtable %lx\n",
			//		tcm_pgtable->table_tcm[0]);
		}

//		else {
//			LOG_ERR("No free page %lu\n", i);
//			LOG_DEBUG("pg_table->table_tcm %lx\n",
//					pg_table->table_tcm[0]);
//			LOG_DEBUG("Before restore tcm %lx g_tcm_free %d\n",
//					table_tcm[0], g_tcm_free);
//			//Restore bitmap
//			bitmap_andnot(table_tcm, table_tcm, pg_table->table_tcm,
//					TABLE_TCM_MAX);
//			g_tcm_free = g_tcm_free + pg_table->page_num;
//			bitmap_zero(pg_table->table_tcm, TABLE_TCM_MAX);
//			pg_table->page_num = 0;
//			LOG_DEBUG("After restore tcm %lx g_tcm_free %d\n",
//					table_tcm[0], g_tcm_free);
//			goto free_mutex;
//		}

	}

	LOG_DEBUG("[in] pg(%u) [out] g_tcm(%lx) g_tcm_free(%d) tcm_pgtb(%lx)\n",
			page_num,
			table_tcm[0], g_tcm_free, tcm_pgtable->table_tcm[0]);
	mutex_unlock(&reviser_device->mutex_tcm);


	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_tcm);
	return -1;
}

int reviser_table_free_tcm(void *drvinfo, struct table_tcm *pg_table)
{
	struct reviser_dev_info *reviser_device = NULL;


	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (pg_table == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}

	if (pg_table->page_num == 0) {
		LOG_DEBUG("[in] pg(%u) tcm_pgtb(%lx)\n",
				pg_table->page_num, pg_table->table_tcm[0]);
		return 0;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;




	mutex_lock(&reviser_device->mutex_tcm);


	if (pg_table->page_num <= TABLE_TCM_MAX) {

		//LOG_DEBUG("pg_table->table_tcm %lx\n",
		//		pg_table->table_tcm[0]);
		//LOG_DEBUG("Before restore tcm %lx g_tcm_free %d\n",
		//		table_tcm[0], g_tcm_free);
		bitmap_andnot(table_tcm, table_tcm, pg_table->table_tcm,
				TABLE_TCM_MAX);
		g_tcm_free = g_tcm_free + pg_table->page_num;
		//LOG_DEBUG("After restore tcm %lx g_tcm_free %d\n",
		//		table_tcm[0], g_tcm_free);
		wake_up_interruptible(&reviser_device->wait_tcm);
	} else {
		LOG_ERR("Out of range %u\n", pg_table->page_num);
		goto free_mutex;
	}

	LOG_DEBUG("[in] pg(%u) tcm_pgtb(%lx) [out] g_tcm(%lx) g_tcm_free(%d)\n",
			pg_table->page_num, pg_table->table_tcm[0],
			table_tcm[0], g_tcm_free);

	mutex_unlock(&reviser_device->mutex_tcm);


	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_tcm);
	return -1;
}

void reviser_table_print_tcm(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	uint32_t i;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}


	reviser_device = (struct reviser_dev_info *)drvinfo;
	mutex_lock(&reviser_device->mutex_tcm);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " TCM Table\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < BITS_TO_LONGS(TABLE_TCM_MAX); i++)
		LOG_CON(s, "%d: [%lx]\n", i, table_tcm[i]);

	LOG_CON(s, "=============================\n");

	mutex_unlock(&reviser_device->mutex_tcm);
}

int reviser_table_init_vlm(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_vlm_pgtable);
	memset(g_vlm_pgtable, 0,
			sizeof(struct vlm_pgtable) * VLM_CTXT_CTX_ID_MAX);
	reviser_device->pvlm = g_vlm_pgtable;
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);

	return 0;
}
static int _reviser_set_vlm_pgtable(void *drvinfo,
		unsigned long ctxID, struct table_vlm *vlm_pgtable)
{
	struct reviser_dev_info *reviser_device = NULL;
	unsigned long index = 0;
	uint32_t page_num;

	uint32_t i;


	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (ctxID >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}


	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	/* Set TCM Info */
	index = 0;
	page_num = 0;
	for (i = 0; i < vlm_pgtable->tcm_pgtable.page_num; i++) {
		index = find_next_bit(vlm_pgtable->tcm_pgtable.table_tcm,
				TABLE_TCM_MAX, index);
		//LOG_DEBUG("Find Bit index %lu!!\n", index);
		g_vlm_pgtable[ctxID].page[i].type = REVISER_MEM_TYPE_TCM;
		g_vlm_pgtable[ctxID].page[i].dst = index;
		g_vlm_pgtable[ctxID].page[i].valid = 1;

		index++;
		page_num++;
	}
	/* Save and use when clear TCM*/
	memcpy(&g_vlm_pgtable[ctxID].tcm, &vlm_pgtable->tcm_pgtable,
			sizeof(struct table_tcm));

	/* Set Dram Info */
	for (i = page_num; i < VLM_REMAP_TABLE_MAX; i++) {
		g_vlm_pgtable[ctxID].page[i].type = REVISER_MEM_TYPE_DRAM;
		g_vlm_pgtable[ctxID].page[i].dst = i;
		g_vlm_pgtable[ctxID].page[i].valid = 1;
	}

	/*sys_page_num = tcm + mmsys(todo) */
	g_vlm_pgtable[ctxID].sys_page_num = vlm_pgtable->tcm_pgtable.page_num;
	g_vlm_pgtable[ctxID].page_num = vlm_pgtable->page_num;


	LOG_DEBUG("[out] ctx(%lu) sys(%u) pg(%d) tcm_pg(%d) tcm_pgtb(%lx)\n",
			ctxID,
			g_vlm_pgtable[ctxID].sys_page_num,
			g_vlm_pgtable[ctxID].page_num,
			g_vlm_pgtable[ctxID].tcm.page_num,
			g_vlm_pgtable[ctxID].tcm.table_tcm[0]
			);
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);

	return 0;
}
static int _reviser_clear_vlm_pgtable(void *drvinfo,
		unsigned long ctxID, struct table_tcm *tcm_pgtable)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (ctxID >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (tcm_pgtable == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}



	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	/* Return TCM page table for clearing TCM */
	memcpy(tcm_pgtable, &g_vlm_pgtable[ctxID].tcm,
			sizeof(struct table_tcm));


	memset(&g_vlm_pgtable[ctxID], 0, sizeof(struct vlm_pgtable));

	LOG_DEBUG("ctxid(%lu)\n", ctxID);
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);

	return 0;
}

int reviser_table_get_vlm(void *drvinfo,
		uint32_t requset_size, bool force,
		unsigned long *id, uint32_t *tcm_size)
{
	//uint32_t page_num;
	unsigned long ctxid;
	//struct table_tcm tcm_pgtable;
	struct table_vlm vlm_pgtable;

	if (requset_size > VLM_SIZE) {
		LOG_ERR("requset_size(%x) is too larger\n", requset_size);
		goto fail;
	}

	memset(&vlm_pgtable, 0, sizeof(struct table_vlm));
	vlm_pgtable.page_num = DIV_ROUND_UP(requset_size, VLM_BANK_SIZE);
	LOG_DEBUG("[in] requset_size(%x) page_num(%u) force(%d)\n",
			requset_size, vlm_pgtable.page_num, force);

	if (reviser_table_get_ctxID_sync(drvinfo, &ctxid)) {
		LOG_ERR("Get CTX ID Fail\n");
		goto fail;
	}
	//LOG_DEBUG("ctxID: %lu\n", ctxid);

	if (VLM_TCM_BANK_MAX == 0) {
		LOG_DEBUG("Force set to false because TCM is zero\n");
		force = false;
	}

	if (force) {
		if (reviser_table_get_tcm_sync(drvinfo,
				vlm_pgtable.page_num,
				&vlm_pgtable.tcm_pgtable)) {
			LOG_ERR("Force Get TCM Fail\n");
			goto free_ctxid;
		}

	} else {
		//Get TCM fail , all page to DRAM
		//To Do List: Fragmentation , may return max available TCM page?
		if (reviser_table_get_tcm(drvinfo,
				vlm_pgtable.page_num,
				&vlm_pgtable.tcm_pgtable)) {
			LOG_DEBUG("Use Dram ctxid %lu\n", ctxid);
		}
	}

	if (_reviser_set_vlm_pgtable(drvinfo, ctxid, &vlm_pgtable)) {
		LOG_ERR("Set VLM Page Table Fail\n");
		goto free_tcm;
	}

	if (!reviser_power_on(drvinfo)) {
		/* Set HW remap table */
		if (reviser_table_set_remap(drvinfo, ctxid)) {
			LOG_ERR("Set Remap Fail and power off\n");
			if (reviser_power_off(drvinfo))
				LOG_ERR("Power OFF Fail\n");

			goto free_vlm;
		}
	} else {
		LOG_ERR("Power ON Fail\n");
		goto free_vlm;
	}

	LOG_DEBUG("[out] vlm page_num(%u) tcm_valid(%lx) ctxid(%lu)\n",
			vlm_pgtable.tcm_pgtable.page_num,
			vlm_pgtable.tcm_pgtable.table_tcm[0], ctxid);

	*tcm_size = vlm_pgtable.tcm_pgtable.page_num * VLM_BANK_SIZE;
	*id = ctxid;

	LOG_DEBUG("[out] CtxID(%lu) page_num(%u)\n",
			ctxid,
			vlm_pgtable.tcm_pgtable.page_num);
	return 0;

free_vlm:
	if (_reviser_clear_vlm_pgtable(drvinfo, ctxid,
			&vlm_pgtable.tcm_pgtable))
		LOG_ERR("Clear VLM PageTable fail\n");

free_tcm:
	if (reviser_table_free_tcm(drvinfo, &vlm_pgtable.tcm_pgtable))
		LOG_ERR("Free TCM fail\n");

free_ctxid:
	if (reviser_table_free_ctxID(drvinfo, ctxid))
		LOG_ERR("Free ctxID fail\n");

fail:
	*id = 0;
	*tcm_size = 0;
	return -1;
}

int reviser_table_free_vlm(void *drvinfo, uint32_t ctxid)
{
	struct table_tcm tcm_pgtable;
	int ret = 0;

	//LOG_DEBUG("free ctxid: %u\n", ctxid);
	if (ctxid >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		ret = -1;
		goto power_off;
	}
	if (reviser_table_clear_remap(drvinfo, ctxid)) {
		LOG_ERR("Clear Remap Fail\n");
		ret = -1;
		goto power_off;
	}

	//LOG_DEBUG("reviser_table_clear_remap done %d\n", ctxid);

	memset(&tcm_pgtable, 0, sizeof(struct table_tcm));
	if (_reviser_clear_vlm_pgtable(drvinfo, ctxid, &tcm_pgtable)) {
		LOG_ERR("Clear VLM PageTable Fail\n");
		ret = -1;
		goto power_off;
	}

	//LOG_DEBUG("_reviser_clear_vlm_pgtable ctxid(%d)\n", ctxid);

	if (reviser_table_free_tcm(drvinfo, &tcm_pgtable)) {
		LOG_ERR("Free TCM Fail\n");
		ret = -1;
		goto power_off;
	}
	//LOG_DEBUG("reviser_table_free_tcm ctxid(%d) page_num(%u)\n",
	//		ctxid, tcm_pgtable.page_num);

	if (reviser_table_free_ctxID(drvinfo, ctxid)) {
		LOG_ERR("Free ctxID Fail\n");
		ret = -1;
		goto power_off;
	}
	LOG_DEBUG("ctxid(%u) page_num(%u)\n",
			ctxid, tcm_pgtable.page_num);

power_off:
	if (reviser_power_off(drvinfo)) {
		LOG_ERR("Power OFF Fail\n");
		ret = -1;
	}
	return ret;
}

int reviser_table_init_remap(void *drvinfo)
{
	struct reviser_dev_info *reviser_device = NULL;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_remap);
	memset(&g_table_remap, 0, sizeof(struct table_remap));
	mutex_unlock(&reviser_device->mutex_remap);

	return 0;
}
int reviser_table_set_remap(void *drvinfo, unsigned long ctxid)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t setbits;
	unsigned long index = 0;
	uint32_t i;


	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_remap);
	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	setbits = bitmap_weight(g_table_remap.valid, VLM_REMAP_TABLE_MAX);
	//LOG_DEBUG(" setbits [%d]\n", setbits);

	if (VLM_REMAP_TABLE_MAX - setbits < g_vlm_pgtable[ctxid].sys_page_num) {

		LOG_ERR("Remap Zero bits (%d) > vlm[%lu] page[%d]\n",
				VLM_REMAP_TABLE_MAX - setbits,
				ctxid, g_vlm_pgtable[ctxid].sys_page_num);
		goto free_mutex;
	}

	index = 0;
	for (i = 0; i < g_vlm_pgtable[ctxid].sys_page_num; i++) {
		index = find_next_zero_bit(g_table_remap.valid,
				VLM_REMAP_TABLE_MAX, index);
		//LOG_DEBUG("Find Zero Bit index %lu!!\n", index);
		if (index == VLM_REMAP_TABLE_MAX) {
			LOG_ERR("CanNot Find Zero Bit!!\n");
			goto free_mutex;
		}

		g_table_remap.table_remap_mem[index].ctxid = ctxid;
		g_table_remap.table_remap_mem[index].src = i;
		g_table_remap.table_remap_mem[index].dst =
				g_vlm_pgtable[ctxid].page[i].dst;
		//Set to page table for clear VLM
		g_vlm_pgtable[ctxid].page[i].vlm = index;

		bitmap_set(g_table_remap.valid, index, 1);
		/* Set HW remap table */
		if (reviser_set_remap_table(drvinfo, index, 1,
				ctxid, i, g_vlm_pgtable[ctxid].page[i].dst)) {
			goto free_mutex;
		}

		index++;
	}
	/* DEBUG and force set remap to specific value*/
	_reviser_force_remap(drvinfo);
	//setbits = bitmap_weight(g_table_remap.valid, VLM_REMAP_TABLE_MAX);
	//LOG_DEBUG("Done setbits [%d]\n", setbits);

	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	mutex_unlock(&reviser_device->mutex_remap);

	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	mutex_unlock(&reviser_device->mutex_remap);

	return -1;
}
int reviser_table_clear_remap(void *drvinfo, unsigned long ctxid)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t setbits;
	unsigned long index = 0;
	uint32_t i;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (ctxid >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_remap);
	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	setbits = bitmap_weight(g_table_remap.valid, VLM_REMAP_TABLE_MAX);
	//LOG_DEBUG(" setbits [%d]\n", setbits);
	if (setbits < g_vlm_pgtable[ctxid].sys_page_num) {

		LOG_ERR("Remap (%u)[%lu][%u]\n",
				setbits, ctxid,
				g_vlm_pgtable[ctxid].sys_page_num);
		goto free_mutex;
	}

	index = 0;
	for (i = 0; i < g_vlm_pgtable[ctxid].sys_page_num; i++) {
		index = g_vlm_pgtable[ctxid].page[i].vlm;

		bitmap_clear(g_table_remap.valid, index, 1);

		if (reviser_set_remap_table(drvinfo,
				index, 0, ctxid, index, index))
			goto free_mutex;
	}
	/* DEBUG and force set remap to specific value*/
	_reviser_force_remap(drvinfo);

	LOG_DEBUG("ctxid [%lu]\n", ctxid);

	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	mutex_unlock(&reviser_device->mutex_remap);

	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	mutex_unlock(&reviser_device->mutex_remap);

	return -1;
}
void reviser_table_print_vlm(void *drvinfo, uint32_t ctxid, void *s_file)
{
	struct reviser_dev_info *reviser_device = NULL;
	uint32_t i;
	struct seq_file *s = (struct seq_file *)s_file;
	char strtype[8];
	int count = 0;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	if (ctxid >= VLM_REMAP_TABLE_MAX) {
		LOG_ERR("invalid argument\n");
		return;
	}

	reviser_device = (struct reviser_dev_info *)drvinfo;
	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " vlm [%d] page_num[%d]\n",
			ctxid, g_vlm_pgtable[ctxid].page_num);

	LOG_CON(s, "-----------------------------\n");
	for (i = 0; i < VLM_REMAP_TABLE_MAX; i++) {
		switch (g_vlm_pgtable[ctxid].page[i].type) {
		case REVISER_MEM_TYPE_TCM:
			count = snprintf(strtype, sizeof(strtype), "TCM");
			break;
		case REVISER_MEM_TYPE_DRAM:
			count = snprintf(strtype, sizeof(strtype), "DRAM");
			break;
		default:
			count = snprintf(strtype, sizeof(strtype), "NONE");
			break;
		}

	if (count > 0)
		LOG_CON(s, "v[%d] src[%02d] dst[%02d] vlm[%02d] type[%s]\n",
				g_vlm_pgtable[ctxid].page[i].valid, i,
				g_vlm_pgtable[ctxid].page[i].dst,
				g_vlm_pgtable[ctxid].page[i].vlm, strtype);
	}
	LOG_CON(s, "=============================\n");

	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
}

int reviser_table_swapout_vlm(void *drvinfo, unsigned long ctxid)
{
	struct reviser_dev_info *reviser_device = NULL;
	void *buffer;
	size_t size;

	DEBUG_TAG;

	if (ctxid >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	// if there is tcm then swap
	if (g_vlm_pgtable[ctxid].tcm.page_num > 0) {
		size = VLM_BANK_SIZE * g_vlm_pgtable[ctxid].tcm.page_num;
		buffer = (void *) __get_free_pages(GFP_KERNEL, get_order(size));
		if (buffer == NULL) {
			LOG_ERR("failed to allocate 0x%lx buffer.\n", size);
			goto free_mutex;
		}

		memcpy_fromio(buffer, reviser_device->tcm_base, size);
		g_vlm_pgtable[ctxid].swap_addr = (uint64_t) buffer;

		LOG_DEBUG("Copy to kva %p\n", buffer);
		LOG_DEBUG("Copy to g_vlm_pgtable[%lu].swap_addr %llx\n",
				ctxid, g_vlm_pgtable[ctxid].swap_addr);
	} else {
		LOG_DEBUG("No TCM!\n");
	}



	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	return 0;

free_mutex:
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	return -1;

}
int reviser_table_swapin_vlm(void *drvinfo, unsigned long ctxid)
{
	struct reviser_dev_info *reviser_device = NULL;
	void *buffer;
	size_t size;

	DEBUG_TAG;

	if (ctxid >= VLM_CTXT_CTX_ID_MAX) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}
	reviser_device = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&reviser_device->mutex_vlm_pgtable);

	// if there is tcm then swap
	if (g_vlm_pgtable[ctxid].tcm.page_num > 0) {

		buffer = (void *) g_vlm_pgtable[ctxid].swap_addr;
		size = g_vlm_pgtable[ctxid].tcm.page_num * VLM_BANK_SIZE;
		memcpy_toio(reviser_device->tcm_base, buffer, size);

		g_vlm_pgtable[ctxid].swap_addr = 0;
		free_pages((unsigned long)buffer, get_order(size));

		LOG_DEBUG("Restore kva %p to context %lu\n2", buffer, ctxid);
	} else {
		LOG_DEBUG("No TCM!\n");
	}



	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	return 0;



}
