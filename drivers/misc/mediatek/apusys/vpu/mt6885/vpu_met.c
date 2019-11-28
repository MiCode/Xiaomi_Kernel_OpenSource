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

#include "vpu_met.h"
#include "vpu_reg.h"
#include "vpu_debug.h"
#include "vpu_trace.h"

#define CREATE_TRACE_POINTS
#include "met_vpusys_events.h"

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
	// TODO: remove debug log
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

	memcpy_fromio(ptr, m, size);
	/* notify VPU buffer copy is finished */
	vpu_reg_write(vd, XTENSA_INFO18, 0x00000000);
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

#if 0 // TODO: add trace
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_begin("%s|vpu%d|@%p/%d",
		__func__, vd->id, ptr, buf_leng);
#endif
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
	// TODO: add trace
//	if (vpu_debug_on(VPU_DBG_PEF))
//		vpu_trace_end();
}

static void vpu_met_wq(struct work_struct *work)
{
	unsigned long flags;
	struct vpu_met_work *w =
		container_of(work,	struct vpu_met_work, work);
	struct vpu_device *vd = container_of(w, struct vpu_device, met);
	struct vpu_met_log *mlog, *tmp;

#if 0 // TODO: add trace
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_begin("VPU_LOG_ISR_BOTTOM_HALF");
#endif

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
		return;

	vpu_trace_dump("%s %d addr/size/buf: %08x/%08x/%p",
		__func__, __LINE__, mlog->buf_addr,
		mlog->buf_size, mlog->buf);

	vpu_met_log_show(vd, mlog->buf, mlog->buf_size);
	kfree(mlog);
	goto restart;

// TODO: add trace
//	if (vpu_debug_on(VPU_DBG_PEF))
//		vpu_trace_end();
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
	cancel_work(&vd->met.work);
}

static void vpu_met_log_dump(struct vpu_device *vd)
{
	char *ptr;
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

#if 0 // TODO: add trace
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_begin("VPULOG_ISR_TOPHALF|VPU%d", vd->id);
#endif

	/* fill vpu_log reader's information */
	mlog = (struct vpu_met_log *)ptr;
	mlog->buf_addr = log_buf_addr;
	mlog->buf_size = log_buf_size;
	mlog->buf = (void *)(ptr + sizeof(struct vpu_met_log));

	// TODO: remove debug log
	vpu_pef_debug("%s: vpu%d: addr/size/buf: %08x/%08x/%p\n",
		__func__, vd->id,
		log_buf_addr,
		log_buf_size,
		mlog->buf);

// TODO: add trace
#if 0
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_begin("VPULOG_CLONE_BUFFER|VPU%d|%08x/%u->%p",
			vd->id,
			log_buf_addr,
			log_buf_size,
			ptr);
#endif
	/* clone buffer in isr*/
	vpu_met_cpy(vd,
		apmcu_log_buf_ofst, log_buf_size, mlog->buf);
// TODO: add trace
#if 0
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_end();
	/* dump_buf(ptr, log_buf_size); */
#endif

	spin_lock(&vd->met.lock);
	list_add_tail(&(mlog->list), &vd->met.list);
	spin_unlock(&vd->met.lock);

	/* dump log to ftrace on BottomHalf */
	schedule_work(&vd->met.work);

// TODO: add trace
#if 0
	if (vpu_debug_on(VPU_DBG_PEF))
		vpu_trace_end();
#endif
}

void vpu_met_isr(struct vpu_device *vd)
{
	int dump = 0;

	/* INFO18 was used to dump MET Log */
	dump = vpu_reg_read(vd, XTENSA_INFO18);

	// TODO: remove debug log
	pr_info("%s: vpu%d: INFO18=0x%08x\n", __func__, vd->id, dump);
	vpu_trace_dump("VPU%d_ISR_RECV|INFO18=0x%08x", vd->id, dump);
	/* dispatch interrupt by INFO18 */
	switch (dump) {
	case 0:
		break;
	case VPU_REQ_DO_DUMP_LOG:
		if (!vpu_drv->met) // g_func_mask & VFM_ROUTINE_PRT_SYSLOG
			break;
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
