/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Chiachun.wang <chiachun.wang@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include "xhci.h"
#include "xhci-ssusb-mtk.h"



#define SCH_SUCCESS	1
#define SCH_FAIL	0

#define MAX_PORT_NUM 4
#define SS_BW_BOUND	51000
#define HS_BW_BOUND	6144

/* mtk scheduler bitmasks */
#define EP_BPKTS(p)	((p) & 0x3f)
#define EP_BCSCOUNT(p)	(((p) & 0x7) << 8)
#define EP_BBM(p)		((p) << 11)
#define EP_BOFFSET(p)	((p) & 0x3fff)
#define EP_BREPEAT(p)	(((p) & 0x7fff) << 16)

static int add_sch_ep(struct sch_ep *sep, struct sch_port *sport)
{
	struct sch_ep **ep_array;
	int speed = sep->speed;
	int ep_type = sep->ep_type;
	int i;

	if (sep->is_in && speed == USB_SPEED_SUPER)
		ep_array = sport->ss_in_eps;
	else if (speed == USB_SPEED_SUPER)
		ep_array = sport->ss_out_eps;
	else if (speed == USB_SPEED_HIGH ||
		(sep->is_tt && ep_type == USB_ENDPOINT_XFER_ISOC))
		ep_array = sport->hs_eps;
	else
		ep_array = sport->tt_intr_eps;

	for (i = 0; i < MAX_EP_NUM; i++) {
		if (ep_array[i] == NULL) {
			ep_array[i] = sep;
			return SCH_SUCCESS;
		}
	}
	return SCH_FAIL;
}


static int need_more_bw_cost(int old_interval, int old_offset,
			int new_interval, int new_offset)
{
	int tmp_offset;
	int tmp_interval;
	int ret = 0;

	if (old_interval >= new_interval) {
		tmp_offset = old_offset + old_interval - new_offset;
		tmp_interval = new_interval;
	} else {
		tmp_offset = new_offset + new_interval - old_offset;
		tmp_interval = old_interval;
	}
	if (tmp_offset % tmp_interval == 0)
		ret = 1;

	return ret;
}

static int get_bw_cost(struct sch_ep *sep, int interval, int offset)
{
	int ep_offset;
	int ep_interval;
	int ep_repeat;
	int ep_mult;
	int m_offset;
	int k;
	int bw_cost = 0;

	ep_interval = sep->interval;
	ep_offset = sep->offset;
	if (sep->repeat == 0) {
		if (need_more_bw_cost(ep_interval, ep_offset, interval, offset))
			bw_cost = sep->bw_cost;
	} else {
		ep_repeat = sep->repeat;
		ep_mult = sep->mult;
		for (k = 0; k <= ep_mult; k++) {
			m_offset = ep_offset + (k * ep_repeat);
			if (need_more_bw_cost(ep_interval, m_offset,
						interval, offset)) {
				bw_cost = sep->bw_cost;
				break;
			}
		}
	}
	return bw_cost;
}


static int count_ss_bw_single(struct sch_ep **ep_array, int interval,
			int offset, int td_size)
{
	struct sch_ep *cur_sep;
	int bw_required;
	int i;

	bw_required = 0;
	for (i = 0; i < MAX_EP_NUM; i++) {
		cur_sep = ep_array[i];
		if (cur_sep == NULL)
			continue;
		bw_required += get_bw_cost(cur_sep, interval, offset);
	}
	bw_required += td_size;
	return bw_required;
}

static int count_ss_bw_repeat(struct sch_ep **ep_array, int maxp,
		int interval, int burst, int mult, int offset, int repeat)
{
	int bw_required_per_repeat;
	int final_bw_required;
	int tmp_bw_required;
	int bw_required[3] = {0, 0, 0};
	struct sch_ep *cur_sep;
	int cur_offset;
	int i, j;

	bw_required_per_repeat = maxp * (burst + 1);
	for (j = 0; j <= mult; j++) {
		tmp_bw_required = 0;
		cur_offset = offset + (j * repeat);
		for (i = 0; i < MAX_EP_NUM; i++) {
			cur_sep = ep_array[i];
			if (cur_sep == NULL)
				continue;
			tmp_bw_required +=
				get_bw_cost(cur_sep, interval, cur_offset);
		}
		bw_required[j] = tmp_bw_required;
	}
	final_bw_required = SS_BW_BOUND;
	for (j = 0; j <= mult; j++) {
		if (bw_required[j] < final_bw_required)
			final_bw_required = bw_required[j];
	}
	final_bw_required += bw_required_per_repeat;

	return final_bw_required;
}


static int count_ss_bw(struct sch_port *sport, struct sch_ep *sep,
			int offset, int repeat, int td_size)
{
	struct sch_ep **ep_array;
	int interval = sep->interval;
	int bw_cost;

	ep_array = sep->is_in ? sport->ss_in_eps : sport->ss_out_eps;

	if (repeat) {
		bw_cost = count_ss_bw_repeat(ep_array, sep->maxp, interval,
					sep->burst, sep->mult, offset, repeat);
	} else
		bw_cost = count_ss_bw_single(ep_array, interval,
					offset, td_size);

	return bw_cost;
}

static int is_isoc_tt_mframe_overlap(struct sch_ep *sep,
			int interval, int offset)
{
	int ep_offset = sep->offset;
	int ep_interval = sep->interval << 3;
	int tmp_offset;
	int tmp_interval;
	int is_overlap = 0;

	if (ep_interval >= interval) {
		tmp_offset = ep_offset + ep_interval - offset;
		tmp_interval = interval;
	} else {
		tmp_offset = offset + interval - ep_offset;
		tmp_interval = ep_interval;
	}
	if (sep->is_in) {
		if ((tmp_offset % tmp_interval >= 2)
			&& (tmp_offset % tmp_interval <= sep->cs_count)) {
			is_overlap = 1;
		}
	} else {
		if (tmp_offset % tmp_interval <= sep->cs_count)
			is_overlap = 1;
	}
	return is_overlap;
}


static int count_hs_bw(struct sch_port *sport, int ep_type, int maxp,
		int interval, int offset, int td_size)
{
	int i;
	int bw_required;
	struct sch_ep *cur_sep;
	int ep_offset;
	int ep_interval;

	bw_required = 0;
	for (i = 0; i < MAX_EP_NUM; i++) {
		cur_sep = sport->hs_eps[i];
		if (cur_sep == NULL)
			continue;

		ep_offset = cur_sep->offset;
		ep_interval = cur_sep->interval;

		if (cur_sep->is_tt &&
			(cur_sep->ep_type == USB_ENDPOINT_XFER_ISOC)) {
			if (is_isoc_tt_mframe_overlap(cur_sep,
					interval, offset))
				bw_required += 188;
		} else {
			if (need_more_bw_cost(ep_interval, ep_offset,
				interval, offset))
				bw_required += cur_sep->bw_cost;
		}
	}
	bw_required += td_size;
	return bw_required;
}

static int count_tt_isoc_bw(int is_in, int maxp, int interval,
	int offset, int td_size, struct sch_port *sport)
{
	char is_cs;
	int s_frame, s_mframe, cur_mframe;
	int bw_required, max_bw;
	int ss_cs_count;
	int cs_mframe;
	int i, j;
	struct sch_ep *cur_sep;
	int ep_offset;
	int ep_interval;
	int tt_isoc_interval;

	tt_isoc_interval = interval << 3;	/* frame to mframe */
	is_cs = is_in ? 1 : 0;

	s_frame = offset / 8;
	s_mframe = offset % 8;
	ss_cs_count = (maxp + (188 - 1)) / 188;
	if (is_cs) {
		cs_mframe = offset % 8 + 2 + ss_cs_count;
		if (cs_mframe <= 6)
			ss_cs_count += 2;
		else if (cs_mframe == 7)
			ss_cs_count++;
		else if (cs_mframe > 8)
			return -1;
	}
	max_bw = 0;

	i = is_in ? 2 : 0;
	for (cur_mframe = offset + i; i < ss_cs_count; cur_mframe++, i++) {
		bw_required = 0;
		for (j = 0; j < MAX_EP_NUM; j++) {
			cur_sep = sport->hs_eps[j];
			if (cur_sep == NULL)
				continue;

			ep_offset = cur_sep->offset;
			ep_interval = cur_sep->interval;
			if (cur_sep->is_tt &&
				(cur_sep->ep_type == USB_ENDPOINT_XFER_ISOC)) {
				/*
				 * isoc tt
				 * check if mframe offset overlap
				 * if overlap, add 188 to the bw
				 */
				if (is_isoc_tt_mframe_overlap(cur_sep,
					tt_isoc_interval, cur_mframe))
					bw_required += 188;
			} else if (cur_sep->ep_type == USB_ENDPOINT_XFER_INT
				|| cur_sep->ep_type == USB_ENDPOINT_XFER_ISOC) {
				/* check if mframe */
				if (need_more_bw_cost(ep_interval, ep_offset,
						tt_isoc_interval, cur_mframe))
					bw_required += cur_sep->bw_cost;
			}
		}
		bw_required += 188;
		if (bw_required > max_bw)
			max_bw = bw_required;
	}
	return max_bw;
}

static int count_tt_intr_bw(int interval, int offset,
		struct sch_port *u3h_sch_port)
{
	struct sch_ep *cur_sep;
	int ep_offset;
	int ep_interval;
	int i;

	/* check all eps in tt_intr_eps */
	for (i = 0; i < MAX_EP_NUM; i++) {
		cur_sep = u3h_sch_port->tt_intr_eps[i];
		if (cur_sep == NULL)
			continue;
		ep_offset = cur_sep->offset;
		ep_interval = cur_sep->interval;
		if (need_more_bw_cost(ep_interval, ep_offset, interval, offset))
			return SCH_FAIL;
	}
	return SCH_SUCCESS;
}

static int check_tt_intr_bw(struct sch_ep *sep, struct sch_port *sport)
{
	int frame_idx;
	int frame_interval;
	int interval = sep->interval;

	frame_interval = interval >> 3;
	for (frame_idx = 0; frame_idx < frame_interval; frame_idx++) {
		if (count_tt_intr_bw(frame_interval, frame_idx, sport)
				== SCH_SUCCESS) {
			sep->offset = frame_idx << 3;
			sep->pkts = 1;
			sep->cs_count = 3;
			sep->bw_cost = sep->maxp;
			sep->repeat = 0;
			return SCH_SUCCESS;
		}
	}
	return SCH_FAIL;
}

static int check_tt_iso_bw(struct sch_ep *sep, struct sch_port *sport)
{
	int cs_count = 0;
	int td_size;
	int mframe_idx, frame_idx;
	int cur_bw, best_bw, best_bw_idx;
	int cur_offset, cs_mframe;
	int interval;

	best_bw = HS_BW_BOUND;
	best_bw_idx = -1;
	cur_bw = 0;
	td_size = sep->maxp;
	interval = sep->interval >> 3;
	for (frame_idx = 0; frame_idx < interval; frame_idx++) {
		for (mframe_idx = 0; mframe_idx < 8; mframe_idx++) {
			cur_offset = (frame_idx * 8) + mframe_idx;
			cur_bw = count_tt_isoc_bw(sep->is_in, sep->maxp,
				interval, cur_offset, td_size, sport);
			if (cur_bw >= 0 && cur_bw < best_bw) {
				best_bw_idx = cur_offset;
				best_bw = cur_bw;
				if (cur_bw == td_size ||
					cur_bw < (HS_BW_BOUND >> 1))
					goto found_best_offset;
			}
		}
	}
	if (best_bw_idx == -1)
		return SCH_FAIL;

found_best_offset:
	sep->offset = best_bw_idx;
	sep->pkts = 1;
	cs_count = (sep->maxp + (188 - 1)) / 188;
	if (sep->is_in) {
		cs_mframe = (sep->offset >> 3) + 2 + cs_count;
		if (cs_mframe <= 6)
			cs_count += 2;
		else if (cs_mframe == 7)
			cs_count++;
	}
	sep->cs_count = cs_count;
	sep->bw_cost = 188;
	sep->repeat = 0;

	return SCH_SUCCESS;
}

static int check_hs_bw(struct sch_ep *sep, struct sch_port *sport)
{
	int td_size;
	int cur_bw, best_bw, best_bw_idx;
	int cur_offset;
	int interval = sep->interval;

	best_bw = HS_BW_BOUND;
	best_bw_idx = -1;
	cur_bw = 0;
	td_size = sep->maxp * (sep->burst + 1);
	for (cur_offset = 0; cur_offset < interval; cur_offset++) {
		cur_bw = count_hs_bw(sport, sep->ep_type, sep->maxp, interval,
							cur_offset, td_size);
		if (cur_bw >= 0 && cur_bw < best_bw) {
			best_bw_idx = cur_offset;
			best_bw = cur_bw;
			if (cur_bw == td_size || cur_bw < (HS_BW_BOUND >> 1))
				break;
		}
	}
	if (best_bw_idx == -1)
		return SCH_FAIL;

	sep->offset = best_bw_idx;
	sep->pkts = sep->burst + 1;
	sep->cs_count = 0;
	sep->bw_cost = td_size;
	sep->repeat = 0;

	return SCH_SUCCESS;
}

static int check_ss_bw(struct sch_ep *sep, struct sch_port *sport)
{
	int cur_bw, best_bw, best_bw_idx;
	int repeat, max_repeat, best_bw_repeat;
	int maxp = sep->maxp;
	int interval = sep->interval;
	int burst = sep->burst;
	int mult = sep->mult;
	int frame_idx;
	int td_size;

	best_bw = SS_BW_BOUND;
	best_bw_idx = -1;
	cur_bw = 0;
	td_size = maxp * (mult + 1) * (burst + 1);
	if (mult == 0)
		max_repeat = 0;
	else
		max_repeat = (interval - 1) / (mult + 1);

	best_bw_repeat = 0;
	for (frame_idx = 0; frame_idx < interval; frame_idx++) {
		for (repeat = max_repeat; repeat >= 0; repeat--) {
			cur_bw = count_ss_bw(sport, sep,
						frame_idx, repeat, td_size);
			if (cur_bw >= 0 && cur_bw < best_bw) {
				best_bw_idx = frame_idx;
				best_bw_repeat = repeat;
				best_bw = cur_bw;
				if (cur_bw <= td_size ||
					cur_bw < (SS_BW_BOUND >> 1))
					goto found_best_offset;
			}
		}
	}
	if (best_bw_idx == -1)
		return SCH_FAIL;

found_best_offset:
	sep->offset = best_bw_idx;
	sep->cs_count = 0;
	sep->repeat = best_bw_repeat;
	if (sep->repeat == 0) {
		sep->bw_cost = (burst + 1) * (mult + 1) * maxp;
		sep->pkts = (burst + 1) * (mult + 1);
	} else {
		sep->bw_cost = (burst + 1) * maxp;
		sep->pkts = (burst + 1);
	}

	return SCH_SUCCESS;

}

static struct sch_port *xhci_to_sch_port(struct xhci_hcd *xhci, int rh_port)
{
	struct sch_port *port_array = (struct sch_port *)xhci->sch_ports;

	if (rh_port < 1 || rh_port > MAX_PORT_NUM)
		return NULL;

	return port_array + (rh_port - 1);
}

static struct sch_ep *scheduler_remove_ep(struct xhci_hcd *xhci,
	int rh_port, int speed, int is_tt, struct usb_host_endpoint *ep)
{
	struct sch_ep **ep_array;
	struct sch_ep *cur_ep;
	struct sch_port *sport;
	int is_in;
	int i;

	sport = xhci_to_sch_port(xhci, rh_port);
	if (sport == NULL) {
		xhci_dbg(xhci, "can't get sch_port for roothub-port%d\n", rh_port);
		return NULL;
	}
	is_in = usb_endpoint_dir_in(&ep->desc);
	if (is_in && speed == USB_SPEED_SUPER)
		ep_array = sport->ss_in_eps;
	else if (speed == USB_SPEED_SUPER)
		ep_array = sport->ss_out_eps;
	else if (speed == USB_SPEED_HIGH ||
		(is_tt && usb_endpoint_xfer_isoc(&ep->desc)))
		ep_array = sport->hs_eps;
	else
		ep_array = sport->tt_intr_eps;

	for (i = 0; i < MAX_EP_NUM; i++) {
		cur_ep = ep_array[i];
		if (cur_ep != NULL && cur_ep->ep == ep) {
			ep_array[i] = NULL;
			xhci_err(xhci, "rm_ep -- ep:0x%p\n", ep);
			return cur_ep;
		}
	}
	return NULL;
}

static int scheduler_add_ep(struct xhci_hcd *xhci, struct sch_ep *sep)
{
	struct sch_port *sport;
	struct xhci_ep_ctx *ep_ctx;
	int speed, is_tt, ep_type;
	int ret = SCH_SUCCESS;

	speed = sep->speed;
	is_tt = sep->is_tt;
	ep_type = sep->ep_type;
	ep_ctx = sep->ep_ctx;
	sport = xhci_to_sch_port(xhci, sep->rh_port);
	if (sport == NULL) {
		xhci_dbg(xhci, "can't get sch_port for roothub-port%d\n", sep->rh_port);
		return SCH_FAIL;
	}

	xhci_err(xhci, "add_ep -- rh_port:%d, speed:%d, in:%d tt:%d ep_type:%d\n",
			sep->rh_port, speed, sep->is_in, is_tt, ep_type);
	xhci_err(xhci, "\t maxp:%d, interval:%d, burst:%d, mult:%d, ep:0x%p\n",
			sep->maxp, sep->interval, sep->burst, sep->mult,
			sep->ep);
	/* only process special cases */
	if (is_tt && ep_type == USB_ENDPOINT_XFER_INT &&
		((speed == USB_SPEED_LOW) || (speed == USB_SPEED_FULL)))
		ret = check_tt_intr_bw(sep, sport);
	else if (is_tt && ep_type == USB_ENDPOINT_XFER_ISOC)
		ret = check_tt_iso_bw(sep, sport);
	else if (speed == USB_SPEED_HIGH &&
			(ep_type == USB_ENDPOINT_XFER_INT ||
			ep_type == USB_ENDPOINT_XFER_ISOC))
		ret = check_hs_bw(sep, sport);
	else if (speed == USB_SPEED_SUPER &&
			(ep_type == USB_ENDPOINT_XFER_INT ||
			ep_type == USB_ENDPOINT_XFER_ISOC))
		ret = check_ss_bw(sep, sport);
	else
		sep->pkts = 1;

	if (ret == SCH_FAIL)
		return ret;

	/* all transfers are fixed as burst mode-1 */
	sep->burst_mode = 1;

	if (add_sch_ep(sep, sport) == SCH_FAIL) {
		xhci_err(xhci, "%s: no space to save sch_ep\n", __func__);
		return SCH_FAIL;
	}

	ep_ctx->reserved[0] |= (EP_BPKTS(sep->pkts) |
						EP_BCSCOUNT(sep->cs_count) |
						EP_BBM(sep->burst_mode));
	ep_ctx->reserved[1] |= (EP_BOFFSET(sep->offset) |
						EP_BREPEAT(sep->repeat));
	xhci_dbg(xhci, "\tBPKTS:%x, BCSCOUNT:%x, BBM:%x, BOFFSET:%x, BREPEAT:%x\n",
			sep->pkts, sep->cs_count, sep->burst_mode, sep->offset,
			sep->repeat);

	return SCH_SUCCESS;
}


int xhci_mtk_drop_ep_quirk(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	struct xhci_hcd *xhci;
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_virt_device	*vdev;
	struct sch_ep *sch_ep = NULL;
	int is_tt;
	int rh_port;

	xhci = hcd_to_xhci(hcd);
	vdev = xhci->devs[udev->slot_id];
	slot_ctx = xhci_get_slot_ctx(xhci, vdev->out_ctx);
	is_tt = !!(slot_ctx->tt_info & TT_SLOT);
	rh_port = DEVINFO_TO_ROOT_HUB_PORT(slot_ctx->dev_info2);

	sch_ep = scheduler_remove_ep(xhci, rh_port, udev->speed, is_tt, ep);
	if (sch_ep != NULL) {
		kfree(sch_ep);
		xhci_dbg(xhci, "remove ep:0x%p\n", ep);
	} else
		xhci_warn(xhci, "don't find sch_ep when drop ep(0x%p)\n", ep);

	return 0;
}


int xhci_mtk_add_ep_quirk(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_container_ctx *in_ctx;
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_ep_ctx *in_ep_ctx;
	struct sch_ep *sch_ep;
	unsigned int ep_index;

	xhci = hcd_to_xhci(hcd);
	/* sch_ep struct should init as zero */
	sch_ep = kzalloc(sizeof(struct sch_ep), GFP_KERNEL);
	if (sch_ep == NULL)
		return -ENOMEM;

	virt_dev = xhci->devs[udev->slot_id];
	in_ctx = virt_dev->in_ctx;
	ep_index = xhci_get_endpoint_index(&ep->desc);
	in_ep_ctx = xhci_get_ep_ctx(xhci, in_ctx, ep_index);
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	sch_ep->is_tt = !!(slot_ctx->tt_info & TT_SLOT);

	if (usb_endpoint_xfer_int(&ep->desc))
		sch_ep->ep_type = USB_ENDPOINT_XFER_INT;
	else if (usb_endpoint_xfer_isoc(&ep->desc))
		sch_ep->ep_type = USB_ENDPOINT_XFER_ISOC;
	else if (usb_endpoint_xfer_bulk(&ep->desc))
		sch_ep->ep_type = USB_ENDPOINT_XFER_BULK;

	if (udev->speed == USB_SPEED_FULL || udev->speed == USB_SPEED_HIGH
		|| udev->speed == USB_SPEED_LOW) {
		sch_ep->burst = (usb_endpoint_maxp(&ep->desc) & 0x1800) >> 11;
		sch_ep->mult = 0;
	} else if (udev->speed == USB_SPEED_SUPER) {
		sch_ep->burst = ep->ss_ep_comp.bMaxBurst;
		sch_ep->mult = ep->ss_ep_comp.bmAttributes & 0x3;
	}
	sch_ep->maxp = GET_MAX_PACKET(usb_endpoint_maxp(&ep->desc));
	sch_ep->speed = udev->speed;
	sch_ep->interval = EP_INTERVAL_TO_UFRAMES(in_ep_ctx->ep_info);
	sch_ep->is_in = usb_endpoint_dir_in(&ep->desc);
	sch_ep->ep = ep;
	sch_ep->rh_port = DEVINFO_TO_ROOT_HUB_PORT(slot_ctx->dev_info2);
	sch_ep->ep_ctx = in_ep_ctx;

	if (scheduler_add_ep(xhci, sch_ep) != SCH_SUCCESS) {
		kfree(sch_ep);
		xhci_err(xhci, "there is not enough bandwidth for mtk xhci\n");
		return -ENOSPC;
	}
	return 0;
}

/* @dev : struct device pointer of xhci platform_device */
int xhci_mtk_init_quirk(struct xhci_hcd *xhci)
{
	xhci->sch_ports = kzalloc(sizeof(struct sch_port) * MAX_PORT_NUM, GFP_KERNEL);
	if (xhci->sch_ports == NULL)
		return -ENOMEM;

	xhci->quirks |= XHCI_MTK_HOST;

	return 0;
}

void xhci_mtk_exit_quirk(struct xhci_hcd *xhci)
{
	kfree(xhci->sch_ports);
}

/*
 * The TD size is the number of bytes remaining in the TD (including this TRB),
 * right shifted by 10.
 * It must fit in bits 21:17, so it can't be bigger than 31.
 */
u32 xhci_mtk_td_remainder_quirk(unsigned int td_running_total,
	unsigned trb_buffer_length, struct urb *urb)
{
	u32 max = 31;
	int remainder, td_packet_count, packet_transferred;
	unsigned int td_transfer_size = urb->transfer_buffer_length;
	unsigned int maxp;


	/* no scatter-gather for control transfer */
	if (usb_endpoint_xfer_control(&urb->ep->desc))
		return 0;

	maxp = GET_MAX_PACKET(usb_endpoint_maxp(&urb->ep->desc));

	/* 0 for the last TRB */
	if (td_running_total + trb_buffer_length == td_transfer_size)
		return 0;

	packet_transferred = td_running_total / maxp;
	td_packet_count = DIV_ROUND_UP(td_transfer_size, maxp);
	remainder = td_packet_count - packet_transferred;

	if (remainder > max)
		return max << 17;
	else
		return remainder << 17;
}


