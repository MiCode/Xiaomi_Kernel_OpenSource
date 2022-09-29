// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/random.h>

#include <linux/dma-mapping.h>

#include "apusys_power.h"
#include "apusys_device.h"
#include "apu_config.h"

#include "mvpu_plat_device.h"
#include "mvpu_sysfs.h"
#include "mvpu_ipi.h"
#include "mvpu_cmd_data.h"
#include "mvpu_driver.h"
#include "mvpu_sec.h"

static struct device *mvpu_dev;

//#define FULL_RP_INFO

void set_sec_log_lvl(int log_lvl)
{
	mvpu_loglvl_sec = log_lvl;
}

bool get_mvpu_algo_available(void)
{
	return mvpu_algo_available;
}

uint32_t get_ptn_total_size(void)
{
	uint32_t ptn_total_size = mvpu_algo_img[3];

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
		pr_info("[MVPU][IMG] PTN total size: 0x%08x\n", ptn_total_size);

	return ptn_total_size;
}

uint32_t get_ptn_size(uint32_t hash)
{
	uint32_t ptn_size = 0;
	uint32_t ptn_total_num = mvpu_algo_img[KER_NUM_OFFSET];
	int i = 0;

	uint32_t shift = 0;
	uint32_t ptn_hash = 0;
	uint32_t ptn_size_offset = 0;

	for (i = 0; i < ptn_total_num; i++) {
		// get hash by ptn.bin size
		shift = IMG_HEADER_SIZE + ptn_size_offset/sizeof(uint32_t) + PTN_INFO_SIZE*i;
		ptn_hash = mvpu_algo_img[shift];

		// check hash
		if ((ptn_hash & MPVU_BATCH_MASK) == (hash & MPVU_BATCH_MASK)) {
			ptn_size = mvpu_algo_img[shift + KER_SIZE_OFFSET];
			break;
		}

		// count ptn.bin size to shift
		ptn_size_offset += mvpu_algo_img[shift + PNT_SIZE_OFFSET];

		// alignment
		ptn_size_offset = ptn_size_offset + 4 - (ptn_size_offset % 4);
	}

	if ((ptn_hash & MPVU_BATCH_MASK) != (hash & MPVU_BATCH_MASK))
		pr_info("[MVPU][Sec] PTN HASH: 0x%08x not found\n", hash);

	//printf("[SEC_IMG] get PTN HASH: 0x%08x, img addr 0x%08x,
	//			size: 0x%08x\n", ptn_hash, img_addr, ptn_size);
	return ptn_size;
}

bool get_ptn_hash(uint32_t hash)
{
	uint32_t ptn_total_num = 0;
	int i = 0;

	uint32_t shift = 0;
	uint32_t ptn_hash = 0;
	uint32_t ptn_size_offset = 0;

	if (mvpu_algo_available == false)
		return false;

	ptn_total_num = mvpu_algo_img[KER_NUM_OFFSET];

	for (i = 0; i < ptn_total_num; i++) {
		// get hash by ptn.bin size
		shift = IMG_HEADER_SIZE + ptn_size_offset/sizeof(uint32_t) + PTN_INFO_SIZE*i;
		ptn_hash = mvpu_algo_img[shift];

		// check hash
		if ((ptn_hash & MPVU_BATCH_MASK) == (hash & MPVU_BATCH_MASK))
			return true;

		// count ptn.bin size to shift
		ptn_size_offset += mvpu_algo_img[shift + PNT_SIZE_OFFSET];

		// alignment
		if ((ptn_size_offset % 4) != 0)
			ptn_size_offset = ptn_size_offset + 4 - (ptn_size_offset % 4);
	}

	return false;
}

uint32_t get_kerbin_total_size(void)
{
	uint32_t kerbin_total_size =
			mvpu_algo_img[ker_img_offset/sizeof(uint32_t) + KER_BIN_SIZE_OFFSET];

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
		pr_info("[MVPU][IMG] KER total size: 0x%08x\n", kerbin_total_size);

	return kerbin_total_size;
}

uint32_t get_ker_img_offset(void)
{
	uint32_t offset = mvpu_algo_img[3] + 0x10;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] ker_img_offset : 0x%08x\n", offset);

	return offset;
}

void get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_bin_num)
{
	uint32_t ker_img_total_num =
				mvpu_algo_img[ker_img_offset/sizeof(uint32_t) + KER_NUM_OFFSET];

	uint32_t shift = 0;
	uint32_t ker_hash = 0;
	uint32_t ker_size_offset = 0;
	int i = 0;

	if (ker_img_total_num == 0)
		pr_info("[MVPU][IMG] [ERROR] not found Kernel_*.bin in mvpu_algo.img, please check\n");

	for (i = 0; i < ker_img_total_num; i++) {
		// get hash by ker.bin size
		shift = ker_img_offset/sizeof(uint32_t)
				+ IMG_HEADER_SIZE
				+ ker_size_offset/sizeof(uint32_t)
				+ KER_INFO_SIZE*i;

		ker_hash = mvpu_algo_img[shift];

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
			pr_info("[MVPU][IMG] shift 0x%08x, 0x%08x 0x%08x 0x%08x 0x%08x\n",
				shift,
				mvpu_algo_img[shift + 0],
				mvpu_algo_img[shift + 1],
				mvpu_algo_img[shift + 2],
				mvpu_algo_img[shift + 3]
				);
		}

		// check hash
		if ((ker_hash & MPVU_BATCH_MASK) == (hash & MPVU_BATCH_MASK)) {
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][IMG] Get KNL HASH 0x%08x\n", hash);

			*ker_bin_offset = (shift + KER_INFO_SIZE)*sizeof(uint32_t);
			//*ker_size = mvpu_algo_img[shift + KER_SIZE_OFFSET];
			*ker_bin_num =	mvpu_algo_img[shift + KER_NUM_OFFSET];
			break;
		}

		// count ker.bin size to shift
		ker_size_offset +=	mvpu_algo_img[shift + KER_SIZE_OFFSET];

		// alignment
		//ker_size_offset = ker_size_offset + 4 - (ker_size_offset % 4);
	}

	return;
}

void set_ker_iova(uint32_t ker_bin_offset, uint32_t ker_bin_num, uint32_t *ker_bin_each_iova)
{
	uint32_t shift = 0;
	uint32_t ker_size_offset = 0;
	int i = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s, ker_bin_num %d\n", __func__, ker_bin_num);

	for (i = 0; i < ker_bin_num; i++) {
		shift = ker_bin_offset/sizeof(uint32_t)
				+ ker_size_offset/sizeof(uint32_t)
				+ KER_BIN_INFO_SIZE*i;

		//shift = 4*(ker_size_offset/sizeof(uint32_t) + KER_BIN_INFO_SIZE*i);
		ker_bin_each_iova[i] = (shift + KER_BIN_INFO_SIZE)*sizeof(uint32_t)
							+ mvpu_algo_iova;

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][IMG] ker_bin_each_iova[%d] = 0x%08x\n",
					i, ker_bin_each_iova[i]);

		ker_size_offset += mvpu_algo_img[shift + KER_SIZE_OFFSET];
	}
}

void map_base_buf_id(uint32_t buf_num,
						uint32_t *sec_chk_addr,
						uint32_t *sec_buf_attr,
						uint32_t rp_num,
						uint32_t *target_old_map,
						uint32_t *target_old_base,
						uint32_t *target_new_map,
						uint32_t *target_new_base,
						uint32_t buf_cmd_kreg,
						uint32_t buf_cmd_next)
{
	int i = 0, j = 0;
	bool mapped_new_buf = false;
	bool mapped_old_buf = false;

	for (j = 0; j < rp_num; j++) {
		mapped_new_buf = false;
		mapped_old_buf = false;

		if (target_new_base[j] == sec_chk_addr[buf_cmd_next]) {
			target_new_map[j] = buf_cmd_next;
			mapped_new_buf = true;
		} else if (target_new_base[j] == 0) {
			mapped_new_buf = true;
		}

		if (target_old_base[j] == sec_chk_addr[buf_cmd_next]) {
			target_old_map[j] = buf_cmd_next;
			mapped_old_buf = true;
#ifdef MVPU_SEC_KREG_IN_POOL
		} else if (target_old_base[j] == 0) {
			target_old_map[j] = buf_cmd_kreg;
			mapped_old_buf = true;
#endif
		}

		if (mapped_new_buf && mapped_old_buf)
			continue;

		for (i = 0; i < buf_num; i++) {
			if (mapped_new_buf && mapped_old_buf)
				break;

			if (i == buf_cmd_next)
				continue;

			if (sec_chk_addr[i] == 0x0)
				continue;

			if (mapped_new_buf == false) {
				if (target_new_base[j] == sec_chk_addr[i]) {
					target_new_map[j] = i;
					mapped_new_buf = true;
				}
			}

			if (sec_buf_attr[i] == BUF_KERNEL ||
				sec_buf_attr[i] == BUF_IO)
				continue;

			if (mapped_old_buf == false) {
				if (target_old_base[j] == sec_chk_addr[i]) {
					target_old_map[j] = i;
					mapped_old_buf = true;
				}
			}
		}
	}
}

uint32_t get_saved_session_id(void *session)
{
	uint32_t cnt = 0;
	uint32_t session_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_SESSION; cnt++) {
		if (saved_session[cnt] == -1)
			continue;

		//if (memcmp(session, saved_session[cnt], sizeof(uint64_t)) == 0) {
		if (saved_session[cnt] == (uint64_t)session) {
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] Get saved_session at %d: 0x%llx\n",
							cnt, saved_session[cnt]);
			session_id = cnt;
			break;
		}
	}

	return session_id;
}

uint32_t get_avail_session_id(void)
{
	uint32_t cnt = 0;
	uint32_t session_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_SESSION; cnt++) {
		if (saved_session[cnt] == (uint64_t)(-1)) {
			session_id = cnt;
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] Get available session_id %d\n",
						session_id);
			break;
		}
	}

#ifdef MVPU_SEC_USE_OLDEST_SESSION_ID
	// all session place are used, take the oldest
	if (session_id == -1) {
		session_id = sess_oldest;

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] Take oldest session_id %d\n",
					session_id);
	}
#endif

	return session_id;
}


void clear_session(void *session)
{
	uint32_t session_id = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	for (session_id = 0; session_id < MAX_SAVE_SESSION; session_id++) {
		if (saved_session[session_id] == (uint64_t)(-1))
			continue;

		//if (memcmp(session, saved_session[session_id], sizeof(uint64_t)) == 0) {
		if (saved_session[session_id] == (uint64_t)session) {
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
				pr_info("[MVPU][Sec] apusys session %llx match saved_session[%d]\n",
					(uint64_t)session,
					session_id);
			}

			free_all_hash(session_id);
			//memset(saved_session[session_id], -1, sizeof(uint64_t));
			saved_session[session_id] = (uint64_t)(-1);
			break;
		}
	}
}

void update_session_id(uint32_t session_id, void *session)
{
#ifdef MVPU_SEC_USE_OLDEST_SESSION_ID
	sess_oldest++;
	if (sess_oldest == MAX_SAVE_SESSION)
		sess_oldest = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] set oldest session_id: %d\n", sess_oldest);
#endif

	if (saved_session[session_id] == (uint64_t)(-1) && session != NULL) {
		//memcpy(saved_session[session_id], session, sizeof(uint64_t));
		saved_session[session_id] = (uint64_t)session;
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][Sec] set saved_session[%d]: 0x%llx to apu_session: 0x%llx\n",
					session_id,
					saved_session[session_id],
					(uint64_t)session);
		}
	}
}

uint32_t get_saved_hash_id(uint32_t session_id, uint32_t batch_name_hash)
{
	uint32_t cnt = 0;
	uint32_t hash_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (batch_name_hash == hash_pool[session_id]->hash_list[cnt]) {
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] Get saved_hash_list at %d: 0x%08x\n",
						cnt, batch_name_hash);
			hash_id = cnt;
			break;
		}
	}

	return hash_id;
}

uint32_t get_avail_hash_id(uint32_t session_id)
{
	uint32_t cnt = 0;
	uint32_t hash_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (hash_pool[session_id]->hash_list[cnt] == 0) {
			hash_id = cnt;
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] Get available hash id %d\n", hash_id);
			break;
		}
	}

#ifdef MVPU_SEC_USE_OLDEST_HASH_ID
	// all hash place are used, take the oldest
	if (hash_id == -1) {
		hash_id = hash_pool[session_id]->hash_oldest;
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] Take oldest hash id %d\n", hash_id);
	}
#endif

	return hash_id;
}

void clear_hash(uint32_t session_id, uint32_t hash_id)
{
	// clear old hash settings
	if (hash_pool[session_id]->hash_list[hash_id] != 0) {
		hash_pool[session_id]->buf_num[hash_id] = 0;
		hash_pool[session_id]->rp_num[hash_id] = 0;
		hash_pool[session_id]->hash_list[hash_id] = 0;

		dma_buf_unmap_attachment(hash_pool[session_id]->attach[hash_id],
								hash_pool[session_id]->sgt[hash_id],
								DMA_BIDIRECTIONAL);

		dma_buf_detach(hash_pool[session_id]->hash_dma_buf[hash_id],
						hash_pool[session_id]->attach[hash_id]);

		dma_heap_buffer_free(hash_pool[session_id]->hash_dma_buf[hash_id]);

		hash_pool[session_id]->hash_base_iova[hash_id] = 0;
		hash_pool[session_id]->hash_pool_size[hash_id] = 0;

		kfree(hash_pool[session_id]->sec_chk_addr[hash_id]);

#ifdef FULL_RP_INFO
		kfree(hash_pool[session_id]->target_buf_old_base[hash_id]);
		kfree(hash_pool[session_id]->target_buf_old_offset[hash_id]);
		kfree(hash_pool[session_id]->target_buf_new_base[hash_id]);
		kfree(hash_pool[session_id]->target_buf_new_offset[hash_id]);
		kfree(hash_pool[session_id]->target_buf_old_map[hash_id]);
		kfree(hash_pool[session_id]->target_buf_new_map[hash_id]);
#endif

		kfree(hash_pool[session_id]->hash_offset[hash_id]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] Clear hash_id[%d] settings\n",
						hash_id);

	} else {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] bypass Clear hash_id[%d] settings\n",
						hash_id);
	}
}

void free_all_hash(uint32_t session_id)
{
	uint32_t hash_id = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s saved_session[%d]\n", __func__, session_id);

	for (hash_id = 0; hash_id < MAX_SAVE_HASH; hash_id++) {
		if (hash_pool[session_id]->hash_list[hash_id] != 0)
			clear_hash(session_id, hash_id);
		else
			break;
	}
}

int update_hash_pool(void *session,
							bool algo_in_img,
							uint32_t session_id,
							uint32_t hash_id,
							uint32_t batch_name_hash,
							uint32_t buf_num,
							void *kreg_kva,
							uint32_t *sec_chk_addr,
							uint32_t *sec_buf_size,
							uint32_t *sec_buf_attr)
{
	uint32_t cnt = 0;
	uint32_t buf_size = 0;
	uint32_t buf_ofst = 0;
	void *p_buf;
	void *cp_buff;
	void *buf_kva;
	bool copy_to_pool = true;
	int ret_dma_buf_vmap = 0;
	struct dma_buf_map sys_map;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	hash_pool[session_id]->buf_num[hash_id] = buf_num;
	hash_pool[session_id]->hash_list[hash_id] = batch_name_hash;

#ifdef MVPU_SEC_USE_OLDEST_HASH_ID
	hash_pool[session_id]->hash_oldest++;
	if (hash_pool[session_id]->hash_oldest >= MAX_SAVE_HASH)
		hash_pool[session_id]->hash_oldest = 0;
#endif

	hash_pool[session_id]->dma_heap[hash_id] = dma_heap_find("mtk_mm");

	if (!hash_pool[session_id]->dma_heap[hash_id]) {
		pr_info("[MVPU][Sec] heap find fail\n");
		return -1;
	}

	// count buf size
	for (cnt = 0; cnt < hash_pool[session_id]->buf_num[hash_id]; cnt++) {
		if (sec_buf_attr[cnt] != BUF_IO) {
			buf_size = sec_buf_size[cnt];

			// alignment
			buf_size =
				(((buf_size) + MVPU_ADDR_ALIGN - 1)
					/MVPU_ADDR_ALIGN)
					*MVPU_ADDR_ALIGN;
			hash_pool[session_id]->hash_pool_size[hash_id] += buf_size;
		}
	}

	// set MPU region size
	hash_pool[session_id]->hash_pool_size[hash_id] =
		(((hash_pool[session_id]->hash_pool_size[hash_id])
			+ MVPU_MPU_SIZE - 1)
			/MVPU_MPU_SIZE)
			*MVPU_MPU_SIZE;

	hash_pool[session_id]->hash_dma_buf[hash_id] =
		dma_heap_buffer_alloc(hash_pool[session_id]->dma_heap[hash_id],
		hash_pool[session_id]->hash_pool_size[hash_id],
		O_RDWR | O_CLOEXEC,
		DMA_HEAP_VALID_HEAP_FLAGS);

	if (IS_ERR(hash_pool[session_id]->hash_dma_buf[hash_id])) {
		pr_info("[MVPU][Sec] buffer alloc fail\n");
		return PTR_ERR(hash_pool[session_id]->hash_dma_buf[hash_id]);
	}

	if (mvpu_dev == NULL) {
		pr_info("[MVPU][Sec] mvpu_dev is NULL\n");
		return -1;
	}

	hash_pool[session_id]->attach[hash_id] =
		dma_buf_attach(hash_pool[session_id]->hash_dma_buf[hash_id],
		mvpu_dev);

	if (IS_ERR(hash_pool[session_id]->attach[hash_id])) {
		pr_info("[MVPU][Sec] attach fail, return\n");
		return PTR_ERR(hash_pool[session_id]->attach[hash_id]);
	}

	hash_pool[session_id]->sgt[hash_id] =
		dma_buf_map_attachment(hash_pool[session_id]->attach[hash_id],
		DMA_BIDIRECTIONAL);

	if (IS_ERR(hash_pool[session_id]->sgt[hash_id])) {
		pr_info("[MVPU][Sec] map failed, detach and return\n");
		dma_buf_detach(hash_pool[session_id]->hash_dma_buf[hash_id],
						hash_pool[session_id]->attach[hash_id]);
		return PTR_ERR(hash_pool[session_id]->sgt[hash_id]);
	}

	// get iova
	hash_pool[session_id]->hash_base_iova[hash_id] =
		(uint32_t)sg_dma_address(hash_pool[session_id]->sgt[hash_id]->sgl);

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][Sec] hash_base_iova 0x%08x\n",
				hash_pool[session_id]->hash_base_iova[hash_id]);
		pr_info("[MVPU][Sec] hash_pool_size 0x%08x\n",
				hash_pool[session_id]->hash_pool_size[hash_id]);
	}

	// get *kva
	ret_dma_buf_vmap = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id], &sys_map);
	p_buf = sys_map.vaddr;
	//hash_pool[session_id]->hash_base_kva[hash_id] = (uint64_t *)p_buf;

	if ((ret_dma_buf_vmap != 0) || (!p_buf)) {
		pr_info("[MVPU][Sec] kva map failed\n");
		return -ENOMEM;
	}

	hash_pool[session_id]->hash_offset[hash_id] =
		kcalloc(hash_pool[session_id]->buf_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	//cache sync
	dma_buf_begin_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (hash_pool[session_id]->hash_offset[hash_id] != NULL) {
		buf_size = 0;

		for (cnt = 0; cnt < hash_pool[session_id]->buf_num[hash_id]; cnt++) {
			//iova
			hash_pool[session_id]->hash_offset[hash_id][cnt] = buf_ofst;
			cp_buff = (void *)((uintptr_t)p_buf + buf_ofst);

			buf_size = sec_buf_size[cnt];
			buf_kva = apusys_mem_query_kva_by_sess(session, sec_chk_addr[cnt]);

			switch (sec_buf_attr[cnt]) {
			case BUF_NORMAL:
			case BUF_RINGBUFFER:
				if (buf_kva != 0x0) {
					if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
						pr_info("[MVPU][Sec] buf[%3d]: copy to pool\n",
								cnt);
					copy_to_pool = true;
				} else {
#ifndef MVPU_SEC_KREG_IN_POOL
					if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
						pr_info("[MVPU][Sec] buf[%3d]: bypass cmd_buf\n",
								cnt);
					copy_to_pool = false;
#else
					if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
						pr_info("[MVPU][Sec] buf[%3d]: copy cmd_buf\n",
								cnt);
					copy_to_pool = true;
					buf_kva = kreg_kva;
#endif
				}
				break;
			case BUF_KERNEL:
				if (!algo_in_img) {
					if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
						pr_info("[MVPU][Sec] buf[%3d]: copy to pool\n",
								cnt);
					copy_to_pool = true;
				} else {
					if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
						pr_info("[MVPU][Sec] buf[%3d]: use kernel from image\n",
								cnt);
					copy_to_pool = false;
				}
				break;
			case BUF_IO:
				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
					pr_info("[MVPU][Sec] buf[%3d]: bypass IO buffer\n",
							cnt);
				copy_to_pool = false;
				break;
			default:
				pr_info("[MVPU][Sec] [ERROR] unrecognized buf[%3d] attr: %d\n",
						cnt, sec_buf_attr[cnt]);
				copy_to_pool = false;
				break;
			}

			if (copy_to_pool) {
				if (buf_kva == NULL) {
					pr_info("[MVPU][Sec] buf_kva is NULL, memcpy to cp_buff fail\n");
					return -ENOMEM;
				}
				memcpy(cp_buff, buf_kva, buf_size);

				//alignment
				buf_size = (((buf_size) + MVPU_ADDR_ALIGN - 1)
							/MVPU_ADDR_ALIGN)
							*MVPU_ADDR_ALIGN;

				//next buf
				buf_ofst += buf_size;

				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
					pr_info("[MVPU][Sec] buf[%3d]: copy 0x%llx to 0x%llx\n",
						cnt,
						(uintptr_t)buf_kva,
						(uintptr_t)cp_buff);
				}

				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
					pr_info("[MVPU][Sec] buf[%3d]: copy size 0x%08x, offset 0x%08x\n",
						cnt,
						buf_size,
						hash_pool[session_id]->hash_offset[hash_id][cnt]);
				}
			}
		}
	} else {
		pr_info("[MVPU][Sec] hash_offset alloc fail\n");
		return -ENOMEM;
	}

	//cache sync
	dma_buf_end_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (p_buf)
		dma_buf_vunmap(hash_pool[session_id]->hash_dma_buf[hash_id], p_buf);

	return 0;
}

#ifdef FULL_RP_INFO
int save_hash_info(uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_num,
						uint32_t rp_num,
						uint32_t *sec_chk_addr,
						uint32_t *target_buf_old_base,
						uint32_t *target_buf_old_offset,
						uint32_t *target_buf_new_base,
						uint32_t *target_buf_new_offset,
						uint32_t *target_buf_old_map,
						uint32_t *target_buf_new_map)
#else
int save_hash_info(uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_num,
						uint32_t *sec_chk_addr)

#endif
{
	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

#ifdef FULL_RP_INFO
	hash_pool[session_id]->rp_num[hash_id] = rp_num;
#endif

	hash_pool[session_id]->sec_chk_addr[hash_id] =
		kcalloc(hash_pool[session_id]->buf_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->sec_chk_addr[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->sec_chk_addr[hash_id],
			sec_chk_addr,
			hash_pool[session_id]->buf_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

#ifdef FULL_RP_INFO
	hash_pool[session_id]->target_buf_old_base[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_old_base[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_old_base[hash_id],
			target_buf_old_base,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

	hash_pool[session_id]->target_buf_old_offset[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_old_offset[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_old_offset[hash_id],
			target_buf_old_offset,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

	hash_pool[session_id]->target_buf_new_base[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_new_base[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_new_base[hash_id],
			target_buf_new_base,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

	hash_pool[session_id]->target_buf_new_offset[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_new_offset[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_new_offset[hash_id],
			target_buf_new_offset,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

	hash_pool[session_id]->target_buf_old_map[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_old_map[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_old_map[hash_id],
			target_buf_old_map,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}

	hash_pool[session_id]->target_buf_new_map[hash_id] =
		kcalloc(hash_pool[session_id]->rp_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (hash_pool[session_id]->target_buf_new_map[hash_id] != NULL) {
		memcpy(hash_pool[session_id]->target_buf_new_map[hash_id],
			target_buf_new_map,
			hash_pool[session_id]->rp_num[hash_id]*sizeof(uint32_t));
	} else {
		return -ENOMEM;
	}
#endif

	return 0;
}

bool get_hash_info(void *session,
						uint32_t batch_name_hash,
						uint32_t *session_id,
						uint32_t *hash_id,
						uint32_t buf_num)
{
	if (batch_name_hash == 0x0)
		return false;

	*session_id = get_saved_session_id(session);

	if (*session_id == -1)
		return false;

	*hash_id = get_saved_hash_id(*session_id, batch_name_hash);

	if (*hash_id == -1)
		return false;
	else if (*hash_id == hash_pool[*session_id]->hash_oldest)
		hash_pool[*session_id]->hash_oldest++;

	if (hash_pool[*session_id]->buf_num[*hash_id] != buf_num)
		return false;

	return true;
}


int replace_img_knl(void *session,
					uint32_t buf_num,
					uint32_t *sec_chk_addr,
					uint32_t *sec_buf_attr,
					uint32_t rp_num,
					uint32_t *target_buf_old_map,
					uint32_t *target_buf_old_base,
					uint32_t *target_buf_old_offset,
					uint32_t *target_buf_new_map,
					uint32_t *target_buf_new_base,
					uint32_t *target_buf_new_offset,
					uint32_t ker_bin_num,
					uint32_t *ker_bin_each_iova)
{
	int ret = 0;
	uint32_t cnt = 0;
	uint32_t ker_img_cnt = 0;
	void *buf_ptr;
	void *buf_ptr_base;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	for (cnt = 0; cnt < buf_num; cnt++) {
		if (sec_buf_attr[cnt] != BUF_KERNEL)
			continue;

		if (ker_img_cnt > ker_bin_num) {
			pr_info("[MVPU][IMG] [ERROR] User's KNL buf num > mvpu_algo.img Kernel_*.bin num %d\n",
						ker_bin_num);

			ret = -1;
			return ret;
		}

		sec_chk_addr[cnt] = ker_bin_each_iova[ker_img_cnt];

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
			pr_info("[MVPU][Sec] set target_buf[%d] = 0x%08x, from partition\n",
							cnt, sec_chk_addr[cnt]);

		ker_img_cnt++;
	}

	for (cnt = 0; cnt < rp_num; cnt++) {
		buf_ptr_base = apusys_mem_query_kva_by_sess(session, target_buf_old_base[cnt]);
		buf_ptr = (void *)((uintptr_t)buf_ptr_base
							+ target_buf_old_offset[cnt]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
			pr_info("[MVPU][Sec] DRV rp cnt %03d, replace *[0x%llx + 0x%08x] from 0x%08x to [0x%08x + 0x%08x]\n",
					cnt,
					target_buf_old_base[cnt],
					target_buf_old_offset[cnt],
					*((uint32_t *)buf_ptr),
					sec_chk_addr[target_buf_new_map[cnt]],
					target_buf_new_offset[cnt]);
		}

		// replacement, set new values
		*((uint32_t *)buf_ptr) =
			(uint32_t)(sec_chk_addr[target_buf_new_map[cnt]]
				+ target_buf_new_offset[cnt]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
			pr_info("[MVPU][Sec] new value: 0x%08x\n",
					*((uint32_t *)buf_ptr));
	}

	return ret;
}


bool set_rp_skip_buf(uint32_t session_id,
							uint32_t hash_id,
							uint32_t buf_num,
							uint32_t *sec_chk_addr,
							uint32_t *sec_buf_attr,
							uint32_t *rp_skip_buf)
{
	int i = 0;
	bool algo_all_same_buf = true;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	for (i = 0; i < buf_num; i++) {
		if (sec_buf_attr[i] != BUF_IO) {
			rp_skip_buf[i] = 1;
			continue;
		}

		if (hash_pool[session_id]->sec_chk_addr[hash_id][i] == sec_chk_addr[i])
			rp_skip_buf[i] = 1;
		else
			algo_all_same_buf = false;
	}

	return algo_all_same_buf;
}

int update_new_base_addr(bool algo_in_img,
						bool algo_in_pool,
						uint32_t session_id,
						uint32_t hash_id,
						uint32_t *sec_chk_addr,
						uint32_t *sec_buf_attr,
						uint32_t *rp_skip_buf,
						uint32_t rp_num,
						uint32_t *target_buf_new_map,
						uint32_t *target_buf_new_base,
						uint32_t ker_bin_num,
						uint32_t *ker_bin_each_iova,
						void *kreg_kva)
{
	int ret = 0;
	uint32_t cnt = 0;
	uint32_t ker_img_cnt = 0;
	uint32_t *target_pool_addr;
	uint32_t target_pool_ofst = 0;
	uint32_t *rp_buf_new_base;
	uint32_t *rp_buf_new_map = NULL;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	target_pool_addr =
		kcalloc(hash_pool[session_id]->buf_num[hash_id], sizeof(uint32_t), GFP_KERNEL);

	if (!target_pool_addr)
		return -ENOMEM;

#ifdef FULL_RP_INFO
	if (algo_in_pool == true) {
		rp_buf_new_base = hash_pool[session_id]->target_buf_new_base[hash_id];
		rp_buf_new_map = hash_pool[session_id]->target_buf_new_map[hash_id];
	} else {
		rp_buf_new_base = target_buf_new_base;
		rp_buf_new_map = target_buf_new_map;
	}
#else
	rp_buf_new_base = target_buf_new_base;
	rp_buf_new_map = target_buf_new_map;
#endif

	// update pool addr
	for (cnt = 0; cnt < hash_pool[session_id]->buf_num[hash_id]; cnt++) {
		if (rp_skip_buf[cnt] == 1)
			continue;

		if (algo_in_img && sec_buf_attr[cnt] == BUF_KERNEL) {
			if (ker_img_cnt > ker_bin_num) {
				pr_info("[MVPU][IMG] [ERROR] User's KNL buf num > mvpu_algo.img Kernel_*.bin num %d\n",
							ker_bin_num);
				kfree(target_pool_addr);
				ret = -1;
				return ret;
			}

			target_pool_addr[cnt] = ker_bin_each_iova[ker_img_cnt];

			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
				pr_info("[MVPU][Sec] set target_pool_addr[%d] = 0x%08x, from partition\n",
								cnt, target_pool_addr[cnt]);

			ker_img_cnt++;
		} else if (sec_buf_attr[cnt] == BUF_IO) {
			if (algo_in_pool == false)
				continue;
			else
				target_pool_addr[cnt] = sec_chk_addr[cnt];
		} else {
			target_pool_ofst = hash_pool[session_id]->hash_offset[hash_id][cnt];
			target_pool_addr[cnt] = hash_pool[session_id]->hash_base_iova[hash_id]
									+ target_pool_ofst;

			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
				pr_info("[MVPU][Sec] set target_pool_addr[%d]: 0x%08x\n",
						cnt, target_pool_addr[cnt]);
		}
	}

	// update new base addr to pool addr with buf_map
	for (cnt = 0; cnt < rp_num; cnt++) {
#ifdef MVPU_SEC_USE_MEM_POOL
		if (rp_skip_buf[rp_buf_new_map[cnt]] == 1)
			continue;

		if (rp_buf_new_base[cnt] != 0) {
			if (algo_in_pool == false &&
				(sec_buf_attr[rp_buf_new_map[cnt]] == BUF_IO))
				continue;

			rp_buf_new_base[cnt] = target_pool_addr[rp_buf_new_map[cnt]];
		}
#endif

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
			pr_info("[MVPU][Sec] update target_buf_new_base[%02d]: 0x%08x\n",
					cnt, rp_buf_new_base[cnt]);
	}

	kfree(target_pool_addr);

	return ret;
}

int replace_mem(uint32_t session_id,
					uint32_t hash_id,
					uint32_t *sec_buf_attr,
					bool algo_in_pool,
					uint32_t *rp_skip_buf,
					uint32_t rp_num,
					uint32_t *target_buf_old_map,
					uint32_t *target_buf_old_base,
					uint32_t *target_buf_old_offset,
					uint32_t *target_buf_new_map,
					uint32_t *target_buf_new_base,
					uint32_t *target_buf_new_offset,
					void *kreg_kva)
{
	int ret = 0;
	int cnt = 0;

	// get *kva
	void *buf_ptr_base;
	void *buf_ptr;
	uint32_t target_pool_ofst = 0;

	uint32_t *rp_buf_old_map;
	uint32_t *rp_buf_old_base;
	uint32_t *rp_buf_old_offset;
	uint32_t *rp_buf_new_map;
	uint32_t *rp_buf_new_base;
	uint32_t *rp_buf_new_offset;
	int ret_dma_buf_vmap = 0;
	struct dma_buf_map sys_map;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	ret_dma_buf_vmap = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id], &sys_map);
	buf_ptr_base = sys_map.vaddr;

	if ((ret_dma_buf_vmap != 0) || (!buf_ptr_base)) {
		pr_info("[MVPU][Sec] buf_ptr_base kva map failed\n");
		return -ENOMEM;
	}

	buf_ptr = buf_ptr_base;

#ifdef FULL_RP_INFO
	if (algo_in_pool == true) {
		rp_buf_old_map = hash_pool[session_id]->target_buf_old_map[hash_id];
		rp_buf_old_base = hash_pool[session_id]->target_buf_old_base[hash_id];
		rp_buf_old_offset = hash_pool[session_id]->target_buf_old_offset[hash_id];
		rp_buf_new_map = hash_pool[session_id]->target_buf_new_map[hash_id];
		rp_buf_new_base = hash_pool[session_id]->target_buf_new_base[hash_id];
		rp_buf_new_offset = hash_pool[session_id]->target_buf_new_offset[hash_id];
	} else {
		rp_buf_old_map = target_buf_old_map;
		rp_buf_old_base = target_buf_old_base;
		rp_buf_old_offset = target_buf_old_offset;
		rp_buf_new_map = target_buf_new_map;
		rp_buf_new_base = target_buf_new_base;
		rp_buf_new_offset = target_buf_new_offset;
	}
#else
	rp_buf_old_map = target_buf_old_map;
	rp_buf_old_base = target_buf_old_base;
	rp_buf_old_offset = target_buf_old_offset;
	rp_buf_new_map = target_buf_new_map;
	rp_buf_new_base = target_buf_new_base;
	rp_buf_new_offset = target_buf_new_offset;
#endif

	//cache sync
	dma_buf_begin_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	for (cnt = 0; cnt < rp_num; cnt++) {
		if (rp_skip_buf[rp_buf_new_map[cnt]] == 1)
			continue;

#ifndef MVPU_SEC_KREG_IN_POOL
		// get addr kva
		if (rp_buf_old_base[cnt] == 0) {
			buf_ptr = (void *)((uintptr_t)kreg_kva + rp_buf_old_offset[cnt]);
		} else {
			target_pool_ofst =
				hash_pool[session_id]
					->hash_offset[hash_id][rp_buf_old_map[cnt]];
			buf_ptr = (void *)((uintptr_t)buf_ptr_base
								+ target_pool_ofst
								+ rp_buf_old_offset[cnt]);
		}
#else
		target_pool_ofst =
				hash_pool[session_id]
					->hash_offset[hash_id][rp_buf_old_map[cnt]];
		buf_ptr = (void *)((uintptr_t)buf_ptr_base
							+ target_pool_ofst
							+ rp_buf_old_offset[cnt]);
#endif

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
			pr_info("[MVPU][Sec] DRV rp cnt %03d, replace *[0x%llx + 0x%08x] from 0x%08x to [0x%08x + 0x%08x]\n",
					cnt,
#ifndef MVPU_SEC_KREG_IN_POOL
					(rp_buf_old_base[cnt] == 0) ?
						((uintptr_t)kreg_kva):(rp_buf_old_base[cnt]),
#else
					rp_buf_old_base[cnt],
#endif
					rp_buf_old_offset[cnt],
					*((uint32_t *)buf_ptr),
					rp_buf_new_base[cnt],
					rp_buf_new_offset[cnt]);
		}

		// replacement, set new values
		if (*((uint32_t *)buf_ptr) != 0x80000000)
			*((uint32_t *)buf_ptr) =
				(uint32_t)(rp_buf_new_base[cnt] + rp_buf_new_offset[cnt]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
			pr_info("[MVPU][Sec] new value: 0x%08x\n",
					*((uint32_t *)buf_ptr));
	}

	//cache sync
	dma_buf_end_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (buf_ptr_base)
		dma_buf_vunmap(hash_pool[session_id]->hash_dma_buf[hash_id], buf_ptr_base);

	return ret;
}

void CopyArgToPrimem(char *dst_ptr, char *src_ptr, int src_size)
{
	char dup_buf[MVPU_DUP_BUF_SIZE] = { 0 };
	char *src_data;
	int i, j;

	for (i = 0; i < src_size; i += 2) {
		src_data = src_ptr + i;
		for (j = 0; j < MVPU_PE_NUM * 2; j += 2)
			memcpy(dup_buf + j, src_data, 2);

		memcpy(dst_ptr, dup_buf, MVPU_DUP_BUF_SIZE);
		dst_ptr = dst_ptr + MVPU_DUP_BUF_SIZE;
	}
}

void CheckPrimemArg(char *dst_ptr, int src_size)
{
	int i, j;

	for (i = 0; i < src_size; i += 2) {
		for (j = 0; j < MVPU_PE_NUM * 2; j += 2)
			pr_info("[MVPU][Sec] check primem_ptr 0x%02x%02x\n",
					*((char *)dst_ptr + j + 1), *((char *)dst_ptr + j));

		dst_ptr = dst_ptr + MVPU_DUP_BUF_SIZE;
	}
}

int replace_kerarg(void *session,
					uint32_t session_id,
					uint32_t hash_id,
					uint32_t kerarg_num,
					uint32_t *sec_chk_addr,
					uint32_t *kerarg_buf_id,
					uint32_t *kerarg_offset,
					uint32_t *kerarg_size,
					uint32_t primem_num,
					uint32_t *primem_src_buf_id,
					uint32_t *primem_dst_buf_id,
					uint32_t *primem_src_offset,
					uint32_t *primem_dst_offset,
					uint32_t *primem_size)
{
	int ret = 0;
	int cnt = 0;

	// get *kva
	void *sec_chk_addr_kva;
	void *pool_ptr_base;

	void *buf_ptr;
	void *kerarg_ptr;
	void *primem_ptr;

	uint32_t target_pool_ofst = 0;
	uint32_t target_src_ofst = 0;
	uint32_t target_dst_ofst = 0;
	int i;
	int ret_dma_buf_vmap = 0;
	struct dma_buf_map sys_map;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][Sec] %s\n", __func__);

	ret_dma_buf_vmap = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id], &sys_map);
	pool_ptr_base = sys_map.vaddr;

	if ((ret_dma_buf_vmap != 0) || (!pool_ptr_base)) {
		pr_info("[MVPU][Sec] buf_ptr_base kva map failed\n");
		return -ENOMEM;
	}

	//cache sync
	dma_buf_begin_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	for (cnt = 0; cnt < kerarg_num; cnt++) {
		sec_chk_addr_kva =
			apusys_mem_query_kva_by_sess(session, sec_chk_addr[kerarg_buf_id[cnt]]);
		buf_ptr = (void *)((uintptr_t)sec_chk_addr_kva
								+ kerarg_offset[cnt]);

		target_pool_ofst = hash_pool[session_id]->hash_offset[hash_id][kerarg_buf_id[cnt]];
		kerarg_ptr = (void *)((uintptr_t)pool_ptr_base
								+ target_pool_ofst
								+ kerarg_offset[cnt]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][Sec] kerarg cnt %03d, set kerarg[%03d][0x%llx] from buf[%03d][0x%llx] offset 0x%08x with 0x%x bytes\n",
					cnt,
					kerarg_buf_id[cnt],
					pool_ptr_base + target_pool_ofst,
					kerarg_buf_id[cnt],
					sec_chk_addr_kva,
					kerarg_offset[cnt],
					kerarg_size[cnt]);

			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
				for (i = 0; i < kerarg_size[cnt]; i++) {
					pr_info("[MVPU][Sec] set kerarg_ptr 0x%02x from buf_ptr 0x%02x\n",
						*((char *)kerarg_ptr + i), *((char *)buf_ptr + i));
				}
			}
		}

		memcpy(kerarg_ptr, buf_ptr, kerarg_size[cnt]);

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
			for (i = 0; i < kerarg_size[cnt]; i++) {
				pr_info("[MVPU][Sec] kerarg_ptr 0x%02x\n",
					*((char *)kerarg_ptr + i));
			}
		}

		// replace primem
		for (i = 0; i < primem_num; i++) {
			if ((kerarg_buf_id[cnt] == primem_src_buf_id[i]) &&
				(kerarg_offset[cnt] >= primem_src_offset[i]) &&
				(kerarg_offset[cnt] < primem_src_offset[i] + primem_size[i])) {
				target_pool_ofst =
					hash_pool[session_id]->hash_offset
						[hash_id][primem_dst_buf_id[i]];
				target_src_ofst = kerarg_offset[cnt] - primem_src_offset[i];
				target_dst_ofst =
					primem_dst_offset[i] + target_src_ofst * MVPU_PE_NUM;

				primem_ptr = (void *)((uintptr_t)pool_ptr_base
								+ target_pool_ofst
								+ target_dst_ofst);

				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
					pr_info("[MVPU][Sec] CopyArgToPrimem buf[%d]->buf[%d], ker_ofst 0x%08x, dst_ofst (0x%08x + 0x%08x), size 0x%x\n",
							primem_src_buf_id[i],
							primem_dst_buf_id[i],
							kerarg_offset[cnt],
							primem_dst_offset[i],
							target_dst_ofst,
							kerarg_size[cnt]);
				}

				CopyArgToPrimem(primem_ptr, kerarg_ptr, kerarg_size[cnt]);

				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
					CheckPrimemArg(primem_ptr, kerarg_size[cnt]);

				break;
			}
		}
	}

	//cache sync
	dma_buf_end_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (pool_ptr_base)
		dma_buf_vunmap(hash_pool[session_id]->hash_dma_buf[hash_id], pool_ptr_base);

	return ret;
}

void get_pool_kreg_iova(uint32_t *kreg_iova_pool,
						uint32_t session_id,
						uint32_t hash_id,
						uint32_t buf_cmd_kreg)
{
	*kreg_iova_pool = hash_pool[session_id]->hash_base_iova[hash_id]
				+ hash_pool[session_id]->hash_offset[hash_id][buf_cmd_kreg];
}

void region_sort(uint32_t region_num, uint32_t *region)
{
	int i = 0, j = 0;
	uint32_t tmp = 0;

	for (i = 0; i < region_num; i++) {
		for (j = i + 1; j < region_num; j++) {
			if (region[i] > region[j]) {
				tmp = region[i];
				region[i] = region[j];
				region[j] = tmp;
			}
		}
	}
}

int region_info_set(uint32_t buf_num,
							uint32_t *sec_chk_addr,
							uint32_t *sec_buf_size,
							uint32_t *sec_buf_attr,
							uint32_t *buf_io_addr,
							uint32_t *buf_io_size)
{
	int i = 0, j = 0;
	uint32_t buf_io_cnt = 0;
	uint32_t buf_io_total = 0;

	//get all IO buf addr
	for (i = 0; i < buf_num; i++) {
		if (sec_buf_attr[i] == BUF_IO) {
			buf_io_addr[buf_io_cnt] = sec_chk_addr[i];
			buf_io_cnt++;
		}
	}

	buf_io_total = buf_io_cnt;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
		pr_info("[MVPU][SEC] [MPU] buf_io_total = %3d\n", buf_io_total);
		for (i = 0; i < buf_io_total; i++)
			pr_info("[MVPU][SEC] [MPU] origin buf_io_addr[%3d] = 0x%08x\n",
						i, buf_io_addr[i]);
	}

	//Sorting
	region_sort(buf_io_total, buf_io_addr);

	//mapping size info
	for (i = 0; i < buf_io_total; i++) {
		for (j = 0; j < buf_num; j++) {
			if (buf_io_addr[i] == sec_chk_addr[j]) {
				buf_io_size[i] =
					((sec_buf_size[j] + MVPU_MPU_SIZE - 1)
						/MVPU_MPU_SIZE)
						*MVPU_MPU_SIZE;
			}
		}
	}

	//base aligned (TBD: 4K page alreadly?)
	for (i = 0; i < buf_io_total; i++)
		buf_io_addr[i] = buf_io_addr[i] & 0xFFFFF000;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
		pr_info("[MVPU][SEC] [MPU] %s buf_io_total = %3d\n", __func__, buf_io_total);
		for (i = 0; i < buf_io_total; i++) {
			pr_info("[MVPU][SEC] [MPU] sorted buf_io_addr[%3d] = 0x%08x (aligned)\n",
						i, buf_io_addr[i]);
			pr_info("                         buf_io_size[%3d] = 0x%08x (aligned)\n",
						i, buf_io_size[i]);
		}
	}

	return buf_io_total;
}

int region_merge(uint32_t region_num,
						uint32_t *curr_addr_list,
						uint32_t *curr_size_list,
						uint32_t *merged_region)
{
	int i = 0;

	uint32_t addr_curr = 0, addr_next = 0;
	uint32_t addr_curr_end = 0, addr_next_end = 0;

	int merged_region_cnt = 0;

	for (i = 0; i < region_num; i++) {
		addr_curr = curr_addr_list[i];
		addr_curr_end = addr_curr + curr_size_list[i];
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
			pr_info("[MVPU][Sec] [MPU] %s buf_curr[%3d] region 0x%08x - 0x%08x\n",
				__func__, i, addr_curr, addr_curr_end);

		while (i < region_num) {
			if (i == region_num - 1)
				break;

			addr_next = curr_addr_list[i + 1];
			addr_next_end = addr_next + curr_size_list[i + 1];
			if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
				pr_info("[MVPU][Sec] [MPU] %s buf_next[%3d] region 0x%08x - 0x%08x\n",
						__func__, i + 1, addr_next, addr_next_end);

			if (addr_curr_end >= addr_next) {
				addr_curr_end =
					(addr_next_end >= addr_curr_end) ?
						addr_next_end : addr_curr_end;
				i++;

				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
					pr_info("[MVPU][Sec] [MPU] %s merge region to 0x%08x - 0x%08x\n",
						__func__, addr_curr, addr_curr_end);
			} else {
				if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL)
					pr_info("[MVPU][Sec] [MPU] %s find next region\n",
						__func__);
				break;
			}
		}

		merged_region[merged_region_cnt] = addr_curr;
		merged_region[merged_region_cnt + 1] = addr_curr_end;
		merged_region_cnt = merged_region_cnt + 2;
	}

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][SEC] [MPU] IO region merged: %d to %d\n",
					region_num*2, merged_region_cnt);

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
		for (i = 0; i < merged_region_cnt; i++)
			pr_info("[MVPU][SEC] [MPU] merged_region[%3d] 0x%08x\n",
					i, merged_region[i]);
	}

	return merged_region_cnt;
}

int region_mpu_set(uint32_t session_id,
						uint32_t hash_id,
						uint32_t pmu_buff,
						uint32_t buff_size,
						uint32_t *mpu_seg,
						uint32_t buf_io_total_merged,
						uint32_t *buf_io_addr_merged)
{
	int i = 0;
	int buf_io_cnt = 0;
	int total_mpu_cnt = 0;

	//IO buffers
	for (i = 0; i < buf_io_total_merged; i++) {
		mpu_seg[buf_io_cnt] = buf_io_addr_merged[i];
		buf_io_cnt++;
	}

	//SYS buffers
	mpu_seg[buf_io_cnt + ITCM_BASE_SFT] = ITCM_VIRTUAL_BASE;
	mpu_seg[buf_io_cnt + ITCM_END_SFT]	= ITCM_VIRTUAL_BASE_END;
	mpu_seg[buf_io_cnt + TCM_BASE_SFT]	= VIRTUAL_APUSYS_TCM_BASE;
	mpu_seg[buf_io_cnt + TCM_END_SFT]	= VIRTUAL_APUSYS_TCM_BASE_END;

	mpu_seg[buf_io_cnt + IMG_BASE_SFT] = mvpu_algo_iova;
	if (mvpu_algo_available == true) {
		mpu_seg[buf_io_cnt + IMG_END_SFT] =
			(((mvpu_algo_iova + 32
				+ ptn_img_size
				+ knl_img_size)
				+ MVPU_MPU_SIZE - 1)
				/MVPU_MPU_SIZE)
				*MVPU_MPU_SIZE;
	} else {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][IMG] [MPU][WARN] set fake image region: 0x%08x\n",
				mvpu_algo_iova + MVPU_MPU_SIZE);
		mpu_seg[buf_io_cnt + IMG_END_SFT] = mvpu_algo_iova + MVPU_MPU_SIZE;
	}

	mpu_seg[buf_io_cnt + POOL_BASE_SFT] = hash_pool[session_id]->hash_base_iova[hash_id];
	mpu_seg[buf_io_cnt + POOL_END_SFT] =
		(((hash_pool[session_id]->hash_base_iova[hash_id] +
				hash_pool[session_id]->hash_pool_size[hash_id])
				+ MVPU_MPU_SIZE - 1)
				/MVPU_MPU_SIZE)
				*MVPU_MPU_SIZE;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][SEC] [MPU] set algo region: 0x%08x - 0x%08x\n",
			mpu_seg[buf_io_cnt + IMG_BASE_SFT], mpu_seg[buf_io_cnt + IMG_END_SFT]);
		pr_info("[MVPU][SEC] [MPU] set pool region: 0x%08x - 0x%08x\n",
			mpu_seg[buf_io_cnt + POOL_BASE_SFT], mpu_seg[buf_io_cnt + POOL_END_SFT]);
	}

	if (pmu_buff != 0) {
		mpu_seg[buf_io_cnt + PMU_BASE_SFT] =
			((pmu_buff + MVPU_MPU_SIZE - 1)
				/MVPU_MPU_SIZE)
				*MVPU_MPU_SIZE;

		mpu_seg[buf_io_cnt + PMU_END_SFT] =
			(((pmu_buff + buff_size) + MVPU_MPU_SIZE - 1)
				/MVPU_MPU_SIZE)
				*MVPU_MPU_SIZE;

		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][SEC] [MPU] set PMU  region: 0x%08x - 0x%08x\n",
				mpu_seg[buf_io_cnt + PMU_BASE_SFT],
				mpu_seg[buf_io_cnt + PMU_END_SFT]);
		}

		buf_io_cnt = buf_io_cnt + PMU_END_SFT;
	} else {
		buf_io_cnt = buf_io_cnt + POOL_END_SFT;
	}

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_ALL) {
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] sec mpu_reg[%3d] = 0x%08x\n",
						i, mpu_seg[i]);
	}

	//Sorting
	region_sort(MVPU_MPU_SEGMENT_NUMS, mpu_seg);

	total_mpu_cnt = buf_io_cnt + 1;
	return total_mpu_cnt;
}

int add_img_mpu(void *mvpu_cmd)
{
	struct mvpu_request *mvpu_req;

	int ret = 0;
	int i = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][SEC] %s\n", __func__);

	mvpu_req = (struct mvpu_request *)mvpu_cmd;

	if (mvpu_req->mpu_num >= MVPU_MPU_SEGMENT_NUMS - 3) {
		mvpu_req->mpu_num = 0;
		memset(mvpu_req->mpu_seg, 0, sizeof(mvpu_req->mpu_seg));
	} else {
		mvpu_req->mpu_seg[0] = mvpu_algo_iova;
		mvpu_req->mpu_seg[1] =
			(((mvpu_algo_iova + 32
				+ ptn_img_size
				+ knl_img_size)
				+ MVPU_MPU_SIZE - 1)
				/MVPU_MPU_SIZE)
				*MVPU_MPU_SIZE;

		region_sort(MVPU_MPU_SEGMENT_NUMS, mvpu_req->mpu_seg);
		mvpu_req->mpu_num = mvpu_req->mpu_num + 2;
	}

	if (mvpu_req->mpu_num != 0 && mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] drv mpu_reg[%3d] = 0x%08x\n",
						i, mvpu_req->mpu_seg[i]);
	}

	return ret;
}


int update_mpu(void *mvpu_cmd,
					uint32_t session_id,
					uint32_t hash_id,
					uint32_t *sec_chk_addr,
					uint32_t *sec_buf_size,
					uint32_t *sec_buf_attr)
{
	uint32_t mpu_seg[MVPU_MPU_SEGMENT_NUMS] = {0};
	struct mvpu_request *mvpu_req;

	int ret = 0;
	int i = 0;
	uint32_t buf_num = 0;

	uint32_t buf_io_total = 0;
	uint32_t buf_io_total_merged = 0;

	uint32_t *buf_io_addr = NULL;
	uint32_t *buf_io_size = NULL;
	uint32_t *buf_io_addr_merged = NULL;

	uint32_t total_mpu_cnt = 0;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU][SEC] %s\n", __func__);

	mvpu_req = (struct mvpu_request *)mvpu_cmd;
	buf_num = mvpu_req->buf_num & BUF_NUM_MASK;

#ifdef MVPU_SEC_CLEAR_MPU
	//clear MPU
	mvpu_req->mpu_num = 0;
	memset(mvpu_req->mpu_seg, 0, sizeof(mvpu_req->mpu_seg));
#endif

	//get total IO buffers
	for (i = 0; i < buf_num; i++) {
		if (sec_buf_attr[i] == BUF_IO)
			buf_io_total++;
	}

	buf_io_addr = kcalloc(buf_io_total, sizeof(uint32_t), GFP_KERNEL);
	if (!buf_io_addr) {
		ret = -ENOMEM;
		goto END;
	}

	buf_io_size = kcalloc(buf_io_total, sizeof(uint32_t), GFP_KERNEL);
	if (!buf_io_size) {
		ret = -ENOMEM;
		goto END;
	}

	buf_io_total = region_info_set(buf_num,
						sec_chk_addr, sec_buf_size, sec_buf_attr,
						buf_io_addr, buf_io_size);

	//try to merge IO buf region
	buf_io_addr_merged = kcalloc(buf_io_total*2, sizeof(uint32_t), GFP_KERNEL);
	if (!buf_io_addr_merged) {
		ret = -ENOMEM;
		goto END;
	}

	buf_io_total_merged =
			region_merge(buf_io_total, buf_io_addr, buf_io_size, buf_io_addr_merged);

	if (buf_io_total_merged > (MVPU_MPU_SEGMENT_NUMS - SYS_BUF_NUM)) {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][SEC] [MPU][NOTICE] IO buff num %d is too much\n",
				buf_io_total);

#ifdef MVPU_SEC_MPU_NUM_BLOCK
		ret = -1;
#else
		ret = 0;
#endif
		goto END;
	}

	total_mpu_cnt = region_mpu_set(session_id, hash_id,
						mvpu_req->pmu_buff, mvpu_req->buff_size,
						mpu_seg, buf_io_total_merged, buf_io_addr_merged);

#ifdef MVPU_SEC_UPDT_MPU
	//set MPU
	mvpu_req->mpu_num = total_mpu_cnt;
	memcpy(mvpu_req->mpu_seg, mpu_seg, sizeof(mvpu_req->mpu_seg));
#else
	pr_info("[MVPU][SEC] [MPU] Bypass MPU setting\n");
#endif

END:
	kfree(buf_io_addr);
	kfree(buf_io_size);
	kfree(buf_io_addr_merged);

	return ret;
}

bool mem_use_iova(uint32_t addr)
{
	if ((addr == 0) ||
		((addr & 0x02000000) == 0x02000000) ||
		((addr & 0x19600000) == 0x19600000) ||
		((addr & 0xFFFC0000) == 0))
		return false;

	return true;
}

int check_iova(void *session,
					void *cmd,
					uint32_t desc_type,
					uint32_t chk_num,
					uint32_t chk_base,
					uint32_t chk_size)
{
	int ret = 0;
	int i = 0;

	uint32_t desc_size = 64;

	uint32_t *desc_ptr;
	uint32_t desc_addr_ofst = 0;
	uint32_t desc_addr = 0;

	if (mem_use_iova(chk_base) == false)
		return 0;

	switch (desc_type) {
	case DESC_TYPE_NONE:
		if (apusys_mem_validate_by_cmd(session, cmd,
			chk_base, chk_size) != 0) {
			pr_info("[MVPU][Sec] [ERROR] ker mem 0x%08x integrity checked FAIL\n",
						chk_base);
			ret = -EINVAL;
			goto END;
		}
		break;
	case DESC_TYPE_GLSU:
	case DESC_TYPE_EDMA:
		if (chk_num == 0)
			goto END;

		if (apusys_mem_validate_by_cmd(session, cmd,
			chk_base, chk_num*desc_size) != 0) {
			pr_info("[MVPU][Sec] [ERROR] desc base 0x%08x integrity checked FAIL\n",
						chk_base);
			ret = -EINVAL;
			goto END;
		}

		desc_ptr = (uint32_t *)apusys_mem_query_kva_by_sess(session, chk_base);

		if (desc_ptr == NULL) {
			pr_info("[MVPU] %s, desc_ptr == NULL\n", __func__);
			ret = -EINVAL;
			goto END;
		}

		if (desc_type == DESC_TYPE_GLSU)
			desc_addr_ofst = 1;
		else
			desc_addr_ofst = 7;

		for (i = 0; i < 2; i++) {
			desc_addr = desc_ptr[desc_addr_ofst + i];

			if (mem_use_iova(desc_addr) == false)
				continue;

			if (apusys_mem_validate_by_cmd(session, cmd,
					desc_addr, 0) != 0) {
				pr_info("[MVPU][Sec] [ERROR] desc[%d][%d] addr 0x%08x integrity checked FAIL\n",
						desc_type, desc_addr_ofst + i, desc_addr);
				ret = -EINVAL;
				goto END;
			}
		}
		break;
	default:
		break;
	}

END:
	return ret;
}

int check_batch_flow(void *session,
						void *cmd,
						uint32_t sec_level,
						uint32_t *kreg_kva,
						uint32_t knl_num)
{
	int ret = 0;
	int i = 0, j = 0;

	uint32_t chk_num = 0;
	uint32_t chk_base = 0;
	uint32_t chk_size = 0;

	uint32_t desc[8] = {7, 8, 10, 35, 36, 38};
	uint32_t sample = 1;

	uint32_t *buf_ptr;

	buf_ptr = (uint32_t *)((uintptr_t)kreg_kva);

	if (sec_level != SEC_LVL_CHECK_ALL) {
		sample = get_random_u32()%100;

		if (sample == 0)
			sample = 1;
	}

	for (i = 0; i < knl_num; i = i + sample) {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][CHK] knl_num %d, check %03d\n", knl_num, i);

		buf_ptr = (uint32_t *)((uintptr_t)kreg_kva + i*72);

		if (buf_ptr == NULL)
			goto END;

		if ((buf_ptr[0] & 0x2) != 0) {
			chk_base = buf_ptr[KREG_OFST_26];
			chk_size = buf_ptr[KREG_OFST_28];

			ret = check_iova(session, cmd, DESC_TYPE_NONE, 0, chk_base, chk_size);
			if (ret != 0) {
				pr_info("[MVPU][Sec] [ERROR] KREG[%03d][%d] instr 0x%08x FAIL\n",
					i, KREG_OFST_26, chk_base);
				goto END;
			}
		}

		for (j = 0; j < 6; j = j + 3) {
			chk_num = buf_ptr[desc[j]] & 0x0000FFFF;
			chk_base = buf_ptr[desc[j + 1]];
			chk_size = 0;

			ret = check_iova(session, cmd, DESC_TYPE_GLSU, chk_num, chk_base, chk_size);
			if (ret != 0) {
				pr_info("[MVPU][Sec] [ERROR] KREG[%03d] desc 0x%08x FAIL\n",
					i, chk_base);
				goto END;
			}

			chk_num = (buf_ptr[desc[j]] & 0xFFFF0000) >> 16;
			chk_base = buf_ptr[desc[j + 2]];
			chk_size = 0;

			ret = check_iova(session, cmd, DESC_TYPE_EDMA, chk_num, chk_base, chk_size);
			if (ret != 0) {
				pr_info("[MVPU][Sec] [ERROR] KREG[%03d] desc 0x%08x FAIL\n",
					i, chk_base);
				goto END;
			}
		}
	}

END:
	return ret;
}

int mvpu_load_img(struct device *dev)
{
	int ret = 0;
	struct device_node *mvpu_sec_mem_node;
	struct reserved_mem *mvpu_algo;
	phys_addr_t pa;
	phys_addr_t size;

	if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
		pr_info("[MVPU] %s start\n", __func__);

	mvpu_sec_mem_node = of_find_compatible_node(NULL, NULL, "mediatek,apu_mvpu_algo");
	if (!mvpu_sec_mem_node) {
		dev_info(dev, "(f:%s/l:%d) DT,mediatek,mvpu_algo not found\n", __func__, __LINE__);
		ret = -EINVAL;
		goto END;
	}

	mvpu_algo = of_reserved_mem_lookup(mvpu_sec_mem_node);
	pa = mvpu_algo->base;
	size = mvpu_algo->size;

	mvpu_algo_img = (uint32_t *)phys_to_virt(pa);
	if (!mvpu_algo_img) {
		// user may not need image partition
		mvpu_algo_available = false;
		goto END;
	}

	mvpu_algo_iova =
		(uint32_t)dma_map_single_attrs(dev, mvpu_algo_img, size,
						DMA_FROM_DEVICE, DMA_ATTR_SKIP_CPU_SYNC);

	if (dma_mapping_error(dev, mvpu_algo_iova)) {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x, VA: 0x%llx\n",
					mvpu_algo_iova, mvpu_algo_img);
		} else {
			pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x\n",
					mvpu_algo_iova);
		}
	} else {
		if (mvpu_loglvl_sec >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][SEC] get mvpu_algo_iova: 0x%08x\n",
					mvpu_algo_iova);
	}

	ker_img_offset = get_ker_img_offset();
	//ker_img_iova = mvpu_algo_iova + ker_img_offset;

	ptn_img_size = get_ptn_total_size();
	knl_img_size = get_kerbin_total_size();
	if ((ptn_img_size % 4 == 0) && (knl_img_size % 4 == 0)) {
		mvpu_algo_available = true;
	} else {
		mvpu_algo_available = false;
		pr_info("[MVPU][IMG] [WARN] get mvpu_algo.img size error, PTN: 0x%08x, KNL: 0x%08x\n",
					ptn_img_size, knl_img_size);
	}

END:
	return ret;
}

int mvpu_sec_init(struct device *dev)
{
	int ret = 0;
	uint32_t session_id = 0;
	uint32_t hash_id = 0;

	mvpu_dev = dev;

	//image settings
	mvpu_algo_iova = 0x0;

	ker_img_offset = 0x0;
	//ker_img_iova = 0x0;

	mvpu_loglvl_sec = 0;

	//mem pool settings
	sess_oldest = 0;

	for (session_id = 0; session_id < MAX_SAVE_SESSION; session_id++) {
		saved_session[session_id] = (uint64_t)(-1);

/*
		saved_session[session_id] = kmalloc(sizeof(uint64_t), GFP_KERNEL);
		if (!saved_session[session_id]) {
			ret = -ENOMEM;
			goto END;
		} else {
			memset(saved_session[session_id], -1, sizeof(uint64_t));
		}
*/

		hash_pool[session_id] = kzalloc(sizeof(struct mvpu_hash_pool), GFP_KERNEL);
		if (!hash_pool[session_id]) {
			ret = -ENOMEM;
			goto END;
		}

		for (hash_id = 0; hash_id < MAX_SAVE_HASH; hash_id++)
			hash_pool[session_id]->hash_list[hash_id] = 0;

		hash_pool[session_id]->hash_oldest = 0;
	}

END:
	return ret;
}


