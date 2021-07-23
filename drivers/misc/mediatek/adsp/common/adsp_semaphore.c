// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include "adsp_semaphore.h"
#include "adsp_core.h"

DEFINE_SPINLOCK(adsp_sem_spinlock);

struct adsp_sem_info sem_info;

int adsp_sem_init(unsigned int way_bits,
		unsigned int ctrl_bit,
		unsigned int timeout,
		void __iomem *reg)
{
	if (ctrl_bit >= way_bits || !reg)
		return ADSP_ERROR;

	sem_info.way_bits = way_bits;
	sem_info.ctrl_bit = ctrl_bit;
	sem_info.reg = reg;
	if (!timeout)
		timeout = 1;
	sem_info.timeout = timeout;

	return ADSP_OK;
}

static void set_clr_adsp_sem_flag(unsigned int flags)
{
	writel((1 << flags), sem_info.reg);
}


static unsigned int get_adsp_sem_flag(unsigned int flags)
{
	return (readl(sem_info.reg) >> flags) & 0x1;
}

/*
 * acquire a hardware semaphore
 * @param flag: semaphore id
 * return ADSP_OK: get sema success
 *        ADSP_ERROR: get sema fail
 */
int get_adsp_semaphore(unsigned int flags)
{
	enum adsp_status ret = ADSP_SEMAPHORE_BUSY;
	unsigned int cnt;
	unsigned long spin_flags;

	/* return ADSP_ERROR to prevent from access when adsp not ready.
	 * Both adsp enter suspend/resume at the same time.
	 */
	if (!is_adsp_system_running()) {
		pr_notice("%s: adsp not enabled.", __func__);
		return ADSP_ERROR;
	}

	/* spinlock context safe*/
	spin_lock_irqsave(&adsp_sem_spinlock, spin_flags);

	flags = flags * sem_info.way_bits
		+ sem_info.ctrl_bit;

	if (get_adsp_sem_flag(flags) == 0) {
		cnt = sem_info.timeout;

		while (cnt-- > 0) {
			set_clr_adsp_sem_flag(flags);

			if (get_adsp_sem_flag(flags) == 1) {
				ret = ADSP_OK;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&adsp_sem_spinlock, spin_flags);

	return ret;
}

/*
 * release a hardware semaphore
 * @param flag: semaphore id
 * return ADSP_OK: release sema success
 *        ADSP_ERROR: release sema fail
 */
int release_adsp_semaphore(unsigned int flags)
{
	enum adsp_status ret = ADSP_SEMAPHORE_BUSY;
	unsigned long spin_flags;

	if (!is_adsp_system_running()) {
		pr_notice("%s: adsp not enabled.", __func__);
		return ADSP_ERROR;
	}

	/* spinlock context safe*/
	spin_lock_irqsave(&adsp_sem_spinlock, spin_flags);

	flags = flags * sem_info.way_bits
		+ sem_info.ctrl_bit;

	if (get_adsp_sem_flag(flags) == 1) {
		set_clr_adsp_sem_flag(flags);
		ret = ADSP_OK;
	}

	spin_unlock_irqrestore(&adsp_sem_spinlock, spin_flags);

	return ret;
}
