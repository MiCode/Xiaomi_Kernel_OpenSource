// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include "adsp_core.h"
#include "adsp_platform.h"
#include "adsp_semaphore.h"

struct adsp_sem_info {
	unsigned int way_bits;
	unsigned int ctrl_bit;
	unsigned int timeout;
	spinlock_t lock;
};

struct adsp_sem_info sem_info;

int adsp_semaphore_init(unsigned int way_bits,
			unsigned int ctrl_bit,
			unsigned int timeout)
{
	if (ctrl_bit >= way_bits)
		return ADSP_ERROR;

	sem_info.way_bits = way_bits;
	sem_info.ctrl_bit = ctrl_bit;
	if (!timeout)
		timeout = 1;
	sem_info.timeout = timeout;
	spin_lock_init(&sem_info.lock);

	return ADSP_OK;
}

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return ADSP_OK: get sema success
 *        ADSP_ERROR: get sema fail
 */
int get_adsp_semaphore(unsigned int sem_id)
{
	enum adsp_status ret = ADSP_OK;
	unsigned long spin_flags;
	u32 sem_bit = sem_id * sem_info.way_bits + sem_info.ctrl_bit;
	int retry = sem_info.timeout;

	/* return ADSP_ERROR to prevent from access when adsp not ready.
	 * Both adsp enter suspend/resume at the same time.
	 */
	if (!is_adsp_system_running()) {
		pr_notice("%s: adsp not enabled.", __func__);
		return ADSP_ERROR;
	}

	spin_lock_irqsave(&sem_info.lock, spin_flags);

	while (!adsp_mt_get_semaphore(sem_bit)) {
		adsp_mt_toggle_semaphore(sem_bit);

		if (retry-- <= 0) {
			ret = ADSP_SEMAPHORE_BUSY;
			break;
		}
	}

	spin_unlock_irqrestore(&sem_info.lock, spin_flags);
	return ret;
}
EXPORT_SYMBOL_GPL(get_adsp_semaphore);

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return ADSP_OK: release sema success
 *        ADSP_ERROR: release sema fail
 */
int release_adsp_semaphore(unsigned int sem_id)
{
	enum adsp_status ret = ADSP_OK;
	unsigned long spin_flags;
	u32 sem_bit = sem_id * sem_info.way_bits + sem_info.ctrl_bit;

	if (!is_adsp_system_running()) {
		pr_notice("%s: adsp not enabled.", __func__);
		return ADSP_ERROR;
	}

	spin_lock_irqsave(&sem_info.lock, spin_flags);

	if (adsp_mt_get_semaphore(sem_bit))
		adsp_mt_toggle_semaphore(sem_bit);

	spin_unlock_irqrestore(&sem_info.lock, spin_flags);

	return ret;
}
EXPORT_SYMBOL_GPL(release_adsp_semaphore);

