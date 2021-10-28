// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"

#define SYS_SW0_RST_B_REG	0x700
#define SYS_SW1_RST_B_REG	0x704
#define SYS_AID_SEL		0xfa8

#define MML_MAX_SYS_COMPONENTS	10
#define MML_MAX_SYS_MUX_PINS	88
#define MML_MAX_SYS_DL_RELAYS	4
#define MML_MAX_SYS_DBG_REGS	60

int mml_ir_loop = 1;
module_param(mml_ir_loop, int, 0644);

int mml_racing_fast = 1;
module_param(mml_racing_fast, int, 0644);

int mml_racing_sleep = 16000;
module_param(mml_racing_sleep, int, 0644);

enum mml_comp_type {
	MML_CT_COMPONENT = 0,
	MML_CT_SYS,
	MML_CT_PATH,
	MML_CT_DL_IN,
	MML_CT_DL_OUT,

	MML_COMP_TYPE_TOTAL
};

struct mml_sys;

struct mml_data {
	int (*comp_inits[MML_COMP_TYPE_TOTAL])(struct device *dev,
		struct mml_sys *sys, struct mml_comp *comp);
	const struct mtk_ddp_comp_funcs *ddp_comp_funcs[MML_COMP_TYPE_TOTAL];
	u8 gpr[MML_PIPE_CNT];
};

enum mml_mux_type {
	MML_MUX_UNUSED = 0,
	MML_MUX_MOUT,
	MML_MUX_SOUT,
	MML_MUX_SELIN,
};

struct mml_mux_pin {
	u16 index;
	u16 from;
	u16 to;
	u16 type;
	u16 offset;
} __attribute__ ((__packed__));

struct mml_dbg_reg {
	const char *name;
	u32 offset;
};

struct mml_sys {
	/* Device data and component bindings */
	const struct mml_data *data;
	struct mtk_ddp_comp ddp_comps[MML_MAX_SYS_COMPONENTS];
	/* DDP component flags */
	u32 ddp_comp_en;
	/* DDP component bound index */
	u32 ddp_bound;
	struct mml_comp comps[MML_MAX_SYS_COMPONENTS];
	/* MML component count */
	u32 comp_cnt;
	/* MML component bound count */
	u32 comp_bound;

	/* MML multiplexer pins.
	 * The entry 0 leaves empty for efficiency, do not use. */
	struct mml_mux_pin mux_pins[MML_MAX_SYS_MUX_PINS + 1];
	u32 mux_cnt;
	u16 dl_relays[MML_MAX_SYS_DL_RELAYS + 1];
	u32 dl_cnt;

	/* Table of component or adjacency data index.
	 *
	 * The element is an index to data arrays.
	 * The component data by type is indexed by adjacency[id][id];
	 * in the upper-right tri. are MOUTs and SOUTs by adjacency[from][to];
	 * in the bottom-left tri. are SELIN pins by adjacency[to][from].
	 * Direct-wires are not in this table.
	 *
	 * Ex.:
	 *	dl_relays[adjacency[DLI0][DLI0]] is RELAY of comp DLI0.
	 *	mux_pins[adjacency[RDMA0][RSZ0]] is MOUT from RDMA0 to RSZ0.
	 *	mux_pins[adjacency[RSZ0][RDMA0]] is SELIN from RDMA0 to RSZ0.
	 *
	 * Array data would be like:
	 *	[0] = { T  indices of },	T: indices of component data
	 *	[1] = { . T . MOUTs & },	   (by component type, the
	 *	[2] = { . . T . SOUTs },	    indices refer to different
	 *	[3] = { . . . T . . . },	    data arrays.)
	 *	[4] = { . . . . T . . },
	 *	[5] = { indices . T . },
	 *	[6] = { of SELINs . T },
	 */
	u8 adjacency[MML_MAX_COMPONENTS][MML_MAX_COMPONENTS];
	struct mml_dbg_reg dbg_regs[MML_MAX_SYS_DBG_REGS];
	u32 dbg_reg_cnt;

	/* store the bit to enable aid_sel for specific component */
	u8 aid_sel[MML_MAX_COMPONENTS];

	/* register for racing mode select ready signal */
	u16 inline_ready_sel;

	/* component master device, i.e., mml driver device */
	struct device *master;
	/* adaptor for display addon config */
	struct mml_dle_ctx *dle_ctx;
};

struct sys_frame_data {
	/* instruction offset to start of racing loop (tile 0) in pkt */
	u32 racing_tile0_offset;
	/* instruction offset to assign jump target PA to racing_tile0_offset */
	u32 racing_tile0_jump;

	/* instruction offset to skip sync events */
	u32 racing_skip_offset;
	/* instruction offset to assign jump target PA to racing_skip_offset */
	u32 racing_skip_jump;
};

#define sys_frm_data(i)	((struct sys_frame_data *)(i->data))

static inline struct mml_sys *comp_to_sys(struct mml_comp *comp)
{
	return container_of(comp, struct mml_sys, comps[comp->sub_idx]);
}

static inline struct mml_sys *ddp_comp_to_sys(struct mtk_ddp_comp *ddp_comp)
{
	return container_of(ddp_comp, struct mml_sys, ddp_comps[ddp_comp->sub_idx]);
}

static void ddp_prepare(struct mtk_ddp_comp *ddp_comp)
{
	struct mml_comp *comp = &ddp_comp_to_sys(ddp_comp)->comps[ddp_comp->sub_idx];

	comp->hw_ops->clk_enable(comp);
}

static void ddp_unprepare(struct mtk_ddp_comp *ddp_comp)
{
	struct mml_comp *comp = &ddp_comp_to_sys(ddp_comp)->comps[ddp_comp->sub_idx];

	comp->hw_ops->clk_disable(comp);
}

u16 mml_sys_get_reg_ready_sel(struct mml_comp *comp)
{
	return comp_to_sys(comp)->inline_ready_sel;
}

static s32 sys_config_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct sys_frame_data *sys_frm;

	if (task->config->info.mode == MML_MODE_RACING) {
		/* initialize component frame data for current frame config */
		sys_frm = kzalloc(sizeof(*sys_frm), GFP_KERNEL);
		ccfg->data = sys_frm;
	}

	return 0;
}

static void sys_sync_racing(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_dev *mml = task->config->mml;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* synchronize disp and mml task by for MML pkt:
	 *	set MML_READY
	 *	wait_and_clear DISP_READY
	 */
	cmdq_pkt_set_event(pkt, mml_ir_get_mml_ready_event(mml));
	cmdq_pkt_wfe(pkt, mml_ir_get_disp_ready_event(mml));
}

static s32 sys_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 aid_sel = 0, mask = 0;
	u32 in_engine_id = path->nodes[path->tile_engines[0]].comp->id;

	if (task->config->info.mode == MML_MODE_RACING) {
		struct cmdq_operand lhs, rhs;

		cmdq_pkt_clear_event(pkt, mml_ir_get_mml_stop_event(task->config->mml));

		lhs.reg = true;
		lhs.idx = MML_CMDQ_NEXT_SPR;
		rhs.reg = false;
		rhs.value = ~(u16)MML_NEXTSPR_CONTI;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

		if (!mml_racing_fast && likely(!mml_racing_ut))
			sys_sync_racing(comp, task, ccfg);

		/* debug */
		cmdq_pkt_assign_command(pkt, MML_CMDQ_ROUND_SPR,
			MML_ROUND_SPR_INIT + 0x10000);
	}

	if (task->config->info.src.secure)
		aid_sel |= 1 << sys->aid_sel[in_engine_id];
	mask |= 1 << sys->aid_sel[in_engine_id];
	if (task->config->info.dest[0].data.secure)
		aid_sel |= 1 << sys->aid_sel[path->out_engine_ids[0]];
	mask |= 1 << sys->aid_sel[path->out_engine_ids[0]];
	if (task->config->info.dest_cnt > 1) {
		if (task->config->info.dest[1].data.secure)
			aid_sel |= 1 << sys->aid_sel[path->out_engine_ids[1]];
		mask |= 1 << sys->aid_sel[path->out_engine_ids[1]];
	}

	cmdq_pkt_write(pkt, NULL, comp->base_pa + SYS_AID_SEL, aid_sel, mask);

	return 0;
}

static void config_mux(struct mml_sys *sys, struct cmdq_pkt *pkt,
		       const phys_addr_t base_pa, u8 mux_idx, u8 sof_grp,
		       u16 *offset, u32 *mout)
{
	struct mml_mux_pin *mux;

	if (!mux_idx)
		return;
	mux = &sys->mux_pins[mux_idx];

	switch (mux->type) {
	case MML_MUX_MOUT:
		*offset = mux->offset;
		*mout |= 1 << mux->index;
		break;
	case MML_MUX_SOUT:
	case MML_MUX_SELIN:
		cmdq_pkt_write(pkt, NULL, base_pa + mux->offset,
			       mux->index + (sof_grp << 8), U32_MAX);
		break;
	default:
		break;
	}
}

static s32 sys_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_sys *sys = comp_to_sys(comp);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	u8 sof_grp = path->mux_group;
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	u32 i, j;

	if (task->config->info.mode == MML_MODE_RACING && !sys_frm->racing_tile0_offset) {
		struct cmdq_operand lhs, rhs;

		cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX2, MML_ROUND_SPR_INIT);

		sys_frm->racing_tile0_offset = pkt->cmd_buf_size;

		lhs.reg = true;
		lhs.idx = CMDQ_THR_SPR_IDX2;
		rhs.reg = false;
		rhs.value = 1;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, CMDQ_THR_SPR_IDX2, &lhs, &rhs);
	}

	for (i = 0; i < path->node_cnt; i++) {
		const struct mml_path_node *node = &path->nodes[i];
		u16 offset = 0;
		u32 mout = 0;
		u8 from = node->id, to, mux_idx;

		/* TODO: continue if node disabled */
		for (j = 0; j < ARRAY_SIZE(node->next); j++) {
			if (node->next[j]) {	/* && next enabled */
				to = node->next[j]->id;
				mux_idx = sys->adjacency[from][to];
				config_mux(sys, pkt, base_pa, mux_idx, sof_grp,
					   &offset, &mout);
				mux_idx = sys->adjacency[to][from];
				config_mux(sys, pkt, base_pa, mux_idx, sof_grp,
					   &offset, &mout);
			}
		}
		if (mout)
			cmdq_pkt_write(pkt, NULL, base_pa + offset,
				       mout + (sof_grp << 8), U32_MAX);
	}
	return 0;
}

static void sys_racing_addr_update(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	u32 *inst;

	if (task->config->disp_vdo && likely(mml_racing_ut != 1) && mml_ir_loop) {
		inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, sys_frm->racing_tile0_jump);
		*inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_pa_by_offset(pkt,
			sys_frm->racing_tile0_offset));
	}

	if (mml_racing_fast) {
		inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, sys_frm->racing_skip_jump);
		*inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_pa_by_offset(pkt,
			sys_frm->racing_skip_offset));
	}
}

static void sys_racing_loop(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct cmdq_operand lhs, rhs;

	if (!task->config->disp_vdo)
		return;

	if (unlikely(mml_racing_ut == 1))
		return;

	if (unlikely(!mml_ir_loop))
		return;

	if (unlikely(mml_racing_ut))
		cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(mml_racing_sleep),
			sys->data->gpr[ccfg->pipe]);

	/* do eoc to avoid task timeout during self-loop */
	if (likely(!mml_racing_timeout) && likely(!mml_racing_wdone_eoc))
		cmdq_pkt_eoc(pkt, false);

	/* reserve assign inst for jump addr */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	sys_frm->racing_tile0_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* get the MML_NEXTSPR_NEXT bit first
	 * pseudo:
	 *	SPR1 = NEXT_SPR & MML_NEXTSPR_NEXT
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_THR_SPR_IDX1, &lhs, &rhs);

	/* loop if SPR1 is not MML_NEXTSPR_NEXT
	 * pseudo:
	 *	if SPR1 != MML_NEXTSPR_NEXT:
	 *		jump CONFIG_TILE0
	 */
	lhs.reg = true;
	lhs.idx = CMDQ_THR_SPR_IDX1;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_NOT_EQUAL);

	sys_racing_addr_update(comp, task, ccfg);

	cmdq_pkt_set_event(pkt, mml_ir_get_mml_stop_event(task->config->mml));
}

bool sys_sync(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct cmdq_operand lhs, rhs;
	struct mml_tile_config *tile_cfg = &cfg->tile_output[ccfg->pipe]->tiles[idx];

	if (!tile_cfg->eol || !mml_racing_fast)
		return false;

	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	sys_frm->racing_skip_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* get the MML_NEXTSPR_CONTI bit first
	 * pseudo:
	 *	SPR1 = NEXT_SPR & MML_NEXTSPR_CONTI
	 *	if SPR1 == CONTI:
	 *		jump racing_skip_jump
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_CONTI;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_THR_SPR_IDX1, &lhs, &rhs);

	lhs.reg = true;
	lhs.idx = CMDQ_THR_SPR_IDX1;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_CONTI;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_EQUAL);

	if (likely(!mml_racing_ut))
		sys_sync_racing(comp, task, ccfg);

	/* and mark conti bit after wait sync
	 * pseudo:
	 *	NEXT_SPR = NEXT_SPR | MML_NEXTSPR_CONTI;
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_CONTI;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	sys_frm->racing_skip_offset = pkt->cmd_buf_size;

	return true;
}

static s32 sys_post(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	if (task->config->info.mode == MML_MODE_RACING)
		sys_racing_loop(comp, task, ccfg);

	return 0;
}

static s32 sys_repost(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg)
{
	if (task->config->info.mode == MML_MODE_RACING)
		sys_racing_addr_update(comp, task, ccfg);

	return 0;
}

static const struct mml_comp_config_ops sys_config_ops = {
	.prepare = sys_config_prepare,
	.frame = sys_config_frame,
	.tile = sys_config_tile,
	.sync = sys_sync,
	.post = sys_post,
	.repost = sys_repost,
};

static void sys_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_sys *sys = comp_to_sys(comp);
	u32 value;
	u32 i;

	mml_err("mml component %u dump:", comp->id);
	for (i = 0; i < sys->dbg_reg_cnt; i++) {
		value = readl(base + sys->dbg_regs[i].offset);
		mml_err("%s %#010x", sys->dbg_regs[i].name, value);
	}
}

static void sys_reset(struct mml_comp *comp, struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];

	mml_err("[sys]reset bits %#llx for pipe %u", path->reset_bits, pipe);
	if (path->reset0 != U32_MAX) {
		writel(path->reset0, comp->base + SYS_SW0_RST_B_REG);
		writel(U32_MAX, comp->base + SYS_SW0_RST_B_REG);
	}
	if (path->reset1 != U32_MAX) {
		writel(path->reset1, comp->base + SYS_SW1_RST_B_REG);
		writel(U32_MAX, comp->base + SYS_SW1_RST_B_REG);
	}
}

static const struct mml_comp_debug_ops sys_debug_ops = {
	.dump = &sys_debug_dump,
	.reset = &sys_reset,
};

static int sys_comp_init(struct device *dev, struct mml_sys *sys,
			 struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	int cnt, i;
	struct property *prop;
	const char *name;
	const __be32 *p;
	u32 value, comp_id;

	/* Initialize mux-pins */
	cnt = of_property_count_elems_of_size(node, "mux-pins",
						  sizeof(struct mml_mux_pin));
	if (cnt < 0 || cnt > MML_MAX_SYS_MUX_PINS) {
		dev_err(dev, "no mux-pins or out of size in component %s: %d\n",
			node->full_name, cnt);
		return -EINVAL;
	}

	of_property_read_u16_array(node, "mux-pins", (u16 *)&sys->mux_pins[1],
		cnt * (sizeof(struct mml_mux_pin) / sizeof(u16)));
	for (i = 1; i <= cnt; i++) {
		struct mml_mux_pin *mux = &sys->mux_pins[i];

		if (mux->from >= MML_MAX_COMPONENTS ||
		    mux->to >= MML_MAX_COMPONENTS) {
			dev_err(dev, "comp idx %hu %hu out of boundary",
				mux->from, mux->to);
			continue;
		}
		if (mux->type == MML_MUX_SELIN)
			sys->adjacency[mux->to][mux->from] = i;
		else
			sys->adjacency[mux->from][mux->to] = i;
	}

	/* Initialize dbg-regs */
	i = 0;
	of_property_for_each_u32(node, "dbg-reg-offsets", prop, p, value) {
		if (i > MML_MAX_SYS_DBG_REGS) {
			dev_err(dev, "no dbg-reg-offsets or out of size in component %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		sys->dbg_regs[i].offset = value;
		i++;
	}
	sys->dbg_reg_cnt = i;

	i = 0;
	of_property_for_each_string(node, "dbg-reg-names", prop, name) {
		if (i > sys->dbg_reg_cnt) {
			dev_err(dev, "dbg-reg-names size over offsets size %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		sys->dbg_regs[i].name = name;
		i++;
	}

	cnt = of_property_count_u32_elems(node, "aid-sel");
	for (i = 0; i + 1 < cnt; i += 2) {
		of_property_read_u32_index(node, "aid-sel", i, &comp_id);
		of_property_read_u32_index(node, "aid-sel", i + 1, &value);
		sys->aid_sel[comp_id] = (u8)value;
	}

	of_property_read_u16(dev->of_node, "ready-sel", &sys->inline_ready_sel);

	comp->config_ops = &sys_config_ops;
	comp->debug_ops = &sys_debug_ops;
	return 0;
}

static void sys_addon_config(struct mtk_ddp_comp *ddp_comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
	/* Config aid select if mml output */
	/* Config mux select for each path node and each next */
}

static const struct mtk_ddp_comp_funcs sys_ddp_funcs = {
	.addon_config = sys_addon_config,
	.prepare = ddp_prepare,
	.unprepare = ddp_unprepare,
};

static s32 dl_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u16 offset = sys->dl_relays[sys->adjacency[comp->id][comp->id]];
	u32 dl_w = tile->in.xe - tile->in.xs + 1;
	u32 dl_h = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + offset,
		       (dl_h << 16) + dl_w, U32_MAX);
	return 0;
}

static const struct mml_comp_config_ops dl_config_ops = {
	.tile = dl_config_tile,
};

static int dl_comp_init(struct device *dev, struct mml_sys *sys,
			struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	char name[32] = "";
	u16 offset;
	int ret;

	if (sys->dl_cnt >= ARRAY_SIZE(sys->dl_relays) - 1) {
		dev_err(dev, "out of dl-relay size in component %s: %d\n",
			node->full_name, sys->dl_cnt + 1);
		return -EINVAL;
	}
	if (!comp->name) {
		dev_err(dev, "no comp-name of mmlsys comp-%d (type dl-in)\n",
			comp->sub_idx);
		return -EINVAL;
	}

	ret = snprintf(name, sizeof(name), "%s-dl-relay", comp->name);
	if (ret >= sizeof(name)) {
		dev_err(dev, "len:%d over name size:%d", ret, sizeof(name));
		name[sizeof(name) - 1] = '\0';
	}
	ret = of_property_read_u16(node, name, &offset);
	if (ret) {
		dev_err(dev, "no %s property in node %s\n",
			name, node->full_name);
		return ret;
	}

	sys->dl_relays[++sys->dl_cnt] = offset;
	sys->adjacency[comp->id][comp->id] = sys->dl_cnt;
	comp->config_ops = &dl_config_ops;
	/* TODO: pmqos_op */
	return 0;
}

static void dli_addon_config(struct mtk_ddp_comp *ddp_comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
}

static const struct mtk_ddp_comp_funcs dli_ddp_funcs = {
	.addon_config = dli_addon_config,
	.prepare = ddp_prepare,
	.unprepare = ddp_unprepare,
};

static void dlo_addon_config(struct mtk_ddp_comp *ddp_comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
}

static const struct mtk_ddp_comp_funcs dlo_ddp_funcs = {
	.addon_config = dlo_addon_config,
	.prepare = ddp_prepare,
	.unprepare = ddp_unprepare,
};

static int subcomp_init(struct platform_device *pdev, struct mml_sys *sys,
			int subcomponent)
{
	struct device *dev = &pdev->dev;
	struct mml_comp *comp = &sys->comps[subcomponent];
	const struct mml_data *data = sys->data;
	u32 comp_type;
	int ret;

	ret = mml_subcomp_init(pdev, subcomponent, comp);
	if (ret)
		return ret;

	if (of_property_read_u32_index(dev->of_node, "comp-types",
				       subcomponent, &comp_type)) {
		dev_info(dev, "no comp-type of mmlsys comp-%d\n", subcomponent);
		return 0;
	}
	if (data->comp_inits[comp_type]) {
		ret = data->comp_inits[comp_type](dev, sys, comp);
		if (ret)
			return ret;
	}

	if (data->ddp_comp_funcs[comp_type]) {
		ret = mml_ddp_comp_init(dev, &sys->ddp_comps[subcomponent],
					comp, data->ddp_comp_funcs[comp_type]);
		if (unlikely(ret)) {
			mml_log("failed to init ddp comp-%d: %d",
				subcomponent, ret);
			return ret;
		}
		sys->ddp_comp_en |= 1 << subcomponent;
	}
	return ret;
}

static int mml_sys_init(struct platform_device *pdev, struct mml_sys *sys,
			const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int comp_cnt, i;
	int ret;

	sys->data = of_device_get_match_data(dev);

	/* Initialize component and subcomponents */
	comp_cnt = of_mml_count_comps(node);
	if (comp_cnt <= 0) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, comp_cnt);
		return -EINVAL;
	}

	for (i = 0; i < comp_cnt; i++) {
		ret = subcomp_init(pdev, sys, i);
		if (ret) {
			dev_err(dev, "failed to init mmlsys comp-%d: %d\n",
				i, ret);
			return ret;
		}
	}

	ret = component_add(dev, comp_ops);
	if (sys->ddp_comp_en & (1 << 0))
		ret = component_add(dev, comp_ops);
	if (ret) {
		dev_err(dev, "failed to add mmlsys comp-%d: %d\n", 0, ret);
		return ret;
	}
	for (i = 1; i < comp_cnt; i++) {
		ret = component_add_typed(dev, comp_ops, i);
		if (sys->ddp_comp_en & (1 << i))
			ret = component_add_typed(dev, comp_ops, i);
		if (ret) {
			dev_err(dev, "failed to add mmlsys comp-%d: %d\n",
				i, ret);
			goto err_comp_add;
		}
	}
	sys->comp_cnt = comp_cnt;
	return 0;

err_comp_add:
	for (; i > 0; i--) {
		component_del(dev, comp_ops);
		if (sys->ddp_comp_en & (1 << (i - 1)))
			component_del(dev, comp_ops);
	}
	return ret;
}

static struct mml_sys *dbg_probed_components[2];
static int dbg_probed_count;

struct mml_sys *mml_sys_create(struct platform_device *pdev,
			       const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct mml_sys *sys;
	int ret;

	sys = devm_kzalloc(dev, sizeof(*sys), GFP_KERNEL);
	if (!sys)
		return ERR_PTR(-ENOMEM);

	ret = mml_sys_init(pdev, sys, comp_ops);
	if (ret) {
		dev_err(dev, "failed to init mml sys: %d\n", ret);
		devm_kfree(dev, sys);
		return ERR_PTR(ret);
	}

	dbg_probed_components[dbg_probed_count++] = sys;
	return sys;
}

void mml_sys_destroy(struct platform_device *pdev, struct mml_sys *sys,
		     const struct component_ops *comp_ops)
{
	int i;

	for (i = 0; i < sys->comp_cnt; i++) {
		component_del(&pdev->dev, comp_ops);
		if (sys->ddp_comp_en & (1 << i))
			component_del(&pdev->dev, comp_ops);
	}
	devm_kfree(&pdev->dev, sys);
}

static int bind_mml(struct device *dev, struct device *master,
		    struct mml_sys *sys)
{
	s32 ret;

	if (WARN_ON(sys->master && sys->master != master)) {
		dev_err(dev, "failed to register component %s to new master %s from old %s\n",
			dev->of_node->full_name,
			master->of_node->full_name,
			sys->master->of_node->full_name);
		return -EUSERS;
	}
	sys->master = master;

	if (WARN_ON(sys->comp_bound >= sys->comp_cnt))
		return -ERANGE;
	ret = mml_register_comp(master, &sys->comps[sys->comp_bound++]);
	if (ret) {
		dev_err(dev, "failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
		sys->comp_bound--;
	}
	return ret;
}

static int bind_ddp(struct device *dev, struct drm_device *drm_dev,
		    struct mml_sys *sys)
{
	s32 i, ret;

	for (i = sys->ddp_bound; i < sys->comp_cnt; i++)
		if (sys->ddp_comp_en & (1 << i))
			break;
	sys->ddp_bound = i;

	if (WARN_ON(sys->ddp_bound >= sys->comp_cnt))
		return -ERANGE;
	ret = mml_ddp_comp_register(drm_dev, &sys->ddp_comps[sys->ddp_bound++]);
	if (ret) {
		dev_err(dev, "failed to register ddp component %s: %d\n",
			dev->of_node->full_name, ret);
		sys->ddp_bound--;
	}
	return ret;
}

int mml_sys_bind(struct device *dev, struct device *master,
		 struct mml_sys *sys, void *data)
{
	if (!data)
		return bind_mml(dev, master, sys);
	else
		return bind_ddp(dev, data, sys);
}

static void unbind_mml(struct device *master, struct mml_sys *sys)
{
	if (WARN_ON(sys->comp_bound <= 0))
		return;
	mml_unregister_comp(master, &sys->comps[--sys->comp_bound]);
}

static void unbind_ddp(struct drm_device *drm_dev, struct mml_sys *sys)
{
	s32 i;

	for (i = sys->ddp_bound; i > 0; i--)
		if (sys->ddp_comp_en & (1 << (i - 1)))
			break;
	sys->ddp_bound = i;

	if (WARN_ON(sys->ddp_bound <= 0))
		return;
	mml_ddp_comp_unregister(drm_dev, &sys->ddp_comps[--sys->ddp_bound]);
}

void mml_sys_unbind(struct device *dev, struct device *master,
		    struct mml_sys *sys, void *data)
{
	if (!data)
		unbind_mml(master, sys);
	else
		unbind_ddp(data, sys);
}

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	return mml_sys_bind(dev, master, dev_get_drvdata(dev), data);
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	mml_sys_unbind(dev, master, dev_get_drvdata(dev), data);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static int probe(struct platform_device *pdev)
{
	struct mml_sys *priv;

	priv = mml_sys_create(pdev, &mml_comp_ops);
	if (IS_ERR(priv)) {
		dev_err(&pdev->dev, "failed to init mml sys: %d\n",
			PTR_ERR(priv));
		return PTR_ERR(priv);
	}
	platform_set_drvdata(pdev, priv);
	return 0;
}

static int remove(struct platform_device *pdev)
{
	mml_sys_destroy(pdev, platform_get_drvdata(pdev), &mml_comp_ops);
	return 0;
}

static const struct mml_data mt6893_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
	},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R09},
};

static const struct mml_data mt6983_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
		[MML_CT_DL_OUT] = &dl_comp_init,
	},
	.ddp_comp_funcs = {
		[MML_CT_SYS] = &sys_ddp_funcs,
		[MML_CT_DL_IN] = &dli_ddp_funcs,
		[MML_CT_DL_OUT] = &dlo_ddp_funcs,
	},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R09},
};

static const struct mml_data mt6879_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
		[MML_CT_DL_OUT] = &dl_comp_init,
	},
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R09},
};

static const struct mml_data mt6895_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
		[MML_CT_DL_OUT] = &dl_comp_init,
	},
	.gpr = {CMDQ_GPR_R10, CMDQ_GPR_R11},
};

const struct of_device_id mtk_mml_of_ids[] = {
	{
		.compatible = "mediatek,mt6983-mml",
		.data = &mt6983_mml_data,
	},
	{
		.compatible = "mediatek,mt6893-mml",
		.data = &mt6893_mml_data,
	},
	{
		.compatible = "mediatek,mt6879-mml",
		.data = &mt6879_mml_data,
	},
	{
		.compatible = "mediatek,mt6895-mml",
		.data = &mt6895_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_of_ids);

/* Used in platform with more than one mml_sys */
static const struct of_device_id mml_sys_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml_sys",
		.data = &mt6893_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_sys_of_ids);

struct platform_driver mml_sys_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mmlsys",
		.owner = THIS_MODULE,
		.of_match_table = mml_sys_of_ids,
	},
};
//module_platform_driver(mml_sys_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i, j;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (j = 0; j < dbg_probed_count; j++) {
			struct mml_sys *sys = dbg_probed_components[j];

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] component count: %d bound: %d ddp: %d\n", j,
				sys->comp_cnt, sys->comp_bound, sys->ddp_bound);
			for (i = 0; i < sys->comp_cnt; i++) {
				struct mml_comp *comp = &sys->comps[i];
				struct mtk_ddp_comp *ddp_comp = &sys->ddp_comps[i];

				length += snprintf(buf + length, PAGE_SIZE - length,
					"    - [%d] mml comp_id: %d.%d @%08x name: %s bound: %d\n",
					i, comp->id, comp->sub_idx, comp->base_pa,
					comp->name ? comp->name : "(null)", comp->bound);
				length += snprintf(buf + length, PAGE_SIZE - length,
					"    -         larb_port: %d @%08x pw: %d clk: %d\n",
					comp->larb_port, comp->larb_base,
					comp->pw_cnt, comp->clk_cnt);
				length += snprintf(buf + length, PAGE_SIZE - length,
					"    -     ddp comp_id: %d bound: %d\n",
					ddp_comp->id,
					(sys->ddp_comp_en >> i) & 0x1);
			}
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(sys_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(sys_debug, "mml sys debug case");

MODULE_DESCRIPTION("MediaTek SoC display MMLSYS driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
