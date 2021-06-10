// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/bug.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>
#include <linux/fs.h>
#include <linux/mtd/partitions.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include "sec_osal.h"

#define MOD                         "MASP"

/*****************************************************************************
 * MACRO
 *****************************************************************************/
#ifndef ASSERT
#define ASSERT(expr)        WARN_ON(!(expr))
#endif

/*****************************************************************************
 * GLOBAL VARIABLE
 *****************************************************************************/
DEFINE_SEMAPHORE(hacc_sem);
DEFINE_SEMAPHORE(mtd_sem);
DEFINE_SEMAPHORE(rid_sem);
DEFINE_SEMAPHORE(sec_mm_sem);
DEFINE_SEMAPHORE(osal_fp_sem);
DEFINE_SEMAPHORE(osal_verify_sem);
DEFINE_SEMAPHORE(osal_secro_sem);
DEFINE_SEMAPHORE(osal_secro_v5_sem);

/*****************************************************************************
 * LOCAL VARIABLE
 *****************************************************************************/
static mm_segment_t curr_fs;
#define OSAL_MAX_FP_COUNT           4096
#define OSAL_FP_OVERFLOW            OSAL_MAX_FP_COUNT
/* The array 0 will be not be used, and fp_id=0 will be though as NULL file */
static struct file *g_osal_fp[OSAL_MAX_FP_COUNT] = { 0 };

/*****************************************************************************
 * PORTING LAYER
 *****************************************************************************/
void osal_kfree(void *buf)
{
	/* kfree(buf); */
	vfree(buf);
}
EXPORT_SYMBOL(osal_kfree);

void *osal_kmalloc(unsigned int size)
{
	/* return kmalloc(size,GFP_KERNEL); */
	return vmalloc(size);
}
EXPORT_SYMBOL(osal_kmalloc);

unsigned long osal_copy_from_user(void *to, void *from, unsigned long size)
{
	return copy_from_user(to, from, size);
}
EXPORT_SYMBOL(osal_copy_from_user);

unsigned long osal_copy_to_user(void *to, void *from, unsigned long size)
{
	return copy_to_user(to, from, size);
}
EXPORT_SYMBOL(osal_copy_to_user);

int osal_hacc_lock(void)
{
	return down_interruptible(&hacc_sem);
}
EXPORT_SYMBOL(osal_hacc_lock);

void osal_hacc_unlock(void)
{
	up(&hacc_sem);
}
EXPORT_SYMBOL(osal_hacc_unlock);

int osal_verify_lock(void)
{
	return down_interruptible(&osal_verify_sem);
}
EXPORT_SYMBOL(osal_verify_lock);

void osal_verify_unlock(void)
{
	up(&osal_verify_sem);
}
EXPORT_SYMBOL(osal_verify_unlock);

int osal_secro_lock(void)
{
	return down_interruptible(&osal_secro_sem);
}
EXPORT_SYMBOL(osal_secro_lock);

void osal_secro_unlock(void)
{
	up(&osal_secro_sem);
}
EXPORT_SYMBOL(osal_secro_unlock);

int osal_secro_v5_lock(void)
{
	return down_interruptible(&osal_secro_v5_sem);
}
EXPORT_SYMBOL(osal_secro_v5_lock);

void osal_secro_v5_unlock(void)
{
	up(&osal_secro_v5_sem);
}
EXPORT_SYMBOL(osal_secro_v5_unlock);

int osal_mtd_lock(void)
{
	return down_interruptible(&mtd_sem);
}
EXPORT_SYMBOL(osal_mtd_lock);

void osal_mtd_unlock(void)
{
	up(&mtd_sem);
}
EXPORT_SYMBOL(osal_mtd_unlock);

int osal_rid_lock(void)
{
	return down_interruptible(&rid_sem);
}
EXPORT_SYMBOL(osal_rid_lock);

void osal_rid_unlock(void)
{
	up(&rid_sem);
}
EXPORT_SYMBOL(osal_rid_unlock);

void osal_msleep(unsigned int msec)
{
	msleep(msec);
}
EXPORT_SYMBOL(osal_msleep);

void osal_assert(unsigned int val)
{
	ASSERT(val);
}
EXPORT_SYMBOL(osal_assert);

int osal_set_kernel_fs(void)
{
	int val = 0;

	val = down_interruptible(&sec_mm_sem);
	curr_fs = get_fs();
	set_fs(KERNEL_DS);
	return val;
}
EXPORT_SYMBOL(osal_set_kernel_fs);

void osal_restore_fs(void)
{
	set_fs(curr_fs);
	up(&sec_mm_sem);
}
EXPORT_SYMBOL(osal_restore_fs);

void *osal_get_filp_struct(int fp_id)
{
	int val = 0;
	struct file *ret;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		ret = g_osal_fp[fp_id];

		up(&osal_fp_sem);

		return (void *)ret;
	}

	return (struct file *)(-ENOENT);	/* No such file or directory */
}
EXPORT_SYMBOL(osal_get_filp_struct);


loff_t osal_filp_seek_set(int fp_id, loff_t off)
{
	loff_t offset;
	int val = 0;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		offset = g_osal_fp[fp_id]->f_op->llseek(g_osal_fp[fp_id],
							off,
							SEEK_SET);

		up(&osal_fp_sem);

		return offset;
	}

	return OSAL_FILE_SEEK_FAIL;
}
EXPORT_SYMBOL(osal_filp_seek_set);

loff_t osal_filp_seek_end(int fp_id, loff_t off)
{
	loff_t offset;
	int val = 0;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		offset = g_osal_fp[fp_id]->f_op->llseek(g_osal_fp[fp_id],
							off,
							SEEK_END);

		up(&osal_fp_sem);

		return offset;
	}

	return OSAL_FILE_SEEK_FAIL;
}
EXPORT_SYMBOL(osal_filp_seek_end);

loff_t osal_filp_pos(int fp_id)
{
	loff_t offset;
	int val = 0;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		offset = g_osal_fp[fp_id]->f_pos;

		up(&osal_fp_sem);

		return offset;
	}

	return OSAL_FILE_GET_POS_FAIL;
}
EXPORT_SYMBOL(osal_filp_pos);

long osal_filp_read(int fp_id, char *buf, unsigned long len)
{
	ssize_t read_len;
	int val = 0;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		read_len =
			g_osal_fp[fp_id]->f_op->read(g_osal_fp[fp_id], buf, len,
						     &g_osal_fp[fp_id]->f_pos);

		up(&osal_fp_sem);

		return read_len;
	}

	return OSAL_FILE_READ_FAIL;
}
EXPORT_SYMBOL(osal_filp_read);

long osal_is_err(int fp_id)
{
	bool err;
	int val = 0;

	if (fp_id >= 1 && fp_id < OSAL_MAX_FP_COUNT) {
		val = down_interruptible(&osal_fp_sem);

		err = IS_ERR(g_osal_fp[fp_id]);

		up(&osal_fp_sem);

		return err;
	}

	/*osal_assert(0); */
	return 1;
}
EXPORT_SYMBOL(osal_is_err);
