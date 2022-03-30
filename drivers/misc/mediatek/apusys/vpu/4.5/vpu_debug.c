// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>

#include "vpu_cfg.h"
#include "vpu_cmn.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "vpu_hw.h"
#include "vpu_power.h"
#include "vpu_cmd.h"
#include "apu_tags.h"
#include "vpu_reg.h"

u32 vpu_klog;

const char *g_vpu_prop_type_names[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = "char",
	[VPU_PROP_TYPE_INT32]    = "int32",
	[VPU_PROP_TYPE_FLOAT]    = "float",
	[VPU_PROP_TYPE_INT64]    = "int64",
	[VPU_PROP_TYPE_DOUBLE]   = "double"
};

const char *g_vpu_port_usage_names[VPU_NUM_PORT_USAGES] = {
	[VPU_PORT_USAGE_IMAGE]     = "image",
	[VPU_PORT_USAGE_DATA]      = "data",
};

const char *g_vpu_port_dir_names[VPU_NUM_PORT_DIRS] = {
	[VPU_PORT_DIR_IN]       = "in",
	[VPU_PORT_DIR_OUT]      = "out",
	[VPU_PORT_DIR_IN_OUT]   = "in-out",
};

struct name_tag {
	uint32_t t;     /* tag */
	const char *n;  /* name */
};

/* driver state, defined at vpu_cmn.h */
#define VS(a)  { VS_##a, #a }
static const struct name_tag vs_str[] = {
	VS(UNKNOWN),
	VS(DISALBED),
	VS(DOWN),
	VS(UP),
	VS(BOOT),
	VS(IDLE),
	VS(CMD_ALG),
	VS(CMD_D2D),
	VS(CMD_D2D_EXT),
	VS(REMOVING),
	{0, NULL}
};
#undef VS

/* device state, defined at vpu_reg.h */
#define DS(a)  { DS_##a, #a }
static const struct name_tag ds_str[] = {
	DS(DSP_RDY),
	DS(DBG_RDY),
	DS(ALG_RDY),
	DS(ALG_DONE),
	DS(ALG_GOT),
	DS(PREEMPT_RDY),
	DS(PREEMPT_DONE),
	DS(FTRACE_RDY),
	{0, NULL}
};
#undef DS

/* command code, defined at vpu_reg.h */
#define VC(a)  { VPU_CMD_##a, #a }
static const struct name_tag vc_str[] = {
	VC(DO_EXIT),
	VC(DO_LOADER),
	VC(DO_D2D),
	VC(DO_D2D_EXT),
	VC(SET_DEBUG),
	VC(SET_FTRACE_LOG),
	VC(DO_D2D_EXT_TEST),
	{0, NULL}
};
#undef VC

static int vpu_debug_info(struct seq_file *s);

/**
 * vpu_seq_time() - print time to seq_file
 *
 * @time: schedule clock time got from sched_clock()
 */
void vpu_seq_time(struct seq_file *s, uint64_t t)
{
	uint32_t nsec;

	if (!t) {
		seq_puts(s, "N/A");
		return;
	}

	nsec = do_div(t, 1000000000);
	seq_printf(s, "%lu.%06lu", (unsigned long)t,
		(unsigned long)nsec/1000);
}

int vpu_debug_state_seq(struct seq_file *s, uint32_t vs, uint32_t ds, int b)
{
	int i, j;

	if (!s)
		return -ENOENT;

	for (i = 0; vs_str[i].n && (vs != vs_str[i].t); i++)
		;
	seq_printf(s, "driver state: %d (%s)\n", vs,
		vs_str[i].n ? vs_str[i].n : "unknown");

	seq_printf(s, "device state: %xh: ", ds);
	for (i = 0, j = 0; ds_str[i].n; i++) {
		if (ds & ds_str[i].t)
			seq_printf(s, "%s%s", j++ ? "," : "", ds_str[i].n);
	}
	seq_puts(s, "\n");
	seq_puts(s, "boost: ");
	vpu_seq_boost(s, b);
	seq_puts(s, "\n");
	return 0;
}

static inline
int vpu_debug_state_dev(struct seq_file *s, struct vpu_device *vd)
{
	return vpu_debug_state_seq(s, vd->state, vd->dev_state,
		atomic_read(&vd->pw_boost));
}

/**
 * vpu_debug_cmd_entry_seq -Show command control of given priority
 *
 * @s: pointer to seq_file
 * @c: pointer to command control
 * @prio: priority
 *
 * Returns the execution count of given priority.
 */
static uint64_t
vpu_debug_cmd_entry_seq(struct seq_file *s, struct vpu_cmd_ctl *c, int prio)
{
	unsigned int i;

	seq_printf(s, "priority %d: #%llu: ", prio, c->exe_cnt);
	for (i = 0; vc_str[i].n; i++) {
		if (c->cmd == vc_str[i].t) {
			seq_printf(s, "%s: ", vc_str[i].n);
			goto algo;
		}
	}
	seq_printf(s, "(unknown cmd 0x%x): ", c->cmd);
algo:
	seq_printf(s, "algorithm: %s, boost: ",
		c->alg ? c->alg->a.name : "(none)");
	vpu_seq_boost(s, c->boost);
	seq_puts(s, ", start: ");
	vpu_seq_time(s, c->start_t);
	seq_puts(s, ", end: ");
	vpu_seq_time(s, c->end_t);
	seq_printf(s, ", done: %d, result: 0x%x\n", c->done, c->result);

	return c->exe_cnt;
}

int vpu_debug_cmd_seq(struct seq_file *s, struct vpu_device *vd, int prio,
	int prio_max, int active, struct vpu_cmd_ctl *c, uint64_t timeout)
{
	int i;
	uint64_t exe_cnt;
	uint64_t preempt_cnt = 0;

	seq_printf(s, "priority: current: %d (highest: %d)\n",
		prio, prio_max - 1);
	seq_printf(s, "timeout setting: %llu ms\n", timeout);
	seq_printf(s, "number of active commands: %d\n", active);

	if (prio_max > VPU_MAX_PRIORITY)
		prio_max = VPU_MAX_PRIORITY;

	for (i = 0; i < prio_max; i++) {
		exe_cnt = vpu_debug_cmd_entry_seq(s, &c[i], i);
		preempt_cnt += (i) ? exe_cnt : 0;
	}
	seq_printf(s, "preemption count: %llu\n", preempt_cnt);

	return 0;
}

static inline
int vpu_debug_cmd_dev(struct seq_file *s, struct vpu_device *vd)
{
	return vpu_debug_cmd_seq(s, vd, atomic_read(&vd->cmd_prio),
		vd->cmd_prio_max, atomic_read(&vd->cmd_active),
		vd->cmd, vd->cmd_timeout);
}

static int vpu_debug_reg_dev(struct seq_file *s, struct vpu_device *vd)
{
	int ret;
	struct vpu_register *r;

	if (!vd)
		return -ENODEV;

	mutex_lock_nested(&vd->lock, VPU_MUTEX_DEV);
	if (!vd_is_available(vd))
		goto out;

	if (vd->state == VS_DOWN) {
		seq_printf(s, "vpu%d is unpowered\n", vd->id);
		goto out;
	}

	ret = vpu_pwr_get_locked_nb(vd);
	if (ret)
		goto out;

	r = vd_reg(vd);

	seq_puts(s, "name\toffset\tvalue\n");

#define seq_vpu_reg(a) \
	seq_printf(s, "%s\t%4xh\t%08xh\n", #a, r->a, vpu_reg_read(vd, a))

	seq_vpu_reg(cg_con);
	seq_vpu_reg(sw_rst);
	seq_vpu_reg(done_st);
	seq_vpu_reg(ctrl);
	seq_vpu_reg(xtensa_int);
	seq_vpu_reg(ctl_xtensa_int);
	seq_vpu_reg(default0);
	seq_vpu_reg(default1);
	seq_vpu_reg(xtensa_info00);
	seq_vpu_reg(xtensa_info01);
	seq_vpu_reg(xtensa_info02);
	seq_vpu_reg(xtensa_info03);
	seq_vpu_reg(xtensa_info04);
	seq_vpu_reg(xtensa_info05);
	seq_vpu_reg(xtensa_info06);
	seq_vpu_reg(xtensa_info07);
	seq_vpu_reg(xtensa_info08);
	seq_vpu_reg(xtensa_info09);
	seq_vpu_reg(xtensa_info10);
	seq_vpu_reg(xtensa_info11);
	seq_vpu_reg(xtensa_info12);
	seq_vpu_reg(xtensa_info13);
	seq_vpu_reg(xtensa_info14);
	seq_vpu_reg(xtensa_info15);
	seq_vpu_reg(xtensa_info16);
	seq_vpu_reg(xtensa_info17);
	seq_vpu_reg(xtensa_info18);
	seq_vpu_reg(xtensa_info19);
	seq_vpu_reg(xtensa_info20);
	seq_vpu_reg(xtensa_info21);
	seq_vpu_reg(xtensa_info22);
	seq_vpu_reg(xtensa_info23);
	seq_vpu_reg(xtensa_info24);
	seq_vpu_reg(xtensa_info25);
	seq_vpu_reg(xtensa_info26);
	seq_vpu_reg(xtensa_info27);
	seq_vpu_reg(xtensa_info28);
	seq_vpu_reg(xtensa_info29);
	seq_vpu_reg(xtensa_info30);
	seq_vpu_reg(xtensa_info31);
	seq_vpu_reg(mbox_inbox_0);
	seq_vpu_reg(mbox_inbox_1);
	seq_vpu_reg(mbox_inbox_2);
	seq_vpu_reg(mbox_inbox_3);
	seq_vpu_reg(mbox_inbox_4);
	seq_vpu_reg(mbox_inbox_5);
	seq_vpu_reg(mbox_inbox_6);
	seq_vpu_reg(mbox_inbox_7);
	seq_vpu_reg(mbox_inbox_8);
	seq_vpu_reg(mbox_inbox_9);
	seq_vpu_reg(mbox_inbox_10);
	seq_vpu_reg(mbox_inbox_11);
	seq_vpu_reg(mbox_inbox_12);
	seq_vpu_reg(mbox_inbox_13);
	seq_vpu_reg(mbox_inbox_14);
	seq_vpu_reg(mbox_inbox_15);
	seq_vpu_reg(mbox_inbox_16);
	seq_vpu_reg(mbox_inbox_17);
	seq_vpu_reg(mbox_inbox_18);
	seq_vpu_reg(mbox_inbox_19);
	seq_vpu_reg(mbox_inbox_20);
	seq_vpu_reg(mbox_inbox_21);
	seq_vpu_reg(mbox_inbox_22);
	seq_vpu_reg(mbox_inbox_23);
	seq_vpu_reg(mbox_inbox_24);
	seq_vpu_reg(mbox_inbox_25);
	seq_vpu_reg(mbox_inbox_26);
	seq_vpu_reg(mbox_inbox_27);
	seq_vpu_reg(mbox_inbox_28);
	seq_vpu_reg(mbox_inbox_29);
	seq_vpu_reg(mbox_inbox_30);
	seq_vpu_reg(mbox_inbox_31);
	seq_vpu_reg(debug_info00);
	seq_vpu_reg(debug_info01);
	seq_vpu_reg(debug_info02);
	seq_vpu_reg(debug_info03);
	seq_vpu_reg(debug_info04);
	seq_vpu_reg(debug_info05);
	seq_vpu_reg(debug_info06);
	seq_vpu_reg(debug_info07);
	seq_vpu_reg(xtensa_altresetvec);
#undef seq_vpu_reg

	vpu_pwr_put_locked_nb(vd);
out:
	mutex_unlock(&vd->lock);

	return 0;
}

static char *vpu_debug_simple_write(const char __user *buffer, size_t count)
{
	char *buf;
	int ret;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		goto out;

	ret = copy_from_user(buf, buffer, count);
	if (ret) {
		pr_info("%s: copy_from_user: ret=%d\n", __func__, ret);
		kfree(buf);
		buf = NULL;
		goto out;
	}

	buf[count] = '\0';
out:
	return buf;
}

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
static int vpu_debug_algo_entry(struct seq_file *s,
	struct vpu_algo_list *al, struct __vpu_algo *alg)
{
	if (!alg)
		return -ENOENT;

	if (!al || !al->ops || !al->ops->get || !al->ops->put)
		goto out;

	al->ops->get(al, NULL, alg);
	seq_printf(s, "[%s: prog: 0x%llx/0x%x, iram: 0x%llx/0x%x, entry: 0x%x, ref: %d, builtin: %d]\n",
		alg->a.name, alg->a.mva, alg->a.len,
		alg->a.iram_mva, alg->a.iram_len, alg->a.entry_off,
		kref_read(&alg->ref), alg->builtin);
	al->ops->put(alg);
out:
	return 0;
}

static int vpu_debug_algo_list(struct seq_file *s, struct vpu_algo_list *al)
{
	struct __vpu_algo *alg;
	int ret;
	struct list_head *ptr, *tmp;

	if (!al)
		return -ENOENT;

	seq_printf(s, "\n<%s Algorithms>\n", al->name);
	list_for_each_safe(ptr, tmp, &al->a) {
		alg = list_entry(ptr, struct __vpu_algo, list);
		ret = vpu_debug_algo_entry(s, al, alg);
		if (ret)
			break;
	}
	return ret;
}

static int vpu_debug_algo(struct seq_file *s)
{
	struct vpu_device *vd;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;
	if (!vd)
		return -ENODEV;

	vpu_debug_algo_list(s, &vd->aln);

	if (bin_type(vd) == VPU_IMG_PRELOAD)
		vpu_debug_algo_list(s, &vd->alp);

	return 0;
}

static int vpu_debug_reg(struct seq_file *s)
{
	if (!s)
		return -ENOENT;

	return vpu_debug_reg_dev(s, s->private);
}

/**
 * vpu_debug_jtag_set - enable/disable jtag
 *
 * @data: vpu device
 * @val: user write value
 *
 * return 0: enable/disable success
 */
static int vpu_debug_jtag_set(void *data, u64 val)
{
	struct vpu_device *vd = data;
	int ret = 0;

	if (!vd)
		return -ENOENT;

	if (val) {
		/* enable jtag */
		vd->jtag_enabled = 1;
	} else {
		/* disable jtag */
		vd->jtag_enabled = 0;
	}

	return ret;
}

/**
 * vpu_debug_jtag_get - show jtag status of vpu
 * @data: vpu device
 * @val: return value
 *
 * return 0: operation success
 */
static int vpu_debug_jtag_get(void *data, u64 *val)
{
	struct vpu_device *vd = data;

	if (!vd)
		return -ENOENT;

	*val = vd->jtag_enabled;
	return 0;
}

static int vpu_debug_dump(struct seq_file *s)
{
	struct vpu_device *vd;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;
	vpu_dmp_seq_core(s, vd);

	return 0;
}

static ssize_t vpu_debug_dump_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *buf, *cmd, *cur;
	struct vpu_device *vd;

	if (!filp || !filp->f_inode)
		goto out;

	vd = (struct vpu_device *) filp->f_inode->i_private;

	if (!vd)
		goto out;

	buf = vpu_debug_simple_write(buffer, count);

	if (!buf)
		goto out;

	cur = buf;
	cmd = strsep(&cur, " \t\n");
	if (!strcmp(cmd, "free"))
		vpu_dmp_free(vd);
	else if (!strcmp(cmd, "dump"))
		vpu_dmp_create(vd, NULL, "Dump trigger by user");

	kfree(buf);
out:
	return count;
}

static int vpu_debug_cmd(struct seq_file *s)
{
	if (!s || !s->private)
		return -ENOENT;

	return vpu_debug_cmd_dev(s, s->private);
}

static int vpu_debug_state(struct seq_file *s)
{
	if (!s || !s->private)
		return -ENOENT;

	return vpu_debug_state_dev(s, s->private);
}
#endif
/* end of CONFIG_MTK_APUSYS_VPU_DEBUG */

static void *vpu_mesg_pa_to_va(struct vpu_mem *work_buf, unsigned int phys_addr)
{
	unsigned long ret = 0;
	int offset = 0;

	if (!phys_addr)
		return NULL;

	offset = phys_addr - work_buf->pa;
	ret = work_buf->va + offset;

	return (void *)(ret);
}

static void vpu_mesg_clr(struct vpu_device *vd)
{
	char *data = NULL;
	struct vpu_message_ctrl *msg = vpu_mesg(vd);

	if (!msg)
		return;

	data = (char *)vpu_mesg_pa_to_va(&vd->iova_work.m, msg->data);

	msg->head = 0;
	msg->tail = 0;

	if (data)
		memset(data, 0,
		       vd->wb_log_data - sizeof(struct vpu_message_ctrl));
}

int vpu_mesg_seq(struct seq_file *s, struct vpu_device *vd)
{
	int i, wrap = false;
	char *data = NULL;
	struct vpu_message_ctrl *msg = vpu_mesg(vd);

	if (!s || !msg)
		return -ENOENT;

	vd_mops(vd)->sync_for_cpu(vd->dev, &vd->iova_work);
	data = (char *)vpu_mesg_pa_to_va(&vd->iova_work.m, msg->data);
	i = msg->head;
	do {
		if (msg->head == msg->tail || i == msg->tail)
			seq_printf(s, "%s", "<empty log>\n");
		while (i != msg->tail && data) {
			if (i > msg->tail && wrap)
				break;

			seq_printf(s, "%s", data + i);
			i += strlen(data + i) + 1;

			if (i >= msg->buf_size) {
				i = 0;
				wrap = true;
			}
		}
	} while (0);

	return 0;
}

static int vpu_proc_vpu_memory(struct seq_file *s)
{
	vpu_debug_info(s);
	seq_puts(s, "======== Tags ========\n");
	apu_tags_seq(vpu_drv->tags, s);
	vpu_dmp_seq(s);
	return 0;
}

static ssize_t vpu_proc_vpu_memory_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *buf, *cmd, *cur;

	pr_info("%s:\n", __func__);

	buf = vpu_debug_simple_write(buffer, count);
	if (!buf)
		goto out;

	cur = buf;
	cmd = strsep(&cur, " \t\n");
	if (!strcmp(cmd, "free"))
		vpu_dmp_free_all();

	kfree(buf);
out:
	return count;
}

static int vpu_proc_mesg(struct seq_file *s)
{
	if (!s)
		return -ENOENT;

	return vpu_mesg_seq(s, (struct vpu_device *)s->private);
}

static ssize_t vpu_proc_mesg_level_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *buf, *cmd, *cur;
	struct vpu_device *vd;
	struct vpu_message_ctrl *msg = NULL;
	int level = 0;
	int ret = 0;

	if (!filp || !filp->f_inode)
		goto out;

	vd = (struct vpu_device *) PDE_DATA(filp->f_inode);
	if (!vd)
		goto out;

	buf = vpu_debug_simple_write(buffer, count);
	if (!buf)
		goto out;

	cur = buf;
	cmd = strsep(&cur, " \t\n");
	ret = kstrtoint(cmd, 10, &level);
	if (ret)
		goto free_buf;

	if (!level) {
		vpu_mesg_clr(vd);
		goto free_buf;
	}

	if (level < -1 || level >= VPU_DBG_MSG_LEVEL_TOTAL) {
		pr_info("val: %d\n", level);
		goto free_buf;
	}

	msg = vpu_mesg(vd);
	if (!msg)
		goto free_buf;

	if (level != -1)
		msg->level_mask ^= (1 << level);
	else
		msg->level_mask = 0;

free_buf:
	kfree(buf);
out:
	return count;
}

static int vpu_proc_mesg_level(struct seq_file *s)
{
	struct vpu_device *vd;
	struct vpu_message_ctrl *msg;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;

	msg = vpu_mesg(vd);
	if (!msg)
		return -ENOENT;

	seq_printf(s, "%u\n", msg->level_mask);

	return 0;
}

void vpu_seq_boost(struct seq_file *s, int boost)
{
	if (boost == VPU_PWR_NO_BOOST)
		seq_puts(s, "unset");
	else
		seq_printf(s, "%d", boost);
}

const char *vpu_debug_cmd_str(int cmd)
{
	unsigned int i;

	for (i = 0; vc_str[i].n; i++) {
		if (cmd == vc_str[i].t)
			return vc_str[i].n;
	}
	return "(unknown)";
}

static int vpu_debug_iova_seq(struct seq_file *s,
	struct vpu_iova *i, const char *prefix)
{
	if (i->bin == VPU_MEM_ALLOC) {
		if (i->addr)
			seq_printf(s, "<%s> iova: 0x%llx, size: 0x%x (static alloc)\n",
				prefix,	i->iova, i->size);
		else {
			seq_printf(s, "<%s> iova: 0x%llx, size: 0x%x (dynamic alloc)\n",
				prefix,	i->iova, i->size);
		}
	} else if (i->size) {
		seq_printf(s, "<%s> iova: 0x%llx, size: 0x%x, bin offset: 0x%x\n",
			prefix, i->iova, i->size, i->bin);
	}
	return 0;
}

static int vpu_debug_iomem_seq(struct seq_file *s,
	struct vpu_iomem *i, const char *prefix)
{
	struct resource *r;

	if (!i || !i->res)
		return -ENOENT;

	r = i->res;
	seq_printf(s, "<%s> start: 0x%lx, end: 0x%lx, size: 0x%lx\n",
		prefix,	(unsigned long)r->start, (unsigned long)r->end,
		(unsigned long)(r->end - r->start + 1));

	return 0;
}

static int vpu_debug_info_dev(struct seq_file *s, struct vpu_device *vd)
{
	if (!s || !vd)
		return -ENOENT;

	if (!vd_is_available(vd))
		return 0;

	seq_printf(s, "\n----- %s: settings -----\n", vd->name);
	seq_printf(s, "mva_iram: 0x%llx\n", vd->mva_iram);
	seq_printf(s, "cmd_prio_max: %d\n", vd->cmd_prio_max);
	seq_printf(s, "cmd_timeout: %llu\n", vd->cmd_timeout);
	seq_printf(s, "pw_off_latency: %llu\n", vd->pw_off_latency);
	seq_printf(s, "wb_log_size: 0x%x\n", vd->wb_log_size);
	seq_printf(s, "wb_log_data: 0x%x\n", vd->wb_log_data);
	seq_puts(s, "iova:\n");
	vpu_debug_iova_seq(s, &vd->iova_reset, "reset");
	vpu_debug_iova_seq(s, &vd->iova_main, "main");
	vpu_debug_iova_seq(s, &vd->iova_kernel, "kernel");
	vpu_debug_iova_seq(s, &vd->iova_iram, "iram");
	vpu_debug_iova_seq(s, &vd->iova_work, "work");
	seq_puts(s, "iomem:\n");
	vpu_debug_iomem_seq(s, &vd->reg, "reg");
	vpu_debug_iomem_seq(s, &vd->dmem, "dmem");
	vpu_debug_iomem_seq(s, &vd->imem, "imem");
	vpu_debug_iomem_seq(s, &vd->dbg, "dbg");
	seq_printf(s, "----- %s: current state -----\n", vd->name);
	vpu_debug_state_dev(s, vd);
	seq_printf(s, "----- %s: current commands -----\n", vd->name);
	vpu_debug_cmd_dev(s, vd);
	seq_printf(s, "----- %s: current registers -----\n", vd->name);
	vpu_debug_reg_dev(s, vd);

	return 0;
}

static int vpu_debug_info(struct seq_file *s)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;

	if (!s)
		return -ENOENT;

	mutex_lock(&vpu_drv->lock);

	seq_puts(s, "======== Driver Info ========\n");
	seq_puts(s, "Queried Time: ");
	vpu_seq_time(s, sched_clock());
	seq_printf(s, "\n%s%s%s\n",
		VPU_XOS ? "XOS" : "Non-XOS",
		VPU_IMG_LEGACY ? ",Legacy Image" : "",
		VPU_IMG_PRELOAD ? ",Preload Image" : "");
	seq_printf(s, "bin_pa: 0x%lx\n", vpu_drv->bin_pa);
	seq_printf(s, "bin_size: 0x%x\n", vpu_drv->bin_size);
	seq_printf(s, "bin_head_ofs: 0x%x\n", vpu_drv->bin_head_ofs);
	seq_printf(s, "bin_preload_ofs: 0x%x\n", vpu_drv->bin_preload_ofs);
	seq_printf(s, "mva_algo: 0x%llx\n", vpu_drv->mva_algo);
	vpu_debug_iova_seq(s, &vpu_drv->iova_algo, "algo");

	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		vpu_debug_info_dev(s, vd);
	}

	if (vpu_drv->vp && vpu_drv->vp->mops && vpu_drv->vp->mops->show) {
		seq_puts(s, "======== IOVA Info ========\n");
		vpu_drv->vp->mops->show(s);
	}
	mutex_unlock(&vpu_drv->lock);
	seq_puts(s, "\n");

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)

#define VPU_DEBUGFS_FOP_DEF(name) \
static struct dentry *vpu_d##name; \
static int vpu_debug_## name ##_show(struct seq_file *s, void *unused) \
{ \
	vpu_debug_## name(s); \
	return 0; \
} \
static int vpu_debug_## name ##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, vpu_debug_ ## name ## _show, \
		inode->i_private); \
} \

#define VPU_DEBUGFS_DEF(name) \
	VPU_DEBUGFS_FOP_DEF(name) \
static const struct file_operations vpu_debug_ ## name ## _fops = { \
	.open = vpu_debug_ ## name ## _open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define VPU_DEBUGFS_RW_DEF(name) \
	VPU_DEBUGFS_FOP_DEF(name) \
static const struct file_operations vpu_debug_ ## name ## _fops = { \
	.open = vpu_debug_ ## name ## _open, \
	.read = seq_read, \
	.write = vpu_debug_ ## name ## _write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

VPU_DEBUGFS_DEF(algo);
VPU_DEBUGFS_RW_DEF(dump);
VPU_DEBUGFS_DEF(reg);
VPU_DEBUGFS_DEF(state);
VPU_DEBUGFS_DEF(cmd);
VPU_DEBUGFS_DEF(info);

#undef VPU_DEBUGFS_DEF

static struct dentry *vpu_djtag;
DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_jtag_fops, vpu_debug_jtag_get,
			vpu_debug_jtag_set, "%lld\n");

#define VPU_DEBUGFS_MDOE		0644

#define VPU_DEBUGFS_CREATE(name) \
{ \
	vpu_d##name = debugfs_create_file(#name, VPU_DEBUGFS_MDOE, \
		droot,         \
			NULL, &vpu_debug_ ## name ## _fops); \
	if (IS_ERR_OR_NULL(vpu_d##name)) { \
		ret = PTR_ERR(vpu_d##name); \
		pr_info("%s: vpu%d: " #name "): %d\n", \
			__func__, (vd) ? (vd->id) : 0, ret); \
		goto out; \
	} \
	vpu_d##name->d_inode->i_private = vd; \
}

#endif
/* end of CONFIG_MTK_APUSYS_VPU_DEBUG */

#define VPU_PROCFS_FOP_DEF(name) \
static struct proc_dir_entry *vpu_proc_entry_##name; \
static int vpu_proc_## name ##_show(struct seq_file *s, void *unused) \
{ \
	vpu_proc_## name(s); \
	return 0; \
} \
static int vpu_proc_## name ##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, vpu_proc_ ## name ## _show, \
		PDE_DATA(inode)); \
} \

#define VPU_PROCFS_DEF(name) \
	VPU_PROCFS_FOP_DEF(name) \
static const struct proc_ops vpu_proc_ ## name ## _ops = { \
	.proc_open = vpu_proc_ ## name ## _open, \
	.proc_read = seq_read, \
	.proc_lseek = seq_lseek, \
	.proc_release = single_release, \
}

#define VPU_PROCFS_RW_DEF(name) \
	VPU_PROCFS_FOP_DEF(name) \
static const struct proc_ops vpu_proc_ ## name ## _ops = { \
	.proc_open = vpu_proc_ ## name ## _open, \
	.proc_read = seq_read, \
	.proc_write = vpu_proc_ ## name ## _write, \
	.proc_lseek = seq_lseek, \
	.proc_release = single_release, \
}

VPU_PROCFS_RW_DEF(vpu_memory);
VPU_PROCFS_DEF(mesg);
VPU_PROCFS_RW_DEF(mesg_level);

#undef VPU_PROCFS_DEF
#undef VPU_PROCFS_RW_DEF

#define VPU_PROCFS_CREATE(name) \
{ \
	vpu_proc_entry_##name = proc_create_data(#name, 0440, \
		proc_root, &vpu_proc_ ## name ## _ops, vd); \
	if (IS_ERR_OR_NULL(vpu_proc_entry_##name)) { \
		ret = PTR_ERR(vpu_proc_entry_##name); \
		pr_info("%s: vpu%d: " #name "): %d\n", \
			__func__, (vd) ? (vd->id) : 0, ret); \
		goto out; \
	} \
}

#define VPU_PROCFS_CREATE_RW(name) \
{ \
	vpu_proc_entry_##name = proc_create_data(#name, 0640, \
		proc_root, &vpu_proc_ ## name ## _ops, vd); \
	if (IS_ERR_OR_NULL(vpu_proc_entry_##name)) { \
		ret = PTR_ERR(vpu_proc_entry_##name); \
		pr_info("%s: vpu%d: " #name "): %d\n", \
			__func__, (vd) ? (vd->id) : 0, ret); \
		goto out; \
	} \
}

int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret = 0;
	struct proc_dir_entry *proc_root;
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	struct dentry *droot;
#endif

	vd->proc_root = NULL;
	vpu_mesg_init(vd);
	vpu_dmp_init(vd);

	if (!vpu_drv->proc_root)
		return -ENODEV;

	proc_root = proc_mkdir(vd->name, vpu_drv->proc_root);
	if (IS_ERR_OR_NULL(proc_root)) {
		ret = PTR_ERR(proc_root);
		pr_info("%s: failed to create procfs node: vpu/%s: %d\n",
			__func__, vd->name, ret);
		goto out;
	}

	vd->proc_root = proc_root;
	VPU_PROCFS_CREATE(mesg);
	VPU_PROCFS_CREATE_RW(mesg_level);

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	vd->droot = NULL;

	if (!vpu_drv->droot)
		return -ENODEV;

	droot = debugfs_create_dir(vd->name, vpu_drv->droot);
	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: vpu/%s: %d\n",
			__func__, vd->name, ret);
		goto out;
	}
	vd->droot = droot;
	debugfs_create_u64("pw_off_latency", 0660, droot,
		&vd->pw_off_latency);
	debugfs_create_u64("cmd_timeout", 0660, droot,
		&vd->cmd_timeout);

	VPU_DEBUGFS_CREATE(algo);
	VPU_DEBUGFS_CREATE(dump);
	VPU_DEBUGFS_CREATE(reg);
	VPU_DEBUGFS_CREATE(jtag);
	VPU_DEBUGFS_CREATE(state);
	VPU_DEBUGFS_CREATE(cmd);
#endif

out:
	return ret;
}

void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *vd)
{
	if (!vd)
		return;

	if (!IS_ERR_OR_NULL(vd->proc_root)) {
		remove_proc_subtree(vd->name, NULL);
		vd->proc_root = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	if (!IS_ERR_OR_NULL(vd->droot)) {
		debugfs_remove_recursive(vd->droot);
		vd->droot = NULL;
	}
#endif

	vpu_dmp_exit(vd);
}

int vpu_init_debug(void)
{
	int ret = 0;
	struct proc_dir_entry *proc_root;
	struct vpu_device *vd = NULL;
#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	struct dentry *droot;
#endif

	proc_root = proc_mkdir("vpu", NULL);
	if (IS_ERR_OR_NULL(proc_root)) {
		ret = PTR_ERR(proc_root);
		pr_info("%s: fail to create procfs node: %d\n",
			__func__, ret);
		goto out;
	}

	vpu_drv->proc_root = proc_root;
	VPU_PROCFS_CREATE_RW(vpu_memory);

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	droot = debugfs_create_dir("vpu", NULL);

	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: %d\n",
			__func__, ret);
		goto out;
	}

	vpu_drv->droot = droot;
	vpu_klog = VPU_DBG_DRV;
	debugfs_create_u32("klog", 0660, droot, &vpu_klog);

	VPU_DEBUGFS_CREATE(info);
#endif

out:
	return ret;
}

#undef VPU_DEBUGFS_CREATE
#undef VPU_PROCFS_CREATE
#undef VPU_PROCFS_CREATE_RW

void vpu_exit_debug(void)
{
	if (!vpu_drv)
		return;

	if (!IS_ERR_OR_NULL(vpu_drv->proc_root)) {
		remove_proc_subtree("vpu", NULL);
		vpu_drv->proc_root = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_APUSYS_VPU_DEBUG)
	if (!IS_ERR_OR_NULL(vpu_drv->droot)) {
		debugfs_remove_recursive(vpu_drv->droot);
		vpu_drv->droot = NULL;
	}
#endif
}
