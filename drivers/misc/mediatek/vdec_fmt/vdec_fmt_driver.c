// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/suspend.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include "vdec_fmt_driver.h"
#include "vdec_fmt_dmabuf.h"
#include "vdec_fmt_pm.h"
#include "vdec_fmt_utils.h"
#include "iommu_debug.h"

static struct mtk_vdec_fmt *fmt_mtkdev;

int fmt_dbg_level;

module_param(fmt_dbg_level, int, 0644);

static int fmt_check_reg_base(struct mtk_vdec_fmt *fmt, u64 addr, u64 length)
{
	int i;

	if (addr >= MAP_PA_BASE_1GB)
		return -EINVAL;

	for (i = 0; i < (int)FMT_MAP_HW_REG_NUM; i++)
		if (addr >= (u64)fmt->map_base[i].base &&
			addr + length <= (u64)fmt->map_base[i].base + fmt->map_base[i].len)
			return i;
	fmt_err("addr %x length %x not found!", addr, length);

	return -EINVAL;
}

static int fmt_lock(u32 id, bool sec)
{
	unsigned int suspend_block_cnt = 0;
	int ret = -1;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;
	enum fmt_gce_status gce_status = GCE_NONE;

	if (id >= fmt->gce_th_num)
		return ret;

	while (fmt->is_entering_suspend == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
			fmt_debug(0, "blocked by suspend");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	//if (sec != false)
	//	gce_status = GCE_SECURE;
	//else
	// FMT only support sec in TZMP flow, always use normal GCE
	gce_status = GCE_NORMAL;

	fmt_debug(1, "id %d sec %d gce_status %d cur gce_status %d",
				id, sec,
				gce_status,
				fmt->gce_status[id]);

	// only need to trylock semaphore when gce status change
	// NONE -> NORMAL/ NONE -> SECURE / NORMAL <-> SECURE
	if (gce_status != fmt->gce_status[id])
		ret = down_trylock(&fmt->fmt_sem[id]);
	else
		ret = 0;

	if (ret == 0)
		fmt->gce_status[id] = gce_status;

	return ret;
}

static void fmt_unlock(u32 id)
{
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	if (id >= fmt->gce_th_num)
		return;

	fmt_debug(1, "id %d cur gce_status", id, fmt->gce_status[id]);

	up(&fmt->fmt_sem[id]);

	fmt->gce_status[id] = GCE_NONE;
}

static int fmt_set_gce_task(struct cmdq_pkt *pkt_ptr, u32 id,
	struct dmabuf_info iinfo, struct dmabuf_info oinfo)
{
	int i;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	mutex_lock(&fmt->mux_task);
	for (i = 0; i < FMT_INST_MAX; i++) {
		if (fmt->gce_task[i].used == 0 && fmt->gce_task[i].pkt_ptr == NULL) {
			fmt->gce_task[i].used = 1;
			fmt->gce_task[i].pkt_ptr = pkt_ptr;
			fmt->gce_task[i].identifier = id;
			fmt->gce_task[i].iinfo = iinfo;
			fmt->gce_task[i].oinfo = oinfo;
			mutex_unlock(&fmt->mux_task);
			fmt_debug(1, "taskid %d pkt_ptr %p",
				i, pkt_ptr);
			return i;
		}
	}
	mutex_unlock(&fmt->mux_task);

	return -1;
}

static void fmt_clear_gce_task(unsigned int taskid)
{
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	mutex_lock(&fmt->mux_task);
	if (fmt->gce_task[taskid].used == 1 && fmt->gce_task[taskid].pkt_ptr != NULL) {
		fmt_debug(1, "taskid %d pkt_ptr %p",
				taskid, fmt->gce_task[taskid].pkt_ptr);
		fmt->gce_task[taskid].used = 0;
		fmt->gce_task[taskid].pkt_ptr = NULL;
		fmt->gce_task[taskid].identifier = 0;
		fmt_dmabuf_free_iova(fmt->gce_task[taskid].iinfo.dbuf,
		fmt->gce_task[taskid].iinfo.attach, fmt->gce_task[taskid].iinfo.sgt);
		fmt_dmabuf_free_iova(fmt->gce_task[taskid].oinfo.dbuf,
		fmt->gce_task[taskid].oinfo.attach, fmt->gce_task[taskid].oinfo.sgt);
		fmt_dmabuf_put(fmt->gce_task[taskid].iinfo.dbuf);
		fmt_dmabuf_put(fmt->gce_task[taskid].oinfo.dbuf);
	}
	mutex_unlock(&fmt->mux_task);
}

static int fmt_set_gce_cmd(struct cmdq_pkt *pkt,
	unsigned char cmd, u64 addr, u64 data, u32 mask, u32 gpr, unsigned int idx,
	struct dmabuf_info *iinfo, struct dmabuf_info *oinfo, struct dmabufmap map[],
	bool secure)
{
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;
	int type;
	u64 iova = 0;

	switch (cmd) {
	case CMD_READ:
		fmt_debug(3, "CMD_READ addr 0x%x", addr);
		if (fmt_check_reg_base(fmt, addr, 4) >= 0)
			cmdq_pkt_read_addr(pkt, addr, CMDQ_THR_SPR_IDX1);
		else
			fmt_err("CMD_READ wrong addr: 0x%x", addr);
	break;
	case CMD_WRITE:
		fmt_debug(3, "CMD_WRITE addr 0x%x data 0x%x mask 0x%x", addr, data, mask);
		if (fmt_check_reg_base(fmt, addr, 4) >= 0)
			cmdq_pkt_write(pkt, fmt->clt_base, addr, data, mask);
		else
			fmt_err("CMD_WRITE wrong addr: 0x%x 0x%x 0x%x",
				addr, data, mask);
	break;
	case CMD_WRITE_RDMA:
		fmt_debug(3, "CMD_WRITE_RDMA cpridx %d data 0x%x offset 0x%x", addr, data, mask);
		if (addr == CPR_IDX_FMT_RDMA_PIPE_IDX) {
			fmt_debug(3, "cmdq_pkt_assign_command idx %d cpr %d value 0x%x",
					idx, addr, data);
			cmdq_pkt_assign_command(pkt,
					CMDQ_CPR_PREBUILT_PIPE(CMDQ_PREBUILT_VFMT),
					idx);
		} else if (addr >= CPR_IDX_FMT_RDMA_SRC_OFFSET_0
			&& addr <= CPR_IDX_FMT_RDMA_AFBC_PAYLOAD_OST) {
			fmt_debug(3, "cmdq_pkt_assign_command idx %d cpr %d value 0x%x",
					idx, addr, data);
			cmdq_pkt_assign_command(pkt,
					CMDQ_CPR_PREBUILT(CMDQ_PREBUILT_VFMT, idx, addr),
					data);
		} else {
			fmt_err("invalid GCE cpr index %d", addr);
		}
	break;
	case CMD_POLL_REG:
		fmt_debug(3, "CMD_POLL_REG addr 0x%x data 0x%x mask 0x%x", addr, data, mask);
		if (fmt_check_reg_base(fmt, addr, 4) >= 0)
			cmdq_pkt_poll_addr(pkt, data, addr, mask, gpr);
		else
			fmt_err("CMD_POLL_REG wrong addr: 0x%x 0x%x 0x%x",
				addr, data, mask);
	break;
	case CMD_WAIT_EVENT:
		if (data < GCE_EVENT_MAX) { // fmt hw event
			fmt_debug(3, "CMD_WAIT_EVENT eid %d", fmt->gce_codec_eid[data]);
			cmdq_pkt_wfe(pkt, fmt->gce_codec_eid[data]);
		} else if (data >= SYNC_TOKEN_PREBUILT_VFMT_WAIT // rdma sw wa sync event
					&& data <= SYNC_TOKEN_PREBUILT_VFMT_LOCK) {
			switch (data) {
			case SYNC_TOKEN_PREBUILT_VFMT_WAIT:
				fmt_debug(3, "cmdq_pkt_wfe %d",
					CMDQ_TOKEN_PREBUILT_VFMT_WAIT);
				cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_VFMT_WAIT);
			break;
			case SYNC_TOKEN_PREBUILT_VFMT_SET:
				fmt_debug(3, "cmdq_pkt_wfe %d",
					CMDQ_TOKEN_PREBUILT_VFMT_SET);
				cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_VFMT_SET);
			break;
			case SYNC_TOKEN_PREBUILT_VFMT_LOCK:
				fmt_debug(3, "cmdq_pkt_wfe %d",
					CMDQ_TOKEN_PREBUILT_VFMT_LOCK);
				cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_VFMT_LOCK);
			break;
			default:
			break;
			}
			fmt_debug(3, "CMD_WAIT_EVENT %d", data);
		} else
			fmt_err("CMD_WAIT_EVENT got wrong eid %llu",
				data);
	break;
	case CMD_SET_EVENT:
		if (data >= SYNC_TOKEN_PREBUILT_VFMT_WAIT // rdma sw wa sync event
			&& data <= SYNC_TOKEN_PREBUILT_VFMT_LOCK) {
			switch (data) {
			case SYNC_TOKEN_PREBUILT_VFMT_WAIT:
				fmt_debug(3, "cmdq_pkt_set_event %d",
					CMDQ_TOKEN_PREBUILT_VFMT_WAIT);
				cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_VFMT_WAIT);
			break;
			case SYNC_TOKEN_PREBUILT_VFMT_SET:
				fmt_debug(3, "cmdq_pkt_set_event %d",
					CMDQ_TOKEN_PREBUILT_VFMT_SET);
				cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_VFMT_SET);
			break;
			case SYNC_TOKEN_PREBUILT_VFMT_LOCK:
				fmt_debug(3, "cmdq_pkt_set_event %d",
					CMDQ_TOKEN_PREBUILT_VFMT_LOCK);
				cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_VFMT_LOCK);
			break;
			default:
			break;
			}
			fmt_debug(3, "CMD_SET_EVENT %d", data);
		} else
			fmt_err("CMD_WAIT_EVENT got wrong eid %llu",
				data);
	break;
	case CMD_CLEAR_EVENT:
		if (data < GCE_EVENT_MAX) {
			fmt_debug(3, "CMD_CLEAR_EVENT eid %d", fmt->gce_codec_eid[data]);
			cmdq_pkt_clear_event(pkt, fmt->gce_codec_eid[data]);
		} else
			fmt_err("got wrong eid %llu",
				data);
	break;
	case CMD_WRITE_FD:
		fmt_debug(3, "CMD_WRITE_FD addr 0x%x fd 0x%x offset 0x%x", addr, data, mask);
		type = fmt_check_reg_base(fmt, addr, 4);
		if (type >= 0) {
			if (type % 2 == 0) {
				iova = fmt_translate_fd(data, mask, map, fmt->dev,
					&iinfo->dbuf, &iinfo->attach, &iinfo->sgt, false);
				if (iinfo->attach == NULL || iinfo->sgt == NULL)
					return -1;
				if (addr - fmt->map_base[type].base >= 0xf30) { // rdma 34bit
					fmt_debug(3, "cmdq_pkt_write base: 0x%x addr:0x%x value:0x%x",
						fmt->map_base[type].base, addr, (iova >> 32));
					cmdq_pkt_write(pkt, fmt->clt_base, addr,
						(iova >> 32), ~0);
				} else {
					fmt_debug(3, "cmdq_pkt_write base: 0x%x addr:0x%x value:0x%x",
							fmt->map_base[type].base, addr, iova);
					cmdq_pkt_write(pkt, fmt->clt_base, addr,
						iova, ~0);
				}
			} else {
				iova = fmt_translate_fd(data, mask, map, fmt->dev,
						&oinfo->dbuf, &oinfo->attach, &oinfo->sgt, !secure);
				if (oinfo->attach == NULL || oinfo->sgt == NULL)
					return -1;
				if (addr - fmt->map_base[type].base >= 0xf34) { // wrot 34bit
					fmt_debug(3, "cmdq_pkt_write base: 0x%x addr:0x%x value:0x%x",
						fmt->map_base[type].base, addr, (iova >> 32));
					cmdq_pkt_write(pkt, fmt->clt_base, addr,
						(iova >> 32), ~0);
				} else {
					fmt_debug(3, "cmdq_pkt_write base: 0x%x addr:0x%x value:0x%x",
							fmt->map_base[type].base, addr, iova);
					cmdq_pkt_write(pkt, fmt->clt_base, addr,
						iova, ~0);
				}
			}
		} else
			fmt_err("CMD_WRITE_FD wrong addr: 0x%x 0x%x 0x%x",
				addr, data, mask);
	break;
	case CMD_WRITE_FD_RDMA:
		fmt_debug(3, "CMD_WRITE_FD_RDMA cpridx %d fd 0x%x offset 0x%x", addr, data, mask);
		iova = fmt_translate_fd(data, mask, map, fmt->dev,
					&iinfo->dbuf, &iinfo->attach, &iinfo->sgt, false);
		if (iinfo->attach == NULL || iinfo->sgt == NULL)
			return -1;
		if (addr >= CPR_IDX_FMT_RDMA_SRC_OFFSET_0
			&& addr <= CPR_IDX_FMT_RDMA_UFO_DEC_LENGTH_BASE_C) {
			fmt_debug(3, "cmdq_pkt_assign_command idx %d cpr %d value 0x%x",
						idx, addr, iova);
			cmdq_pkt_assign_command(pkt,
					CMDQ_CPR_PREBUILT(CMDQ_PREBUILT_VFMT, idx, addr),
					iova);
		} else if (addr >= CPR_IDX_FMT_RDMA_SRC_BASE_0_MSB
					&& addr <= CPR_IDX_FMT_RDMA_SRC_OFFSET_2_MSB){ // 34bit msb
			fmt_debug(3, "cmdq_pkt_assign_command idx %d cpr %d value 0x%x",
						idx, addr, (iova >> 32));
			cmdq_pkt_assign_command(pkt,
					CMDQ_CPR_PREBUILT(CMDQ_PREBUILT_VFMT, idx, addr),
					(iova >> 32));
		} else {
			fmt_err("invalid GCE cpr index %d", addr);
		}
	break;
	default:
		fmt_err("unknown GCE cmd %d", cmd);
	break;
	}
	return 0;
}

#define ADDR0(offset) \
	(fmt->map_base[0+idx*2].base + offset)
#define ADDR1(offset) \
	(fmt->map_base[1+idx*2].base + offset)
#define REG0(offset) \
	FMT_GET32(fmt->map_base[0+idx*2].va + offset)
#define REG1(offset) \
	FMT_GET32(fmt->map_base[1+idx*2].va + offset)
static void fmt_dump_addr_reg(void)
{
	int idx;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	if (fmt->dtsInfo.RDMA_needWA)
		cmdq_util_prebuilt_dump(1, CMDQ_TOKEN_PREBUILT_VFMT_WAIT);
	for (idx = 0; idx < fmt->dtsInfo.pipeNum; idx++) {
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x0), REG0(0x0), ADDR0(0x4), REG0(0x4),
		ADDR0(0x20), REG0(0x20), ADDR0(0x24), REG0(0x24));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x28), REG0(0x28), ADDR0(0x30), REG0(0x30),
		ADDR0(0x38), REG0(0x38), ADDR0(0x60), REG0(0x60));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x68), REG0(0x68), ADDR0(0x70), REG0(0x70),
		ADDR0(0x78), REG0(0x78), ADDR0(0x90), REG0(0x90),
		ADDR0(0x98), REG0(0x98));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x240), REG0(0x240), ADDR0(0x244), REG0(0x244),
		ADDR0(0x248), REG0(0x248), ADDR0(0x250), REG0(0x250));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x254), REG0(0x254), ADDR0(0x258), REG0(0x258),
		ADDR0(0x260), REG0(0x260), ADDR0(0x264), REG0(0x264));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x268), REG0(0x268), ADDR0(0x270), REG0(0x270),
		ADDR0(0x274), REG0(0x274), ADDR0(0x278), REG0(0x278));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x280), REG0(0x280), ADDR0(0x284), REG0(0x284),
		ADDR0(0x288), REG0(0x288), ADDR0(0x290), REG0(0x290));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x2A0), REG0(0x2A0), ADDR0(0x2A8), REG0(0x2A8),
		ADDR0(0x2B8), REG0(0x2B8));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x2C0), REG0(0x2C0), ADDR0(0x2C4), REG0(0x2C4),
		ADDR0(0x2C8), REG0(0x2C8));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x400), REG0(0x400), ADDR0(0x408), REG0(0x408),
		ADDR0(0x410), REG0(0x410), ADDR0(0x418), REG0(0x418));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x420), REG0(0x420), ADDR0(0x428), REG0(0x428),
		ADDR0(0x430), REG0(0x430), ADDR0(0x438), REG0(0x438));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x440), REG0(0x440), ADDR0(0x448), REG0(0x448),
		ADDR0(0x450), REG0(0x450), ADDR0(0x458), REG0(0x458));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x460), REG0(0x460), ADDR0(0x468), REG0(0x468),
		ADDR0(0x470), REG0(0x470), ADDR0(0x478), REG0(0x478));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x480), REG0(0x480), ADDR0(0x488), REG0(0x488),
		ADDR0(0x490), REG0(0x490), ADDR0(0x498), REG0(0x498));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x4A0), REG0(0x4A0), ADDR0(0x4A8), REG0(0x4A8),
		ADDR0(0x4B0), REG0(0x4B0), ADDR0(0x4B8), REG0(0x4B8));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR0(0x4C0), REG0(0x4C0), ADDR0(0x4C8), REG0(0x4C8),
		ADDR0(0x4D0), REG0(0x4D0), ADDR0(0x4D8), REG0(0x4D8),
		ADDR0(0x4E0), REG0(0x4E0));
		fmt_debug(0, "RDMA%d(0x%x) 0x%x",
		idx, ADDR0(0xF54), REG0(0xF54));

		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x0), REG1(0x0), ADDR1(0x4), REG1(0x4),
		ADDR1(0x8), REG1(0x8), ADDR1(0xC), REG1(0xC));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x10), REG1(0x10), ADDR1(0x14), REG1(0x14),
		ADDR1(0x18), REG1(0x18), ADDR1(0x1C), REG1(0x1C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x20), REG1(0x20), ADDR1(0x24), REG1(0x24),
		ADDR1(0x28), REG1(0x28), ADDR1(0x2C), REG1(0x2C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x30), REG1(0x30), ADDR1(0x34), REG1(0x34),
		ADDR1(0x38), REG1(0x38), ADDR1(0x3C), REG1(0x3C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x40), REG1(0x40), ADDR1(0x44), REG1(0x44),
		ADDR1(0x48), REG1(0x48));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x50), REG1(0x50), ADDR1(0x54), REG1(0x54),
		ADDR1(0x58), REG1(0x58), ADDR1(0x5C), REG1(0x5C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x)",
		idx, ADDR1(0x68), REG1(0x68), ADDR1(0x6C), REG1(0x6C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x70), REG1(0x70), ADDR1(0x74), REG1(0x74),
		ADDR1(0x78), REG1(0x78), ADDR1(0x7C), REG1(0x7C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0x80), REG1(0x80), ADDR1(0x84), REG1(0x84),
		ADDR1(0x88), REG1(0x88), ADDR1(0x8C), REG1(0x8C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xD0), REG1(0xD0), ADDR1(0xD4), REG1(0xD4),
		ADDR1(0xD8), REG1(0xD8), ADDR1(0xDC), REG1(0xDC));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xE0), REG1(0xE0), ADDR1(0xE4), REG1(0xE4),
		ADDR1(0xE8), REG1(0xE8), ADDR1(0xEC), REG1(0xEC),
		ADDR1(0xF0), REG1(0xF0));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xF00), REG1(0xF00), ADDR1(0xF04), REG1(0xF04),
		ADDR1(0xF08), REG1(0xF08), ADDR1(0xF0C), REG1(0xF0C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xF10), REG1(0xF10), ADDR1(0xF14), REG1(0xF14),
		ADDR1(0xF18), REG1(0xF18), ADDR1(0xF1C), REG1(0xF1C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xF20), REG1(0xF20), ADDR1(0xF24), REG1(0xF24),
		ADDR1(0xF28), REG1(0xF28), ADDR1(0xF2C), REG1(0xF2C));
		fmt_debug(0, "WROT%d(0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x (0x%x) 0x%x",
		idx, ADDR1(0xF30), REG1(0xF30), ADDR1(0xF34), REG1(0xF34),
		ADDR1(0xF38), REG1(0xF38), ADDR1(0xF3C), REG1(0xF3C));
	}
}

static void fmt_gce_flush_callback(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt_ptr;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	pkt_ptr = (struct cmdq_pkt *)data.data;

	if (data.err < 0) {
		fmt_err("pkt_ptr %p", pkt_ptr);
		atomic_set(&fmt->fmt_error, 1);
		fmt_dump_addr_reg();
	}
}

static int fmt_gce_timeout_aee(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt_ptr;

	pkt_ptr = (struct cmdq_pkt *)data.data;

	if (pkt_ptr->err_data.wfe_timeout) {
		fmt_err("wfe %d timeout", pkt_ptr->err_data.event);
		return CMDQ_NO_AEE;
	}
	return CMDQ_AEE_WARN;
}

static int fmt_gce_cmd_flush(unsigned long arg)
{
	int i, taskid, ret;
	unsigned char *user_data_addr = NULL;
	struct gce_cmdq_obj buff;
	struct cmdq_pkt *pkt_ptr;
	struct cmdq_client *cl;
	struct gce_cmds *cmds;
	unsigned int suspend_block_cnt = 0;
	unsigned int identifier;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;
	int lock = -1;
	struct dmabuf_info iinfo, oinfo;
	struct dmabufmap map[FMT_FD_RESERVE];

	fmt_debug(1, "+");

	if (atomic_read(&fmt->fmt_error) == 1) {
		fmt_err("fmt in error status, flushed failed!");
		return -EINVAL;
	}

	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&buff, user_data_addr,
				   (unsigned long)sizeof(struct gce_cmdq_obj));
	if (ret != 0L) {
		fmt_err("gce_cmdq_obj copy_from_user failed! %d",
			ret);
		return -EINVAL;
	}

	identifier = buff.identifier;
	if (identifier >= fmt->gce_th_num) {
		fmt_err("invalid identifier %u",
			identifier);
		return -EINVAL;
	}

	cl = fmt->clt_fmt[identifier];

	if (cl == NULL) {
		fmt_err("gce thread is null id %d",
						buff.identifier);
		return -EINVAL;
	}

	while (fmt->is_entering_suspend == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > 500) {
			fmt_debug(0, "gce_flush blocked by suspend");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	mutex_lock(&fmt->mux_gce_th[identifier]);
	while (lock != 0) {
		lock = fmt_lock(identifier,
			(bool)buff.secure);
		if (lock != 0) {
			mutex_unlock(&fmt->mux_gce_th[identifier]);
			usleep_range(1000, 2000);
			mutex_lock(&fmt->mux_gce_th[identifier]);
		}
	}

	cmds = fmt->gce_cmds[identifier];

	user_data_addr = (unsigned char *)
				   (unsigned long)buff.cmds_user_ptr;
	ret = (long)copy_from_user(cmds, user_data_addr,
				   (unsigned long)sizeof(struct gce_cmds));
	if (ret != 0L) {
		fmt_err("gce_cmds copy_from_user failed! %d",
			ret);
		mutex_unlock(&fmt->mux_gce_th[identifier]);
		return -EINVAL;
	}
	if (cmds->cmd_cnt >= FMT_CMDQ_CMD_MAX) {
		fmt_err("cmd_cnt (%d) overflow!!", cmds->cmd_cnt);
		cmds->cmd_cnt = FMT_CMDQ_CMD_MAX;
		mutex_unlock(&fmt->mux_gce_th[identifier]);
		ret = -EINVAL;
		return ret;
	}

	buff.cmds_user_ptr = (u64)(unsigned long)cmds;

	pkt_ptr = cmdq_pkt_create(cl);
	if (IS_ERR_OR_NULL(pkt_ptr)) {
		fmt_err("cmdq_pkt_create fail");
		pkt_ptr = NULL;
		mutex_unlock(&fmt->mux_gce_th[identifier]);
		ret = -EINVAL;
		return ret;
	}

	for (i = 0; i < FMT_FD_RESERVE; i++) {
		map[i].fd = -1;
		map[i].iova = 0;
	}

	for (i = 0; i < cmds->cmd_cnt; i++) {
		ret = fmt_set_gce_cmd(pkt_ptr, cmds->cmd[i],
			cmds->addr[i], cmds->data[i],
			cmds->mask[i], fmt->gce_gpr[identifier], identifier,
			&iinfo, &oinfo, map, (bool)buff.secure);

		if (ret < 0) {
			mutex_unlock(&fmt->mux_gce_th[identifier]);
			fmt_dmabuf_free_iova(iinfo.dbuf,
				iinfo.attach, iinfo.sgt);
			fmt_dmabuf_free_iova(oinfo.dbuf,
				oinfo.attach, oinfo.sgt);
			fmt_dmabuf_put(iinfo.dbuf);
			fmt_dmabuf_put(oinfo.dbuf);
			ret = -EINVAL;
			return ret;
		}
	}

	mutex_lock(&fmt->mux_fmt);
	if ((atomic_read(&fmt->gce_job_cnt[0])
		+ atomic_read(&fmt->gce_job_cnt[1])) == 0) {
		// FMT cores share the same MTCMOS/CLK,
		// pwr/clock on/off only when there's 0 job on both pipes
		fmt_debug(1, "Both pipe job cnt = 0, pwr/clock on");
		ret = fmt_clock_on(fmt);
		if (ret != 0L) {
			fmt_err("fmt_clock_on failed!%d",
			ret);
			mutex_unlock(&fmt->mux_gce_th[identifier]);
			mutex_unlock(&fmt->mux_fmt);
			return -EINVAL;
		}
	}
	atomic_inc(&fmt->gce_job_cnt[identifier]);
	mutex_unlock(&fmt->mux_fmt);

	fmt_start_dvfs_emi_bw(fmt, buff.pmqos_param, identifier);

	mutex_unlock(&fmt->mux_gce_th[identifier]);

	taskid = fmt_set_gce_task(pkt_ptr, identifier, iinfo, oinfo);

	if (taskid < 0) {
		fmt_err("failed to set task id");
		mutex_lock(&fmt->mux_gce_th[identifier]);

		mutex_lock(&fmt->mux_fmt);
		atomic_dec(&fmt->gce_job_cnt[identifier]);
		fmt_debug(0, "id %d job cnt %d %d",
		identifier, atomic_read(&fmt->gce_job_cnt[0]), atomic_read(&fmt->gce_job_cnt[1]));
		if ((atomic_read(&fmt->gce_job_cnt[0])
			+ atomic_read(&fmt->gce_job_cnt[1])) == 0) {
			// FMT cores share the same MTCMOS/CLK,
			// pwr/clock on/off only when there's 0 job on both pipes
			fmt_end_dvfs_emi_bw(fmt, identifier);
			fmt_debug(0, "Both pipe job cnt = 0, pwr/clock off");
			ret = fmt_clock_off(fmt);
				if (ret != 0L) {
					fmt_err("fmt_clock_off failed!%d",
					ret);
					return -EINVAL;
				}
		}
		mutex_unlock(&fmt->mux_fmt);
		if (atomic_read(&fmt->gce_job_cnt[identifier]) == 0)
			fmt_unlock(identifier);

		mutex_unlock(&fmt->mux_gce_th[identifier]);

		cmdq_pkt_destroy(pkt_ptr);
		ret = -EINVAL;
		return ret;
	}

	memcpy(&fmt->gce_task[taskid].cmdq_buff, &buff, sizeof(buff));

	// flush cmd async
	fmt_debug(1, "call cmdq_pkt_flush_async");
	pkt_ptr->aee_cb = fmt_gce_timeout_aee;
	cmdq_pkt_flush_async(pkt_ptr,
		fmt_gce_flush_callback, (void *)fmt->gce_task[taskid].pkt_ptr);

	if (copy_to_user(buff.taskid, &taskid, sizeof(unsigned int))) {
		fmt_err("copy task id to user fail");
		return -EFAULT;
	}

	fmt_debug(1, "-");

	return ret;
}

static int fmt_gce_wait_callback(unsigned long arg)
{
	int ret;
	unsigned int identifier, taskid;
	unsigned char *user_data_addr = NULL;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	user_data_addr = (unsigned char *)arg;
	ret = (long)copy_from_user(&taskid, user_data_addr,
				   (unsigned long)sizeof(unsigned int));
	if (ret != 0L) {
		fmt_err("copy_from_user failed!%d",
			ret);
		return -EINVAL;
	}

	if (taskid >= FMT_INST_MAX) {
		fmt_err("invalid taskid %u",
			taskid);
		return -EINVAL;
	}

	identifier = fmt->gce_task[taskid].identifier;
	if (identifier >= fmt->gce_th_num) {
		fmt_err("invalid identifier %u",
			identifier);
		return -EINVAL;
	}

	fmt_debug(1, "id %d taskid %d pkt_ptr %p",
		identifier, taskid, fmt->gce_task[taskid].pkt_ptr);

	cmdq_pkt_wait_complete(fmt->gce_task[taskid].pkt_ptr);

	if (fmt_dbg_level == 4)
		fmt_dump_addr_reg();

	mutex_lock(&fmt->mux_gce_th[identifier]);

	mutex_lock(&fmt->mux_fmt);
	atomic_dec(&fmt->gce_job_cnt[identifier]);
	if ((atomic_read(&fmt->gce_job_cnt[0])
		+ atomic_read(&fmt->gce_job_cnt[1])) == 0) {
		// FMT cores share the same MTCMOS/CLK,
		// pwr/clock on/off only when there's 0 job on both pipes
		fmt_end_dvfs_emi_bw(fmt, identifier);
		fmt_debug(1, "Both pipe job cnt = 0, pwr/clock off");
		ret = fmt_clock_off(fmt);
			if (ret != 0L) {
				fmt_err("fmt_clock_off failed!%d",
				ret);
				return -EINVAL;
			}
	}
	mutex_unlock(&fmt->mux_fmt);
	if (atomic_read(&fmt->gce_job_cnt[identifier]) == 0)
		fmt_unlock(identifier);

	mutex_unlock(&fmt->mux_gce_th[identifier]);

	cmdq_pkt_destroy(fmt->gce_task[taskid].pkt_ptr);
	fmt_clear_gce_task(taskid);

	return ret;
}

static int fmt_get_platform_dts(unsigned long arg)
{
	struct dts_info *dts;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	dts = &fmt->dtsInfo;

	if (copy_to_user((void *)arg, dts, sizeof(*dts))) {
		fmt_err("Copy dts to user space failed\n");
		return -EFAULT;
	}

	return 0;
}

static int vdec_fmt_open(struct inode *inode, struct file *file)
{
	fmt_debug(1, "tid:%d", current->pid);

	return 0;
}

static int vdec_fmt_release(struct inode *ignored, struct file *file)
{
	fmt_debug(1, "tid:%d", current->pid);

	return 0;
}

static long vdec_fmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;

	fmt_debug(1, "pid:%d tid:%d", current->group_leader->pid, current->pid);

	switch (cmd) {
	case FMT_GCE_SET_CMD_FLUSH:
		ret = fmt_gce_cmd_flush(arg);
		break;
	case FMT_GCE_WAIT_CALLBACK:
		ret = fmt_gce_wait_callback(arg);
		break;
	case FMT_GET_PLATFORM_DTS:
		ret = fmt_get_platform_dts(arg);
		break;
	default:
		fmt_err("Unknown cmd");
		break;
	}

	return ret;
}

#ifdef MTK_VDEC_FMT_DVT_VERIFICATION
void vdec_fmt_vma_open(struct vm_area_struct *vma)
{
	fmt_debug(1, "vdec fmt VMA open, virt %lx, phys %lx",
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void vdec_fmt_vma_close(struct vm_area_struct *vma)
{
	fmt_debug(1, "vdec fmt VMA close, virt %lx, phys %lx",
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

static const struct vm_operations_struct
vdec_fmt_remap_vm_ops = {
	.open = vdec_fmt_vma_open,
	.close = vdec_fmt_vma_close,
};

static int vdec_fmt_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length;
	unsigned long pfn;

	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff<<PAGE_SHIFT;

	if (fmt_check_reg_base(fmt_mtkdev, pfn, length) != 0) {
		fmt_err("illegal mmap pfn 0x%lx, len 0x%lx",
			pfn, length);
		return -EAGAIN;
	}
	fmt_debug(1, "start 0x%lx, end 0x%lx, pgoff 0x%lx",
			(unsigned long)vma->vm_start,
			(unsigned long)vma->vm_end,
			(unsigned long)vma->vm_pgoff);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &vdec_fmt_remap_vm_ops;
	vdec_fmt_vma_open(vma);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_COMPAT)
static int compat_get_gce_cmdq_obj_data(
		struct compat_gce_cmdq_obj __user *data32,
		struct gce_cmdq_obj __user *data)
{
	u64 u64t;
	u32 u32t;
	compat_uptr_t ptrt;
	int err;

	err = get_user(u64t, &data32->cmds_user_ptr);
	err |= put_user(u64t, &data->cmds_user_ptr);
	err |= get_user(u32t, &data32->identifier);
	err |= put_user(u32t, &data->identifier);
	err |= get_user(u32t, &data32->secure);
	err |= put_user(u32t, &data->secure);
	err |= get_user(ptrt, &data32->taskid);
	err |= put_user(compat_ptr(ptrt), &data->taskid);
	err |= get_user(u32t, &data32->pmqos_param.tv_sec);
	err |= put_user(u32t, &data->pmqos_param.tv_sec);
	err |= get_user(u32t, &data32->pmqos_param.tv_usec);
	err |= put_user(u32t, &data->pmqos_param.tv_usec);
	err |= get_user(u32t, &data32->pmqos_param.pixel_size);
	err |= put_user(u32t, &data->pmqos_param.pixel_size);
	err |= get_user(u32t, &data32->pmqos_param.rdma_datasize);
	err |= put_user(u32t, &data->pmqos_param.rdma_datasize);
	err |= get_user(u32t, &data32->pmqos_param.wdma_datasize);
	err |= put_user(u32t, &data->pmqos_param.wdma_datasize);

	return err;

}
static long compat_vdec_fmt_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int err = 0;

	fmt_debug(1, "pid:%d tid:%d", current->group_leader->pid, current->pid);
	switch (cmd) {
	case COMPAT_FMT_GCE_SET_CMD_FLUSH:
	{
		struct compat_gce_cmdq_obj __user *data32;
		struct gce_cmdq_obj __user *data;

		data32 = compat_ptr((uint32_t)arg);
		data = compat_alloc_user_space(sizeof(struct gce_cmdq_obj));
		if (data == NULL)
			return -EFAULT;
		err = compat_get_gce_cmdq_obj_data(data32, data);
		if (err != 0)
			return err;
		return file->f_op->unlocked_ioctl(file, FMT_GCE_SET_CMD_FLUSH,
				(unsigned long)data);
	}
	case FMT_GCE_WAIT_CALLBACK:
	case FMT_GET_PLATFORM_DTS:
		return file->f_op->unlocked_ioctl(file, cmd, arg);
	default:
		fmt_err("Unknown cmd");
		return -EFAULT;
	}
}
#endif

static const struct file_operations vdec_fmt_fops = {
	.owner = THIS_MODULE,
	.open = vdec_fmt_open,
	.release = vdec_fmt_release,
	.unlocked_ioctl = vdec_fmt_ioctl,
#ifdef MTK_VDEC_FMT_DVT_VERIFICATION
	.mmap = vdec_fmt_mmap,
#endif
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_vdec_fmt_ioctl,
#endif
};

static int vdec_fmt_suspend(struct device *pDev)
{
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	fmt_debug(1, "+");

	if (atomic_read(&fmt->gce_job_cnt[0]) > 0 ||
		atomic_read(&fmt->gce_job_cnt[1]) > 0) {
		fmt_debug(0, "waiting %d %d",
			atomic_read(&fmt->gce_job_cnt[0]),
			atomic_read(&fmt->gce_job_cnt[1]));
		return -EBUSY;
	}
	fmt_debug(1, "-");
	return 0;
}

static int vdec_fmt_resume(struct device *pDev)
{
	fmt_debug(1, "resume done");
	return 0;
}

static int vdec_fmt_suspend_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	int wait_cnt = 0;
	struct mtk_vdec_fmt *fmt = fmt_mtkdev;

	fmt_debug(1, "action %lu", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		fmt->is_entering_suspend = 1;
		while (atomic_read(&fmt->gce_job_cnt[0]) > 0 ||
				atomic_read(&fmt->gce_job_cnt[1]) > 0) {
			wait_cnt++;
			if (wait_cnt > 5) {
				fmt_debug(0, "waiting %d %d",
				atomic_read(&fmt->gce_job_cnt[0]),
				atomic_read(&fmt->gce_job_cnt[1]));
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				return NOTIFY_DONE;
			}
			usleep_range(10000, 20000);
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		fmt->is_entering_suspend = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

int fmt_translation_fault_callback(int port, u64 mva, void *data)
{
	fmt_debug(0, "TF callback, port:%d, mva:0x%llx %p", port, mva, data);
	fmt_dump_addr_reg();
	mtk_smi_dbg_hang_detect("debug");
	return 0;
}

void fmt_translation_fault_callback_setting(struct mtk_vdec_fmt *fmt)
{
	int i;

	for (i = 0; i < FMT_PORT_NUM; i++) {
		if (fmt->fmt_m4u_ports[i] != 0)
			mtk_iommu_register_fault_callback(fmt->fmt_m4u_ports[i],
			(mtk_iommu_fault_callback_t)fmt_translation_fault_callback,
			fmt, false);
	}
}

static int vdec_fmt_probe(struct platform_device *pdev)
{
	struct mtk_vdec_fmt *fmt;
	struct device_node *larbnode = NULL;
	struct platform_device *larbdev;
	struct resource *res;
	struct device *dev;
	int ret = 0;
	int i, rdma_swwa;
	u32 port_id;

	fmt_debug(0, "initialization");

	dev = &pdev->dev;
	fmt = devm_kzalloc(dev, sizeof(*fmt), GFP_KERNEL);
	if (fmt == NULL)
		return -ENOMEM;

	fmt_mtkdev = fmt;

	ret = of_property_read_string(dev->of_node, "mediatek,fmtname",
								&fmt->fmtname);
	fmt_debug(0, "name %s", fmt->fmtname);

	if (ret != 0) {
		fmt_err("failed to find mediatek,fmtname");
		return ret;
	}

	fmt->dev = &pdev->dev;
	platform_set_drvdata(pdev, fmt);

	for (i = 0; i < (int)FMT_MAP_HW_REG_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL)
			break;
		fmt->map_base[i].base = res->start;
		fmt->map_base[i].len = resource_size(res);
		fmt->map_base[i].va = (unsigned long)of_iomap(dev->of_node, i);
		if (i == 0)
			fmt->dtsInfo.RDMA_baseAddr = fmt->map_base[i].base;
		else if (i == 1)
			fmt->dtsInfo.WROT_baseAddr = fmt->map_base[i].base;
		fmt_debug(0, "base[%d]: pa:0x%lx va:0x%lx 0x%lx",
			i, fmt->map_base[i].base, fmt->map_base[i].va,
			fmt->map_base[i].len);
	}

	ret = alloc_chrdev_region(&fmt->fmt_devno, 0, 1,
						fmt->fmtname);
	if (ret < 0) {
		fmt_err(
			"alloc_chrdev_region failed (%d)", ret);
		goto err_alloc;
	}

	fmt->fmt_cdev = cdev_alloc();
	fmt->fmt_cdev->owner = THIS_MODULE;
	fmt->fmt_cdev->ops = &vdec_fmt_fops;

	ret = cdev_add(fmt->fmt_cdev,
		fmt->fmt_devno, 1);
	if (ret < 0) {
		fmt_err("cdev_add fail (ret=%d)", ret);
		goto err_add;
	}

	fmt->fmt_class = class_create(THIS_MODULE,
							fmt->fmtname);
	if (IS_ERR_OR_NULL(fmt->fmt_class) == true) {
		ret = (int)PTR_ERR(fmt->fmt_class);
		fmt_err("class create fail (ret=%d)", ret);
		goto err_add;
	}

	fmt->fmt_device =
		device_create(fmt->fmt_class,
					NULL,
					fmt->fmt_devno,
					NULL, fmt->fmtname);
	if (IS_ERR_OR_NULL(fmt->fmt_device) == true) {
		ret = (int)PTR_ERR(fmt->fmt_device);
		fmt_err("device_create fail (ret=%d)", ret);
		goto err_device;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,fmt_gce_th_num",
						&fmt->gce_th_num);
	fmt->dtsInfo.pipeNum = fmt->gce_th_num;
	fmt_debug(0, "gce_th_num %d", fmt->gce_th_num);

	if (ret != 0 || fmt->gce_th_num > FMT_CORE_NUM)
		fmt->gce_th_num = 1;

	ret = of_property_read_u32(dev->of_node, "mediatek,fmt_rdma_swwa",
						&rdma_swwa);
	fmt_debug(0, "rdma_swwa %d", rdma_swwa);
	fmt->dtsInfo.RDMA_needWA = (bool) rdma_swwa;
	if (fmt->dtsInfo.RDMA_needWA)
		fmt_debug(0, "RDMA need software workaround");

	for (i = 0; i < ARRAY_SIZE(fmt->gce_gpr); i++) {
		ret = of_property_read_u32_index(dev->of_node, "gce-gpr",
				i, &fmt->gce_gpr[i]);
		if (ret < 0)
			break;
	}

	fmt->clt_base = cmdq_register_device(dev);
	for (i = 0; i < fmt->gce_th_num; i++) {
		fmt->clt_fmt[i] = cmdq_mbox_create(dev, i);
		fmt_debug(0, "clt_fmt[%d] %p", i, fmt->clt_fmt[i]);
	}

	if (IS_ERR_OR_NULL(fmt->clt_fmt[0]))
		goto err_device;

	of_property_read_u16(dev->of_node,
						"rdma0_sw_rst_done_eng",
						&fmt->gce_codec_eid[FMT_RDMA0_SW_RST_DONE_ENG]);
	of_property_read_u16(dev->of_node,
						"rdma0_tile_done",
						&fmt->gce_codec_eid[FMT_RDMA0_TILE_DONE]);
	of_property_read_u16(dev->of_node,
						"wdma0_sw_rst_done_eng",
						&fmt->gce_codec_eid[FMT_WDMA0_SW_RST_DONE_ENG]);
	of_property_read_u16(dev->of_node,
						"wdma0_tile_done",
						&fmt->gce_codec_eid[FMT_WDMA0_TILE_DONE]);
	of_property_read_u16(dev->of_node,
						"rdma1_sw_rst_done_eng",
						&fmt->gce_codec_eid[FMT_RDMA1_SW_RST_DONE_ENG]);
	of_property_read_u16(dev->of_node,
						"rdma1_tile_done",
						&fmt->gce_codec_eid[FMT_RDMA1_TILE_DONE]);
	of_property_read_u16(dev->of_node,
						"wdma1_sw_rst_done_eng",
						&fmt->gce_codec_eid[FMT_WDMA1_SW_RST_DONE_ENG]);
	of_property_read_u16(dev->of_node,
						"wdma1_tile_done",
						&fmt->gce_codec_eid[FMT_WDMA1_TILE_DONE]);

	for (i = 0; i < (int)FMT_CORE_NUM; i++) {
		fmt->gce_cmds[i] = devm_kzalloc(dev,
		sizeof(struct gce_cmds), GFP_KERNEL);
		if (fmt->gce_cmds[i] == NULL)
			goto err_device;
	}

	for (i = 0; i < GCE_EVENT_MAX; i++)
		fmt_debug(0, "gce event %d id %d", i, fmt->gce_codec_eid[i]);

	for (i = 0; i < FMT_INST_MAX; i++) {
		fmt->gce_task[i].pkt_ptr = NULL;
		fmt->gce_task[i].used = 0;
		fmt->gce_task[i].identifier = 0;
	}

	for (i = 0; i < fmt->gce_th_num; i++) {
		sema_init(&fmt->fmt_sem[i], 1);
		mutex_init(&fmt->mux_gce_th[i]);
	}
	mutex_init(&fmt->mux_fmt);
	mutex_init(&fmt->mux_task);

	for (i = 0; i < fmt->gce_th_num; i++)
		fmt->gce_status[i] = GCE_NONE;

	for (i = 0; i < fmt->gce_th_num; i++)
		atomic_set(&fmt->gce_job_cnt[i], 0);

	fmt_prepare_dvfs_emi_bw(fmt);
	fmt->pm_notifier.notifier_call = vdec_fmt_suspend_notifier;
	register_pm_notifier(&fmt->pm_notifier);
	fmt->is_entering_suspend = 0;

	fmt_init_pm(fmt);

	ret = fmt_sync_device_init();
	if (ret != 0)
		fmt_debug(0, "fmt_sync init failed");

	larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", 0);
	if (!larbnode) {
		fmt_debug(0, "fail to get larbnode");
		return -1;
	}
	larbdev = of_find_device_by_node(larbnode);
	if (WARN_ON(!larbdev)) {
		of_node_put(larbnode);
		fmt_debug(0, "fail to get larbdev");
		return -1;
	}
	fmt->fmtLarb = &larbdev->dev;
	fmt_debug(0, "get larb from node");

	for (i = 0; i < FMT_PORT_NUM; i++) {
		fmt->fmt_m4u_ports[i] = 0;

		ret = of_property_read_u32_index(pdev->dev.of_node,
			"m4u-ports", i, &port_id);
		if (ret) {
			fmt_debug(0, "get m4u port index: %d fail %d",
			i, ret);
			continue;
		}

		fmt->fmt_m4u_ports[i] = (int)port_id;
		fmt_debug(0, "fmt_m4u_ports[%d]=0x%x",
		i, fmt->fmt_m4u_ports[i]);
	}

	fmt_translation_fault_callback_setting(fmt);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret) {
		fmt_debug(0, "64-bit DMA enable failed\n");
		return ret;
	}

	atomic_set(&fmt->fmt_error, 0);

	fmt_debug(0, "initialization completed");
	return 0;

err_device:
	class_destroy(fmt->fmt_class);
err_add:
	cdev_del(fmt->fmt_cdev);
err_alloc:
	unregister_chrdev_region(fmt->fmt_devno, 1);

	return ret;
}

static const struct of_device_id vdec_fmt_match[] = {
	{.compatible = "mediatek-vdec-fmt",},
	{}
};


static int vdec_fmt_remove(struct platform_device *pdev)
{
	struct mtk_vdec_fmt *fmt = platform_get_drvdata(pdev);

	fmt_unprepare_dvfs_emi_bw();
	device_destroy(fmt->fmt_class, fmt->fmt_devno);
	class_destroy(fmt->fmt_class);
	cdev_del(fmt->fmt_cdev);
	unregister_chrdev_region(fmt->fmt_devno, 1);

	return 0;
}

static const struct dev_pm_ops vdec_fmt_pm_ops = {
	.suspend = vdec_fmt_suspend,
	.resume = vdec_fmt_resume,
};

static struct platform_driver vdec_fmt_driver = {
	.probe  = vdec_fmt_probe,
	.remove = vdec_fmt_remove,
	.driver = {
		.name   = VDEC_FMT_DEVNAME,
		.owner  = THIS_MODULE,
		.pm = &vdec_fmt_pm_ops,
		.of_match_table = vdec_fmt_match,
	},
};

module_platform_driver(vdec_fmt_driver);
MODULE_LICENSE("GPL");
