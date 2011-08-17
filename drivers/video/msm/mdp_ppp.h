/* drivers/video/msm/mdp_ppp.h
 *
 * Copyright (C) 2009 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VIDEO_MSM_MDP_PPP_H_
#define _VIDEO_MSM_MDP_PPP_H_

#include <linux/types.h>

struct ppp_regs {
	uint32_t src0;
	uint32_t src1;
	uint32_t dst0;
	uint32_t dst1;
	uint32_t src_cfg;
	uint32_t dst_cfg;
	uint32_t src_pack;
	uint32_t dst_pack;
	uint32_t src_rect;
	uint32_t dst_rect;
	uint32_t src_ystride;
	uint32_t dst_ystride;
	uint32_t op;
	uint32_t src_bpp;
	uint32_t dst_bpp;
	uint32_t edge;
	uint32_t phasex_init;
	uint32_t phasey_init;
	uint32_t phasex_step;
	uint32_t phasey_step;

	uint32_t bg0;
	uint32_t bg1;
	uint32_t bg_cfg;
	uint32_t bg_bpp;
	uint32_t bg_pack;
	uint32_t bg_ystride;

#ifdef CONFIG_MSM_MDP31
	uint32_t src_xy;
	uint32_t src_img_sz;
	uint32_t dst_xy;
	uint32_t bg_xy;
	uint32_t bg_img_sz;
	uint32_t bg_alpha_sel;

	uint32_t scale_cfg;
	uint32_t csc_cfg;
#endif
};

struct mdp_info;
struct mdp_rect;
struct mdp_blit_req;

void mdp_ppp_init_scale(const struct mdp_info *mdp);
int mdp_ppp_cfg_scale(const struct mdp_info *mdp, struct ppp_regs *regs,
		      struct mdp_rect *src_rect, struct mdp_rect *dst_rect,
		      uint32_t src_format, uint32_t dst_format);
int mdp_ppp_load_blur(const struct mdp_info *mdp);

#ifndef CONFIG_MSM_MDP31
int mdp_ppp_cfg_edge_cond(struct mdp_blit_req *req, struct ppp_regs *regs);
#else
static inline int mdp_ppp_cfg_edge_cond(struct mdp_blit_req *req,
				 struct ppp_regs *regs)
{
	return 0;
}
#endif

#endif /* _VIDEO_MSM_MDP_PPP_H_ */
