/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <apusys_dbg.h>
#include "vpu_debug.h"
#include "vpu_reg.h"
#include "vpu_cmn.h"
#include "vpu_dump.h"

static void vpu_dmp_seq_time(struct seq_file *s, uint64_t t)
{
	uint32_t nsec;

	nsec = do_div(t, 1000000000);
	seq_printf(s, "%lu.%06lu", (unsigned long)t,
		(unsigned long)nsec/1000);
}

static int vpu_dmp_alloc(struct vpu_device *vd)
{
	if (vd->dmp)
		goto out;

	vd->dmp = kvmalloc(sizeof(struct vpu_dmp), GFP_KERNEL);

	if (!vd->dmp)
		return -ENOMEM;

out:
	memset(vd->dmp, 0, sizeof(struct vpu_dmp));
	return 0;
}

/**
 * vpu_dmp_free_locked() - Free VPU register and memory dump
 * @vd: vpu device
 *
 * vd->lock or vd->cmd_lock must be locked before calling this function
 */
void vpu_dmp_free_locked(struct vpu_device *vd)
{
	pr_info("%s:\n", __func__);
	if (!vd->dmp)
		return;

	kfree(vd->dmp);
	vd->dmp = NULL;
}

static void vpu_dmp_iova(void *dst, struct vpu_iova *i, size_t size)
{
	unsigned long base = (unsigned long)vpu_drv->bin_va;

	if (!dst || !size)
		return;

	if (i->bin == VPU_MEM_ALLOC) {
		if (!i->m.va)
			return;
		memcpy(dst, (void *)i->m.va, size);
	} else if (i->size) {
		if (!base)
			return;
		memcpy(dst, (void *)(base + i->bin), size);
	}
}

static void
vpu_dmp_reg(struct vpu_device *vd, uint32_t *r, int offset, int max)
{
	int i;

	for (i = 0; i < max; i++)
		r[i] = vpu_reg_read(vd, offset + (i * 4));
}

static void vpu_dmp_reg_file(struct vpu_device *vd)
{
#define dmp_reg(r) \
	{ vd->dmp->r_##r = vpu_reg_read(vd, r); }

	dmp_reg(CG_CON);
	dmp_reg(SW_RST);
	dmp_reg(DONE_ST);
	dmp_reg(CTRL);
#undef dmp_reg
	vpu_dmp_reg(vd, vd->dmp->r_info, XTENSA_INFO00, VPU_DMP_REG_CNT_INFO);
	vpu_dmp_reg(vd, vd->dmp->r_dbg, DEBUG_INFO00, VPU_DMP_REG_CNT_DBG);
}

static void vpu_dmp_iomem(void *dst, void __iomem *src, size_t size)
{
	memcpy_fromio(dst, src, size);
}

static bool vpu_dmp_is_alive(struct vpu_device *vd)
{
	uint32_t pc;
	int i;

	if (!vd || !vd->dmp)
		return false;

	pc = vd->dmp->r_dbg[5];

	pr_info("%s: debug info05: %x\n", __func__, pc);

	if (!pc)
		return false;

	for (i = 0; i < VPU_DMP_REG_CNT_INFO; i++) {
		if (vd->dmp->r_info[i])
			return true;
	}

	pr_info("%s: all info registers are zeros\n", __func__);

	return false;
}

/**
 * vpu_dmp_create_locked() - Create VPU register and memory dump
 * @vd: vpu device
 * @req: on-going vpu request, set NULL if not available
 *
 * vd->lock or vd->cmd_lock must be locked before calling this function
 */
int vpu_dmp_create_locked(struct vpu_device *vd, struct vpu_request *req,
	const char *fmt, ...)
{
	struct vpu_dmp *d;
	int ret = 0;
	va_list args;

	if (!vd)
		return 0;

	ret = vpu_dmp_alloc(vd);
	if (ret)
		goto out;

	apusys_reg_dump();

	d = vd->dmp;
	d->time = sched_clock();

	va_start(args, fmt);
	vsnprintf(d->info, VPU_DMP_INFO_SZ, fmt, args);
	va_end(args);

	vpu_dmp_reg_file(vd);

#define VPU_DMP_IOMEM(a, A) \
	vpu_dmp_iomem(d->m_##a, vd->a.m, VPU_DMP_##A##_SZ)

	VPU_DMP_IOMEM(reg, REG);

	if (vpu_dmp_is_alive(vd)) {
		VPU_DMP_IOMEM(dmem, DMEM);
		VPU_DMP_IOMEM(imem, IMEM);
	}

#define VPU_DMP_IOVA(a, A) \
	vpu_dmp_iova(d->m_##a, &vd->iova_##a, VPU_DMP_##A##_SZ)

	VPU_DMP_IOVA(reset, RESET);
	VPU_DMP_IOVA(main, MAIN);
	VPU_DMP_IOVA(kernel, KERNEL);
	VPU_DMP_IOVA(iram, IRAM);
	VPU_DMP_IOVA(work, WORK);
#undef VPU_DMP_IOVA

	d->vd_state = vd->state;

	if (req)
		memcpy(&d->req, req, sizeof(struct vpu_request));

	if (vd->algo_curr) {
		memcpy(&d->vd_algo_curr, &vd->algo_curr->a,
			sizeof(struct vpu_algo));
	}

out:
	return ret;
}

static void
vpu_dmp_seq_bar(struct seq_file *s, struct vpu_device *vd, const char *str)
{
	seq_printf(s, "======== vpu%d: %s ========\n", vd->id, str);
}

static void
vpu_dmp_seq_mem(struct seq_file *s, struct vpu_device *vd,
	uint32_t iova, const char *str, void *buf, size_t len)
{
	seq_printf(s, "======== vpu%d: %s@0x%x ========\n", vd->id, str, iova);
	seq_hex_dump(s, str, DUMP_PREFIX_OFFSET, 32, 4, buf, len, false);
}

static void
vpu_dmp_seq_iova(struct seq_file *s, struct vpu_device *vd,
	struct vpu_iova *i, const char *str, void *buf, size_t len)
{
	uint32_t iova;

	if (i->bin == VPU_MEM_ALLOC)
		iova = i->m.pa;
	else if (i->size)
		iova = i->addr;
	else
		return;

	vpu_dmp_seq_mem(s, vd, iova, str, buf, len);
}

static void
vpu_dmp_seq_iomem(struct seq_file *s, struct vpu_device *vd,
	struct vpu_iomem *m, const char *str, void *buf, size_t len)
{
	vpu_dmp_seq_mem(s, vd, (uint32_t)m->res->start, str, buf, len);
}


static void
vpu_dmp_seq_reg(struct seq_file *s, struct vpu_device *vd)
{
	struct vpu_dmp *d = vd->dmp;
	int i;

#define seq_reg(r) \
		seq_printf(s, #r ":\t0x%08x\n", d->r_##r)

		seq_reg(CG_CON);
		seq_reg(SW_RST);
		seq_reg(DONE_ST);
		seq_reg(CTRL);
#undef seq_reg

	for (i = 0; i < VPU_DMP_REG_CNT_INFO; i++)
		seq_printf(s, "XTENSA_INFO%02d:\t0x%08x\n", i, d->r_info[i]);
	for (i = 0; i < VPU_DMP_REG_CNT_DBG; i++)
		seq_printf(s, "DEBUG_INFO%02d:\t0x%08x\n", i, d->r_dbg[i]);
}

void vpu_dmp_seq_core(struct seq_file *s, struct vpu_device *vd)
{
	struct vpu_dmp *d = vd->dmp;

	if (!d)
		return;

	vpu_dmp_seq_bar(s, vd, "device info");
	seq_printf(s, "exception reason: %s\n", d->info);
	seq_puts(s, "exception time: [");
	vpu_dmp_seq_time(s, d->time);
	seq_puts(s, "]\n");
	seq_printf(s, "state: %d\n", d->vd_state);
	seq_printf(s, "loaded algo: %s\n", d->vd_algo_curr.name);

	vpu_dmp_seq_bar(s, vd, "message");
	vpu_mesg_seq(s, vd);

	vpu_dmp_seq_bar(s, vd, "register");
	vpu_dmp_seq_reg(s, vd);

#define VPU_SEQ_IOVA(a, A) \
	vpu_dmp_seq_iova(s, vd, &vd->iova_##a, #a" ", \
		d->m_##a, VPU_DMP_##A##_SZ)

	VPU_SEQ_IOVA(reset, RESET);
	VPU_SEQ_IOVA(main, MAIN);
	VPU_SEQ_IOVA(kernel, KERNEL);
	VPU_SEQ_IOVA(iram, IRAM);
	VPU_SEQ_IOVA(work, WORK);
#undef VPU_SEQ_IOVA

#define VPU_SEQ_IOMEM(a, A) \
	vpu_dmp_seq_iomem(s, vd, &vd->a, #a" ", \
		d->m_##a, VPU_DMP_##A##_SZ)

	VPU_SEQ_IOMEM(reg, REG);
	VPU_SEQ_IOMEM(dmem, DMEM);
	VPU_SEQ_IOMEM(imem, IMEM);
#undef VPU_SEQ_IOMEM
}

/**
 * vpu_dmp_seq() - Show VPU dump of all cores
 * @s: output seq file
 *
 * Called by vpu_debug_vpu_memory()
 */
void vpu_dmp_seq(struct seq_file *s)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;

	mutex_lock(&vpu_drv->lock);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		mutex_lock(&vd->lock);
		vpu_dmp_seq_core(s, vd);
		mutex_unlock(&vd->lock);
	}
	mutex_unlock(&vpu_drv->lock);
}

/**
 * vpu_dmp_clear_all() - Free VPU dump of all cores
 */
void vpu_dmp_free_all(void)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;

	mutex_lock(&vpu_drv->lock);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		vpu_dmp_free(vd);
	}
	mutex_unlock(&vpu_drv->lock);
}

void vpu_dmp_init(struct vpu_device *vd)
{
	vd->dmp = NULL;
}

void vpu_dmp_exit(struct vpu_device *vd)
{
	vpu_dmp_free_locked(vd);
}

