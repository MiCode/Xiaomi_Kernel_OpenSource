/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/seq_file.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_debug.h"

#define BUF_DUMP_LAST_N 10

static struct debug_bus dbg_bus_8994[] = {
	/* VIG QSEED */
	{ 0x298, 4, 0},
	{ 0x298, 4, 1},
	{ 0x298, 24, 0},
	{ 0x298, 24, 1},
	{ 0x298, 42, 0},
	{ 0x298, 42, 1},
	{ 0x298, 88, 0},
	{ 0x298, 88, 1},
	/* RGB SCALE */
	{ 0x298, 12, 0},
	{ 0x298, 12, 1},
	{ 0x298, 34, 0},
	{ 0x298, 34, 1},
	{ 0x298, 52, 0},
	{ 0x298, 52, 1},
	{ 0x298, 96, 0},
	{ 0x298, 96, 1},
	/* VIG CSC */
	{ 0x298, 5, 0},
	{ 0x298, 5, 1},
	{ 0x298, 25, 0},
	{ 0x298, 25, 1},
	{ 0x298, 43, 0},
	{ 0x298, 43, 1},
	{ 0x298, 89, 0},
	{ 0x298, 89, 1},
	/* VIG SPA */
	{ 0x298, 6, 0},
	{ 0x298, 26, 0},
	{ 0x298, 44, 0},
	{ 0x298, 90, 0},
	/* DSPP_PA */
	{ 0x348, 13, 0},
	{ 0x348, 19, 0},
	{ 0x348, 25, 0},
	{ 0x348, 3, 0},
	/* VIG ISC */
	{ 0x298, 7, 0},
	{ 0x298, 7, 1},
	{ 0x298, 7, 3},
	{ 0x298, 27, 0},
	{ 0x298, 27, 1},
	{ 0x298, 27, 3},
	{ 0x298, 45, 0},
	{ 0x298, 45, 1},
	{ 0x298, 45, 3},
	{ 0x298, 91, 0},
	{ 0x298, 91, 1},
	{ 0x298, 91, 3},
	/* RGB IGC */
	{ 0x298, 13, 0},
	{ 0x298, 13, 1},
	{ 0x298, 13, 3},
	{ 0x298, 35, 0},
	{ 0x298, 35, 1},
	{ 0x298, 35, 3},
	{ 0x298, 53, 0},
	{ 0x298, 53, 1},
	{ 0x298, 53, 3},
	{ 0x298, 97, 0},
	{ 0x298, 97, 1},
	{ 0x298, 97, 3},
	/* DMA IGC */
	{ 0x298, 58, 0},
	{ 0x298, 58, 1},
	{ 0x298, 58, 3},
	{ 0x298, 65, 0},
	{ 0x298, 65, 1},
	{ 0x298, 65, 3},
	/* DSPP IGC */
	{ 0x348, 14, 0},
	{ 0x348, 14, 1},
	{ 0x348, 14, 3},
	{ 0x348, 20, 0},
	{ 0x348, 20, 1},
	{ 0x348, 20, 3},
	{ 0x348, 26, 0},
	{ 0x348, 26, 1},
	{ 0x348, 26, 3},
	{ 0x348, 4, 0},
	{ 0x348, 4, 1},
	{ 0x348, 4, 3},
	/* DSPP DITHER */
	{ 0x348, 18, 1},
	{ 0x348, 24, 1},
	{ 0x348, 30, 1},
	{ 0x348, 8, 1},
	/*PPB 0*/
	{ 0x348, 31, 0},
	{ 0x348, 33, 0},
	{ 0x348, 35, 0},
	{ 0x348, 42, 0},
	/*PPB 1*/
	{ 0x348, 32, 0},
	{ 0x348, 34, 0},
	{ 0x348, 36, 0},
	{ 0x348, 43, 0},
};

void mdss_mdp_hw_rev_debug_caps_init(struct mdss_data_type *mdata)
{
	mdata->dbg_bus = NULL;
	mdata->dbg_bus_size = 0;

	switch (mdata->mdp_rev) {
	case MDSS_MDP_HW_REV_105:
	case MDSS_MDP_HW_REV_109:
	case MDSS_MDP_HW_REV_110:
		mdata->dbg_bus = dbg_bus_8994;
		mdata->dbg_bus_size = ARRAY_SIZE(dbg_bus_8994);
		break;
	default:
		break;
	}

	return;
}

static void __print_time(char *buf, u32 size, u64 ts)
{
	unsigned long rem_ns = do_div(ts, NSEC_PER_SEC);

	snprintf(buf, size, "%llu.%06lu", ts, rem_ns);
}

static void __print_buf(struct seq_file *s, struct mdss_mdp_data *buf,
		bool show_pipe)
{
	char tmpbuf[20];
	const char const *stmap[] = {
		[MDP_BUF_STATE_UNUSED]  = "UNUSED ",
		[MDP_BUF_STATE_READY]   = "READY  ",
		[MDP_BUF_STATE_ACTIVE]  = "ACTIVE ",
		[MDP_BUF_STATE_CLEANUP] = "CLEANUP",
	};

	seq_puts(s, "\t");
	if (show_pipe && buf->last_pipe)
		seq_printf(s, "pnum=%d ", buf->last_pipe->num);

	seq_printf(s, "state=%s addr=%pa size=%lu ",
		buf->state < ARRAY_SIZE(stmap) && stmap[buf->state] ?
			stmap[buf->state] : "?",
		&buf->p[0].addr, buf->p[0].len);

	if (buf->state != MDP_BUF_STATE_UNUSED)
		seq_printf(s, "ihdl=0x%p ", buf->p[0].srcp_ihdl);

	__print_time(tmpbuf, sizeof(tmpbuf), buf->last_alloc);
	seq_printf(s, "alloc_time=%s ", tmpbuf);
	if (buf->state == MDP_BUF_STATE_UNUSED) {
		__print_time(tmpbuf, sizeof(tmpbuf), buf->last_freed);
		seq_printf(s, "freed_time=%s ", tmpbuf);
	}
	seq_puts(s, "\n");
}

static void __dump_pipe(struct seq_file *s, struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_data *buf;
	int format;
	int smps[4];

	seq_printf(s, "\nSSPP #%d type=%s ndx=%x flags=0x%08x play_cnt=%u\n",
			pipe->num, mdss_mdp_pipetype2str(pipe->type),
			pipe->ndx, pipe->flags, pipe->play_cnt);
	seq_printf(s, "\tstage=%d alpha=0x%x transp=0x%x blend_op=%d\n",
			pipe->mixer_stage, pipe->alpha,
			pipe->transp, pipe->blend_op);

	format = pipe->src_fmt->format;
	seq_printf(s, "\tsrc w=%d h=%d format=%d (%s)\n",
			pipe->img_width, pipe->img_height, format,
			mdss_mdp_format2str(format));
	seq_printf(s, "\tsrc_rect x=%d y=%d w=%d h=%d H.dec=%d V.dec=%d\n",
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->horz_deci, pipe->vert_deci);
	seq_printf(s, "\tdst_rect x=%d y=%d w=%d h=%d\n",
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	smps[0] = bitmap_weight(pipe->smp_map[0].allocated,
			MAX_DRV_SUP_MMB_BLKS);
	smps[1] = bitmap_weight(pipe->smp_map[1].allocated,
			MAX_DRV_SUP_MMB_BLKS);
	smps[2] = bitmap_weight(pipe->smp_map[0].reserved,
			MAX_DRV_SUP_MMB_BLKS);
	smps[3] = bitmap_weight(pipe->smp_map[1].reserved,
			MAX_DRV_SUP_MMB_BLKS);

	seq_printf(s, "\tSMP allocated=[%d %d] reserved=[%d %d]\n",
			smps[0], smps[1], smps[2], smps[3]);

	seq_puts(s, "Data:\n");

	list_for_each_entry(buf, &pipe->buf_queue, pipe_list)
		__print_buf(s, buf, false);
}

static void __dump_mixer(struct seq_file *s, struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe;
	int i, cnt = 0;

	if (!mixer)
		return;

	seq_printf(s, "\n%s Mixer #%d  res=%dx%d  %s\n",
			mixer->type != MDSS_MDP_MIXER_TYPE_WRITEBACK ?
			(mixer->type != MDSS_MDP_MIXER_TYPE_INTF ?
			 "Intf without DSPP" : "Intf") : "Writeback",
			mixer->num, mixer->width, mixer->height,
			mixer->cursor_enabled ? "w/cursor" : "");

	for (i = 0; i < ARRAY_SIZE(mixer->stage_pipe); i++) {
		pipe = mixer->stage_pipe[i];
		if (pipe) {
			__dump_pipe(s, pipe);
			cnt++;
		}
	}

	seq_printf(s, "\nTotal pipes=%d\n", cnt);
}

static void __dump_buf_data(struct seq_file *s, struct msm_fb_data_type *mfd)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_data *buf;
	int i = 0;

	seq_printf(s, "List of buffers for fb%d\n", mfd->index);

	mutex_lock(&mdp5_data->list_lock);
	if (!list_empty(&mdp5_data->bufs_used)) {
		seq_puts(s, " Buffers used:\n");
		list_for_each_entry(buf, &mdp5_data->bufs_used, buf_list)
			__print_buf(s, buf, true);
	}

	if (!list_empty(&mdp5_data->bufs_freelist)) {
		seq_puts(s, " Buffers in free list:\n");
		list_for_each_entry(buf, &mdp5_data->bufs_freelist, buf_list)
			__print_buf(s, buf, true);
	}

	if (!list_empty(&mdp5_data->bufs_pool)) {
		seq_printf(s, " Last %d buffers used:\n", BUF_DUMP_LAST_N);

		list_for_each_entry_reverse(buf, &mdp5_data->bufs_pool,
				buf_list) {
			if (buf->last_freed == 0 || i == BUF_DUMP_LAST_N)
				break;
			__print_buf(s, buf, true);
			i++;
		}
	}
	mutex_unlock(&mdp5_data->list_lock);
}

static void __dump_timings(struct seq_file *s, struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return;

	pinfo = &ctl->panel_data->panel_info;
	seq_printf(s, "Panel #%d %dx%dp%d\n",
			pinfo->pdest, pinfo->xres, pinfo->yres,
			mdss_panel_get_framerate(pinfo));
	seq_printf(s, "\tvbp=%d vfp=%d vpw=%d hbp=%d hfp=%d hpw=%d\n",
			pinfo->lcdc.v_back_porch,
			pinfo->lcdc.v_front_porch,
			pinfo->lcdc.v_pulse_width,
			pinfo->lcdc.h_back_porch,
			pinfo->lcdc.h_front_porch,
			pinfo->lcdc.h_pulse_width);

	if (pinfo->lcdc.border_bottom || pinfo->lcdc.border_top ||
			pinfo->lcdc.border_left ||
			pinfo->lcdc.border_right) {
		seq_printf(s, "\tborder (l,t,r,b):[%d,%d,%d,%d] off xy:%d,%d\n",
				pinfo->lcdc.border_left,
				pinfo->lcdc.border_top,
				pinfo->lcdc.border_right,
				pinfo->lcdc.border_bottom,
				ctl->border_x_off,
				ctl->border_y_off);
	}
}

static void __dump_ctl(struct seq_file *s, struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_perf_params *perf;
	if (!mdss_mdp_ctl_is_power_on(ctl))
		return;

	seq_printf(s, "\n--[ Control path #%d - ", ctl->num);

	if (ctl->panel_data) {
		struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

		seq_printf(s, "%s%s]--\n",
			sctl && sctl->panel_data ? "DUAL " : "",
			mdss_panel2str(ctl->panel_data->panel_info.type));
		__dump_timings(s, ctl);
		__dump_timings(s, sctl);
	} else {
		struct mdss_mdp_mixer *mixer;
		mixer = ctl->mixer_left;
		if (mixer) {
			seq_printf(s, "%s%d",
					(mixer->rotator_mode ? "rot" : "wb"),
					mixer->num);
		} else {
			seq_puts(s, "unknown");
		}
		seq_puts(s, "]--\n");
	}
	perf = &ctl->cur_perf;
	seq_printf(s, "MDP Clk=%u  Final BW=%llu\n",
			perf->mdp_clk_rate,
			perf->bw_ctl);
	seq_printf(s, "Play Count=%u  Underrun Count=%u\n",
			ctl->play_cnt, ctl->underrun_cnt);

	__dump_mixer(s, ctl->mixer_left);
	__dump_mixer(s, ctl->mixer_right);
}

static int __dump_mdp(struct seq_file *s, struct mdss_data_type *mdata)
{
	struct mdss_mdp_ctl *ctl;
	int i, ignore_ndx = -1;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		/* ignore slave ctl in split display case */
		if (ctl->num == ignore_ndx)
			continue;
		if (ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
			ignore_ndx = ctl->mixer_right->ctl->num;
		__dump_ctl(s, ctl);
	}
	return 0;
}

static int __dump_buffers(struct seq_file *s, struct mdss_data_type *mdata)
{
	struct mdss_mdp_ctl *ctl;
	int i, ignore_ndx = -1;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		/* ignore slave ctl in split display case */
		if (ctl->num == ignore_ndx)
			continue;
		if (ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
			ignore_ndx = ctl->mixer_right->ctl->num;

		if (ctl->mfd)
			__dump_buf_data(s, ctl->mfd);
	}
	return 0;
}

#define DUMP_CHUNK 256
#define DUMP_SIZE SZ_32K
void mdss_mdp_dump(struct mdss_data_type *mdata)
{
	struct seq_file s = {
		.size = DUMP_SIZE - 1,
	};
	int i;

	s.buf = kzalloc(DUMP_SIZE, GFP_KERNEL);
	if (!s.buf)
		return;

	__dump_mdp(&s, mdata);
	seq_puts(&s, "\n");

	pr_info("MDP DUMP\n------------------------\n");
	for (i = 0; i < s.count; i += DUMP_CHUNK) {
		if ((s.count - i) > DUMP_CHUNK) {
			char c = s.buf[i + DUMP_CHUNK];
			s.buf[i + DUMP_CHUNK] = 0;
			pr_cont("%s", s.buf + i);
			s.buf[i + DUMP_CHUNK] = c;
		} else {
			s.buf[s.count] = 0;
			pr_cont("%s", s.buf + i);
		}
	}

	kfree(s.buf);
}

#ifdef CONFIG_DEBUG_FS
static int mdss_debugfs_dump_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;

	return __dump_mdp(s, mdata);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_dump);

static int mdss_debugfs_buffers_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;

	return __dump_buffers(s, mdata);
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_buffers);

static void __stats_ctl_dump(struct mdss_mdp_ctl *ctl, struct seq_file *s)
{
	if (!ctl->ref_cnt)
		return;

	if (ctl->intf_num) {
		seq_printf(s, "intf%d: play: %08u \t",
				ctl->intf_num, ctl->play_cnt);
		seq_printf(s, "vsync: %08u \tunderrun: %08u\n",
				ctl->vsync_cnt, ctl->underrun_cnt);
		if (ctl->mfd) {
			seq_printf(s, "user_bl: %08u \tmod_bl: %08u\n",
				ctl->mfd->bl_level, ctl->mfd->bl_level_scaled);
		}
	} else {
		seq_printf(s, "wb: \tmode=%x \tplay: %08u\n",
				ctl->opmode, ctl->play_cnt);
	}
}

static int mdss_debugfs_stats_show(struct seq_file *s, void *v)
{
	struct mdss_data_type *mdata = (struct mdss_data_type *)s->private;
	struct mdss_mdp_pipe *pipe;
	int i;

	seq_puts(s, "\nmdp:\n");

	for (i = 0; i < mdata->nctl; i++)
		__stats_ctl_dump(mdata->ctl_off + i, s);
	seq_puts(s, "\n");

	for (i = 0; i < mdata->nvig_pipes; i++) {
		pipe = mdata->vig_pipes + i;
		seq_printf(s, "VIG%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	for (i = 0; i < mdata->nrgb_pipes; i++) {
		pipe = mdata->rgb_pipes + i;
		seq_printf(s, "RGB%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	for (i = 0; i < mdata->ndma_pipes; i++) {
		pipe = mdata->dma_pipes + i;
		seq_printf(s, "DMA%d :   %08u\t", i, pipe->play_cnt);
	}
	seq_puts(s, "\n");

	return 0;
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdss_debugfs_stats);

int mdss_mdp_debugfs_init(struct mdss_data_type *mdata)
{
	struct mdss_debug_data *mdd;

	if (!mdata)
		return -ENODEV;

	mdd = mdata->debug_inf.debug_data;
	if (!mdd)
		return -ENOENT;

	debugfs_create_file("dump", 0644, mdd->root, mdata,
			&mdss_debugfs_dump_fops);
	debugfs_create_file("buffers", 0644, mdd->root, mdata,
			&mdss_debugfs_buffers_fops);
	debugfs_create_file("stat", 0644, mdd->root, mdata,
			&mdss_debugfs_stats_fops);
	debugfs_create_bool("serialize_wait4pp", 0644, mdd->root,
		(u32 *)&mdata->serialize_wait4pp);

	return 0;
}
#endif
