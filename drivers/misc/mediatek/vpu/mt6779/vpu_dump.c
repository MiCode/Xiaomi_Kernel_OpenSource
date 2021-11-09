// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#else
#define m4u_dump_reg_for_vpu_hang_issue(...)
#endif

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/io.h>
#include "vpu_reg.h"
#include "vpu_cmn.h"
#include "vpu_cfg.h"
#include "vpu_dump.h"
#include "vpu_hw.h"
#include <plat_debug_api.h>

#define INFRA_CFG 0x10001000
#define SLEEP 0x10006000
#define GALS 0x1020E000
#define SMI_CMN0 0x10254000
#define SMI_CMN1 0x10255000
#define IPU_CONN 0x19000000
#define IPU_VCORE 0x19020000

static unsigned long m_infra_cfg;
static unsigned long m_sleep;
static unsigned long m_gals;
static unsigned long m_smi_cmn0;
static unsigned long m_smi_cmn1;
static unsigned long m_ipu_conn;
static unsigned long m_ipu_vcore;

#define VPU_MEM_ALLOC  (0xFFFFFFFF)

struct vpu_iova {
	uint32_t addr; /* iova setting */
	uint32_t size;
	uint32_t bin;  /* offset in binary */
	unsigned long va;  /* mapped va */
	uint32_t iova; /* allocated iova */
};

static struct vpu_dmp *vd[MTK_VPU_CORE];

static struct vpu_dmp *vpu_dmp_get(int c)
{
	if (c < 0 || c >= MTK_VPU_CORE)
		return NULL;

	return vd[c];
}

static void vpu_dmp_set(int c, struct vpu_dmp *d)
{
	if (c < 0 || c >= MTK_VPU_CORE)
		return;

	vd[c] = d;
}

static void vpu_dmp_seq_time(struct seq_file *s, uint64_t t)
{
	uint32_t nsec;

	nsec = do_div(t, 1000000000);
	seq_printf(s, "%lu.%06lu", (unsigned long)t,
		(unsigned long)nsec/1000);
}

static int vpu_dmp_alloc(int c)
{
	struct vpu_dmp *d;

	if (c < 0 || c >= MTK_VPU_CORE)
		return -EINVAL;

	d = vpu_dmp_get(c);

	if (d)
		goto out;

	d = kvmalloc(sizeof(struct vpu_dmp), GFP_KERNEL);

	if (!d)
		return -ENOMEM;

out:
	memset(d, 0, sizeof(struct vpu_dmp));
	vpu_dmp_set(c, d);
	return 0;
}

/**
 * vpu_dmp_free_locked() - Free VPU register and memory dump
 * @vd: vpu device
 *
 * vd->lock must be locked before calling this function
 */
void vpu_dmp_free_locked(int c)
{
	struct vpu_dmp *d;

	if (c < 0 || c >= MTK_VPU_CORE)
		return;

	pr_info("%s:\n", __func__);
	d = vpu_dmp_get(c);
	if (!d)
		return;

	kfree(d);
	vpu_dmp_set(c, NULL);
}

static void vpu_dmp_iova(void *dst, struct vpu_iova *i, size_t size)
{
	unsigned long base = vpu_bin_base();

	if (!dst || !size || !base)
		return;

	size = min_t(size_t, size, i->size);

	if (i->bin == VPU_MEM_ALLOC) {
		if (!i->va)
			return;
		memcpy(dst, (void *)i->va, size);
	} else if (i->size) {
		if (!base)
			return;
		memcpy(dst, (void *)(base + i->bin), size);
	}
}

static inline
unsigned long vpu_reg_base(int c)
{
	return vpu_ctl_base(c) + CTRL_BASE_OFFSET;
}

static inline
uint32_t vpu_reg_read(int c, int offset)
{
	return ioread32((void *) (vpu_reg_base(c) + offset));
}

static void
vpu_dmp_reg(int c, uint32_t *r, int offset, int max)
{
	int i;

	for (i = 0; i < max; i++)
		r[i] = vpu_reg_read(c, offset + (i * 4));
}

static void vpu_dmp_reg_file(int c)
{
	struct vpu_dmp *d = vpu_dmp_get(c);

	if (!d)
		return;

#define dmp_reg(r) \
	{ d->r_##r = vpu_reg_read(c, r); }

	dmp_reg(CG_CON);
	dmp_reg(SW_RST);
	dmp_reg(DONE_ST);
	dmp_reg(CTRL);
#undef dmp_reg
	vpu_dmp_reg(c, d->r_info, XTENSA_INFO00, VPU_DMP_REG_CNT_INFO);
	vpu_dmp_reg(c, d->r_dbg, DEBUG_INFO00, VPU_DMP_REG_CNT_DBG);
}

static void
vpu_dmp_ctrl(void *dst, int c, unsigned long offset, size_t size)
{
	unsigned long base = vpu_get_ctrl_base(c);

	if (!base)
		return;

	memcpy_fromio(dst, (void *)(base + offset), size);
}

static void
vpu_dmp_io(void *dst, unsigned long base, size_t size)
{
	if (!base)
		return;

	memcpy_fromio(dst, (void *)(base), size);
}


static bool vpu_dmp_is_alive(int c)
{
	struct vpu_dmp *d;
	uint32_t pc;
	int i;

	d = vpu_dmp_get(c);

	if (!d)
		return false;

	pc = d->r_dbg[5];

	pr_info("%s: vpu%d: debug info05: %x\n", __func__, c, pc);
	if (!pc)
		return false;

	for (i = 0; i < VPU_DMP_REG_CNT_INFO; i++) {
		if (d->r_info[i])
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

struct vpu_dmp_cfg {
	unsigned long reg;
	unsigned long dmem;
	unsigned long imem;
	struct vpu_iova iova_reset;
	struct vpu_iova iova_main;
	struct vpu_iova iova_kernel;
	struct vpu_iova iova_iram;
	struct vpu_iova iova_work;
};

static struct vpu_dmp_cfg vdc[2] = {
	{
		.reg = CTRL_BASE_OFFSET,
		.dmem = 0x0,
		.imem = 0x40000,
		.iova_reset = {0x7da00000, 0x100000, 0x0, 0x0, 0x0},
		.iova_main = {0x7db00000, 0x300000, 0x100000, 0x0, 0x0},
		.iova_kernel = {0x7de00000, 0x500000, VPU_MEM_ALLOC, 0x0, 0x0},
		.iova_iram = {0x0, 0x30000, 0x2950000, 0x0, 0x0},
		.iova_work = {0x0, 0x12000, VPU_MEM_ALLOC, 0x0, 0x0},
	},
	{
		.reg = CTRL_BASE_OFFSET,
		.dmem = 0x0,
		.imem = 0x40000,
		.iova_reset = {0x7e300000, 0x100000, 0x400000, 0x0, 0x0},
		.iova_main = {0x7e400000, 0x300000, 0x500000, 0x0, 0x0},
		.iova_kernel = {0x7e700000, 0x500000, VPU_MEM_ALLOC, 0x0, 0x0},
		.iova_iram = {0x0, 0x30000, 0x2980000, 0x0, 0x0},
		.iova_work = {0x0, 0x12000, VPU_MEM_ALLOC, 0x0, 0x0},
	}
};

int vpu_dmp_create_locked(int c, struct vpu_request *req,
	const char *fmt, ...)
{
	struct vpu_dmp *d;
	int ret = 0;
	va_list args;

	pr_info("%s: vpu%d\n", __func__, c);

	if (c < 0 || c >= MTK_VPU_CORE) {
		pr_info("%s: vpu%d: %d\n", __func__, c, MTK_VPU_CORE);
		ret = -EINVAL;
		goto out;
	}

	ret = vpu_dmp_alloc(c);
	if (ret) {
		pr_info("%s: vpu%d: vpu_dmp_alloc: %d\n", __func__, c, ret);
		goto out;
	}

	d = vpu_dmp_get(c);
	if (!d) {
		pr_info("%s: vpu%d: vpu_dmp_get: %d\n", __func__, c, d);
		goto out;
	}
	d->time = sched_clock();

	va_start(args, fmt);
	ret = vsnprintf(d->info, VPU_DMP_INFO_SZ, fmt, args);
	va_end(args);
	if (ret < 0)
		pr_info("%s: vsnprintf: %d\n", __func__, ret);

#define VPU_DMP_STATE(a) \
	pr_info("%s: vpu%d: %s done. pc: 0x%x\n", \
		__func__, c, #a, vpu_reg_read(c, DEBUG_INFO05))

#define VPU_DMP_IO(a) do { \
		vpu_dmp_io(d->m_##a, m_##a, VPU_DMP_SZ); \
		VPU_DMP_STATE(#a); \
	} while (0)

	VPU_DMP_IO(infra_cfg);
	VPU_DMP_IO(sleep);
	VPU_DMP_IO(gals);
	VPU_DMP_IO(smi_cmn0);
	VPU_DMP_IO(smi_cmn1);

	dump_emi_outstanding();
	VPU_DMP_STATE("emi");

	VPU_DMP_IO(ipu_conn);
	VPU_DMP_IO(ipu_vcore);
#undef VPU_DMP_IO

	m4u_dump_reg_for_vpu_hang_issue();
	VPU_DMP_STATE("m4u");
	vpu_dmp_reg_file(c);
	VPU_DMP_STATE("vpu ctrl (partial)");
	if (req)
		memcpy(&d->req, req, sizeof(struct vpu_request));

	if (!vpu_dmp_is_alive(c))
		goto out;

#define VPU_DMP_CTRL(a, A) do { \
		vpu_dmp_ctrl(d->m_##a, c, vdc[c].a, VPU_DMP_##A##_SZ); \
		VPU_DMP_STATE(#a); \
	} while (0)

	VPU_DMP_CTRL(reg, REG);

	if (vpu_dmp_is_alive(c)) {
		VPU_DMP_CTRL(dmem, DMEM);
		VPU_DMP_CTRL(imem, IMEM);
	}
#undef VPU_DMP_CTRL

#define VPU_DMP_IOVA(a, A) do { \
		vpu_dmp_iova(d->m_##a, &vdc[c].iova_##a, VPU_DMP_##A##_SZ); \
		VPU_DMP_STATE(#a); \
	} while (0)

	VPU_DMP_IOVA(reset, RESET);
	VPU_DMP_IOVA(main, MAIN);
	VPU_DMP_IOVA(kernel, KERNEL);
	VPU_DMP_IOVA(iram, IRAM);
	VPU_DMP_IOVA(work, WORK);
#undef VPU_DMP_IOVA

#undef VPU_DMP_STATE

out:
	return ret;
}

static void
vpu_dmp_seq_bar(struct seq_file *s, int c, const char *str)
{
	seq_printf(s, "======== vpu%d: %s ========\n", c, str);
}

static void
vpu_dmp_seq_mem(struct seq_file *s, int c,
	uint32_t iova, const char *str, void *buf, size_t len)
{
	seq_printf(s, "======== vpu%d: %s@0x%x ========\n", c, str, iova);
	seq_hex_dump(s, str, DUMP_PREFIX_OFFSET, 32, 4, buf, len, false);
}

static void
vpu_dmp_seq_iova(struct seq_file *s, int c,
	struct vpu_iova *i, const char *str, void *buf, size_t len)
{
	uint32_t iova;

	if (i->iova)
		iova = i->iova; /* dynamic alloc */
	else if (i->size)
		iova = i->addr;
	else
		return;

	vpu_dmp_seq_mem(s, c, iova, str, buf, len);
}

static void
vpu_dmp_seq_ctrl(struct seq_file *s, int c,
	unsigned long addr, const char *str, void *buf, size_t len)
{
	uint32_t base;

	if (c == 0)
		base = 0x19100000;
	else if (c == 1)
		base = 0x19200000;
	else
		return;

	vpu_dmp_seq_mem(s, c, base + addr, str, buf, len);
}

static void
vpu_dmp_seq_reg(struct seq_file *s, int c)
{
	struct vpu_dmp *d = vpu_dmp_get(c);
	int i;

	if (!d)
		return;

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

static void vpu_dmp_seq_req(struct seq_file *s, int c)
{
	struct vpu_dmp *d = vpu_dmp_get(c);
	struct vpu_request *req;
	struct vpu_buffer *b;
	struct vpu_plane *p;
	int buf_cnt;
	int plane_cnt;
	int i, j;

	if (!d)
		return;

	req = &d->req;

	if (!req->user_id) {
		seq_puts(s, "N/A\n");
		return;
	}

	buf_cnt = min_t(int, VPU_MAX_NUM_PORTS, req->buffer_count);
	seq_printf(s,
		"sett_ptr: 0x%llx, sett_length: %d, buf_cnt: %d\n",
		req->sett.sett_ptr,
		req->sett.sett_lens,
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

void vpu_dmp_seq_core(struct seq_file *s, int c_s)
{
	unsigned int c = (unsigned int)c_s;
	struct vpu_dmp *d = vpu_dmp_get(c);

	if (!d)
		return;

	vpu_dmp_seq_bar(s, c, "device info");
	seq_printf(s, "exception reason: %s\n", d->info);
	seq_puts(s, "exception time: [");
	vpu_dmp_seq_time(s, d->time);
	seq_puts(s, "]\n");

	vpu_dmp_seq_bar(s, c, "request info");
	vpu_dmp_seq_req(s, c);

	vpu_dmp_seq_bar(s, c, "message");
	vpu_dump_mesg_seq(s, c);

	vpu_dmp_seq_bar(s, c, "register");
	vpu_dmp_seq_reg(s, c);

#define VPU_SEQ_IO(a, A) \
	vpu_dmp_seq_mem(s, c, A, #a" ", d->m_##a, VPU_DMP_SZ)

	VPU_SEQ_IO(infra_cfg, INFRA_CFG);
	VPU_SEQ_IO(sleep, SLEEP);
	VPU_SEQ_IO(gals, GALS);
	VPU_SEQ_IO(smi_cmn0, SMI_CMN0);
	VPU_SEQ_IO(smi_cmn1, SMI_CMN1);
	VPU_SEQ_IO(ipu_conn, IPU_CONN);
	VPU_SEQ_IO(ipu_vcore, IPU_VCORE);
#undef VPU_SEQ_IO

#define VPU_SEQ_IOVA(a, A) \
	vpu_dmp_seq_iova(s, c, &vdc[c].iova_##a, #a" ", \
		d->m_##a, VPU_DMP_##A##_SZ)

	VPU_SEQ_IOVA(reset, RESET);
	VPU_SEQ_IOVA(main, MAIN);
	VPU_SEQ_IOVA(kernel, KERNEL);
	VPU_SEQ_IOVA(iram, IRAM);
	VPU_SEQ_IOVA(work, WORK);
#undef VPU_SEQ_IOVA

#define VPU_SEQ_CTRL(a, A) \
	vpu_dmp_seq_ctrl(s, c, vdc[c].a, #a" ", \
		d->m_##a, VPU_DMP_##A##_SZ)

	VPU_SEQ_CTRL(reg, REG);
	VPU_SEQ_CTRL(dmem, DMEM);
	VPU_SEQ_CTRL(imem, IMEM);
#undef VPU_SEQ_CTRL

}

/**
 * vpu_dmp_seq() - Show VPU dump of all cores
 * @s: output seq file
 *
 * Called by vpu_debug_vpu_memory()
 */
void vpu_dmp_seq(struct seq_file *s)
{
	int c;

	for (c = 0; c < MTK_VPU_CORE; c++)
		vpu_dmp_seq_core(s, c);
}

/**
 * vpu_dmp_clear_all() - Free VPU dump of all cores
 */
void vpu_dmp_free_all(void)
{
	int c;

	for (c = 0; c < MTK_VPU_CORE; c++)
		vpu_dmp_free(c);
}

static void vpu_dmp_init_iova(struct vpu_iova *i, struct vpu_shared_memory *s)
{
	if (!s)
		return;

	i->va = s->va;
	i->iova = s->pa;
}

static void vpu_dmp_init_map(int c)
{
	if (c)
		return;

#define MAPIO(a, A) do { \
		m_##a = (unsigned long)ioremap_wc(A, 0x1000); \
		if (!m_##a) \
			pr_info("%s: unable to map %xh\n", __func__, A); \
	} while (0)

	MAPIO(infra_cfg, INFRA_CFG);
	MAPIO(sleep, SLEEP);
	MAPIO(gals, GALS);
	MAPIO(smi_cmn0, SMI_CMN0);
	MAPIO(smi_cmn1, SMI_CMN1);
#undef MAPIO

	m_ipu_conn = vpu_syscfg_base();
	m_ipu_vcore = vpu_vcore_base();
}

void vpu_dmp_init(int c)
{
	if (c < 0 || c >= MTK_VPU_CORE)
		return;

	vpu_dmp_set(c, NULL);
	vpu_dmp_init_map(c);
	vpu_dmp_init_iova(&vdc[c].iova_kernel, vpu_get_kernel_lib(c));
	vpu_dmp_init_iova(&vdc[c].iova_work, vpu_get_work_buf(c));
	vdc[c].iova_iram.iova = vpu_get_iram_data(c);
}

void vpu_dmp_exit(int c)
{
	if (c < 0 || c >= MTK_VPU_CORE)
		return;
	vpu_dmp_free_locked(c);
}

