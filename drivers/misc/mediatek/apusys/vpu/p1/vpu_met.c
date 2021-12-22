// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/debugfs.h>

#include "vpu_met.h"
#include "vpu_reg.h"
#include "vpu_debug.h"
#include "vpu_trace.h"

#define CREATE_TRACE_POINTS
#include "met_vpusys_events.h"

#define vpu_trace_dump(fmt, args...) \
{ \
	if (vpu_drv->met & VPU_MET_LEGACY) \
		trace_printk("MET_DUMP|" fmt "\n", ##args); \
}

/* log format */
#define VPUBUF_MARKER		(0xBEEF)
#define VPULOG_START_MARKER	(0x55AA)
#define VPULOG_END_MARKER	(0xAA55)
#define MX_LEN_STR_DESC		(128)

#pragma pack(push)
#pragma pack(2)
struct vpu_met_header_t {
	unsigned short start_mark;
	unsigned char desc_len;
	unsigned char action_id;
	unsigned int sys_timer_h;
	unsigned int sys_timer_l;
	unsigned short sessid;
};
#pragma pack(pop)

struct vpu_met_log {
	struct list_head list;
	unsigned int buf_addr;
	unsigned int buf_size;
	void *buf;
};

static void vpu_met_packet(long long wclk, char action, int core,
	int sessid, char *str_desc, int val)
{
	vpu_pef_debug("%s: wclk: %lld, action: %c, core: %d, ssid: %d, val: %d, desc: %s\n",
		__func__, wclk, action, core, sessid, val, str_desc);
	trace___MET_PACKET__(wclk, action, core, sessid, str_desc, val);
}

static void __MET_PACKET__(int vpu_core, unsigned long long wclk,
	 unsigned char action_id, char *str_desc, unsigned int sessid)
{
	char action = 'Z';
	char null_str[] = "null";
	char *__str_desc = str_desc;
	int val = 0;

	switch (action_id) {
	/* For Sync Maker Begin/End */
	case 0x01:
		action = 'B';
		break;
	case 0x02:
		action = 'E';
		break;
	/* For Counter Maker */
	case 0x03:
		action = 'C';
		/* counter type: */
		/* bit 0~11: string desc */
		/* bit 12~15: count val */
		val = *(unsigned int *)(str_desc + 12);
		break;
	/* for Async Marker Start/Finish */
	case 0x04:
		action = 'S';
		break;
	case 0x05:
		action = 'F';
		break;
	}
	if (str_desc[0] == '\0') {
		/* null string handle */
		__str_desc =  null_str;
	}

	vpu_met_packet(wclk, action, vpu_core,
		sessid + 0x8000 * vpu_core, __str_desc, val);
}

static void dump_buf(void *ptr, int leng)
{
	int idx = 0;
	unsigned short *usaddr;

	for (idx = 0; idx < leng; idx += 16) {
		usaddr = (unsigned short *)(ptr + idx);
		vpu_trace_dump("%08x: %04x%04x %04x%04x %04x%04x %04x%04x,",
			idx,
			usaddr[0], usaddr[1], usaddr[2], usaddr[3],
			usaddr[4], usaddr[5], usaddr[6], usaddr[7]
		);
	}

}

static int vpu_met_cpy(struct vpu_device *vd, unsigned int addr,
	unsigned int size, void *ptr)
{
	void *m = (void *)(((unsigned long)vd->dmem.m) + addr);

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_begin("vpu_%d|%s|%08x/%u->%p", vd->id, __func__,
			addr, size, ptr);

	memcpy_fromio(ptr, m, size);
	/* notify VPU buffer copy is finished */
	vpu_reg_write(vd, XTENSA_INFO18, 0x00000000);

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_end("vpu_%d|%s|%08x/%u->%p", vd->id,
			__func__, addr, size, ptr);

	return 0;
}

static bool vpu_met_log_ready(void *buf, int buf_len)
{
	if (VPUBUF_MARKER != *(unsigned short *)buf) {
		/* front marker is invalid*/
		vpu_trace_dump("Error front maker: %04x",
		*(unsigned short *)buf);
		return false;
	}
	if (VPUBUF_MARKER != *(unsigned short *)(buf + buf_len - 2)) {
		vpu_trace_dump("Error end maker: %04x",
			*(unsigned short *)(buf + buf_len - 2));
		return false;
	}
	return true;
}

static bool vpu_met_log_valid(struct vpu_met_header_t *h)
{
	if (h->start_mark != VPULOG_START_MARKER) {
		vpu_trace_dump("Error h->start_mark: %02x", h->start_mark);
		return false;
	}
	if (h->desc_len > 0x10) {
		vpu_trace_dump("Error h->desc_len: %02x", h->desc_len);
		return false;
	}
	return true;
}

static void vpu_met_log_show(struct vpu_device *vd, void *ptr, int buf_leng)
{
	int idx = 0;
	void *start_ptr = ptr;
	int valid_len = 0;
	struct vpu_met_header_t *h;
	char trace_data[MX_LEN_STR_DESC];
	int header_size = sizeof(struct vpu_met_header_t);

	/* check buffer status */
	if (!vpu_met_log_ready(start_ptr, buf_leng)) {
		vpu_trace_dump("vpu_met_log_ready: false, %p %d",
			ptr, buf_leng);
		dump_buf(start_ptr, buf_leng);
		return;
	}

	/* get valid length*/
	valid_len = *(unsigned short *)(start_ptr + 2);
	if (valid_len >= buf_leng) {
		vpu_trace_dump("valid_len: %d large than buf_leng: %d",
			valid_len, buf_leng);
		return;
	}
	/*data offset start after Marker,Length*/
	idx += 4;

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_begin("vpu_%d|%s|@%p/%d",
			vd->id, __func__, ptr, buf_leng);

	while (1) {
		unsigned long long sys_t;
		int packet_size = 0;
		void *data_ptr;

		if (idx >= valid_len)
			break;

		h = (struct vpu_met_header_t *)(start_ptr + idx);
		data_ptr = (start_ptr + idx) + header_size;
		if (!vpu_met_log_valid(h)) {
			vpu_trace_dump("vpu_met_log_valid: false");
			dump_buf(start_ptr, buf_leng);
			break;
		}

		/*calculate packet size: header + data + end_mark*/
		packet_size = header_size + h->desc_len + 2;
		if (idx + packet_size > valid_len) {
			vpu_trace_dump(
				"error length (idx: %d, packet_size: %d)",
				idx, packet_size);
			vpu_trace_dump(
				"out of bound: valid_len: %d", valid_len);
			dump_buf(start_ptr, buf_leng);
			break;
		}

		if (h->desc_len > MX_LEN_STR_DESC) {
			vpu_trace_dump(
				"h->desc_len(%d) >	MX_LEN_STR_DESC(%d)",
				h->desc_len, MX_LEN_STR_DESC);
			dump_buf(start_ptr, buf_leng);
			break;
		}
		memset(trace_data, 0x00, MX_LEN_STR_DESC);
		if (h->desc_len > 0) {
			/*copy data buffer*/
			memcpy(trace_data, data_ptr, h->desc_len);
		}
		sys_t = h->sys_timer_h;
		sys_t = (sys_t << 32) + (h->sys_timer_l & 0xFFFFFFFF);

		__MET_PACKET__(
			vd->id,
			sys_t,
			h->action_id,
			trace_data,
			h->sessid
		);
		idx += packet_size;
	}

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_end("vpu_%d|%s|@%p/%d",
			vd->id, __func__, ptr, buf_leng);
}

static void vpu_met_wq(struct work_struct *work)
{
	unsigned long flags;
	struct vpu_met_work *w =
		container_of(work,	struct vpu_met_work, work);
	struct vpu_device *vd = container_of(w, struct vpu_device, met);
	struct vpu_met_log *mlog, *tmp;

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_begin("vpu_%d|%s", vd->id, __func__);

restart:
	spin_lock_irqsave(&w->lock, flags);
	if (list_empty(&w->list)) {
		mlog = NULL;
	} else {
		list_for_each_entry_safe(mlog, tmp, &w->list, list) {
			list_del(&mlog->list);
			break;
		}
	}
	spin_unlock_irqrestore(&w->lock, flags);

	if (!mlog)
		goto out;

	vpu_trace_dump("%s %d addr/size/buf: %08x/%08x/%p",
		__func__, __LINE__, mlog->buf_addr,
		mlog->buf_size, mlog->buf);

	vpu_met_log_show(vd, mlog->buf, mlog->buf_size);
	kfree(mlog);
	goto restart;
out:
	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_end("vpu_%d|%s", vd->id, __func__);
}

static void vpu_met_log_dump(struct vpu_device *vd)
{
	char *ptr;
	unsigned long flags;
	unsigned int apmcu_log_buf_ofst;
	unsigned int log_buf_addr = 0x0;
	unsigned int log_buf_size = 0x0;
	struct vpu_met_log *mlog;

	/* handle output log */
	log_buf_addr = vpu_reg_read(vd, XTENSA_INFO05);
	log_buf_size = vpu_reg_read(vd, XTENSA_INFO06);

	/* translate vpu address to apmcu */
	apmcu_log_buf_ofst = log_buf_addr & 0x000fffff;

	/* in ISR we need use ATOMIC flag to alloc memory */
	ptr = kmalloc(sizeof(struct vpu_met_log) + log_buf_size,
		GFP_ATOMIC);

	if (!ptr) {
		pr_info("%s: met log alloc fail: %zu, %d\n",
			__func__, sizeof(struct vpu_met_log),
			log_buf_size);
		return;
	}

	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_begin("vpu_%d|%s", vd->id, __func__);
	/* fill vpu_log reader's information */
	mlog = (struct vpu_met_log *)ptr;
	mlog->buf_addr = log_buf_addr;
	mlog->buf_size = log_buf_size;
	mlog->buf = (void *)(ptr + sizeof(struct vpu_met_log));

	vpu_met_debug("%s: vpu%d: addr/size/buf: %08x/%08x/%p\n",
		__func__, vd->id,
		log_buf_addr,
		log_buf_size,
		mlog->buf);

	/* clone buffer in isr*/
	vpu_met_cpy(vd,
		apmcu_log_buf_ofst, log_buf_size, mlog->buf);

	spin_lock_irqsave(&vd->met.lock, flags);
	list_add_tail(&(mlog->list), &vd->met.list);
	spin_unlock_irqrestore(&vd->met.lock, flags);

	/* dump log to ftrace on BottomHalf */
	schedule_work(&vd->met.work);
	if (vpu_debug_on(VPU_DBG_MET))
		vpu_trace_end("vpu_%d|%s", vd->id, __func__);
}

void vpu_met_isr(struct vpu_device *vd)
{
	int dump = 0;

	if (!vpu_drv->met)
		return;

	/* INFO18 was used to dump MET Log */
	dump = vpu_reg_read(vd, XTENSA_INFO18);

	/* dispatch interrupt by INFO18 */
	switch (dump) {
	case 0:
		break;
	case VPU_REQ_DO_DUMP_LOG:
		vpu_met_log_dump(vd);
		break;
	case VPU_REQ_DO_CLOSED_FILE:
		break;
	default:
		pr_info("%s: vpu%d: unsupported cmd: %d\n",
			__func__, vd->id, dump);
		break;
	}
}

#define PM_CTRL_SEL(sel, mask) \
	((PERF_PMCTRL_TRACELEVEL) | \
	((sel) << PERF_PMCTRL_SELECT_SHIFT) | \
	((mask) << PERF_PMCTRL_MASK_SHIFT))

static uint32_t pm_sel[VPU_MET_PM_MAX] = {
	PM_CTRL_SEL(XTPERF_CNT_INSN, XTPERF_MASK_INSN_ALL),
	PM_CTRL_SEL(XTPERF_CNT_IDMA, XTPERF_MASK_IDMA_ACTIVE_CYCLES),
	PM_CTRL_SEL(XTPERF_CNT_D_STALL, XTPERF_MASK_D_STALL_UNCACHED_LOAD),
	PM_CTRL_SEL(XTPERF_CNT_I_STALL, XTPERF_MASK_I_STALL_CACHE_MISS),
	0,
	0,
	0,
	0
};

#define PMG_EN      0x1000
#define PM_COUNTER  0x1080
#define PM_CTRL     0x1100
#define PM_STAT     0x1180

static inline
unsigned long vpu_dbg_base(struct vpu_device *vd)
{
	return (unsigned long)vd->dbg.m;
}

static inline
uint32_t vpu_dbg_read(struct vpu_device *vd, int offset)
{
	return ioread32((void *) (vpu_dbg_base(vd) + offset));
}

static inline
void vpu_dbg_write(struct vpu_device *vd, int offset, uint32_t val)
{
	mt_reg_sync_writel(val, (void *) (vpu_dbg_base(vd) + offset));
}

static inline
void vpu_dbg_clr(struct vpu_device *vd, int offset, uint32_t mask)
{
	vpu_reg_write(vd, offset, vpu_dbg_read(vd, offset) & ~mask);
}

static inline
void vpu_dbg_set(struct vpu_device *vd, int offset, uint32_t mask)
{
	vpu_dbg_write(vd, offset, vpu_dbg_read(vd, offset) | mask);
}

#define VPU_MET_PM_LATENCY_NS  (1000000)
#define VPU_MET_PM_LATENCY_MIN (50000)

static int vpu_met_pm_hrt_start(void)
{
	if (vpu_drv->met_hrt.latency < VPU_MET_PM_LATENCY_MIN)
		return 0;

	hrtimer_start(&vpu_drv->met_hrt.t,
		ns_to_ktime(vpu_drv->met_hrt.latency),
		HRTIMER_MODE_REL);
	vpu_met_debug("%s:\n", __func__);
	return 0;
}

static int vpu_met_pm_hrt_stop(int sync)
{
	int ret = 0;

	if (sync)
		hrtimer_cancel(&vpu_drv->met_hrt.t);
	else
		ret = hrtimer_try_to_cancel(&vpu_drv->met_hrt.t);

	vpu_met_debug("%s: sync: %d, ret: %d\n", __func__, sync, ret);
	return ret;
}

void vpu_met_pm_get(struct vpu_device *vd)
{
	int i;
	unsigned long flags;
	uint32_t offset;

	if (!vpu_drv->met)
		return;

	/* read register and send to met */
	spin_lock_irqsave(&vpu_drv->met_hrt.lock, flags);
	for (i = 0; i < VPU_MET_PM_MAX; i++) {
		if (!pm_sel[i])
			continue;
		offset = i * 4;
		vpu_dbg_write(vd, PM_CTRL + offset, pm_sel[i]);
		vpu_dbg_write(vd, PM_COUNTER + offset, 0);
		vpu_dbg_write(vd, PM_STAT + offset, 0);
		vd->pm.val[i] = 0;
	}

	vpu_dbg_set(vd, PMG_EN, 0x1);

	if (kref_get_unless_zero(&vpu_drv->met_hrt.ref)) {
		vpu_met_debug("%s: vpu%d: ref: %d\n",
			__func__, vd->id, kref_read(&vpu_drv->met_hrt.ref));
		goto out;
	}

	kref_init(&vpu_drv->met_hrt.ref);
	vpu_met_debug("%s: vpu%d: ref: 1\n", __func__, vd->id);
	vpu_met_pm_hrt_start();
out:
	spin_unlock_irqrestore(&vpu_drv->met_hrt.lock, flags);

}


static void vpu_met_pm_release(struct kref *ref)
{
	vpu_met_pm_hrt_stop(1 /* sync */);
}

void vpu_met_pm_put(struct vpu_device *vd)
{
	if (!vpu_drv->met)
		return;

	if (!kref_read(&vpu_drv->met_hrt.ref)) {
		vpu_met_debug("%s: vpu%d: ref is already zero\n",
			__func__, vd->id);
		return;
	}
	vpu_met_debug("%s: vpu%d: ref: %d--\n",
		__func__, vd->id, kref_read(&vpu_drv->met_hrt.ref));
	kref_put(&vpu_drv->met_hrt.ref, vpu_met_pm_release);
}

static void vpu_met_pm_dbg_read(struct vpu_device *vd)
{
	int i;
	uint32_t offset;
	uint32_t tmp[VPU_MET_PM_MAX];
	uint32_t df[VPU_MET_PM_MAX];
	bool dump = false;

	for (i = 0; i < VPU_MET_PM_MAX; i++) {
		df[i] = 0;
		if (!pm_sel[i])
			continue;
		offset = i * 4;
		tmp[i] = vpu_dbg_read(vd, PM_COUNTER + offset);
		if (tmp[i] != vd->pm.val[i]) {
			dump = true;
			df[i] = tmp[i] - vd->pm.val[i];
			vd->pm.val[i] = tmp[i];
		}
	}

	if (vpu_drv->met & VPU_MET_LEGACY)
		trace_VPU__polling(vd->id, df[0], df[1], df[2], df[3]);

	if (dump && (vpu_drv->met & VPU_MET_COMPACT))
		trace_VPU__pm(vd->id, vd->pm.val);

}

static enum hrtimer_restart vpu_met_pm_hrt_func(struct hrtimer *timer)
{
	struct vpu_device *vd;
	unsigned long flags;
	struct list_head *ptr, *tmp;

	/* for all vpu cores, dump their registers */
	spin_lock_irqsave(&vpu_drv->met_hrt.lock, flags);
	list_for_each_safe(ptr, tmp, &vpu_drv->devs) {
		vd = list_entry(ptr, struct vpu_device, list);
		if (vd->state > VS_BOOT && vd->state < VS_REMOVING)
			vpu_met_pm_dbg_read(vd);
	}
	hrtimer_forward_now(&vpu_drv->met_hrt.t,
		ns_to_ktime(VPU_MET_PM_LATENCY_NS));
	spin_unlock_irqrestore(&vpu_drv->met_hrt.lock, flags);

	return HRTIMER_RESTART;
}

int vpu_init_drv_met(void)
{
	struct dentry *droot = vpu_drv->droot;
	struct dentry *dpm;
	int i;

	spin_lock_init(&vpu_drv->met_hrt.lock);
	hrtimer_init(&vpu_drv->met_hrt.t,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vpu_drv->met_hrt.t.function = vpu_met_pm_hrt_func;
	refcount_set(&vpu_drv->met_hrt.ref.refcount, 0);

	vpu_drv->ilog = 0;
	vpu_drv->met = VPU_MET_DISABLED;
	vpu_drv->met_hrt.latency = VPU_MET_PM_LATENCY_NS;

	if (!droot)
		goto out;

	debugfs_create_u32("ilog", 0660, droot, &vpu_drv->ilog);
	debugfs_create_u32("met", 0660, droot, &vpu_drv->met);
	dpm = debugfs_create_dir("met_pm", droot);
	if (IS_ERR_OR_NULL(dpm))
		goto out;

	for (i = 0; i < VPU_MET_PM_MAX; i++) {
		char name[32];

		if (snprintf(name, sizeof(name), "ctrl%d", i) < 0) {
			name[0] = '\0';
			vpu_met_debug("%s: snprintf fail\n", __func__);
		}
		debugfs_create_u32(name, 0660, dpm, &pm_sel[i]);
	}

	debugfs_create_u64("met_pm_latency", 0660, droot,
		&vpu_drv->met_hrt.latency);
out:
	return 0;
}

int vpu_exit_drv_met(void)
{
	vpu_met_pm_hrt_stop(1 /* sync */);
	return 0;
}

int vpu_init_dev_met(struct platform_device *pdev,
	struct vpu_device *vd)
{
	spin_lock_init(&vd->met.lock);
	INIT_LIST_HEAD(&vd->met.list);
	INIT_WORK(&vd->met.work, vpu_met_wq);
	return 0;
}

void vpu_exit_dev_met(struct platform_device *pdev,
	struct vpu_device *vd)
{
	cancel_work_sync(&vd->met.work);
}

