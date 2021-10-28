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

	for (i = 0; i < ker_bin_num; i++) {
		shift = ker_bin_offset/sizeof(uint32_t) + ker_size_offset/sizeof(uint32_t) + KER_BIN_INFO_SIZE*i;
		//shift = 4*(ker_size_offset/sizeof(uint32_t) + KER_BIN_INFO_SIZE*i);
		ker_bin_each_iova[i] = (shift + KER_BIN_INFO_SIZE)*sizeof(uint32_t) + ker_img_iova;

		ker_size_offset += mvpu_algo_img[shift + KER_SIZE_OFFSET];

		// alignment
		//ker_size_offset = ker_size_offset + 4 - (ker_size_offset % 4);
	}

	return;
}

//FIXME: fake function
uint64_t iova_to_kva(uint64_t iova)
{
	return 0;
}

uint64_t kva_to_iova(uint64_t kva)
{
	return 0;
}

void map_base_buf_id(uint32_t buf_num, uint32_t *sec_chk_addr, uint32_t rp_num, uint32_t *target_map, uint32_t *target_base)
{
	int i = 0, j = 0;
	bool buf_mapped = false;
	for (j = 0; j < rp_num; j++) {
		buf_mapped = false;
		for (i = 0; i < buf_num; i++) {
			if (target_base[j] == sec_chk_addr[i]) {
				target_map[j] = i;
				buf_mapped = true;
				pr_info("[MVPU][Sec] set target_base[%03d]: 0x%08x map to sec_chk_addr[%d]: 0x%08x\n", j, target_base[j], target_map[j], sec_chk_addr[target_map[j]]);
			}
		}

		if (buf_mapped == false)
			pr_info("[MVPU][Sec] target_base[%03d]: 0x%08x not mapped\n", j, target_base[j]);
	}
}

uint32_t get_saved_session_id(void *session)
{
	uint32_t cnt = 0;
	uint32_t session_id = -1;
	for (cnt = 0; cnt < MAX_SAVE_SESSION; cnt++) {
		if (memcmp(session, saved_session[cnt], sizeof(uint64_t)) == 0) {
			pr_info("[MVPU][Sec] Get saved_session at %d: 0x%lx\n", cnt, saved_session[cnt]);
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
	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (saved_session[cnt] == NULL) {
			session_id = cnt;
			pr_info("[MVPU][Sec] Get available session id %d\n", session_id);
			break;
		}
	}

	// all session place are used, take the oldest
	if (session_id == -1) {
		session_id = sess_oldest;

		pr_info("[MVPU][Sec] Take oldest session id %d\n", session_id);
	}

	return session_id;
}


void clear_session(void *session)
{
	uint32_t cnt = 0;

	for (cnt = 0; cnt < MAX_SAVE_SESSION; cnt++) {
		if (memcmp(session, saved_session[cnt], sizeof(uint64_t)) == 0) {
			pr_info("[MVPU][Sec] free session id %d\n", cnt);
			saved_session[cnt] = NULL;
			break;
		}
	}

	return;
}

/* seems no need this function, just update saved_session
void clear_session_id(uint32_t session_id)
{
	uint32_t cnt = 0;

	if (saved_session[session_id] != NULL)
		saved_session[session_id] = NULL;

	return;
}
*/

void update_session_id(uint32_t session_id, void *session)
{
	sess_oldest++;
	if (sess_oldest == MAX_SAVE_SESSION)
		sess_oldest = 0;

	saved_session[session_id] = session;

	return;
}

uint32_t get_saved_hash_id(uint32_t session_id, uint32_t batch_name_hash)
{
	uint32_t cnt = 0;
	uint32_t hash_id = -1;
	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (batch_name_hash == hash_pool[session_id]->hash_list[cnt]) {
			pr_info("[MVPU][Sec] Get saved_hash_list at %d: 0x%08x\n", cnt, batch_name_hash);
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

		if (hash_pool[session_id]->hash_offset[hash_id])
			kfree(hash_pool[session_id]->hash_offset[hash_id]);

		pr_info("[MVPU][Sec] Clear hash id %d settings\n", hash_id);
	}

	return;
}

void free_all_hash(uint32_t session_id)
{
	uint32_t cnt = 0;

	for (cnt = 0; cnt < MAX_SAVE_HASH; cnt++) {
		if (hash_pool[session_id]->hash_list[cnt] != 0) {
			clear_hash(session_id, cnt);
		} else {
			break;
		}
	}

	return;
}

uint32_t update_hash_pool(bool algo_in_img, uint32_t session_id, uint32_t hash_id, uint32_t batch_name_hash, uint32_t buf_num, uint32_t *sec_chk_addr, uint32_t *sec_buf_size, uint32_t *mem_is_kernel)
{
	uint32_t cnt = 0;
	uint32_t buf_size = 0;
	uint32_t total_buf_size = 0;
	void *p_buf;

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

			buf_size = buf_size + 4 - (buf_size % 4);
			total_buf_size += buf_size;
		}
	}

	hash_pool[session_id]->hash_dma_buf[hash_id] = dma_heap_buffer_alloc(hash_pool[session_id]->dma_heap[hash_id], total_buf_size, O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(hash_pool[session_id]->hash_dma_buf[hash_id])) {
		pr_info("[MVPU][Sec] buffer alloc fail\n");
		return PTR_ERR(hash_pool[session_id]->hash_dma_buf[hash_id]);
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

	// get *kva
	p_buf = dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id]);
	//hash_pool[session_id]->hash_base_kva[hash_id] = (uint64_t *)p_buf;

	if (!p_buf) {
		pr_info("[MVPU][Sec] map failed\n");
		return -1;
	}

	hash_pool[session_id]->hash_offset[hash_id] = (uint32_t *)kzalloc(hash_pool[session_id]->buf_num*sizeof(uint32_t), GFP_KERNEL);

	for (cnt = 0; cnt < hash_pool[session_id]->buf_num; cnt++) {
		//iova
		hash_pool[session_id]->hash_offset[hash_id][cnt] = buf_size;
		p_buf += buf_size;

		buf_size = sec_buf_size[cnt];

		if (!algo_in_img || (mem_is_kernel[cnt] != 1))
			memcpy(p_buf, (void *)iova_to_kva((uint32_t)sec_chk_addr[cnt]), buf_size); //FIXME: iova_to_kva is provided by APUSYS

		// alignment
		buf_size = buf_size + 4 - (buf_size % 4);
	}

	if (p_buf)
		dma_buf_vunmap(hash_pool[session_id]->hash_dma_buf[hash_id], p_buf);

	return 0;
}

uint32_t update_new_base_addr(bool algo_in_img, uint32_t session_id, uint32_t hash_id, uint32_t *mem_is_kernel, uint32_t rp_num, uint32_t *target_buf_new_map, uint32_t *target_buf_new_base, uint32_t ker_bin_num, uint32_t *ker_bin_each_iova)
{
	uint32_t ret = 0;
	uint32_t cnt = 0;
	uint32_t ker_img_cnt = 0;
	uint32_t *target_pool_addr = (uint32_t *)kzalloc(hash_pool[session_id]->buf_num*sizeof(uint32_t), GFP_KERNEL);
	uint32_t target_pool_ofst = 0;

	if (!target_pool_addr)
		return -ENOMEM;

	// update pool addr
	for (cnt = 0; cnt < hash_pool[session_id]->buf_num; cnt++) {
		if (algo_in_img && mem_is_kernel[cnt] == 1) {
			if (ker_img_cnt > ker_bin_num) {
				pr_info("[MVPU][Sec] mem_is_kernel %d, total num >= image kernel num %d\n", cnt, ker_bin_num);
				ret = -1;
				break;
			}

			target_pool_addr[cnt] = ker_bin_each_iova[ker_img_cnt];
			ker_img_cnt++;
		} else {
			target_pool_ofst = hash_pool[session_id]->hash_offset[hash_id][cnt];
			target_pool_addr[cnt] = hash_pool[session_id]->hash_base_iova[hash_id] + target_pool_ofst;
		}
	}

	// update new base addr to pool addr with buf_map
	for (cnt = 0; cnt < rp_num; cnt++) {
		//target_buf_old_base[cnt] = (uint32_t)hash_pool[session_id]->hash_base_kva[hash_id] + target_pool_addr[target_buf_old_map[cnt]];
		target_buf_new_base[cnt] = target_pool_addr[target_buf_new_map[cnt]];
	}

	if (target_pool_addr)
		kfree(target_pool_addr);

	return ret;
}

void replace_mem(uint32_t session_id, uint32_t hash_id, uint32_t rp_num, uint32_t *target_buf_old_map, uint32_t *target_buf_old_base, uint32_t *target_buf_old_offset, uint32_t *target_buf_new_base, uint32_t *target_buf_new_offset)
{
	int cnt = 0;

	// get *kva
	uint32_t *buf_ptr_base = (uint32_t *)dma_buf_vmap(hash_pool[session_id]->hash_dma_buf[hash_id]);
	uint32_t *buf_ptr = NULL;
	uint32_t target_pool_ofst = 0;

	for (cnt = 0; cnt < rp_num; cnt++) {
		// get addr kva
		target_pool_ofst = hash_pool[session_id]->hash_offset[hash_id][target_buf_old_map[cnt]];
		buf_ptr = buf_ptr_base + target_pool_ofst + target_buf_old_offset[cnt];
		pr_info("[MVPU][Sec] DRV rp cnt %3d, replace *[0x%08x + 0x%08x] from 0x%08x to [0x%08x + 0x%08x]\n", cnt, target_buf_old_base[cnt], target_buf_old_offset[cnt], *buf_ptr, target_buf_new_base[cnt], target_buf_new_offset[cnt]);

		// set value
		*buf_ptr = target_buf_new_base[cnt] + target_buf_new_offset[cnt];
	}

	return;
}

int mvpu_load_img(struct device *dev)
{
	int ret = 0;
	struct device_node *mvpu_sec_mem_node;
	struct reserved_mem *mvpu_algo;
	phys_addr_t pa;
	phys_addr_t size;

	mvpu_algo_iova = 0;

#ifdef MVPU_SEC_DEBUG
	pr_info("[MVPU] mvpu_load_img start\n");
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
	mvpu_algo_iova = dma_map_resource(dev, pa, size, DMA_BIDIRECTIONAL, 0);

	if (mvpu_algo_iova == DMA_MAPPING_ERROR) {
#ifdef MVPU_SEC_DEBUG
		pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x, PA: 0x%llx\n", mvpu_algo_iova, pa);
#else
		pr_info("[MVPU][WARN] get mvpu_algo_iova error: 0x%08x\n", mvpu_algo_iova);
#endif
	}

	ker_img_offset = get_ker_img_offset();
	ker_img_iova = (uint32_t)mvpu_algo_iova + ker_img_offset;

END:
	return ret;
}


