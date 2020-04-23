/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <fdrv.h>
#include "teei_id.h"
#include "sched_status.h"
#include "nt_smc_call.h"
#include "teei_log.h"
#include "teei_common.h"
#include "switch_queue.h"
#include "teei_client_main.h"
#include "backward_driver.h"
#include "utdriver_macro.h"

#include <teei_secure_api.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

static LIST_HEAD(fdrv_list);

int create_fdrv(struct teei_fdrv *fdrv)
{
	unsigned long temp_addr = 0;
	long retVal = 0;

	if (fdrv->buff_size > VDRV_MAX_SIZE) {
		IMSG_ERROR("[%s][%d]: FDrv buffer size is too large.\n",
				__FILE__, __LINE__);
		return -EINVAL;
	}

#ifdef UT_DMA_ZONE
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL | GFP_DMA,
				get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));
#else
	temp_addr = (unsigned long) __get_free_pages(GFP_KERNEL,
				get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));
#endif
	if ((unsigned char *)temp_addr == NULL) {
		IMSG_ERROR("[%s][%d]: Faile to alloc fdrv buffer failed.\n",
						__FILE__, __LINE__);
		return -ENOMEM;
	}

	retVal = add_work_entry(SMC_CALL_TYPE, N_INVOKE_T_NQ, 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);
		goto free_memory;
	}

	retVal = add_nq_entry(TEEI_CREAT_FDRV, fdrv->call_type,
				(unsigned long long)(&boot_sema),
				virt_to_phys((void *)temp_addr),
				fdrv->buff_size, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add one nq to n_t_buffer\n");
		goto free_memory;
	}

	teei_notify_switch_fn();

	down(&boot_sema);

	fdrv->buf = (void *)temp_addr;

	return 0;

free_memory:
	free_pages((unsigned long)temp_addr,
			get_order(ROUND_UP(fdrv->buff_size, SZ_4K)));

	return retVal;
}

void register_fdrv(struct teei_fdrv *fdrv)
{
	list_add_tail(&fdrv->list, &fdrv_list);
}
EXPORT_SYMBOL(register_fdrv);

int fdrv_notify(struct teei_fdrv *fdrv)
{
	struct completion *wait_completion = NULL;
	int retVal = 0;

	wait_completion = kmalloc(sizeof(struct completion), GFP_KERNEL);

	init_completion(wait_completion);

	cpus_read_lock();

	retVal = add_work_entry(SMC_CALL_TYPE, N_INVOKE_T_NQ, 0, 0, 0);
	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add_work_entry[%s]\n", __func__);
		cpus_read_unlock();
		kfree(wait_completion);
		return retVal;
	}

	retVal = add_nq_entry(TEEI_FDRV_CALL, fdrv->call_type,
				(unsigned long long)(wait_completion),
				fdrv->buff_size, 0, 0);

	if (retVal != 0) {
		IMSG_ERROR("TEEI: Failed to add one nq to n_t_buffer\n");
		cpus_read_unlock();
		kfree(wait_completion);
		return retVal;
	}

	teei_notify_switch_fn();

	wait_for_completion(wait_completion);

	cpus_read_unlock();

	kfree(wait_completion);

	return 0;
}
EXPORT_SYMBOL(fdrv_notify);

void teei_handle_fdrv_call(struct NQ_entry *entry)
{
	struct completion *bp = NULL;

	bp = (struct completion *)(entry->block_p);
	if (bp == NULL) {
		IMSG_ERROR("The block_p of entry is NULL!\n");
		return;
	}

	complete(bp);
}

int create_all_fdrv(void)
{
	struct teei_fdrv *fdrv;
	int retVal = 0;

	list_for_each_entry(fdrv, &fdrv_list, list) {
		retVal = create_fdrv(fdrv);

		if (retVal != 0)
			return retVal;
	}

	return 0;
}
