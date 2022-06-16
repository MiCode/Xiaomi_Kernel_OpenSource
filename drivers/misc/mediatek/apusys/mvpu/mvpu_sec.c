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

#define MVPU_SEC_MEM_POOL
#define MVPU_SEC_RP_ADDR

enum buffer_attr {
	BUF_NORMAL = 0,
	BUF_KERNEL,
	BUF_IO,
	BUF_RINGBUFFER,
};

uint32_t get_ptn_total_size(void)
{
	uint32_t ptn_total_size = mvpu_algo_img[3];
	//printf("[SEC_IMG] PTN total size: 0x%08x\n", ptn_total_size);
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
		if (ptn_hash == hash) {
			ptn_size = mvpu_algo_img[shift + KER_SIZE_OFFSET];
			break;
		}

		// count ptn.bin size to shift
		ptn_size_offset += mvpu_algo_img[shift + PNT_SIZE_OFFSET];

		// alignment
		ptn_size_offset = ptn_size_offset + 4 - (ptn_size_offset % 4);
	}

	if (ptn_hash != hash)
		pr_info("[MVPU][Sec] PTN HASH: 0x%08x not found\n", hash);

	//printf("[SEC_IMG] get PTN HASH: 0x%08x, img addr 0x%08x, size: 0x%08x\n", ptn_hash, img_addr, ptn_size);
	return ptn_size;
}

uint32_t get_ker_img_offset(void)
{
	uint32_t offset = mvpu_algo_img[3] + 0x10;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] ker_img_offset : 0x%08x\n", offset);
#endif

	return offset;
}

//static bool get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_size, uint32_t *ker_bin_num)
bool get_ker_info(uint32_t hash, uint32_t *ker_bin_offset, uint32_t *ker_bin_num)
{
	uint32_t ker_img_total_num = mvpu_algo_img[ker_img_offset/sizeof(uint32_t) + KER_NUM_OFFSET];

	uint32_t shift = 0;
	uint32_t ker_hash = 0;
	uint32_t ker_size_offset = 0;
	int i = 0;

	for (i = 0; i < ker_img_total_num; i++) {
		// get hash by ker.bin size
		shift = ker_img_offset/sizeof(uint32_t) + IMG_HEADER_SIZE + ker_size_offset/sizeof(uint32_t) + KER_INFO_SIZE*i;
		ker_hash = mvpu_algo_img[shift];

#ifdef MVPU_SEC_DEBUG_ADV
		pr_info("[MVPU][Sec] image hash[%d]: 0x%08x\n", i, ker_hash);
#endif

		// check hash
		if (ker_hash == hash) {
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

	if (ker_hash != hash) {
		pr_info("[MVPU][Sec] KER HASH: 0x%08x not found in mvpu image\n", hash);
		return false;
	}

	//pr_info("[SEC_IMG] get KER HASH: 0x%08x, img addr 0x%08x, addr: 0x%08x\n", ker_hash, ker_img_addr, ker_addr);
	return true;
}

void set_ker_iova(uint32_t ker_bin_offset, uint32_t ker_bin_num, uint32_t *ker_bin_each_iova)
{
	uint32_t shift = 0;
	uint32_t ker_size_offset = 0;
	int i = 0;

	pr_info("[MVPU][Sec] %s\n", __func__);

	for (i = 0; i < ker_bin_num; i++) {
		shift = ker_bin_offset/sizeof(uint32_t) + ker_size_offset/sizeof(uint32_t) + KER_BIN_INFO_SIZE*i;
		//shift = 4*(ker_size_offset/sizeof(uint32_t) + KER_BIN_INFO_SIZE*i);
		ker_bin_each_iova[i] = (shift + KER_BIN_INFO_SIZE)*sizeof(uint32_t)
							+ mvpu_algo_iova;

		ker_size_offset += mvpu_algo_img[shift + KER_SIZE_OFFSET];
	}
}

void map_base_buf_id(uint32_t buf_num, uint32_t *sec_chk_addr, uint32_t rp_num, uint32_t *target_map, uint32_t *target_base)
{
	int i = 0, j = 0;
	bool buf_mapped = false;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] === %s ===\n", __func__);
#endif

	for (j = 0; j < rp_num; j++) {
		buf_mapped = false;
		for (i = 0; i < buf_num; i++) {
			if (target_base[j] == sec_chk_addr[i]) {
				target_map[j] = i;
				buf_mapped = true;
#ifdef MVPU_SEC_DEBUG_ADV
				if (sec_chk_addr[i] == 0x0) {
					pr_info("[MVPU][Sec] NOTICE: set target_base[%d]: 0x%08x map to sec_chk_addr[%d]: 0x%08x\n",
							j, target_base[j],
							target_map[j],
							sec_chk_addr[target_map[j]]);
				}
#endif
			}
		}

		if (buf_mapped == false)
			pr_info("[MVPU][Sec] NOTICE: target_base[%d]: 0x%08x not mapped\n",
						j, target_base[j]);
	}
}

uint32_t get_saved_session_id(void *session)
{
	uint32_t cnt = 0;
	uint32_t session_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_SESSION; cnt++) {
		if (saved_session[cnt] == NULL) {
			pr_info("[MVPU][Sec] error: saved_session[%d] is NULL\n",
						cnt);
			continue;
		}

		if (*((uint64_t *)saved_session[cnt]) == -1)
			continue;

		if (memcmp(session, saved_session[cnt], sizeof(uint64_t)) == 0) {
#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] Get saved_session at %d: 0x%llx\n",
						cnt, saved_session[cnt]);
#endif
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
		if (saved_session[cnt] == NULL) {
			pr_info("[MVPU][Sec] error: saved_session[%d] is NULL\n",
						cnt);
			continue;
		}

		if (*((uint64_t *)saved_session[cnt]) == -1) {
			session_id = cnt;
			pr_info("[MVPU][Sec] Get available session id %d\n",
						session_id);
			break;
		}
	}

	// all session place are used, take the oldest
	if (session_id == -1) {
		session_id = sess_oldest;

		pr_info("[MVPU][Sec] Take oldest session id %d\n",
					session_id);
	}

	return session_id;
}


void clear_session(void *session)
{
	uint32_t session_id = 0;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] %s\n", __func__);
#endif

	for (session_id = 0; session_id < MAX_SAVE_SESSION; session_id++) {
		if (saved_session[session_id] == NULL) {
			pr_info("[MVPU][Sec] error: saved_session[%d] is NULL\n",
						session_id);
			continue;
		}

		if (*((uint64_t *)saved_session[session_id]) == (uint64_t)(-1))
			continue;

		if (memcmp(session, saved_session[session_id], sizeof(uint64_t)) == 0) {
#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] apusys session %llx match saved_session[%d]\n",
					*((uint64_t *)session),
					session_id);
#endif
			free_all_hash(session_id);
			memset(saved_session[session_id], -1, sizeof(uint64_t));
			break;
		}
	}
}

void update_session_id(uint32_t session_id, void *session)
{
	sess_oldest++;
	if (sess_oldest == MAX_SAVE_SESSION)
		sess_oldest = 0;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] set oldest session_id: %d\n", sess_oldest);
#endif

	if (saved_session[session_id] != NULL && session != NULL) {
		memcpy(saved_session[session_id], session, sizeof(uint64_t));
#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][Sec] set saved_session[%d]: 0x%llx to apu_session: 0x%llx\n",
				session_id,
				*((uint64_t *)saved_session[session_id]),
				*((uint64_t *)session));
#endif
	}
}

uint32_t get_saved_hash_id(uint32_t session_id, uint32_t batch_name_hash)
{
	uint32_t cnt = 0;
	uint32_t hash_id = -1;

	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (batch_name_hash == hash_pool[session_id]->hash_list[cnt]) {
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
			pr_info("[MVPU][Sec] Get available hash id %d\n", hash_id);
			break;
		}
	}

	// all hash place are used, take the oldest
	if (hash_id == -1) {
		hash_id = hash_pool[session_id]->hash_oldest;

		pr_info("[MVPU][Sec] Take oldest hash id %d\n", hash_id);
	}

	return hash_id;
}

void clear_hash(uint32_t session_id, uint32_t hash_id)
{
	// clear old hash settings
	if (hash_pool[session_id]->hash_list[hash_id] != 0) {
		hash_pool[session_id]->buf_num = 0;
		hash_pool[session_id]->hash_list[hash_id] = 0;

		dma_buf_unmap_attachment(hash_pool[session_id]->attach[hash_id], hash_pool[session_id]->sgt[hash_id], DMA_BIDIRECTIONAL);
		dma_buf_detach(hash_pool[session_id]->hash_dma_buf[hash_id], hash_pool[session_id]->attach[hash_id]);
		dma_heap_buffer_free(hash_pool[session_id]->hash_dma_buf[hash_id]);

		hash_pool[session_id]->hash_base_iova[hash_id] = 0;

		kfree(hash_pool[session_id]->hash_offset[hash_id]);

#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][Sec] Clear hash_id[%d] settings\n",
					hash_id);
#endif
	} else {
#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][Sec] bypass Clear hash_id[%d] settings\n",
					hash_id);
#endif
	}
}

void free_all_hash(uint32_t session_id)
{
	uint32_t hash_id = 0;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] %s saved_session[%d]\n", __func__, session_id);
#endif

	for (hash_id = 0; hash_id < MAX_SAVE_HASH; hash_id++) {
		if (hash_pool[session_id]->hash_list[hash_id] != 0)
			clear_hash(session_id, hash_id);
		else
			break;
	}
}

int update_hash_pool(void *session, bool algo_in_img,
					uint32_t session_id, uint32_t hash_id,
					uint32_t batch_name_hash, uint32_t buf_num,
					uint32_t *sec_chk_addr, uint32_t *sec_buf_size,
					uint32_t *mem_is_kernel)
{
	uint32_t cnt = 0;
	uint32_t buf_size = 0;
	uint32_t total_buf_size = 0;
	uint32_t buf_ofst = 0;
	void *p_buf;
	void *cp_buff;
	void *buf_kva;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU][Sec] %s\n", __func__);
#endif

	hash_pool[session_id]->buf_num = buf_num;
	hash_pool[session_id]->hash_list[hash_id] = batch_name_hash;
	hash_pool[session_id]->hash_oldest++;
	if (hash_pool[session_id]->hash_oldest >= MAX_SAVE_HASH)
		hash_pool[session_id]->hash_oldest = 0;

	hash_pool[session_id]->dma_heap[hash_id] = dma_heap_find("mtk_mm");

	if (!hash_pool[session_id]->dma_heap[hash_id]) {
		pr_info("[MVPU][Sec] heap find fail\n");
		return -1;
	}

	// count buf size
	if (algo_in_img) {
		//TBD: get PTN size from image is faster?
		total_buf_size = get_ptn_size(batch_name_hash);
	} else {
		for (cnt = 0; cnt < hash_pool[session_id]->buf_num; cnt++) {
			buf_size = sec_buf_size[cnt];

			buf_size = ((buf_size + 16 - 1)/16)*16;
			total_buf_size += buf_size;
		}
	}

	hash_pool[session_id]->hash_dma_buf[hash_id] = dma_heap_buffer_alloc(hash_pool[session_id]->dma_heap[hash_id], total_buf_size, O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(hash_pool[session_id]->hash_dma_buf[hash_id])) {
		pr_info("[MVPU][Sec] buffer alloc fail\n");
		return PTR_ERR(hash_pool[session_id]->hash_dma_buf[hash_id]);
	}

	if (mvpu_dev == NULL) {
		pr_info("[MVPU][Sec] mvpu_dev is NULL\n");
		return -1;
	}

	hash_pool[session_id]->attach[hash_id] = dma_buf_attach(hash_pool[session_id]->hash_dma_buf[hash_id], mvpu_dev);
	if (IS_ERR(hash_pool[session_id]->attach[hash_id])) {
		pr_info("[MVPU][Sec] attach fail, return\n");
		return PTR_ERR(hash_pool[session_id]->attach[hash_id]);
	}

	hash_pool[session_id]->sgt[hash_id] = dma_buf_map_attachment(hash_pool[session_id]->attach[hash_id], DMA_BIDIRECTIONAL);
	if (IS_ERR(hash_pool[session_id]->sgt[hash_id])) {
		pr_info("[MVPU][Sec] map failed, detach and return\n");
		dma_buf_detach(hash_pool[session_id]->hash_dma_buf[hash_id], hash_pool[session_id]->attach[hash_id]);
		return PTR_ERR(hash_pool[session_id]->sgt[hash_id]);
	}

	// get iova
	hash_pool[session_id]->hash_base_iova[hash_id] = (uint32_t)sg_dma_address(hash_pool[session_id]->sgt[hash_id]->sgl);
	pr_info("[MVPU][Sec] hash_base_iova 0x%08x\n",
				hash_pool[session_id]->hash_base_iova[hash_id]);

	// get *kva
	p_buf = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id]);
	//hash_pool[session_id]->hash_base_kva[hash_id] = (uint64_t *)p_buf;

	if (!p_buf) {
		pr_info("[MVPU][Sec] kva map failed\n");
		return -ENOMEM;
	}

	hash_pool[session_id]->hash_offset[hash_id] =
		kcalloc(hash_pool[session_id]->buf_num, sizeof(uint32_t), GFP_KERNEL);

	//cache sync
	dma_buf_begin_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (hash_pool[session_id]->hash_offset[hash_id] != NULL) {
		buf_size = 0;

		for (cnt = 0; cnt < hash_pool[session_id]->buf_num; cnt++) {
			//iova
			hash_pool[session_id]->hash_offset[hash_id][cnt] = buf_ofst;
			cp_buff = (void *)((uintptr_t)p_buf + buf_ofst);

			buf_size = sec_buf_size[cnt];
			buf_kva = apusys_mem_query_kva_by_sess(session, sec_chk_addr[cnt]);

#ifdef MVPU_SEC_DEBUG
			pr_info("[MVPU][Sec] buf[%d]: copy 0x%llx to 0x%llx with size 0x%08x, offset 0x%08x\n",
						cnt,
						(uintptr_t)buf_kva,
						(uintptr_t)cp_buff,
						buf_size,
						hash_pool[session_id]->hash_offset[hash_id][cnt]);
#endif

			if (buf_kva != 0x0) {
				if (algo_in_img && (mem_is_kernel[cnt] == BUF_KERNEL)) {
					pr_info("[MVPU][Sec] buf[%d]: use kernel from image\n",
								cnt);
				} else if (mem_is_kernel[cnt] == BUF_IO) {
					pr_info("[MVPU][Sec] buf[%d]: bypass IO buffer\n",
								cnt);
				} else {
					pr_info("[MVPU][Sec] buf[%d]: copy to pool\n",
								cnt);
					if (buf_size < UINT_MAX) {
						memcpy(cp_buff, buf_kva, buf_size);
					} else {
						pr_info("[MVPU][Sec] buf_size error\n");
						return -1;
					}
				}
			} else {
				pr_info("[MVPU][Sec] buf[%d]: bypass cmd_buf\n",
							cnt);
			}

			// alignment
			buf_size = ((buf_size + 16 - 1)/16)*16;
			buf_ofst += buf_size;
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

int update_new_base_addr(bool algo_in_img, uint32_t session_id, uint32_t hash_id,
			uint32_t *sec_chk_addr, uint32_t *mem_is_kernel, uint32_t rp_num,
			uint32_t *target_buf_new_map, uint32_t *target_buf_new_base,
			uint32_t ker_bin_num, uint32_t *ker_bin_each_iova,
			void *kreg_kva)
{
	int ret = 0;
	uint32_t cnt = 0;
	uint32_t ker_img_cnt = 0;
	uint32_t *target_pool_addr = NULL;
	uint32_t target_pool_ofst = 0;

	target_pool_addr =
		kcalloc(hash_pool[session_id]->buf_num, sizeof(uint32_t), GFP_KERNEL);


	if (!target_pool_addr)
		return -ENOMEM;

	// update pool addr
	for (cnt = 0; cnt < hash_pool[session_id]->buf_num; cnt++) {
		if (algo_in_img && mem_is_kernel[cnt] == BUF_KERNEL) {
			if (sec_chk_addr[cnt] == 0) {
				pr_info("[MVPU][Sec] WARNING: buf[%d] is not kernel !!!\n",
							cnt);
				continue;
			}

			if (ker_img_cnt > ker_bin_num) {
				pr_info("[MVPU][Sec] WARNING: mem_is_kernel[%d], total num >= image kernel num %d\n",
							cnt, ker_bin_num);

				ret = -1;
				break;
			}

			target_pool_addr[cnt] = ker_bin_each_iova[ker_img_cnt];

#ifdef MVPU_SEC_DEBUG_ADV
			pr_info("[MVPU][Sec] set target_pool_addr[%d] = 0x%08x, from partition\n",
							cnt, target_pool_addr[cnt]);
#endif

			ker_img_cnt++;
		} else if (mem_is_kernel[cnt] == BUF_IO) {
			continue;
		} else {
			target_pool_ofst = hash_pool[session_id]->hash_offset[hash_id][cnt];
			target_pool_addr[cnt] = hash_pool[session_id]->hash_base_iova[hash_id] + target_pool_ofst;
			pr_info("[MVPU][Sec] set target_pool_addr[%d]: 0x%08x\n",
						cnt, target_pool_addr[cnt]);
		}
	}

	// update new base addr to pool addr with buf_map
	for (cnt = 0; cnt < rp_num; cnt++) {
#ifdef MVPU_SEC_MEM_POOL
		if ((target_buf_new_base[cnt] != 0) &&
			(mem_is_kernel[target_buf_new_map[cnt]] != BUF_IO))
			target_buf_new_base[cnt] = target_pool_addr[target_buf_new_map[cnt]];
#endif

#ifdef MVPU_SEC_DEBUG_ADV
		pr_info("[MVPU][Sec] target_buf_new_base[%d]: 0x%08x\n",
					cnt, target_buf_new_base[cnt]);
#endif
	}

	kfree(target_pool_addr);

	return ret;
}

int replace_mem(uint32_t session_id, uint32_t hash_id, uint32_t rp_num,
			uint32_t *target_buf_old_map,
			uint32_t *target_buf_old_base, uint32_t *target_buf_old_offset,
			uint32_t *target_buf_new_base, uint32_t *target_buf_new_offset,
			void *kreg_kva)
{
	int ret = 0;
	int cnt = 0;

	// get *kva
	void *buf_ptr_base;
	void *buf_ptr;
	uint32_t target_pool_ofst = 0;

	buf_ptr_base = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id]);

	if (!buf_ptr_base) {
		pr_info("[MVPU][Sec] buf_ptr_base kva map failed\n");
		return -ENOMEM;
	}

	buf_ptr = buf_ptr_base;

	//cache sync
	dma_buf_begin_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	for (cnt = 0; cnt < rp_num; cnt++) {
		// get addr kva
		if (target_buf_old_base[cnt] == 0) {
			buf_ptr = (void *)((uintptr_t)kreg_kva + target_buf_old_offset[cnt]);
		} else {
			target_pool_ofst =
			hash_pool[session_id]->hash_offset[hash_id][target_buf_old_map[cnt]];
			buf_ptr = (void *)((uintptr_t)buf_ptr_base
								+ target_pool_ofst
								+ target_buf_old_offset[cnt]);
		}

#ifdef MVPU_SEC_DEBUG_RP_INFO
		pr_info("[MVPU][Sec] DRV rp cnt %03d, replace *[0x%llx + 0x%08x] from 0x%08x to [0x%08x + 0x%08x]\n",
					cnt,
					(target_buf_old_base[cnt] == 0) ?
						((uintptr_t)kreg_kva):(target_buf_old_base[cnt]),
					target_buf_old_offset[cnt],
					*((uintptr_t *)buf_ptr),
					target_buf_new_base[cnt],
					target_buf_new_offset[cnt]);
#endif

		// replacement, set new values
		if (*((uint32_t *)buf_ptr) != 0x80000000)
			*((uint32_t *)buf_ptr) =
				(uint32_t)(target_buf_new_base[cnt] + target_buf_new_offset[cnt]);

#ifdef MVPU_SEC_DEBUG_RP_INFO
		pr_info("[MVPU][Sec] new value: 0x%08x\n",
					*((uintptr_t *)buf_ptr));
#endif
	}

	//cache sync
	dma_buf_end_cpu_access(hash_pool[session_id]->hash_dma_buf[hash_id], DMA_TO_DEVICE);

	if (buf_ptr_base)
		dma_buf_vunmap(hash_pool[session_id]->hash_dma_buf[hash_id], buf_ptr_base);

	return ret;
}

int mvpu_load_img(struct device *dev)
{
	int ret = 0;
	struct device_node *mvpu_sec_mem_node;
	struct reserved_mem *mvpu_algo;
	phys_addr_t pa;
	phys_addr_t size;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU] %s start\n", __func__);
#endif

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

	mvpu_algo_iova = (uint32_t)dma_map_single(dev, mvpu_algo_img, size, DMA_FROM_DEVICE);

	if (dma_mapping_error(dev, mvpu_algo_iova)) {
#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x, VA: 0x%llx\n",
				mvpu_algo_iova, mvpu_algo_img);
#else
		pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x\n",
				mvpu_algo_iova);
#endif
	} else {
		pr_info("[MVPU][SEC] get mvpu_algo_iova: 0x%08x\n",
				mvpu_algo_iova);
	}

	ker_img_offset = get_ker_img_offset();
	//ker_img_iova = mvpu_algo_iova + ker_img_offset;

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

	//mem pool settings
	sess_oldest = 0;

	for (session_id = 0; session_id < MAX_SAVE_SESSION; session_id++) {
		saved_session[session_id] = kmalloc(sizeof(uint64_t), GFP_KERNEL);
		if (!saved_session[session_id]) {
			ret = -ENOMEM;
			goto END;
		} else {
			memset(saved_session[session_id], -1, sizeof(uint64_t));
		}

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


