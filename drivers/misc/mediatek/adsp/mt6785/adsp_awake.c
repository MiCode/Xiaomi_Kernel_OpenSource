/*
 * Copyright (C) 2018-2022 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/pm_wakeup.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/aee.h>
#include <linux/delay.h>
#include "adsp_feature_define.h"
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_excep.h"
#include "adsp_dvfs.h"

#define AP_AWAKE_STATE_NORMAL       (0)
#define AP_AWAKE_STATE_FORCE_LOCK   (1)
#define AP_AWAKE_STATE_FORCE_UNLOCK (2)
#define ADSP_SW_INT1_BIT            (0x1 << ADSP_SW_INT1)
#define AP_AWAKE_LOCK_MASK          (0x1 << AP_AWAKE_LOCK_BIT)
#define AP_AWAKE_UNLOCK_MASK        (0x1 << AP_AWAKE_UNLOCK_BIT)
#define AP_AWAKE_DUMP_MASK          (0x1 << AP_AWAKE_DUMP_BIT)
#define AP_AWAKE_UPDATE_MASK        (0x1 << AP_AWAKE_UPDATE_BIT)
#define AP_AWAKE_STATE_MASK         (0x3 << AP_AWAKE_STATE_BIT)
#define ADSPPLL_UNLOCK_MASK         (0x1 << ADSPPLL_UNLOCK_BIT)
#define REG_AP_AWAKE                (readl(ADSP_A_AP_AWAKE))
#define CHECK_AP_AWAKE_MASK(mask)   ((REG_AP_AWAKE & mask) == 0)

#define GET_AP_AWAKE_STATE()    \
	((REG_AP_AWAKE & AP_AWAKE_STATE_MASK) >> AP_AWAKE_STATE_BIT)

#define SET_AP_AWAKE_STATE(reg_val, state)    \
	((reg_val & ~AP_AWAKE_STATE_MASK) | (state << AP_AWAKE_STATE_BIT))

struct mutex adsp_awake_mutexs[ADSP_CORE_TOTAL];
int adsp_awake_counts[ADSP_CORE_TOTAL];
DEFINE_SPINLOCK(adsp_awake_spinlock);

static int adsp_awake_send_swint (int ret, uint32_t reg_val, uint32_t mask)
{
	writel(reg_val | mask, ADSP_A_AP_AWAKE);
	writel(ADSP_SW_INT1_BIT, ADSP_SWINT_REG);
	{	/* Wait for ADSP response */
		uint32_t cnt = 0;

		do {
			if (CHECK_AP_AWAKE_MASK(mask)) {
				ret = 0;
				break;
			}
			udelay(10);
			cnt++;
		} while (cnt != ADSP_AWAKE_TIMEOUT);
	}
	return ret;
}

/*
 * acquire adsp lock flag, keep adsp awake
 * @param adsp_id: adsp core id
 * return 0     : get lock success
 *        non-0 : get lock fail
 */
int adsp_awake_lock(enum adsp_core_id adsp_id)
{
	if (adsp_id >= ADSP_CORE_TOTAL) {
		pr_err("%s() ID %d >= CORE TOTAL", __func__, adsp_id);
		return -EINVAL;
	}
	if (is_adsp_ready(adsp_id) == 0) {
		char *p_id = adsp_core_ids[adsp_id];

		pr_warn("%s() %s not enabled\n", __func__, p_id);
		return -1;
	}
	{	/* Protected by spin lock */
		int *p_count = (int *)&adsp_awake_counts[adsp_id];
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;

		spin_lock_irqsave(p_spin, spin_flags);
		if (*p_count > 0) {
			*p_count += 1;
			spin_unlock_irqrestore(p_spin, spin_flags);
			return 0;
		}
		spin_unlock_irqrestore(p_spin, spin_flags);
	}
	{	/* Protected by mutex lock & spin lock */
		int ret = -1;
		int *p_count = (int *)&adsp_awake_counts[adsp_id];
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		struct mutex *p_mutex = &adsp_awake_mutexs[adsp_id];
		uint32_t reg_val, mask;

		mutex_lock(p_mutex);
		spin_lock_irqsave(p_spin, spin_flags);
		reg_val = REG_AP_AWAKE;
		mask = AP_AWAKE_LOCK_MASK;
		ret = adsp_awake_send_swint (ret, reg_val, mask);
		if (ret != -1)
			*p_count += 1;
		spin_unlock_irqrestore(p_spin, spin_flags);
		if (ret == -1) {
			char *p_id = adsp_core_ids[adsp_id];

			pr_err("%s() %s fail\n", __func__, p_id);
			WARN_ON(1);
		}
		mutex_unlock(p_mutex);
		return ret;
	}
}
EXPORT_SYMBOL_GPL(adsp_awake_lock);

/*
 * release adsp awake lock flag
 * @param adsp_id: adsp core id
 * return 0     : release lock success
 *        non-0 : release lock fail
 */
int adsp_awake_unlock(enum adsp_core_id adsp_id)
{
	if (adsp_id >= ADSP_CORE_TOTAL) {
		pr_err("%s() ID %d >= CORE TOTAL", __func__, adsp_id);
		return -EINVAL;
	}
	if (is_adsp_ready(adsp_id) == 0) {
		char *p_id = adsp_core_ids[adsp_id];

		pr_warn("%s() %s not enabled\n", __func__, p_id);
		return -1;
	}
	{	/* Protected by spin lock */
		int *p_count = (int *)&adsp_awake_counts[adsp_id];
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;

		spin_lock_irqsave(p_spin, spin_flags);
		if (*p_count > 1) {
			*p_count -= 1;
			spin_unlock_irqrestore(p_spin, spin_flags);
			return 0;
		}
		spin_unlock_irqrestore(p_spin, spin_flags);
	}
	{	/* Protected by mutex lock & spin lock */
		int ret = -1;
		int *p_count = (int *)&adsp_awake_counts[adsp_id];
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		struct mutex *p_mutex = &adsp_awake_mutexs[adsp_id];
		uint32_t reg_val, mask;

		mutex_lock(p_mutex);
		spin_lock_irqsave(p_spin, spin_flags);
		reg_val = REG_AP_AWAKE;
		mask = AP_AWAKE_UNLOCK_MASK;
		ret = adsp_awake_send_swint (ret, reg_val, mask);
		if (ret != -1)
			*p_count -= 1;
		spin_unlock_irqrestore(p_spin, spin_flags);
		if (*p_count < 0)
			pr_err("%s() count = %d NOT SYNC\n", __func__,
			       *p_count);
		mutex_unlock(p_mutex);
		return ret;
	}
}
EXPORT_SYMBOL_GPL(adsp_awake_unlock);

void adsp_awake_init(void)
{
	uint32_t i = 0;

	for (i = 0; i < ADSP_CORE_TOTAL; i++)
		adsp_awake_counts[i] = 0;
	for (i = 0; i < ADSP_CORE_TOTAL; i++)
		mutex_init(&adsp_awake_mutexs[i]);
}

/*
 * lock/unlock switch of adsppll clock
 * @param adsp_id: adsp core id
 *        unlock: if unlock adsppll
 * return 0     : send command success
 *        non-0 : adsp is not ready
 */
int adsp_awake_unlock_adsppll(enum adsp_core_id adsp_id, uint32_t unlock)
{
	if (adsp_id >= ADSP_CORE_TOTAL) {
		pr_err("%s() ID %d >= CORE TOTAL", __func__, adsp_id);
		return -EINVAL;
	}
	if (is_adsp_ready(adsp_id) == 0) {
		char *p_id = adsp_core_ids[adsp_id];

		pr_warn("%s() %s not enabled\n", __func__, p_id);
		return -1;
	}
	{	/* Protected by mutex lock & spin lock */
		int ret = 0;
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		struct mutex *p_mutex = &adsp_awake_mutexs[adsp_id];
		uint32_t reg_val;

		mutex_lock(p_mutex);
		spin_lock_irqsave(p_spin, spin_flags);
		reg_val = REG_AP_AWAKE;
		if (unlock)
			reg_val |= ADSPPLL_UNLOCK_MASK;
		else
			reg_val &= ~ADSPPLL_UNLOCK_MASK;
		writel(reg_val, ADSP_A_AP_AWAKE);
		writel(ADSP_SW_INT1_BIT, ADSP_SWINT_REG);
		spin_unlock_irqrestore(p_spin, spin_flags);
		mutex_unlock(p_mutex);
		return ret;
	}
}
EXPORT_SYMBOL_GPL(adsp_awake_unlock_adsppll);

/*
 * dump adsp lock list in adsp log
 * @param adsp_id: adsp core id
 * return 0     : dump list success
 *        non-0 : dump list fail
 */
int adsp_awake_dump_list(enum adsp_core_id adsp_id)
{
	if (adsp_id >= ADSP_CORE_TOTAL) {
		pr_err("%s() ID %d >= CORE TOTAL", __func__, adsp_id);
		return -EINVAL;
	}
	if (is_adsp_ready(adsp_id) == 0) {
		char *p_id = adsp_core_ids[adsp_id];

		pr_warn("%s() %s not enabled\n", __func__, p_id);
		return -1;
	}
	{	/* Protected by mutex lock & spin lock */
		int ret = -1;
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		struct mutex *p_mutex = &adsp_awake_mutexs[adsp_id];
		uint32_t reg_val, mask;

		mutex_lock(p_mutex);
		spin_lock_irqsave(p_spin, spin_flags);
		reg_val = REG_AP_AWAKE;
		mask = AP_AWAKE_DUMP_MASK;
		ret = adsp_awake_send_swint (ret, reg_val, mask);
		spin_unlock_irqrestore(p_spin, spin_flags);
		if (ret == -1) {
			char *p_id = adsp_core_ids[adsp_id];

			pr_err("%s() %s fail\n", __func__, p_id);
			WARN_ON(1);
		}
		mutex_unlock(p_mutex);
		return ret;
	}
}
EXPORT_SYMBOL_GPL(adsp_awake_dump_list);

static int adsp_awake_set_state(enum adsp_core_id adsp_id, uint32_t next_state)
{
	if (adsp_id >= ADSP_CORE_TOTAL) {
		pr_err("%s() ID %d >= ADSP_CORE_TOTAL", __func__, adsp_id);
		return -EINVAL;
	}
	if (is_adsp_ready(adsp_id) == 0) {
		char *p_id = adsp_core_ids[adsp_id];

		pr_warn("%s() %s not enabled\n", __func__, p_id);
		return -1;
	}
	{	/* Protected by spin lock */
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		uint32_t curr_state = GET_AP_AWAKE_STATE();

		spin_lock_irqsave(p_spin, spin_flags);
		if (curr_state == next_state) {
			spin_unlock_irqrestore(p_spin, spin_flags);
			return 0;
		}
		spin_unlock_irqrestore(p_spin, spin_flags);
	}
	{	/* Protected by mutex lock & spin lock */
		int ret = -1;
		unsigned long spin_flags = 0;
		spinlock_t *p_spin = &adsp_awake_spinlock;
		struct mutex *p_mutex = &adsp_awake_mutexs[adsp_id];
		uint32_t reg_val, mask;

		mutex_lock(p_mutex);
		spin_lock_irqsave(p_spin, spin_flags);
		reg_val = REG_AP_AWAKE;
		mask = AP_AWAKE_UPDATE_MASK;
		reg_val = SET_AP_AWAKE_STATE(reg_val, next_state);
		ret = adsp_awake_send_swint (ret, reg_val, mask);
		spin_unlock_irqrestore(p_spin, spin_flags);
		if (ret == -1) {
			char *p_id = adsp_core_ids[adsp_id];

			pr_err("%s() %s fail\n", __func__, p_id);
			WARN_ON(1);
		}
		mutex_unlock(p_mutex);
		return ret;
	}
}

/*
 * force adsp lock, keep adsp awake
 * @param adsp_id: adsp core id
 * return 0     : force lock success
 *        non-0 : force lock fail
 */
int adsp_awake_force_lock(enum adsp_core_id adsp_id)
{
	return adsp_awake_set_state(adsp_id, AP_AWAKE_STATE_FORCE_LOCK);
}
EXPORT_SYMBOL_GPL(adsp_awake_force_lock);

/*
 * force adsp unlock
 * @param adsp_id: adsp core id
 * return 0     : force unlock success
 *        non-0 : force unlock fail
 */
int adsp_awake_force_unlock(enum adsp_core_id adsp_id)
{
	return adsp_awake_set_state(adsp_id, AP_AWAKE_STATE_FORCE_UNLOCK);
}
EXPORT_SYMBOL_GPL(adsp_awake_force_unlock);

/*
 * set adsp normal
 * @param adsp_id: adsp core id
 * return 0     : set normal success
 *        non-0 : set normal fail
 */
int adsp_awake_set_normal(enum adsp_core_id adsp_id)
{
	return adsp_awake_set_state(adsp_id, AP_AWAKE_STATE_NORMAL);
}
EXPORT_SYMBOL_GPL(adsp_awake_set_normal);

static inline ssize_t adsp_awake_force_lock_show(struct device *kobj,
						 struct device_attribute *att,
						 char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_force_lock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake force lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_force_lock);

static inline ssize_t adsp_awake_force_unlock_show(struct device *kobj,
						   struct device_attribute *att,
						   char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_force_unlock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake force unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_force_unlock);

static inline ssize_t adsp_awake_set_normal_show(struct device *kobj,
						 struct device_attribute *att,
						 char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_set_normal(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake set normal\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_set_normal);

static inline ssize_t adsp_awake_dump_list_show(struct device *kobj,
						struct device_attribute *att,
						char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_dump_list(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake dump list\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_dump_list);

static inline ssize_t adsp_awake_lock_show(struct device *kobj,
					   struct device_attribute *att,
					   char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_lock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake lock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_lock);

static inline ssize_t adsp_awake_unlock_show(struct device *kobj,
					     struct device_attribute *att,
					     char *buf)
{
	if (is_adsp_ready(ADSP_A_ID) == 1) {
		adsp_awake_unlock(ADSP_A_ID);
		return scnprintf(buf, PAGE_SIZE, "ADSP awake unlock\n");
	} else
		return scnprintf(buf, PAGE_SIZE, "ADSP is not ready\n");
}
DEVICE_ATTR_RO(adsp_awake_unlock);

static struct attribute *adsp_awake_attrs[] = {
	&dev_attr_adsp_awake_force_lock.attr,
	&dev_attr_adsp_awake_force_unlock.attr,
	&dev_attr_adsp_awake_set_normal.attr,
	&dev_attr_adsp_awake_dump_list.attr,
	&dev_attr_adsp_awake_lock.attr,
	&dev_attr_adsp_awake_unlock.attr,
	NULL,
};

struct attribute_group adsp_awake_attr_group = {
	.attrs = adsp_awake_attrs,
};
