// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <apusys_dbg.h>
#include "vpu_cfg.h"
#include "vpu_debug.h"
#include "vpu_reg.h"
#include "vpu_cmn.h"
#include "vpu_cmd.h"
#include "vpu_dump.h"
#include "vpu_tag.h"
#include "vpu_events.h"

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
 * vd->lock must be locked before calling this function
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

	size = min_t(size_t, size, i->size);

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
	vpu_dmp_reg(vd, vd->dmp->r_mbox, MBOX_INBOX_0, VPU_DMP_REG_CNT_MBOX);
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
 * vd->lock must be locked before calling this function
 */
int vpu_dmp_create_locked(struct vpu_device *vd, struct vpu_request *req,
	const char *fmt, ...)
{
	struct vpu_dmp *d;
	int ret = 0;
	int i;
	va_list args;

	if (!vd)
		return 0;

#define VPU_DMP_TRACE(a) \
	trace_vpu_dmp(vd->id, a, vpu_reg_read(vd, DEBUG_INFO05))

	if (vd->dmp && !vd->dmp->read_cnt)
		return 0;

	VPU_DMP_TRACE("checked read_cnt");
	ret = vpu_dmp_alloc(vd);
	if (ret)
		goto out;

	VPU_DMP_TRACE("alloc");
	apusys_reg_dump();
	VPU_DMP_TRACE("apusys_reg_dump");

	d = vd->dmp;
	d->time = sched_clock();

	va_start(args, fmt);
	if (vsnprintf(d->info, VPU_DMP_INFO_SZ, fmt, args) < 0)
		d->info[0] = '\0';
	va_end(args);

	vpu_dmp_reg_file(vd);
	VPU_DMP_TRACE("reg_file");

#define VPU_DMP_IOMEM(a, A) do { \
		vpu_dmp_iomem(d->m_##a, vd->a.m, VPU_DMP_##A##_SZ); \
		VPU_DMP_TRACE(#a); \
	} while (0)

	VPU_DMP_IOMEM(reg, REG);

	if (vpu_dmp_is_alive(vd)) {
		VPU_DMP_IOMEM(dmem, DMEM);
		VPU_DMP_IOMEM(imem, IMEM);
	}

#define VPU_DMP_IOVA(a, A) do { \
		vpu_dmp_iova(d->m_##a, &vd->iova_##a, VPU_DMP_##A##_SZ); \
		VPU_DMP_TRACE(#a); \
	} while (0)

	VPU_DMP_IOVA(reset, RESET);
	VPU_DMP_IOVA(main, MAIN);
	VPU_DMP_IOVA(kernel, KERNEL);
	VPU_DMP_IOVA(iram, IRAM);
	VPU_DMP_IOVA(work, WORK);
#undef VPU_DMP_IOVA

	d->vd_state = vd->state;
	d->vd_dev_state = vd->dev_state;
	d->vd_pw_boost = atomic_read(&vd->pw_boost);

	memcpy(&d->c_ctl, vd->cmd,
		sizeof(struct vpu_cmd_ctl) * VPU_MAX_PRIORITY);

	for (i = 0; i < VPU_MAX_PRIORITY; i++) {
		if (!vd->cmd[i].alg)
			continue;
		memcpy(&d->c_alg[i], vd->cmd[i].alg,
			sizeof(struct __vpu_algo));
		d->c_ctl[i].alg = &d->c_alg[i];
#if VPU_XOS
		if (d->c_alg[i].al == &vd->alp) {
			vpu_dmp_iova(d->m_pl_algo[i],
				&d->c_alg[i].prog,
				VPU_DMP_PRELOAD_SZ);
			vpu_dmp_iova(d->m_pl_iram[i],
				&d->c_alg[i].iram,
				VPU_DMP_IRAM_SZ);
			VPU_DMP_TRACE(d->c_ctl[i].alg->a.name);
		}
#endif
	}

	d->c_prio = atomic_read(&vd->cmd_prio);
	d->c_prio_max = vd->cmd_prio_max;
	d->c_timeout = vd->cmd_timeout;
	d->c_active = atomic_read(&vd->cmd_active);

	if (req) {
		memcpy(&d->req, req, sizeof(struct vpu_request));
		VPU_DMP_TRACE("req");
	}
#undef VPU_DMP_TRACE
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
	for (i = 0; i < VPU_DMP_REG_CNT_MBOX; i++)
		seq_printf(s, "MBOX_INBOX_%d:\t0x%08x\n", i, d->r_mbox[i]);
}

static void vpu_dmp_seq_pl_algo(struct seq_file *s, struct vpu_device *vd)
{
#if VPU_XOS
	struct vpu_dmp *d = vd->dmp;
	int prio_max = d->c_prio_max;
	int i;

	if (prio_max > VPU_MAX_PRIORITY)
		prio_max = VPU_MAX_PRIORITY;

	for (i = 0; i < prio_max; i++) {
		char str[64];
		char *a_name = NULL;
		struct __vpu_algo *alg = &d->c_alg[i];

		if (!alg->a.name[0])
			continue;
		if (alg->al != &vd->alp)
			continue;

		/* get algo short name, after last '_' */
		a_name = strrchr(alg->a.name, '_');
		a_name = (a_name) ? (a_name + 1) : alg->a.name;

		if (snprintf(str, sizeof(str), "p%d/%s/prog ",
			i, a_name) <= 0)
			str[0] = '\0';
		vpu_dmp_seq_mem(s, vd, alg->a.mva, str,
			d->m_pl_algo[i],
			min(VPU_DMP_PRELOAD_SZ, alg->prog.size));

		if (snprintf(str, sizeof(str), "p%d/%s/iram ",
			i, a_name) <= 0)
			str[0] = '\0';
		vpu_dmp_seq_mem(s, vd, alg->a.iram_mva, str,
			d->m_pl_iram[i],
			min(VPU_DMP_IRAM_SZ, alg->iram.size));
	}
#endif
}

static void vpu_dmp_seq_req(struct seq_file *s, struct vpu_device *vd)
{
	struct vpu_dmp *d = vd->dmp;
	struct vpu_request *req;
	struct vpu_buffer *b;
	struct vpu_plane *p;
	int buf_cnt;
	int plane_cnt;
	int i, j;

	if (!d)
		return;

	req = &d->req;

	if (!req->algo[0]) {
		seq_puts(s, "N/A\n");
		return;
	}

	buf_cnt = min_t(int, VPU_MAX_NUM_PORTS, req->buffer_count);
	seq_printf(s,
		"algo: %s, sett_ptr: 0x%llx, sett_length: %d, buf_cnt: %d\n",
		req->algo,
		req->sett_ptr,
		req->sett_length,
		req->buffer_count);

	for (i = 0; i < buf_cnt; i++) {
		b = &req->buffers[i];
		seq_printf(s,
			"buf%d: port:%d, %dx%d, format:%d, plane_cnt: %d\n",
			i,
			b->port_id,
			b->width, b->height,
			b->format,
			b->plane_count);

		plane_cnt = min_t(int, 3,  b->plane_count);
		for (j = 0; j < plane_cnt; j++) {
			p = &b->planes[j];
			seq_printf(s,
				"buf%d.plane%d: ptr:0x%llx, length:%d, stride:%d\n",
				i, j,
				p->ptr,
				p->length,
				p->stride);
		}
	}
}

void vpu_dmp_seq_core(struct seq_file *s, struct vpu_device *vd)
{
	struct vpu_dmp *d = vd->dmp;

	if (!d) {
		vpu_dmp_seq_bar(s, vd, "there's no exception");
		vpu_dmp_seq_bar(s, vd, "current message");
		vpu_mesg_seq(s, vd);
		return;
	}

	d->read_cnt++;
	vpu_dmp_seq_bar(s, vd, "exception info");
	seq_printf(s, "exception reason: %s\n", d->info);
	seq_puts(s, "exception time: [");
	vpu_seq_time(s, d->time);
	seq_puts(s, "]\n");

	vpu_debug_state_seq(s, d->vd_state, d->vd_dev_state, d->vd_pw_boost);

	vpu_dmp_seq_bar(s, vd, "commands");
	vpu_debug_cmd_seq(s, vd, d->c_prio,
		d->c_prio_max, d->c_active, d->c_ctl, d->c_timeout);

	vpu_dmp_seq_bar(s, vd, "request info");
	vpu_dmp_seq_req(s, vd);

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

	vpu_dmp_seq_pl_algo(s, vd);
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
		mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV);
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

