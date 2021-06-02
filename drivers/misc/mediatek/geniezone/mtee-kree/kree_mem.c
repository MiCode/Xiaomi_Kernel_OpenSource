/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <kree/mem.h>
#include <kree/system.h>
#include <tz_cross/ta_mem.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define EACH_MAP_ENTRY_SIZE sizeof(struct KREE_SHM_RUNLENGTH_ENTRY)

#define MAX_PA_ENTRY (256 / EACH_MAP_ENTRY_SIZE)
#define MAX_MARY_SIZE MAX_PA_ENTRY
#define MAX_NUM_OF_PARAM 3

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
#define KREE_DEBUG(fmt...) pr_debug("[KREE_MEM]" fmt)
#define KREE_INFO(fmt...) pr_info("[KREE_MEM]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE_MEM][ERR]" fmt)
#else
#define KREE_DEBUG(fmt...)
#define KREE_INFO(fmt...) pr_info("[KREE_MEM]" fmt)
#define KREE_ERR(fmt...) pr_info("[KREE_MEM][ERR]" fmt)
#endif

DEFINE_MUTEX(shared_mem_mutex_trusty);
DEFINE_MUTEX(shared_mem_mutex_nebula);
DEFINE_MUTEX(chmem_mutex);
DEFINE_MUTEX(chmem_mutex_alloc);
DEFINE_MUTEX(chmem_mutex_unref);


/*Translate mem_handle to ION_handle*/
/*If this is optimized. set IONHandle_Transfer:0*/
/*IONHandle_Transfer=1, as usual*/
#define IONHandle_Transfer 1 /*1:ION_handle opt not impl */

#if IONHandle_Transfer

#define MAX_TEST_CHM_SIZE (320 * 1024 * 1024)   /*320MB */
#define MIN_TEST_ALLOC_CHM_SIZE 4096            /*4KB */
#define t_size (MAX_TEST_CHM_SIZE / MIN_TEST_ALLOC_CHM_SIZE)
struct ION_MEM_TABLE {
	KREE_SECUREMEM_HANDLE memHandle;
	KREE_ION_HANDLE ionHandle;
};

struct ION_MEM_TABLE ion_mem_handleAry[t_size]; /*~640KB */

#endif

/*only for code separation, don't update*/
#define API_sharedMem 1
#define API_secureMem 1
#define API_chunkMem  1
#define API_NotSupport 1
/******/

#if API_sharedMem
static void add_shm_list_node(struct KREE_SHM_RUNLENGTH_LIST **tail,
	uint64_t start, uint32_t size)
{
	struct KREE_SHM_RUNLENGTH_LIST *mylist;

	if (!tail)
		return;

	/* add a record */
	mylist = kmalloc(sizeof(*mylist), GFP_KERNEL);
	if (!mylist) {
		KREE_ERR("[%s] kmalloc failed!\n", __func__);
		return;
	}
	mylist->entry.low = (uint32_t) (start & (0x00000000ffffffff));
	mylist->entry.high = (uint32_t) (start >> 32);
	mylist->entry.size = size;
	mylist->next = NULL;
	(*tail)->next = mylist;
	(*tail) = mylist;
}

static struct KREE_SHM_RUNLENGTH_ENTRY *shmem_param_run_length_encoding(
	int numOfPA, int *runLeng_arySize, uint64_t *ary)
{
	int arySize = numOfPA + 1;
	int i = 0;
	uint64_t start;
	uint64_t now, next;
	uint32_t size = 1;
	int xx = 0;
	int idx = 0;

	struct KREE_SHM_RUNLENGTH_LIST *head, *tail;
	struct KREE_SHM_RUNLENGTH_LIST *curr;
	struct KREE_SHM_RUNLENGTH_ENTRY *runLengAry = NULL;

	/* compress by run length coding */
	KREE_DEBUG("[%s] numOfPA=%d, runLeng_arySize=%d\n",
		__func__, numOfPA, *runLeng_arySize);

	head = kmalloc(sizeof(*head), GFP_KERNEL);
	if (!head) {
		KREE_ERR("[%s] kmalloc failed!\n", __func__);
		return runLengAry;
	}
	head->next = NULL;
	tail = head;

	for (i = 1; i < arySize; i++) {
		now = ary[i];

		if (size == 1)
			start = now;

		if ((i + 1) < arySize) {

			next = ary[i + 1];
			if ((next - now) == (1 * PAGE_SIZE)) {
				size++;
			} else {
				add_shm_list_node(&tail, start, size);
				size = 1;	/* reset */
				xx++;
			}

		} else if (i == (arySize - 1)) {
			/* process the last entry */
			if ((ary[i] - start + (1 * PAGE_SIZE))
				== (size * PAGE_SIZE)) {

				add_shm_list_node(&tail, start, size);
				size = 1;	/* reset */
				xx++;
			}
		}
	}

	*runLeng_arySize = xx;
	KREE_DEBUG("[%s]runLeng_arySize = %d\n", __func__, *runLeng_arySize);

	runLengAry = kmalloc((*runLeng_arySize + 1)
			* sizeof(struct KREE_SHM_RUNLENGTH_ENTRY),
			GFP_KERNEL);
	if (!runLengAry) {
		KREE_ERR("[%s] kmalloc failed!\n", __func__);
		kfree(head);
		return runLengAry;
	}

	runLengAry[0].high = numOfPA;
	runLengAry[0].low = *runLeng_arySize;
	runLengAry[0].size = 0x0;

	idx = 1;

	if (head->next) {
		curr = head->next;
		while (curr) {
			runLengAry[idx].high = curr->entry.high;
			runLengAry[idx].low = curr->entry.low;
			runLengAry[idx].size = curr->entry.size;

			idx++;
			tail = curr;
			curr = curr->next;

			kfree(tail);
		}
	}
	kfree(head);

#ifdef DBG_KREE_SHM
	for (idx = 0; idx <= xx; idx++)
		KREE_DEBUG("[%s]runLengAry[%d]. h=0x%x,l=0x%x, sz=0x%x\n",
			__func__, idx, runLengAry[idx].high,
			runLengAry[idx].low, runLengAry[idx].size);
#endif
	KREE_DEBUG("[%s]==> end of run length encoding\n", __func__);

	return runLengAry;
}

static TZ_RESULT send_shm_cmd(int round, union MTEEC_PARAM *p,
	KREE_SESSION_HANDLE session, uint32_t numOfPA, uint32_t cmd)
{
	uint32_t paramTypes;
	TZ_RESULT ret = 0;

	p[3].value.a = round;
	p[3].value.b = numOfPA;
	paramTypes = TZ_ParamTypes4(TZPT_MEM_INPUT, TZPT_MEM_INPUT,
				    TZPT_MEM_INPUT, TZPT_VALUE_INOUT);
	ret = KREE_TeeServiceCall(session, cmd, paramTypes, p);

	return ret;
}

static TZ_RESULT send_shm_ending_cmd(union MTEEC_PARAM *p,
	KREE_SESSION_HANDLE session, uint32_t numOfPA, uint32_t cmd,
	uint32_t region_id)
{
	int i;
	uint32_t paramTypes;
	TZ_RESULT ret = 0;

	/* send ending command */
	for (i = 0; i < MAX_NUM_OF_PARAM; i++) {
		p[i].mem.buffer = NULL;
		p[i].mem.size = 0;
	}
	p[3].value.a = -99;
	//p[3].value.b = numOfPA;
	p[3].value.b = region_id;
	paramTypes = TZ_ParamTypes4(TZPT_MEM_INPUT, TZPT_MEM_INPUT,
				    TZPT_MEM_INPUT, TZPT_VALUE_INOUT);
	ret = KREE_TeeServiceCall(session, cmd, paramTypes, p);

	return ret;
}

#ifdef DBG_KREE_SHM
static void print_runlength_arr(union MTEEC_PARAM *p, int *prt_id)
{
	int i, j;
	int entry_num;
	struct KREE_SHM_RUNLENGTH_ENTRY *pp2;

	if (!p)
		return;

	for (i = 0; i < MAX_NUM_OF_PARAM; i++) {
		if (!p[i].mem.size)
			continue;

		pp2 = (struct KREE_SHM_RUNLENGTH_ENTRY *)p[i].mem.buffer;
		entry_num = (int)(p[i].mem.size / EACH_MAP_ENTRY_SIZE);

		KREE_DEBUG
		    ("[%s] p[%d].mem.buffer done --> size =0x%x [%d entries]\n",
		     __func__, i, p[i].mem.size, entry_num);

		for (j = 0; j < entry_num; j++) {
			if ((j == 0) || j == (entry_num - 1)) {
				KREE_DEBUG
				("[%d].pp2[%d] high=0x%x,low=0x%x,size=0x%x\n",
				(*prt_id)++, j, (uint32_t) pp2[j].high,
				(uint32_t) pp2[j].low, (uint32_t) pp2[j].size);
			}
		}
	}
}
#endif

static void init_shm_params(union MTEEC_PARAM *p, int *arr)
{
	int i;

	for (i = 0; i < MAX_NUM_OF_PARAM; i++) {
		p[i].mem.buffer = NULL;
		p[i].mem.size = 0;
		if (arr)
			arr[i] = 0;
	}
}

static TZ_RESULT kree_register_cont_shm(union MTEEC_PARAM *p,
	KREE_SESSION_HANDLE session, void *start, uint32_t size,
	uint32_t cmd, uint32_t region_id)
{
	TZ_RESULT ret = 0;
	int numOfPA;
#ifdef DBG_KREE_SHM
	int prt_id;
#endif
	struct KREE_SHM_RUNLENGTH_ENTRY *tmpAry = NULL;

	/* input data: ex: start = 0xaa000000; size = 8192(8KB) */
	KREE_DEBUG("[%s] Map Continuous Pages\n", __func__);

	/* init mtee param & other buffer */
	init_shm_params(p, NULL);

	numOfPA = size / PAGE_SIZE;
	if ((size % PAGE_SIZE) != 0)
		numOfPA++;
	KREE_DEBUG("[%s] numOfPA= 0x%x\n", __func__, numOfPA);

	tmpAry = kmalloc((MAX_MARY_SIZE)
			 * sizeof(struct KREE_SHM_RUNLENGTH_ENTRY), GFP_KERNEL);
	tmpAry[0].high = (uint32_t) ((uint64_t) start >> 32);
	tmpAry[0].low = (uint32_t) ((uint64_t) start & (0x00000000ffffffff));
	tmpAry[0].size = numOfPA;

	p[0].mem.buffer = tmpAry;
	p[0].mem.size = EACH_MAP_ENTRY_SIZE;

	KREE_DEBUG
	    ("[%s] [Case1]====> tmpAry[0] high= 0x%x, low= 0x%x, size= 0x%x\n",
	     __func__, (uint32_t) tmpAry[0].high, (uint32_t) tmpAry[0].low,
	     (uint32_t) tmpAry[0].size);

#ifdef DBG_KREE_SHM
	print_runlength_arr(p, &prt_id);
#endif
	ret = send_shm_cmd(0, p, session, numOfPA, cmd);
	ret = send_shm_ending_cmd(p, session, numOfPA, cmd, region_id);
	/* ------------------------------------------------------------------ */
	kfree(tmpAry);

	return ret;
}

static TZ_RESULT kree_register_desc_shm(union MTEEC_PARAM *p,
	KREE_SESSION_HANDLE session, void *start, uint32_t size,
	void *mapAry, uint32_t cmd, uint32_t region_id)
{
	TZ_RESULT ret = 0;
	int numOfPA;
	int i, idx = 0, p_idx = 0, offset = 0;
	uint64_t *ary;
	int round = 0;
	int runLeng_arySize = 0;
#ifdef DBG_KREE_SHM
	int prt_id;
#endif
	int tmp_ary_entryNum[MAX_NUM_OF_PARAM] = {0};
	struct KREE_SHM_RUNLENGTH_ENTRY tmp_ary[MAX_NUM_OF_PARAM]
	    [MAX_MARY_SIZE];
	struct KREE_SHM_RUNLENGTH_ENTRY *runLengAry = NULL;

	KREE_DEBUG("[%s] Map Discontinuous Pages\n", __func__);

	/* init mtee param & other buffer */
	init_shm_params(p, tmp_ary_entryNum);

	ary = (uint64_t *) mapAry;
	numOfPA = ary[0];
	KREE_DEBUG("[%s] numOfPA = %d, MAX_MARY_SIZE = %lu\n", __func__,
		numOfPA, MAX_MARY_SIZE);

	/* encode page tables */
	runLengAry =
	shmem_param_run_length_encoding(numOfPA, &runLeng_arySize, ary);

	if (runLengAry == NULL) {
		KREE_ERR("runLengAry malloc fail.\n");
		return TZ_RESULT_ERROR_GENERIC;
	}
#ifdef DBG_KREE_SHM
	for (i = 0; i <= numOfPA; i++)
		KREE_DEBUG("[%s] ====> mapAry[%d]= 0x%llx\n", __func__, i,
		(uint64_t) ary[i]);
	for (idx = 0; idx <= runLeng_arySize; idx++)
		KREE_DEBUG
	    ("runLengAry[%d]. high = 0x%x, low=0x%x, size = 0x%x\n",
		idx, runLengAry[idx].high, runLengAry[idx].low,
		runLengAry[idx].size);
#endif

	/* start sending page tables... */
	idx = 1;
	do {
#ifdef DBG_KREE_SHM
		KREE_DEBUG("[%s]==> idx [%d] runs.\n", __func__, idx);
#endif
		if (idx % (MAX_NUM_OF_PARAM * MAX_MARY_SIZE)
			== 1) {	/* each round restarts */

			if (tmp_ary_entryNum[0] > 0) {
				for (i = 0; i < MAX_NUM_OF_PARAM; i++) {
					p[i].mem.buffer = tmp_ary[i];
					/* #ofEntry * 12 Bytes */
					p[i].mem.size = tmp_ary_entryNum[i]
						* EACH_MAP_ENTRY_SIZE;
#ifdef DBG_KREE_SHM
					KREE_DEBUG
	("[loop]p[i].mem.size=%d[entryNum =%d]\n",
	p[i].mem.size, tmp_ary_entryNum[i]);
#endif
				}

#ifdef DBG_KREE_SHM
				prt_id = 1;
				print_runlength_arr(p, &prt_id);
#endif
				/* send a command */
				ret = send_shm_cmd(round, p, session, numOfPA,
						   cmd);
				KREE_DEBUG("[%s]====> round %d done, restart\n",
					   __func__, round);
				round++;
			}

			init_shm_params(p, tmp_ary_entryNum);
		}

		round = (int)((idx - 1) / (MAX_MARY_SIZE * MAX_NUM_OF_PARAM));
		p_idx = (int)((idx - 1) / MAX_MARY_SIZE) % MAX_NUM_OF_PARAM;
		offset = (int)(((idx - 1)
				- (round * MAX_MARY_SIZE * MAX_NUM_OF_PARAM))
				% MAX_MARY_SIZE);
		tmp_ary[p_idx][offset] = runLengAry[idx];
#ifdef DBG_KREE_SHM
		KREE_DEBUG
		("[%s] tmp_ary[%d][%d] high= 0x%x, low= 0x%x, size= 0x%x\n",
		__func__, p_idx, offset, tmp_ary[p_idx][offset].high,
		tmp_ary[p_idx][offset].low, tmp_ary[p_idx][offset].size);
#endif
		tmp_ary_entryNum[p_idx]++;
		idx++;

	} while (idx <= runLeng_arySize);

	KREE_DEBUG("[%s] do the last print (send command).\n", __func__);
	if (tmp_ary_entryNum[0] > 0) {
		for (i = 0; i < MAX_NUM_OF_PARAM; i++) {
			p[i].mem.buffer = tmp_ary[i];
			p[i].mem.size =
			tmp_ary_entryNum[i] * EACH_MAP_ENTRY_SIZE;
#ifdef DBG_KREE_SHM
			KREE_DEBUG
	("[%s] [last time] p[i].mem.size = %d [tmp_ary_entryNum =%d]\n",
	__func__, p[i].mem.size, tmp_ary_entryNum[i]);
#endif
		}

#ifdef DBG_KREE_SHM
		print_runlength_arr(p, &prt_id);
#endif
		/* send commnad. */
		KREE_DEBUG("===>  round = %d\n", round);
		ret = send_shm_cmd(round, p, session, numOfPA, cmd);
	}

	ret = send_shm_ending_cmd(p, session, numOfPA, cmd, region_id);
	kfree(runLengAry);

	return ret;
}

static TZ_RESULT kree_register_sharedmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE *mem_handle, void *start, uint32_t size,
	void *mapAry, uint32_t cmd, uint32_t region_id)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret = 0;
	int locktry;
	enum tee_id_t tee_id = 0;
	struct mutex *shared_mem_mutex;

	KREE_DEBUG("[%s]is calling.\n", __func__);

	ret = KREE_SessionToTID(session, &tee_id);
	if (ret != TZ_RESULT_SUCCESS)
		pr_info("[%s] err %d, get default tee_id %d\n",
			__func__, ret, tee_id);

	/*FIXME: mutex should be removed after re-implement sending procedure */
	if (tee_id == TEE_ID_TRUSTY)
		shared_mem_mutex = &shared_mem_mutex_trusty;
	else
		shared_mem_mutex = &shared_mem_mutex_nebula;

	do {
		locktry = mutex_lock_interruptible(shared_mem_mutex);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("[%s]mutex lock fail(0x%x)\n",
				 __func__, locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	if (mapAry == NULL)
		ret = kree_register_cont_shm(p, session, start, size,
					     cmd, region_id);
	else
		ret = kree_register_desc_shm(p, session, start, size, mapAry,
					     cmd, region_id);

	mutex_unlock(shared_mem_mutex);	/* FIXME: should be removed */

	if (ret != TZ_RESULT_SUCCESS) {
		*mem_handle = 0;
		return ret;
	}
	*mem_handle = p[3].value.a;

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_RegisterSharedmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE *shm_handle, KREE_SHAREDMEM_PARAM *param)
{
	TZ_RESULT ret;

	if (shm_handle == NULL)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	ret = kree_register_sharedmem(session, shm_handle,
			param->buffer, param->size, param->mapAry,
			TZCMD_MEM_SHAREDMEM_REG, param->region_id);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_UnregisterSharedmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE shm_handle)
{
	TZ_RESULT ret;
	union MTEEC_PARAM p[4];

	if (shm_handle == 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = (uint32_t) shm_handle;
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SHAREDMEM_UNREG,
			TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}
	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_UnregisterSharedmem);

#endif

#if API_secureMem		/*secure memory APIs */
/* notiec: handle type is the same */
static inline TZ_RESULT _allocFunc(uint32_t cmd, KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment,
	uint32_t size, char *dbg)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((mem_handle == NULL) || (size == 0)) {
		KREE_ERR("[%s] Fail. invalid parameters\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = alignment;
	p[1].value.a = size;
	ret = KREE_TeeServiceCall(session, cmd,
		TZ_ParamTypes3(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
		TZPT_VALUE_OUTPUT), p);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]%s Fail(0x%x)\n", __func__, dbg, ret);
		return ret;
	}

	*mem_handle = (KREE_SECUREMEM_HANDLE) p[2].value.a;
	return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc(uint32_t cmd,
	KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle,
	char *dbg)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if (mem_handle == 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(session, cmd,
			TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]%s Fail(0x%x)\n", __func__, dbg, ret);
		return ret;
	}
	return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc_1(uint32_t cmd,
	KREE_SESSION_HANDLE session, KREE_SECUREMEM_HANDLE mem_handle,
	uint32_t *count, char *dbg)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((mem_handle == 0) || (count == NULL))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(session, cmd,
		TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]%s Fail(0x%x)\n", __func__, dbg, ret);
		*count = 0;
		return ret;
	}
	*count = p[1].value.a;
	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_AllocSecuremem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size)
{
	TZ_RESULT ret;

	ret = _allocFunc(TZCMD_MEM_SECUREMEM_ALLOC, session, mem_handle,
		alignment, size, "KREE Alloc Secure mem");
	return ret;
}

TZ_RESULT KREE_ZallocSecuremem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size)
{
	TZ_RESULT ret;

	ret = _allocFunc(TZCMD_MEM_SECUREMEM_ZALLOC, session, mem_handle,
		alignment, size, "KREE_ZallocSecuremem");
	return ret;
}

TZ_RESULT KREE_ZallocSecurememWithTag(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment, uint32_t size)
{
	TZ_RESULT ret;

	ret = _allocFunc(TZCMD_MEM_SECUREMEM_ZALLOC, session, mem_handle,
		alignment, size, "KREE_ZallocSecurememWithTag");
	return ret;
}

TZ_RESULT KREE_ReferenceSecuremem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle)
{
	TZ_RESULT ret;

	ret = _handleOpFunc(TZCMD_MEM_SECUREMEM_REF, session, mem_handle,
		"KREE_ReferenceSecuremem");
	return ret;
}

TZ_RESULT KREE_UnreferenceSecuremem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle)
{
	TZ_RESULT ret;
	uint32_t count = 0;

	ret = _handleOpFunc_1(TZCMD_MEM_SECUREMEM_UNREF, session, mem_handle,
		&count, "KREE_UnreferenceSecuremem");
	KREE_DEBUG("[%s] unref count = 0x%x\n", __func__, count);
	return ret;
}

#endif

#if API_chunkMem /*chunk memory APIs */

static TZ_RESULT _add_HandleMapping_ION_MEM(
	KREE_SECUREMEM_HANDLE mem_handle, KREE_ION_HANDLE IONHandle)
{
	TZ_RESULT ret = TZ_RESULT_ERROR_SHORT_BUFFER;
	int i;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&chmem_mutex_alloc);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("[%s]mutex lock fail(0x%x)\n",
			__func__, locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	for (i = 0; i < (uint32_t) t_size; i++) {
		if ((ion_mem_handleAry[i].memHandle == 0)
		    && (ion_mem_handleAry[i].ionHandle == 0)) {
			ion_mem_handleAry[i].memHandle = mem_handle;
			ion_mem_handleAry[i].ionHandle = IONHandle;
			ret = TZ_RESULT_SUCCESS;
			break;
		}
	}

	mutex_unlock(&chmem_mutex_alloc);
	return ret;
}

static TZ_RESULT _del_HandleMapping_ION_MEM(
	KREE_SECUREMEM_HANDLE mem_handle, KREE_ION_HANDLE IONHandle)
{
	TZ_RESULT ret = TZ_RESULT_ERROR_ITEM_NOT_FOUND;
	int i;
	int locktry;

	do {
		locktry = mutex_lock_interruptible(&chmem_mutex_unref);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("[%s]mutex lock fail(0x%x)\n",
			__func__, locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	for (i = 0; i < (uint32_t) t_size; i++) {
		if ((ion_mem_handleAry[i].memHandle == mem_handle)
		    && (ion_mem_handleAry[i].ionHandle == IONHandle)) {
			ion_mem_handleAry[i].memHandle = 0;
			ion_mem_handleAry[i].ionHandle = 0;
			ret = TZ_RESULT_SUCCESS;
			break;
		}
	}
	mutex_unlock(&chmem_mutex_unref);
	return ret;
}

uint32_t _IONHandle2MemHandle(KREE_ION_HANDLE IONHandle,
	KREE_SECUREMEM_HANDLE *mem_handle)
{
#if IONHandle_Transfer
	TZ_RESULT ret = TZ_RESULT_ERROR_ITEM_NOT_FOUND;
	int i;

	for (i = 0; i < (uint32_t) t_size; i++) {
		if (ion_mem_handleAry[i].ionHandle == IONHandle) {
			*mem_handle = ion_mem_handleAry[i].memHandle;
			ret = TZ_RESULT_SUCCESS;
			break;
		}
	}
	return ret;
#else
	*mem_handle = IONHandle;
	return TZ_RESULT_SUCCESS;
#endif
}

static TZ_RESULT _kree_mcm_Append(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE *cm_hd, KREE_SHAREDMEM_PARAM *param,
	uint32_t cmd)
{
	TZ_RESULT ret;
	int locktry;

	if (cm_hd == NULL)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	do {
		locktry = mutex_lock_interruptible(&chmem_mutex);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("[%s]mutex lock fail(0x%x)\n",
				__func__, locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	ret = kree_register_sharedmem(session, cm_hd,
			param->buffer, param->size, param->mapAry,
			cmd, param->region_id);

	if ((ret != TZ_RESULT_SUCCESS) || (*cm_hd == 0)) {
		KREE_ERR("[%s]append chmem(cm_hd=0x%x) Fail(0x%x)\n",
			__func__, *cm_hd, ret);
		mutex_unlock(&chmem_mutex);
		return TZ_RESULT_ERROR_GENERIC;
	}
#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	ret = gz_do_m4u_map(*cm_hd, param->buffer, param->size,
			param->region_id);
#endif

	mutex_unlock(&chmem_mutex);
	return ret;
}

static TZ_RESULT _kree_mcm_Release(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE cm_handle, uint32_t cmd)
{
	TZ_RESULT ret;
	int locktry;
	union MTEEC_PARAM p[4];

	if (cm_handle == 0)
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	do {
		locktry = mutex_lock_interruptible(&chmem_mutex);
		if (locktry && locktry != -EINTR) {
			KREE_ERR("[%s]mutex lock fail(0x%x)\n",
				__func__, locktry);
			return TZ_RESULT_ERROR_GENERIC;
		}
	} while (locktry);

	p[0].value.a = (uint32_t) cm_handle;
	ret = KREE_TeeServiceCall(session, cmd,
			TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] release Fail(0x%x)\n", __func__, ret);
		mutex_unlock(&chmem_mutex);
		return ret;
	}
#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	ret = gz_do_m4u_umap(cm_handle);
#endif

	mutex_unlock(&chmem_mutex);
	return TZ_RESULT_SUCCESS;
}

static TZ_RESULT _kree_mcm_Alloc(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_SECUREMEM_HANDLE *mem_handle,
	uint32_t alignment, uint32_t size, uint32_t cmd)
{
	TZ_RESULT ret;
	union MTEEC_PARAM p[4];

	if ((chm_handle == 0) || (size == 0)) {
		KREE_ERR("[%s] invalid parameters. chm_hd=0x%x, size=0x%x\n",
			__func__, chm_handle, size);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = alignment;
	p[1].value.a = size;
	p[1].value.b = chm_handle;
	ret = KREE_TeeServiceCall(session, cmd,
			TZ_ParamTypes3(TZPT_VALUE_INPUT,
			TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]Alloc chmem Fail(ret=0x%x)\n", __func__, ret);
		return ret;
	}

	*mem_handle = (KREE_SECUREMEM_HANDLE) p[2].value.a;
	if (*mem_handle == 0) {
		KREE_ERR("[%s]Alloc chmem Fail(mem_hd=0x%x)\n", __func__,
			*mem_handle);
		return TZ_RESULT_ERROR_GENERIC;
	}
	KREE_DEBUG("[%s]Alloc ok(mem_hd=0x%x)\n", __func__, *mem_handle);
	return TZ_RESULT_SUCCESS;
}

static TZ_RESULT _ionHD_to_gzHD(KREE_ION_HANDLE ION_hd,
	KREE_ION_HANDLE *GZ_hd)
{
	int ret;

	if (ION_hd == 0) {
		KREE_ERR("[%s]invalid parameter:ion_hd=0x%x\n", __func__,
			ION_hd);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	*GZ_hd = ION_hd; /*init value */

	ret = _IONHandle2MemHandle(ION_hd, GZ_hd);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_IONHandle2MemHandle Fail(0x%x)\n", __func__,
			ret);
		return ret;
	}

	KREE_DEBUG("[%s]input ION_hd=0x%x, get (GZ)mem_hd:0x%x\n",
		   __func__, (uint32_t) ION_hd, (uint32_t) *GZ_hd);

	return TZ_RESULT_SUCCESS;
}

/*for ION*/
TZ_RESULT KREE_AppendSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE *cm_handle, KREE_SHAREDMEM_PARAM *param)
{
	return _kree_mcm_Append(session, cm_handle,
			param, TZCMD_MEM_APPEND_MULTI_CHUNKMEM_ION);
}
EXPORT_SYMBOL(KREE_AppendSecureMultichunkmem);

TZ_RESULT KREE_ReleaseSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE cm_handle)
{
	return _kree_mcm_Release(session, cm_handle,
			TZCMD_MEM_RELEASE_CHUNKMEM_ION);
}
EXPORT_SYMBOL(KREE_ReleaseSecureMultichunkmem);

TZ_RESULT KREE_AllocSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_SECUREMEM_HANDLE *mem_handle,
	uint32_t alignment, uint32_t size)
{
	return _kree_mcm_Alloc(session, chm_handle, mem_handle, alignment, size,
			TZCMD_MEM_SECUREMULTICHUNKMEM_ALLOC);
}

TZ_RESULT KREE_ZallocSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_SECUREMEM_HANDLE *mem_handle,
	uint32_t alignment, uint32_t size)
{
	return _kree_mcm_Alloc(session, chm_handle, mem_handle, alignment, size,
			TZCMD_MEM_SECUREMULTICHUNKMEM_ZALLOC);
}

TZ_RESULT KREE_ReferenceSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle)
{
	union MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if (mem_handle == 0) {
		KREE_ERR("[%s]Fail.invalid parameters\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = mem_handle;
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECUREMULTICHUNKMEM_REF,
			TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] ref(mem_hd=0x%x) fail(ret=0x%x)\n",
			__func__, mem_handle, ret);

	return ret;
}
EXPORT_SYMBOL(KREE_ReferenceSecureMultichunkmem);

TZ_RESULT KREE_UnreferenceSecureMultichunkmem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle, uint32_t *count)
{
	TZ_RESULT ret;
	union MTEEC_PARAM p[4];

	if (!mem_handle) {
		KREE_ERR("[%s] Fail.invalid parameters\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = mem_handle;

	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECUREMULTICHUNKMEM_UNREF,
			TZ_ParamTypes2(TZPT_VALUE_INPUT,
			TZPT_VALUE_OUTPUT), p);

	*count = p[1].value.a;
	KREE_DEBUG("[%s] unref count = 0x%x\n", __func__, *count);
	if (ret != TZ_RESULT_SUCCESS)
		KREE_ERR("[%s] unref Fail(0x%x)\n", __func__, ret);

	return ret;
}


/*for ION */
/*input mem_handle to get ION_Handle*/
TZ_RESULT KREE_ION_QueryIONHandle(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE mem_handle, KREE_ION_HANDLE *IONHandle)
{
	TZ_RESULT ret;
	union MTEEC_PARAM p[4];

	if (!mem_handle) {
		KREE_ERR("[%s] Fail.invalid parameters\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = mem_handle;

	ret = KREE_TeeServiceCall(session, TZCMD_MEM_Query_IONHandle,
			TZ_ParamTypes2(TZPT_VALUE_INPUT,
			TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] query ion_hd Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	*IONHandle = p[1].value.a;
	KREE_DEBUG("[%s] ok(IONHandle=0x%x)\n", __func__, *IONHandle);

	return ret;
}

static TZ_RESULT _kree_mcm_Alloc_ion(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_ION_HANDLE *IONHandle,
	uint32_t alignment, uint32_t size, int isZalloc)
{
	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle = 0;

	*IONHandle = 0;

	/*allocate secure memory from a chunk memory */
	if (isZalloc == 1)
		ret =
			KREE_ZallocSecureMultichunkmem(session, chm_handle,
				&mem_handle, alignment, size);
	else
		ret =
		    KREE_AllocSecureMultichunkmem(session, chm_handle,
				&mem_handle, alignment, size);

	if ((ret != TZ_RESULT_SUCCESS) || (!mem_handle)) {
		KREE_ERR("[%s]alloc Fail: mem_hd=0x%x, ret=0x%x)\n",
			__func__, mem_handle, ret);
		return TZ_RESULT_ERROR_GENERIC;
	}

	KREE_DEBUG("[%s]alloc ok(mem_hd=0x%x)\n", __func__, mem_handle);

	*IONHandle = mem_handle;	/*init value */

#if IONHandle_Transfer
	/*use mem_handle to get ION_Handle */
	ret = KREE_ION_QueryIONHandle(session, mem_handle, IONHandle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]KREE_ION_QueryIONHandle Fail(0x%x)\n", __func__,
			ret);
		return ret;
	}
	KREE_DEBUG("[%s]get IONHandle:0x%x\n", __func__,
		(uint32_t) (*IONHandle));

	/*add (mem_handle, IONHandle) mapping into table */
	ret = _add_HandleMapping_ION_MEM(mem_handle, (*IONHandle));
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_add_HandleMapping_ION_MEM Fail(0x%x)\n",
			__func__, ret);
		return ret;
	}
	KREE_DEBUG("[%s]add mapping table done\n", __func__);
#endif

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_ION_AllocChunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_ION_HANDLE *IONHandle,
	uint32_t alignment, uint32_t size)
{
	return _kree_mcm_Alloc_ion(session, chm_handle,
			IONHandle, alignment, size, 0);
}
EXPORT_SYMBOL(KREE_ION_AllocChunkmem);

TZ_RESULT KREE_ION_ZallocChunkmem(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE chm_handle, KREE_ION_HANDLE *IONHandle,
	uint32_t alignment, uint32_t size)
{
	return _kree_mcm_Alloc_ion(session, chm_handle,
			IONHandle, alignment, size, 1);
}
EXPORT_SYMBOL(KREE_ION_ZallocChunkmem);

TZ_RESULT KREE_ION_ReferenceChunkmem(KREE_SESSION_HANDLE session,
	KREE_ION_HANDLE IONHandle)
{
	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle;

	ret = _ionHD_to_gzHD(IONHandle, &mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_ionHD_to_gzHD Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	KREE_DEBUG("[%s]input ION_hd=0x%x, get (GZ)mem_hd:0x%x\n",
			__func__, (uint32_t) IONHandle, (uint32_t) mem_handle);

	return KREE_ReferenceSecureMultichunkmem(session, mem_handle);
}
EXPORT_SYMBOL(KREE_ION_ReferenceChunkmem);

TZ_RESULT KREE_ION_UnreferenceChunkmem(KREE_SESSION_HANDLE session,
	KREE_ION_HANDLE IONHandle)
{
	TZ_RESULT ret;
	uint32_t count = 0;
	KREE_SECUREMEM_HANDLE mem_handle;

	ret = _ionHD_to_gzHD(IONHandle, &mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_ionHD_to_gzHD Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	KREE_DEBUG("[%s]input ION_hd=0x%x, get (GZ)mem_hd:0x%x\n",
			__func__, (uint32_t) IONHandle, (uint32_t) mem_handle);

	ret = KREE_UnreferenceSecureMultichunkmem(session, mem_handle, &count);
	if (ret != TZ_RESULT_SUCCESS)
		return ret;

#if IONHandle_Transfer
	if (count == 0) {
		ret = _del_HandleMapping_ION_MEM(mem_handle, IONHandle);
		if (ret != TZ_RESULT_SUCCESS) {
			KREE_ERR("[%s]_del_HandleMapping_ION_MEM Fail(0x%x)\n",
				__func__, ret);
			return ret;
		}
	}
#endif

	return ret;
}
EXPORT_SYMBOL(KREE_ION_UnreferenceChunkmem);

TZ_RESULT KREE_ION_AccessChunkmem(KREE_SESSION_HANDLE session,
	union MTEEC_PARAM param[4], uint32_t cmd)
{
	uint32_t paramTypes;

	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle;
	KREE_ION_HANDLE IONHandle;

	IONHandle = param[0].value.a;

	ret = _ionHD_to_gzHD(IONHandle, &mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_ionHD_to_gzHD Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	KREE_DEBUG("[%s]input IONHandle=0x%x, get mem_handle:0x%x\n",
			__func__, (uint32_t) IONHandle, (uint32_t) mem_handle);

	param[0].value.a = mem_handle; /*update param[0] to mem_handle */

	paramTypes = TZ_ParamTypes4(TZPT_VALUE_INPUT, TZPT_VALUE_INPUT,
			TZPT_VALUE_INPUT, TZPT_VALUE_INOUT);

	ret = KREE_TeeServiceCall(session, cmd, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

/*only for debug: copy chm data to shm.*/
TZ_RESULT KREE_ION_CP_Chm2Shm(KREE_SESSION_HANDLE session,
	KREE_ION_HANDLE ION_hd, KREE_SECUREMEM_HANDLE shm_hd, uint32_t size)
{
	TZ_RESULT ret;
	union MTEEC_PARAM param[4];

	param[0].value.a = ION_hd;	/*need to transform to mem_handle */
	param[0].value.b = shm_hd;
	param[1].value.a = size;	/*alloc size */

	ret = KREE_ION_AccessChunkmem(session, param, TZCMD_MEM_CopyChmtoShm);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	KREE_DEBUG("[%s]ret param[3].value.a=0x%x\n", __func__,
		param[3].value.a);
	return param[3].value.a;
}

TZ_RESULT KREE_ION_QueryChunkmem_TEST(KREE_SESSION_HANDLE session,
	KREE_ION_HANDLE IONHandle, uint32_t cmd)
{
	union MTEEC_PARAM p[4];
	uint32_t paramTypes;

	TZ_RESULT ret;
	KREE_SECUREMEM_HANDLE mem_handle;

	ret = _ionHD_to_gzHD(IONHandle, &mem_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s]_ionHD_to_gzHD Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	KREE_DEBUG("[%s]input IONHandle=0x%x, get mem_handle:0x%x\n",
		__func__, (uint32_t) IONHandle, (uint32_t) mem_handle);

	p[0].value.a = mem_handle;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(session, cmd, paramTypes, p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	if (p[1].value.a != 0) {
		KREE_ERR("[%s] Fail(mem_hd=0x%x)\n", __func__, mem_handle);
		return TZ_RESULT_ERROR_GENERIC; /*Query Fail */
	}

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_ION_QueryChunkmem_TEST);
#endif

#if API_chunkMem /*for general case chunk mem (4KB page unit) */

/*for general chunk memory*/
TZ_RESULT KREE_AppendSecureMultichunkmem_basic(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE *cm_handle, KREE_SHAREDMEM_PARAM *param)
{
	return _kree_mcm_Append(session, cm_handle,
			param, TZCMD_MEM_APPEND_MULTI_CHUNKMEM);
}

/*for general chunk memory*/
TZ_RESULT KREE_ReleaseSecureMultichunkmem_basic(KREE_SESSION_HANDLE session,
	KREE_SHAREDMEM_HANDLE cm_handle)
{
	return _kree_mcm_Release(session, cm_handle,
			TZCMD_MEM_RELEASE_CHUNKMEM);
}

TZ_RESULT KREE_QueryChunkmem_TEST(KREE_SESSION_HANDLE session,
	KREE_ION_HANDLE mem_handle, uint32_t cmd)
{
	union MTEEC_PARAM p[4];
	uint32_t paramTypes;

	TZ_RESULT ret;

	if (mem_handle == 0) {
		KREE_ERR("[%s] Fail.invalid parameters\n", __func__);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	p[0].value.a = mem_handle;
	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);

	ret = KREE_TeeServiceCall(session, cmd, paramTypes, p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	if (p[1].value.a != 0) {
		KREE_ERR("[%s] Fail(mem_hd=0x%x)\n", __func__, mem_handle);
		return TZ_RESULT_ERROR_GENERIC; /*Query Fail */
	}

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_QueryChunkmem_TEST);
#endif

#if API_NotSupport /*API_NOT_SUPPORT */

TZ_RESULT KREE_ReleaseSecurechunkmem(KREE_SESSION_HANDLE session,
	uint32_t *size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_AppendSecurechunkmem(KREE_SESSION_HANDLE session)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

/*fix mtee sync*/
TZ_RESULT KREE_AllocSecurememWithTag(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *mem_handle, uint32_t alignment,
	uint32_t size, const char *tag)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_AllocSecurechunkmem(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *cm_handle, uint32_t alignment, uint32_t size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_AllocSecurechunkmemWithTag(KREE_SESSION_HANDLE session,
	KREE_SECUREMEM_HANDLE *cm_handle, uint32_t alignment,
	uint32_t size, const char *tag)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_ReferenceSecurechunkmem(KREE_SESSION_HANDLE session,
	KREE_SECURECM_HANDLE cm_handle)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_UnreferenceSecurechunkmem(KREE_SESSION_HANDLE session,
	KREE_SECURECM_HANDLE cm_handle)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_ReadSecurechunkmem(KREE_SESSION_HANDLE session,
	uint32_t offset, uint32_t size, void *buffer)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_WriteSecurechunkmem(KREE_SESSION_HANDLE session,
	uint32_t offset, uint32_t size, void *buffer)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_GetSecurechunkReleaseSize(KREE_SESSION_HANDLE session,
	uint32_t *size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_StartSecurechunkmemSvc(KREE_SESSION_HANDLE session,
	unsigned long start_pa, uint32_t size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_StopSecurechunkmemSvc(KREE_SESSION_HANDLE session,
	unsigned long *cm_pa, uint32_t *size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_QuerySecurechunkmem(KREE_SESSION_HANDLE session,
	unsigned long *cm_pa, uint32_t *size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

TZ_RESULT KREE_GetTEETotalSize(KREE_SESSION_HANDLE session, uint32_t *size)
{
	KREE_DEBUG("[%s] not support!\n", __func__);
	return TZ_RESULT_ERROR_NOT_SUPPORTED;
}

#endif /*API_NOT_SUPPORT */
TZ_RESULT KREE_ConfigSecureMultiChunkMemInfo(KREE_SESSION_HANDLE session,
	uint64_t pa, uint32_t size, uint32_t region_id)
{
	TZ_RESULT ret;
	union MTEEC_PARAM p[4];
#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	bool is_region_on = ((size == 0x0) ? false : true);
#endif

	p[0].value.a = (pa >> 32);
	p[0].value.b = (pa & 0xFFFFFFFF);
	p[1].value.a = size;
	p[1].value.b = region_id;

	ret = KREE_TeeServiceCall(session, TZCMD_MEM_CONFIG_CHUNKMEM_INFO_ION,
			TZ_ParamTypes3(TZPT_VALUE_INPUT,
			TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT), p);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("[%s] service call failed=0x%x\n", __func__, ret);
		return ret;
	}

	ret = p[2].value.a;
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("[%s] config memory info failed=0x%x\n",
			__func__, ret);
		return ret;
	}
#if IS_ENABLED(CONFIG_GZ_VPU_WITH_M4U)
	if (is_region_on)
		ret = gz_do_m4u_map(session, (void *)pa, size, region_id);
	else
		ret = gz_do_m4u_umap(session);

	if (ret != TZ_RESULT_SUCCESS) {
		KREE_DEBUG("[%s] do m4u map/unmap failed=%d\n", __func__, ret);
		return ret;
	}
#endif

	return TZ_RESULT_SUCCESS;
}
EXPORT_SYMBOL(KREE_ConfigSecureMultiChunkMemInfo);
