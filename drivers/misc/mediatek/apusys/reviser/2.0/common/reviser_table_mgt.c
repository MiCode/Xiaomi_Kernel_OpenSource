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


#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_table_mgt.h"
#include "reviser_power.h"
#include "reviser_hw_mgt.h"

//ONLY FOR DEBUGGING
#define _FORCE_SET_REMAP 0


//static unsigned long pgtable_dram[BITS_TO_LONGS(VLM_DRAM_BANK_MAX)];
//static unsigned long table_tcm[BITS_TO_LONGS(TABLE_TCM_MAX)];
// TCM table
static unsigned long *g_table_tcm;
// TCM table size
static unsigned long g_tcm_size;
// TCM Max Bits
static unsigned long g_tcm_nbits;
// TCM Free
static uint32_t g_tcm_free;

// ctx table size
//static unsigned long g_ctx_size[BITS_TO_LONGS(TABLE_CTXID_MAX)];
static unsigned long *g_table_ctx;
// ctx table size
static unsigned long g_ctx_size;
// ctx Max Bits
static unsigned long g_ctx_nbits;
// ctx free
static bool g_ctx_empty;

// vlm page max number
static unsigned long g_ctxpgt_max;
// bank per vlm
static unsigned long g_bank_max;
//static struct vlm_pgtable g_vlm_pgtable[VLM_CTXT_CTX_ID_MAX];
static struct ctx_pgt *g_ctx_pgt;


static struct rmp_table g_rmp;
static unsigned long g_rmp_valid;
// remap rule table size
static unsigned long g_rmp_size;
// ctx Max Bits
static unsigned long g_rmp_nbits;

static int _reviser_set_ctx_pgt(void *drvinfo,
		unsigned long ctx, struct pgt_vlm *pgt_vlm);
static int _reviser_clear_ctx_pgt(void *drvinfo,
		unsigned long ctx, struct pgt_vlm *pgt_vlm);
static int _reviser_force_remap(void *drvinfo);
static int _reviser_alloc_pgt_vlm(struct pgt_vlm *pgt_vlm);
static int _reviser_free_pgt_vlm(struct pgt_vlm *pgt_vlm);
static int _reviser_copy_pgt_vlm(struct pgt_vlm *dst, struct pgt_vlm *src);
static int _reviser_clear_pgt_vlm(struct pgt_vlm *pgt_vlm);
static int _reviser_alloc_pgt(struct pgt_tcm *pgt, unsigned long nbits);
static int _reviser_free_pgt(struct pgt_tcm *pgt);
static int _reviser_copy_pgt(struct pgt_tcm *dst, struct pgt_tcm *src, unsigned long nbits);
static int _reviser_clear_pgt(struct pgt_tcm *pgt, unsigned long nbits);
static int _reviser_clear_bank_vlm(struct bank_vlm *bank);

static int _reviser_force_remap(void *drvinfo)
{
	int ret = 0;
/* Debug ONLY */
#if _FORCE_SET_REMAP

	int valid = 1;
	int ctx = 1;

	/* Set HW remap table */
	if (reviser_mgt_set_rmp(drvinfo, 0, valid, ctx, 0, 0)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 1, valid, ctx, 1, 1)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 2, valid, ctx, 2, 2)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 3, valid, ctx, 3, 3)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 4, valid, ctx, 0, 1)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 5, valid, ctx, 1, 2)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 6, valid, ctx, 2, 3)) {
		ret = -1;
		goto out;
	}
	if (reviser_mgt_set_rmp(drvinfo, 7, valid, ctx, 3, 0)) {
		ret = -1;
		goto out;
	}
out:
#endif

	return ret;
}
static int _reviser_alloc_pgt(struct pgt_tcm *pgt, unsigned long nbits)
{
	unsigned long size = 0;

	pgt->page_num = 0;
	size = BITS_TO_LONGS(nbits);
	//LOG_DBG_RVR_TBL("size %x\n", size);
	if (!size) {
		//Set Min size to avoid access zero length array
		//LOG_ERR("size is Zero\n");
		pgt->pgt = NULL;
		return 0;
	}

	pgt->pgt = kcalloc(size, sizeof(unsigned long), GFP_KERNEL);
	if (!pgt->pgt) {
		LOG_ERR("allocate fail\n");
		return -ENOMEM;
	}

	return 0;
}
static int _reviser_free_pgt(struct pgt_tcm *pgt)
{
	//kfree(NULL) is safe
	kfree(pgt->pgt);
	pgt->page_num = 0;

	return 0;
}

static int _reviser_copy_pgt(struct pgt_tcm *dst, struct pgt_tcm *src, unsigned long nbits)
{
	unsigned long size = 0;

	if (!nbits) {
		dst->page_num = src->page_num;
		return 0;
	}

	if (!dst->pgt) {
		LOG_ERR("invalid dst pgt\n");
		return -EINVAL;
	}
	if (!src->pgt) {
		LOG_ERR("invalid src pgt\n");
		return -EINVAL;
	}

	size = BITS_TO_LONGS(nbits);
	LOG_DBG_RVR_TBL("size %x\n", size);

	//dst->pgt[0] = src->pgt[0];
	if (size)
		memcpy(dst->pgt, src->pgt, sizeof(unsigned long) * size);

	dst->page_num = src->page_num;

	return 0;
}

static int _reviser_clear_pgt(struct pgt_tcm *pgt, unsigned long nbits)
{
	unsigned long size = 0;

	size = BITS_TO_LONGS(nbits);
	//LOG_DBG_RVR_TBL("size %x\n", size);
	if (size)
		memset(pgt->pgt, 0, sizeof(unsigned long) * size);
	pgt->page_num = 0;

	return 0;
}
static int _reviser_alloc_pgt_vlm(struct pgt_vlm *pgt_vlm)
{
	int ret = 0;

	pgt_vlm->page_num = 0;
	pgt_vlm->sys_num = 0;
	ret = _reviser_alloc_pgt(&pgt_vlm->tcm, g_tcm_nbits);

	return ret;
}
static int _reviser_free_pgt_vlm(struct pgt_vlm *pgt_vlm)
{
	int ret = 0;

	pgt_vlm->sys_num = 0;
	pgt_vlm->page_num = 0;

	ret = _reviser_free_pgt(&pgt_vlm->tcm);

	return ret;
}


static int _reviser_copy_pgt_vlm(struct pgt_vlm *dst, struct pgt_vlm *src)
{
	//Copy tcm
	_reviser_copy_pgt(&dst->tcm, &src->tcm, g_tcm_nbits);

	dst->page_num = src->page_num;
	dst->sys_num = src->sys_num;
	return 0;
}

static int _reviser_clear_pgt_vlm(struct pgt_vlm *pgt_vlm)
{

	pgt_vlm->page_num = 0;
	pgt_vlm->sys_num = 0;
	_reviser_clear_pgt(&pgt_vlm->tcm, g_tcm_nbits);

	return 0;
}
static int _reviser_clear_bank_vlm(struct bank_vlm *bank)
{
	//Clear All bank
	memset(bank, 0, sizeof(struct bank_vlm) * g_bank_max);
	return 0;
}

int reviser_table_init_ctx(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx);

	g_ctx_nbits = rdv->plat.ctx_max;
	g_ctx_size = BITS_TO_LONGS(g_ctx_nbits);
	g_table_ctx = kcalloc(g_ctx_size, sizeof(unsigned long), GFP_KERNEL);
	if (!g_table_ctx) {
		LOG_ERR("kcalloc fail g_table_ctx\n");
		ret = -ENOMEM;
		goto free_mutex;
	}

	g_ctx_empty = false;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx);
	return ret;
}
int reviser_table_uninit_ctx(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx);
	kfree(g_table_ctx);
	g_ctx_nbits = 0;
	g_ctx_size = 0;
	g_ctx_empty = false;

	mutex_unlock(&rdv->lock.mutex_ctx);

	return 0;
}
int reviser_table_get_ctx_sync(void *drvinfo, unsigned long *ctx)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	while (1) {
		if (reviser_table_get_ctx(drvinfo, ctx)) {
			LOG_DBG_RVR_TBL("Wait for Getting ctx\n");
			wait_event_interruptible(rdv->lock.wait_ctx,
					!g_ctx_empty);
		} else {
			break;
		}
	}
	//LOG_DBG_RVR_TBL("Sync Get ctx %lu\n", *ctx);

	return 0;
}

int reviser_table_get_ctx(void *drvinfo, unsigned long *ctx)
{
	unsigned long fist_zero = 0;
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;


	mutex_lock(&rdv->lock.mutex_ctx);

	fist_zero = find_first_zero_bit(g_table_ctx, g_ctx_nbits);
	if (fist_zero < g_ctx_nbits) {
		bitmap_set(g_table_ctx, fist_zero, 1);

		*ctx = fist_zero;
	} else {
		LOG_ERR("No free ctx %lu\n", fist_zero);
		g_ctx_empty = true;
		ret = -EBUSY;
		goto free_mutex;
	}
	LOG_DBG_RVR_TBL("[out] ctx(%lu) g_table_ctx(%08lx)\n",
			*ctx, g_table_ctx[0]);

	mutex_unlock(&rdv->lock.mutex_ctx);


	return 0;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx);
	return ret;
}

int reviser_table_free_ctx(void *drvinfo, unsigned long ctx)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (ctx >= g_ctx_nbits) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;


	mutex_lock(&rdv->lock.mutex_ctx);

	if (ctx < g_ctx_nbits) {

		bitmap_clear(g_table_ctx, ctx, 1);
		g_ctx_empty = false;
		//LOG_DBG_RVR_TBL("Clear table for ctx %lu\n", ctx);
		wake_up_interruptible(&rdv->lock.wait_ctx);
	} else {
		LOG_ERR("Out of range %lu\n", ctx);
		ret = -EINVAL;
		goto free_mutex;
	}
	LOG_DBG_RVR_TBL("[in] ctx(%lu) [out] g_table_ctx(%08lx)\n"
			, ctx, g_table_ctx[0]);

	mutex_unlock(&rdv->lock.mutex_ctx);

	return 0;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx);
	return ret;
}
void reviser_table_print_ctx(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t i;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}


	rdv = (struct reviser_dev_info *)drvinfo;
	mutex_lock(&rdv->lock.mutex_ctx);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " Contex Table\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < BITS_TO_LONGS(g_ctx_nbits); i++)
		LOG_CON(s, "%d: [%lx]\n", i, g_table_ctx[i]);

	LOG_CON(s, "=============================\n");

	mutex_unlock(&rdv->lock.mutex_ctx);
}


int reviser_table_init_tcm(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_tcm);

	g_tcm_nbits = rdv->plat.pool_bank_max[0];
	g_tcm_size = BITS_TO_LONGS(g_tcm_nbits);


	if (g_tcm_nbits != 0) {
		g_table_tcm = kcalloc(g_tcm_size, sizeof(unsigned long), GFP_KERNEL);
		if (!g_table_tcm) {
			LOG_ERR("kcalloc fail g_table_tcm\n");
			ret = -ENOMEM;
			goto free_mutex;
		}
		bitmap_zero(g_table_tcm, g_tcm_nbits);
	}

	g_tcm_free = g_tcm_nbits;
free_mutex:
	mutex_unlock(&rdv->lock.mutex_tcm);

	return ret;
}
int reviser_table_uninit_tcm(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_tcm);


	kfree(g_table_tcm);
	g_tcm_free = 0;
	g_tcm_size = 0;
	g_tcm_nbits = 0;

	mutex_unlock(&rdv->lock.mutex_tcm);

	return 0;
}

int reviser_table_get_tcm_sync(void *drvinfo,
		uint32_t page_num, struct pgt_tcm *pgt_tcm)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (page_num > g_tcm_nbits) {
		LOG_ERR("invalid page_num %d\n", page_num);
		return -EINVAL;
	}
	if (pgt_tcm == NULL) {
		LOG_ERR("invalid pgt_tcm\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	while (1) {
		if (reviser_table_get_tcm(drvinfo, page_num, pgt_tcm)) {
			LOG_DBG_RVR_TBL("Wait for Getting tcm\n");
			wait_event_interruptible(rdv->lock.wait_tcm,
					g_tcm_free >= page_num);
		} else {
			break;
		}
	}

	return 0;
}

int reviser_table_get_tcm(void *drvinfo,
		uint32_t page_num, struct pgt_tcm *pgt_tcm)
{
	unsigned long fist_zero = 0;
	struct reviser_dev_info *rdv = NULL;
	uint32_t i;
	unsigned long setbits = 0;


	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;



	mutex_lock(&rdv->lock.mutex_tcm);

	if (g_tcm_free == 0) {
		LOG_DBG_RVR_TBL("No free TCM (%u/%u)\n",
				page_num, g_tcm_free);
		pgt_tcm->page_num = 0;
		goto free_mutex;
	}

	setbits = bitmap_weight(g_table_tcm, g_tcm_nbits);
	//LOG_DBG_RVR_TBL("setbits %lu\n", setbits);
	if (g_tcm_nbits - setbits < page_num) {
		LOG_DBG_RVR_TBL("No free page (%u/%lu)\n",
				page_num, g_tcm_nbits - setbits);
		pgt_tcm->page_num = 0;
		goto free_mutex;
	}

	for (i = 0; i < page_num; i++) {
		fist_zero = find_first_zero_bit(g_table_tcm, g_tcm_nbits);
		if (fist_zero < g_tcm_nbits) {
			bitmap_set(g_table_tcm, fist_zero, 1);
			bitmap_set(pgt_tcm->pgt, fist_zero, 1);
			g_tcm_free--;
			pgt_tcm->page_num++;

		}
	}

	LOG_DBG_RVR_TBL("[in] pg(%u) [out] g_tcm(%lx) g_tcm_free(%d) tcm_pgtb(%lx)\n",
			page_num,
			g_table_tcm[0], g_tcm_free, pgt_tcm->pgt[0]);
	mutex_unlock(&rdv->lock.mutex_tcm);


	return 0;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_tcm);
	return -1;
}

int reviser_table_free_tcm(void *drvinfo, struct pgt_tcm *pgt_tcm)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (pgt_tcm == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (pgt_tcm->page_num == 0) {
		LOG_DBG_RVR_TBL("[in] pg(%u)\n",
				pgt_tcm->page_num);
		return 0;
	}
	rdv = (struct reviser_dev_info *)drvinfo;




	mutex_lock(&rdv->lock.mutex_tcm);


	if (pgt_tcm->page_num <= g_tcm_nbits) {

		bitmap_andnot(g_table_tcm, g_table_tcm, pgt_tcm->pgt,
				g_tcm_nbits);
		g_tcm_free = g_tcm_free + pgt_tcm->page_num;
		wake_up_interruptible(&rdv->lock.wait_tcm);
	} else {
		LOG_ERR("Out of range %u\n", pgt_tcm->page_num);
		ret = -EINVAL;
		goto free_mutex;
	}

	LOG_DBG_RVR_TBL("[in] pg(%u) [out] g_tcm(%lx) g_tcm_free(%d)\n",
			pgt_tcm->page_num,
			g_table_tcm[0], g_tcm_free);

	mutex_unlock(&rdv->lock.mutex_tcm);


	return ret;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_tcm);
	return ret;
}

void reviser_table_print_tcm(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	uint32_t i;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}


	rdv = (struct reviser_dev_info *)drvinfo;
	mutex_lock(&rdv->lock.mutex_tcm);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " TCM Table\n");
	LOG_CON(s, "-----------------------------\n");

	for (i = 0; i < BITS_TO_LONGS(g_tcm_nbits); i++)
		LOG_CON(s, "%d: [%lx]\n", i, g_table_tcm[i]);

	LOG_CON(s, "=============================\n");

	mutex_unlock(&rdv->lock.mutex_tcm);
}

int reviser_table_init_ctx_pgt(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int i = 0;
	int ret = 0;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	g_ctxpgt_max = rdv->plat.ctx_max;
	g_bank_max = rdv->plat.vlm_bank_max;
	g_ctx_pgt = kcalloc(g_ctxpgt_max, sizeof(struct ctx_pgt), GFP_KERNEL);
	if (!g_ctx_pgt) {
		LOG_ERR("kcalloc fail g_ctx_pgt\n");
		ret = -ENOMEM;
		goto free_mutex;
	}
	for (i = 0; i < g_ctxpgt_max; i++) {
		g_ctx_pgt[i].bank = kcalloc(g_bank_max, sizeof(struct bank_vlm), GFP_KERNEL);
		if (!g_ctx_pgt[i].bank) {
			LOG_ERR("kcalloc fail g_ctx_pgt\n");
			ret = -ENOMEM;
			goto free_mutex;
		}
		if (_reviser_alloc_pgt_vlm(&g_ctx_pgt[i].vlm)) {
			LOG_ERR("_reviser_alloc_pgt_vlm fail\n");
			ret = -ENOMEM;
			goto free_mutex;
		}
	}


	rdv->pvlm = g_ctx_pgt;
free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx_pgt);

	return ret;
}
int reviser_table_uninit_ctx_pgt(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int i = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	for (i = 0; i < g_ctxpgt_max; i++) {
		_reviser_free_pgt_vlm(&g_ctx_pgt[i].vlm);
		kfree(g_ctx_pgt[i].bank);
	}


	kfree(g_ctx_pgt);
	g_ctxpgt_max = 0;

	mutex_unlock(&rdv->lock.mutex_ctx_pgt);

	return 0;
}
static int _reviser_set_ctx_pgt(void *drvinfo,
		unsigned long ctx, struct pgt_vlm *pgt_vlm)
{
	struct reviser_dev_info *rdv = NULL;
	unsigned long index = 0;
	uint32_t page_num = 0;

	uint32_t i;


	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (ctx >= g_ctxpgt_max) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}


	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	/* Set TCM Info */
	for (i = 0; i < pgt_vlm->tcm.page_num; i++) {
		index = find_next_bit(pgt_vlm->tcm.pgt,
				g_tcm_nbits, index);
		LOG_DBG_RVR_TBL("Find Bit index %lu!!\n", index);
		g_ctx_pgt[ctx].bank[i].type = REVISER_MEM_TYPE_TCM;
		g_ctx_pgt[ctx].bank[i].dst = index;

		index++;
		page_num++;
	}
	/* Set Dram Info */
	for (i = page_num; i < rdv->plat.vlm_bank_max; i++) {
		g_ctx_pgt[ctx].bank[i].type = REVISER_MEM_TYPE_DRAM;
		g_ctx_pgt[ctx].bank[i].dst = i;
	}

	/* Save and use when clear TCM*/
	_reviser_copy_pgt_vlm(&g_ctx_pgt[ctx].vlm, pgt_vlm);
	//memcpy(&g_ctx_pgt[ctx].vlm, pgt_vlm, sizeof(struct pgt_vlm));

	DEBUG_TAG;
	/*sys_page_num = tcm + mmsys(todo) */
	g_ctx_pgt[ctx].vlm.sys_num = g_ctx_pgt[ctx].vlm.tcm.page_num;

	DEBUG_TAG;
	LOG_DBG_RVR_TBL("[out] ctx(%lu) sys(%u) pg(%d) tcm_pg(%d)\n",
			ctx,
			g_ctx_pgt[ctx].vlm.sys_num,
			g_ctx_pgt[ctx].vlm.page_num,
			g_ctx_pgt[ctx].vlm.tcm.page_num
			);
	mutex_unlock(&rdv->lock.mutex_ctx_pgt);

	DEBUG_TAG;
	DEBUG_TAG;
	return 0;
}
static int _reviser_clear_ctx_pgt(void *drvinfo,
		unsigned long ctx, struct pgt_vlm *pgt_vlm)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (ctx >= g_ctxpgt_max) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (pgt_vlm == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}



	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	/* Return TCM page table for clearing TCM */
	_reviser_copy_pgt_vlm(pgt_vlm, &g_ctx_pgt[ctx].vlm);
	//memcpy(pgt_vlm, &g_ctx_pgt[ctx].vlm, sizeof(struct pgt_vlm));

	_reviser_clear_bank_vlm(g_ctx_pgt[ctx].bank);
	_reviser_clear_pgt_vlm(&g_ctx_pgt[ctx].vlm);

	LOG_DBG_RVR_TBL("ctx(%lu)\n", ctx);
	mutex_unlock(&rdv->lock.mutex_ctx_pgt);

	return 0;
}

int reviser_table_get_vlm(void *drvinfo,
		uint32_t requset_size, bool force,
		unsigned long *id, uint32_t *tcm_size)
{
	unsigned long ctx;
	struct pgt_vlm pgt_vlm;
	struct reviser_dev_info *rdv = NULL;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	_reviser_alloc_pgt_vlm(&pgt_vlm);
	if (requset_size > rdv->plat.vlm_size) {
		LOG_ERR("requset_size(%x)/(%x) is too larger\n",
				requset_size, rdv->plat.vlm_size);
		goto fail;
	}

	pgt_vlm.page_num = DIV_ROUND_UP(requset_size, rdv->plat.bank_size);
	LOG_DBG_RVR_TBL("[in] requset_size(%x) page_num(%u) force(%d)\n",
			requset_size, pgt_vlm.page_num, force);

	if (reviser_table_get_ctx_sync(drvinfo, &ctx)) {
		LOG_ERR("Get CTX ID Fail\n");
		goto fail;
	}

	//LOG_DBG_RVR_TBL("ctx: %lu\n", ctx);
	if (g_tcm_nbits == 0) {
		LOG_DBG_RVR_TBL("Force set to false because TCM is zero\n");
		force = false;
	}

	if (force) {
		if (reviser_table_get_tcm_sync(drvinfo,
				pgt_vlm.page_num,
				&pgt_vlm.tcm)) {
			LOG_ERR("Force Get TCM Fail\n");
			goto free_ctx;
		}

	} else {
		//Get TCM fail , all page to DRAM
		//To Do List: Fragmentation , may return max available TCM page?
		if (reviser_table_get_tcm(drvinfo,
				pgt_vlm.page_num,
				&pgt_vlm.tcm)) {
			LOG_DBG_RVR_TBL("Use Dram ctx %lu\n", ctx);
		}
	}

	if (_reviser_set_ctx_pgt(drvinfo, ctx, &pgt_vlm)) {
		LOG_ERR("Set VLM Page Table Fail\n");
		goto free_tcm;
	}

	if (!reviser_power_on(drvinfo)) {
		/* Set HW remap table */
		if (reviser_table_set_remap(drvinfo, ctx)) {
			LOG_ERR("Set Remap Fail and power off\n");
			if (reviser_power_off(drvinfo))
				LOG_ERR("Power OFF Fail\n");

			goto free_vlm;
		}
	} else {
		LOG_ERR("Power ON Fail\n");
		goto free_vlm;
	}

	LOG_DBG_RVR_TBL("[out] vlm page_num(%u) ctx(%lu)\n",
			pgt_vlm.tcm.page_num,
			ctx);

	*tcm_size = pgt_vlm.tcm.page_num * rdv->plat.bank_size;
	*id = ctx;

	LOG_DBG_RVR_TBL("[out] ctx(%lu) page_num(%u)\n",
			ctx,
			pgt_vlm.tcm.page_num);

	_reviser_free_pgt_vlm(&pgt_vlm);
	return 0;

free_vlm:
	if (_reviser_clear_ctx_pgt(drvinfo, ctx,
			&pgt_vlm))
		LOG_ERR("Clear VLM PageTable fail\n");

free_tcm:
	if (reviser_table_free_tcm(drvinfo, &pgt_vlm.tcm))
		LOG_ERR("Free TCM fail\n");

free_ctx:
	if (reviser_table_free_ctx(drvinfo, ctx))
		LOG_ERR("Free ctx fail\n");

fail:
	*id = 0;
	*tcm_size = 0;
	_reviser_free_pgt_vlm(&pgt_vlm);
	return -1;
}

int reviser_table_free_vlm(void *drvinfo, uint32_t ctx)
{
	struct pgt_vlm pgt_vlm;
	int ret = 0;

	_reviser_alloc_pgt_vlm(&pgt_vlm);
	//LOG_DBG_RVR_TBL("free ctx: %u\n", ctx);
	if (ctx >= g_ctxpgt_max) {
		LOG_ERR("invalid argument\n");
		ret = -EINVAL;
		goto power_off;
	}
	if (reviser_table_clear_remap(drvinfo, ctx)) {
		LOG_ERR("Clear Remap Fail\n");
		ret = -EINVAL;
		goto power_off;
	}

	//LOG_DBG_RVR_TBL("reviser_table_clear_remap done %d\n", ctx);
	if (_reviser_clear_ctx_pgt(drvinfo, ctx, &pgt_vlm)) {
		LOG_ERR("Clear VLM PageTable Fail\n");
		ret = -1;
		goto power_off;
	}

	//LOG_DBG_RVR_TBL("_reviser_clear_vlm_pgtable ctx(%d)\n", ctx);

	if (reviser_table_free_tcm(drvinfo, &pgt_vlm.tcm)) {
		LOG_ERR("Free TCM Fail\n");
		ret = -1;
		goto power_off;
	}

	if (reviser_table_free_ctx(drvinfo, ctx)) {
		LOG_ERR("Free ctx Fail\n");
		ret = -1;
		goto power_off;
	}
	LOG_DBG_RVR_TBL("ctx(%u) page_num(%u)\n",
			ctx, pgt_vlm.tcm.page_num);

power_off:
	if (reviser_power_off(drvinfo)) {
		LOG_ERR("Power OFF Fail\n");
		ret = -1;
	}
	_reviser_free_pgt_vlm(&pgt_vlm);

	return ret;
}

int reviser_table_init_remap(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	int ret = 0;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_remap);

	g_rmp_valid = 0;
	g_rmp_nbits = rdv->plat.rmp_max;
	g_rmp_size = BITS_TO_LONGS(g_rmp_nbits);
	g_rmp.remap = kcalloc(g_rmp_nbits, sizeof(struct rmp_rule), GFP_KERNEL);
	if (!g_rmp.remap) {
		LOG_ERR("kcalloc fail g_rmp.remap\n");
		ret = -ENOMEM;
		goto free_mutex;
	}
	g_rmp.valid = kcalloc(g_rmp_size, sizeof(unsigned long), GFP_KERNEL);
	if (!g_rmp.valid) {
		LOG_ERR("kcalloc fail g_rmp.valid\n");
		ret = -ENOMEM;
		goto free_mutex;
	}

free_mutex:
	mutex_unlock(&rdv->lock.mutex_remap);
	return ret;
}
int reviser_table_uninit_remap(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_remap);
	g_rmp_valid = 0;
	g_rmp_size = 0;
	g_rmp_nbits = 0;
	kfree(g_rmp.valid);
	kfree(g_rmp.remap);

	mutex_unlock(&rdv->lock.mutex_remap);

	return 0;
}
int reviser_table_set_remap(void *drvinfo, unsigned long ctx)
{
	struct reviser_dev_info *rdv = NULL;
	unsigned long index = 0;
	uint32_t i;


	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_remap);
	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	if (g_ctx_pgt[ctx].vlm.sys_num + g_rmp_valid > g_rmp_nbits) {

		LOG_ERR("sys_num (%lu) g_rmp_valid(%lu) is large than Max [%lu]\n",
				g_ctx_pgt[ctx].vlm.sys_num,
				g_rmp_valid, g_rmp_nbits);
		goto free_mutex;
	}

	for (i = 0; i < g_ctx_pgt[ctx].vlm.sys_num; i++) {
		index = find_next_zero_bit(g_rmp.valid,
				g_rmp_nbits, index);
		//LOG_DBG_RVR_TBL("Find Zero Bit index %lu!!\n", index);
		if (index == g_rmp_nbits) {
			LOG_ERR("CanNot Find Zero Bit!!\n");
			goto free_mutex;
		}
		/* Set HW remap table */
		if (reviser_mgt_set_rmp(drvinfo, index, 1,
				ctx, i, g_ctx_pgt[ctx].bank[i].dst)) {
			goto free_mutex;
		}

		g_rmp.remap[index].ctx = ctx;
		g_rmp.remap[index].src = i;
		g_rmp.remap[index].dst =
				g_ctx_pgt[ctx].bank[i].dst;
		//Set to page table for clear VLM
		g_ctx_pgt[ctx].bank[i].vlm = index;

		bitmap_set(g_rmp.valid, index, 1);
		g_rmp_valid++;
		index++;
	}
	/* DEBUG and force set remap to specific value*/
	_reviser_force_remap(drvinfo);

	mutex_unlock(&rdv->lock.mutex_ctx_pgt);
	mutex_unlock(&rdv->lock.mutex_remap);

	return 0;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx_pgt);
	mutex_unlock(&rdv->lock.mutex_remap);

	return -1;
}
int reviser_table_clear_remap(void *drvinfo, unsigned long ctx)
{
	struct reviser_dev_info *rdv = NULL;
	unsigned long index = 0;
	uint32_t i;

	DEBUG_TAG;
	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	if (ctx >= g_ctxpgt_max) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	mutex_lock(&rdv->lock.mutex_remap);
	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	if (g_rmp_valid < g_ctx_pgt[ctx].vlm.sys_num) {
		LOG_ERR("Clear fail(%u)[%lu][%u]\n",
				g_rmp_valid, ctx,
				g_ctx_pgt[ctx].vlm.sys_num);
		goto free_mutex;
	}
	for (i = 0; i < g_ctx_pgt[ctx].vlm.sys_num; i++) {
		index = g_ctx_pgt[ctx].bank[i].vlm;

		if (reviser_mgt_set_rmp(drvinfo,
				index, 0, ctx, index, index))
			goto free_mutex;

		bitmap_clear(g_rmp.valid, index, 1);
		g_rmp_valid--;
	}

	/* DEBUG and force set remap to specific value*/
	_reviser_force_remap(drvinfo);

	LOG_DBG_RVR_TBL("ctx [%lu]\n", ctx);

	mutex_unlock(&rdv->lock.mutex_ctx_pgt);
	mutex_unlock(&rdv->lock.mutex_remap);

	return 0;

free_mutex:
	mutex_unlock(&rdv->lock.mutex_ctx_pgt);
	mutex_unlock(&rdv->lock.mutex_remap);

	return -1;
}
void reviser_table_print_vlm(void *drvinfo, uint32_t ctx, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint32_t i;
	struct seq_file *s = (struct seq_file *)s_file;
	char strtype[8];
	int ret;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	if (ctx >= rdv->plat.ctx_max) {
		LOG_ERR("invalid argument\n");
		return;
	}

	mutex_lock(&rdv->lock.mutex_ctx_pgt);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " vlm [%d] page_num[%d]\n",
			ctx, g_ctx_pgt[ctx].vlm.page_num);

	LOG_CON(s, "-----------------------------\n");
	for (i = 0; i < rdv->plat.vlm_bank_max; i++) {
		switch (g_ctx_pgt[ctx].bank[i].type) {
		case REVISER_MEM_TYPE_TCM:
			ret = snprintf(strtype, sizeof(strtype), "TCM");
			if (ret < 0) {
				LOG_ERR("snprintf fail\n");
				return;
			}
			break;
		case REVISER_MEM_TYPE_DRAM:
			ret = snprintf(strtype, sizeof(strtype), "DRAM");
			if (ret < 0) {
				LOG_ERR("snprintf fail\n");
				return;
			}
			break;
		default:
			ret = snprintf(strtype, sizeof(strtype), "NONE");
			if (ret < 0) {
				LOG_ERR("snprintf fail\n");
				return;
			}
			break;
		}

		LOG_CON(s, "src[%02d] dst[%02d] vlm[%02d] type[%s]\n",
				i,
				g_ctx_pgt[ctx].bank[i].dst,
				g_ctx_pgt[ctx].bank[i].vlm, strtype);
	}
	LOG_CON(s, "=============================\n");

	mutex_unlock(&rdv->lock.mutex_ctx_pgt);
}


int reviser_table_init(void *drvinfo)
{
	int ret = 0;
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	if (reviser_table_init_ctx(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_init_tcm(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_init_ctx_pgt(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_init_remap(rdv)) {
		ret = -EINVAL;
		goto out;
	}


out:
	return ret;
}
int reviser_table_uninit(void *drvinfo)
{
	int ret = 0;
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	if (reviser_table_uninit_ctx(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_uninit_tcm(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_uninit_ctx_pgt(rdv)) {
		ret = -EINVAL;
		goto out;
	}
	if (reviser_table_uninit_remap(rdv)) {
		ret = -EINVAL;
		goto out;
	}


out:
	return ret;
}

int reviser_table_get_pool_index(uint32_t type, uint32_t *index)
{
	int ret = 0;
	uint32_t id = 0;

	switch (type) {
	case REVISER_MEM_TYPE_TCM:
		id = REVSIER_POOL_TCM;
		break;
	case REVISER_MEM_TYPE_SLBS:
		id = REVSIER_POOL_SLBS;
		break;
	default:
		LOG_ERR("type not found %u\n", type);
		ret = -EINVAL;
		break;
	}

	*index = id;

	return ret;
}
