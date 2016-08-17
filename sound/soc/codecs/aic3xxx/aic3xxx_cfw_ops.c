/*
 * linux/sound/soc/codecs/aic3xxx/aic3xxx_cfw_ops.c
 *
 * Copyright (C) 2011 Texas Instruments Inc.,
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/slab.h>
#include <sound/tlv.h>

/* FIXME to be removed/replaced */
#define warn(fmt, ...)	printk(fmt "\n", ##__VA_ARGS__)
#define error(fmt, ...)	printk(fmt "\n", ##__VA_ARGS__)
#define DBG printk

#include "aic3xxx_cfw.h"
#include "aic3xxx_cfw_ops.h"


/* **Code beyond this point is compilable on host** */

/*
 * Firmware version numbers are used to make sure that the
 * host and target code stay in sync.  It is _not_ recommended
 * to provide this number from the outside (E.g., from a makefile)
 * Instead, a set of automated tools are relied upon to keep the numbers
 * in sync at the time of host testing.
 */
#undef CFW_FW_IF_ID
#define CFW_FW_IF_ID 0x3FA6D547
static int aic3xxx_cfw_dlimage(struct cfw_state *ps, struct cfw_image *pim);
static int aic3xxx_cfw_dlcfg(struct cfw_state *ps, struct cfw_image *pim);
static int aic3xxx_cfw_dlctl(struct cfw_state *ps, struct cfw_block *pb,
			     u32 mute_flags);

static void aic3xxx_cfw_dlcmds(struct cfw_state *ps, struct cfw_block *pb);
static int aic3xxx_cfw_set_mode_id(struct cfw_state *ps);
static int aic3xxx_cfw_mute(struct cfw_state *ps, int mute, u32 flags);
static int aic3xxx_cfw_setmode_cfg_u(struct cfw_state *ps, int mode, int cfg);
static int aic3xxx_cfw_setcfg_u(struct cfw_state *ps, int cfg);
static int aic3xxx_cfw_transition_u(struct cfw_state *ps, char *ttype);
static int aic3xxx_cfw_set_pll_u(struct cfw_state *ps, int asi);
static int aic3xxx_cfw_control_u(struct cfw_state *ps, char *cname, int param);
static struct cfw_project *aic3xxx_cfw_unpickle(void *pcfw, int n);

static void aic3xxx_wait(struct cfw_state *ps, unsigned int reg, u8 mask,
			 u8 data);
static void aic3xxx_set_bits(u8 *data, u8 mask, u8 val);
static int aic3xxx_driver_init(struct cfw_state *ps);

int aic3xxx_cfw_init(struct cfw_state *ps, const struct aic3xxx_codec_ops *ops,
		     struct snd_soc_codec *codec)
{
	ps->ops = ops;
	ps->codec = codec;
	ps->pjt = NULL;
	mutex_init(&ps->mutex);

	/* FIXME Need a special CONFIG flag to disable debug driver */
	aic3xxx_driver_init(ps);
	return 0;
}

int aic3xxx_cfw_lock(struct cfw_state *ps, int lock)
{
	if (lock)
		mutex_lock(&ps->mutex);
	else
		mutex_unlock(&ps->mutex);
	return 0;
}

int aic3xxx_cfw_reload(struct cfw_state *ps, void *pcfw, int n)
{
	ps->pjt = aic3xxx_cfw_unpickle(pcfw, n);
	ps->cur_mode_id =
	    ps->cur_mode = ps->cur_pll = ps->cur_pfw =
	    ps->cur_ovly = ps->cur_cfg = -1;
	if (ps->pjt == NULL)
		return -1;
	return 0;
}

int aic3xxx_cfw_setmode(struct cfw_state *ps, int mode)
{
	struct cfw_project *pjt;
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	pjt = ps->pjt;
	if (pjt == NULL) {
		aic3xxx_cfw_lock(ps, 0);
		return -1;
	}
	ret = aic3xxx_cfw_setmode_cfg_u(ps, mode, pjt->mode[mode]->cfg);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

int aic3xxx_cfw_setcfg(struct cfw_state *ps, int cfg)
{
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	ret = aic3xxx_cfw_setcfg_u(ps, cfg);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

static int aic3xxx_cfw_setcfg_u(struct cfw_state *ps, int cfg)
{
	struct cfw_project *pjt = ps->pjt;
	struct cfw_pfw *pfw;
	struct cfw_image *patch;

	if (pjt == NULL)
		return -1;
	if (ps->cur_pfw < 0 || ps->cur_pfw >= pjt->npfw)
		return -1;	/* Non miniDSP */
	if (ps->cur_cfg == cfg)
		return 0;
	pfw = pjt->pfw[ps->cur_pfw];
	if (pfw->ncfg == 0 && cfg != 0)
		return -1;
	if (cfg > 0 && cfg >= pfw->ncfg)
		return -1;
	ps->cur_cfg = cfg;
	aic3xxx_cfw_set_mode_id(ps);
	patch =
	    pfw->ovly_cfg[CFW_OCFG_NDX(pfw, ps->cur_ovly, ps->cur_cfg)];
	if (pfw->ncfg != 0)
		return aic3xxx_cfw_dlcfg(ps, patch);
	return 0;
}

int aic3xxx_cfw_setmode_cfg(struct cfw_state *ps, int mode, int cfg)
{
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	ret = aic3xxx_cfw_setmode_cfg_u(ps, mode, cfg);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

static int aic3xxx_cfw_setmode_cfg_u(struct cfw_state *ps, int mode, int cfg)
{
	struct cfw_project *pjt = ps->pjt;
	struct cfw_mode *pmode;
	int which = 0, ocndx;

	if (pjt == NULL)
		goto err;
	if ((mode < 0) || (mode >= pjt->nmode))
		goto err;
	if (cfg < 0)
		goto err;
	if (mode == ps->cur_mode)
		return aic3xxx_cfw_setcfg_u(ps, cfg);

	/* Apply exit sequence for previous mode if present */
	if (ps->cur_mode >= 0)
		aic3xxx_cfw_dlcmds(ps, pjt->mode[ps->cur_mode]->exit);
	pmode = pjt->mode[mode];
	if (pjt->mode[mode]->pfw < pjt->npfw) { /* New mode uses miniDSP */
		struct cfw_image *im;
		struct cfw_pfw *pfw = pjt->pfw[pmode->pfw];

		/* Make sure cfg is valid and supported in this mode */
		if (pfw->ncfg == 0 && cfg != 0)
			goto err;
		if (cfg > 0 && cfg >= pfw->ncfg)
			goto err;

		/*
		 * Decisions about which miniDSP to stop/restart are taken
		 * on the basis of sections present in the _base_ image
		 * This allows for correct sync mode operation even in cases
		 * where the base PFW uses both miniDSPs where a particular
		 * overlay applies only to one
		 */
		im = pfw->base;

		/* While switching PFW, switch OFF minidsps anyways */
		which |= AIC3XXX_COPS_MDSP_A;
		which |= AIC3XXX_COPS_MDSP_D;

		if (pmode->pfw != ps->cur_pfw) {

			/* New mode requires different PFW */
			ps->cur_pfw = pmode->pfw;
			ps->cur_ovly = 0;
			ps->cur_cfg = 0;

			which = ps->ops->stop(ps->codec, which);
			aic3xxx_cfw_dlimage(ps, im);
			if (pmode->ovly && pmode->ovly < pfw->novly) {

				/* New mode uses ovly */
				ocndx = CFW_OCFG_NDX(pfw, pmode->ovly, cfg);
				aic3xxx_cfw_dlimage(ps,
						    pfw->ovly_cfg[ocndx]);
			} else if (pfw->ncfg > 0) {

				/* new mode needs only a cfg change */
				ocndx = CFW_OCFG_NDX(pfw, 0, cfg);
				aic3xxx_cfw_dlimage(ps,
						    pfw->ovly_cfg[ocndx]);
			}
			ps->ops->restore(ps->codec, which);

		} else if (pmode->ovly != ps->cur_ovly) {

			/* New mode requires only an ovly change */
			ocndx = CFW_OCFG_NDX(pfw, pmode->ovly, cfg);
			which = ps->ops->stop(ps->codec, which);
			aic3xxx_cfw_dlimage(ps, pfw->ovly_cfg[ocndx]);
			ps->ops->restore(ps->codec, which);
		} else if (pfw->ncfg > 0 && cfg != ps->cur_cfg) {

			/* New mode requires only a cfg change */
			ocndx = CFW_OCFG_NDX(pfw, pmode->ovly, cfg);
			aic3xxx_cfw_dlcfg(ps, pfw->ovly_cfg[ocndx]);
		}
		ps->cur_ovly = pmode->ovly;
		ps->cur_cfg = cfg;

		ps->cur_mode = mode;
		aic3xxx_cfw_set_pll_u(ps, 0);

	} else if (pjt->mode[mode]->pfw != 0xFF) {

		/* Not bypass mode */
		warn("Bad pfw setting detected (%d).  Max pfw=%d",
		     pmode->pfw, pjt->npfw);
	}
	ps->cur_mode = mode;
	aic3xxx_cfw_set_mode_id(ps);

	/* Transition to netural mode */
	aic3xxx_cfw_transition_u(ps, "NEUTRAL");

	/* Apply entry sequence if present */
	aic3xxx_cfw_dlcmds(ps, pmode->entry);

	DBG("setmode_cfg: DONE (mode=%d pfw=%d ovly=%d cfg=%d)",
	    ps->cur_mode, ps->cur_pfw, ps->cur_ovly, ps->cur_cfg);
	return 0;

err:
	DBG("Failed to set firmware mode");
	return -EINVAL;
}

int aic3xxx_cfw_transition(struct cfw_state *ps, char *ttype)
{
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	ret = aic3xxx_cfw_transition_u(ps, ttype);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

static int aic3xxx_cfw_transition_u(struct cfw_state *ps, char *ttype)
{
	int i;

	if (ps->pjt == NULL)
		return -EINVAL;
	for (i = 0; i < CFW_TRN_N; ++i) {
		if (!strcasecmp(ttype, cfw_transition_id[i])) {
			struct cfw_transition *pt = ps->pjt->transition[i];
			DBG("Sending transition %s[%d]", ttype, i);
			if (pt)
				aic3xxx_cfw_dlcmds(ps, pt->block);
			return 0;
		}
	}
	warn("Transition %s not present or invalid", ttype);
	return 0;
}

int aic3xxx_cfw_set_pll(struct cfw_state *ps, int asi)
{
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	ret = aic3xxx_cfw_set_pll_u(ps, asi);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

static int aic3xxx_cfw_set_pll_u(struct cfw_state *ps, int asi)
{
	struct cfw_project *pjt = ps->pjt;
	int pll_id;

	if (pjt == NULL)
		return -EINVAL;
	if (ps->cur_mode < 0)
		return -EINVAL;
	pll_id = pjt->mode[ps->cur_mode]->pll;
	if (ps->cur_pll != pll_id) {
		DBG("Re-configuring PLL: %s==>%d", pjt->pll[pll_id]->name,
		    pll_id);
		aic3xxx_cfw_dlcmds(ps, pjt->pll[pll_id]->seq);
		ps->cur_pll = pll_id;
	}
	return 0;
}

int aic3xxx_cfw_control(struct cfw_state *ps, char *cname, int param)
{
	int ret;

	aic3xxx_cfw_lock(ps, 1);
	ret = aic3xxx_cfw_control_u(ps, cname, param);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

static int aic3xxx_cfw_control_u(struct cfw_state *ps, char *cname, int param)
{
	struct cfw_pfw *pfw;
	int i;

	if (ps->cur_pfw < 0 || ps->cur_pfw >= ps->pjt->npfw) {
		warn("Not in MiniDSP mode");
		return 0;
	}
	pfw = ps->pjt->pfw[ps->cur_pfw];
	for (i = 0; i < pfw->nctrl; ++i) {
		struct cfw_control *pc = pfw->ctrl[i];
		if (strcasecmp(cname, pfw->ctrl[i]->name))
			continue;
		if (param < 0 || param > pc->imax) {
			warn("Parameter out of range\n");
			return -EINVAL;
		}
		DBG("Sending control %s[%d]", cname, param);
		pc->icur = param;
		aic3xxx_cfw_dlctl(ps, pc->output[param], pc->mute_flags);
		return 0;
	}
	warn("Control named %s not found in pfw %s", cname, pfw->name);

	return -EINVAL;
}

static void aic3xxx_cfw_op(struct cfw_state *ps, unsigned char *var,
			   struct cfw_cmd_op cmd)
{
	u32 op1, op2;
	u32 cid = cmd.cid;

	op1 = cmd.op1;
	op2 = cmd.op2;
	if (cid & CFW_CMD_OP1_ID)
		op1 = var[op1];
	if (cid & CFW_CMD_OP2_ID)
		op2 = var[op2];
	cid &= ~(CFW_CMD_OP1_ID | CFW_CMD_OP2_ID);

	switch (cid) {
	case CFW_CMD_OP_ADD:
		var[cmd.dst] = op1 + op2;
		break;
	case CFW_CMD_OP_SUB:
		var[cmd.dst] = op1 - op2;
		break;
	case CFW_CMD_OP_MUL:
		var[cmd.dst] = op1 * op2;
		break;
	case CFW_CMD_OP_DIV:
		var[cmd.dst] = op1 / op2;
		break;
	case CFW_CMD_OP_AND:
		var[cmd.dst] = op1 & op2;
		break;
	case CFW_CMD_OP_OR:
		var[cmd.dst] = op1 | op2;
		break;
	case CFW_CMD_OP_SHL:
		var[cmd.dst] = (op1 << op2);
		break;
	case CFW_CMD_OP_SHR:
		var[cmd.dst] = (op1 >> op2);
		break;
	case CFW_CMD_OP_RR:
		while (op2--)
			var[cmd.dst] = (op1 >> 1) | ((op1 & 1) << 7);
		break;
	case CFW_CMD_OP_XOR:
		var[cmd.dst] = op1 ^ op2;
		break;
	case CFW_CMD_OP_NOT:
		var[cmd.dst] = ~op1;
		break;
	case CFW_CMD_OP_LNOT:
		var[cmd.dst] = !op1;
		break;
	default:
		break;
	}
}

static void aic3xxx_cfw_dlcmds(struct cfw_state *ps, struct cfw_block *pb)
{
	int pc = 0, cond = 0;
	unsigned char var[256];

	if (!pb)
		return;
	while (pc < pb->ncmds) {
		union cfw_cmd *c = &(pb->cmd[pc]);
		if (c->cid != CFW_CMD_BRANCH_IM &&
		    c->cid != CFW_CMD_BRANCH_ID && c->cid != CFW_CMD_NOP)
			cond = 0;
		switch ((int)(c->cid)) {
		case 0 ... (CFW_CMD_NOP - 1):
			ps->ops->reg_write(ps->codec, c->reg.bpod,
					   c->reg.data);
			pc += 1;
			break;
		case CFW_CMD_NOP:
			pc += 1;
			break;
		case CFW_CMD_DELAY:
			mdelay(c->delay.delay);
			pc += 1;
			break;
		case CFW_CMD_UPDTBITS:
			ps->ops->set_bits(ps->codec, c[1].reg.bpod,
					  c->bitop.mask, c[1].reg.data);
			pc += 2;
			break;
		case CFW_CMD_WAITBITS:
			aic3xxx_wait(ps, c[1].reg.bpod, c->bitop.mask,
				     c[1].reg.data);
			pc += 2;
			break;
		case CFW_CMD_LOCK:
			if (c->delay.delay)
				ps->ops->lock(ps->codec);
			else
				ps->ops->unlock(ps->codec);
			pc += 1;
			break;
		case CFW_CMD_BURST:
			ps->ops->bulk_write(ps->codec, c[1].reg.bpod,
					    c->bhdr.len, c[1].burst.data);
			pc += CFW_CMD_BURST_LEN(c->bhdr.len);
			break;
		case CFW_CMD_RBURST:
			ps->ops->bulk_read(ps->codec, c[1].reg.bpod,
					    c->bhdr.len, c[1].burst.data);
			pc += CFW_CMD_BURST_LEN(c->bhdr.len);
			break;
		case CFW_CMD_LOAD_VAR_IM:
			aic3xxx_set_bits(&var[c->ldst.dvar],
					 c->ldst.mask, c->ldst.svar);
			pc += 1;
			break;
		case CFW_CMD_LOAD_VAR_ID:
			if (c->ldst.svar != c->ldst.dvar) {
				aic3xxx_set_bits(&var[c->ldst.dvar],
						 c->ldst.mask,
						 var[c->ldst.svar]);
				pc += 1;
			} else {
				u8 data;
				data = ps->ops->reg_read(ps->codec,
							c[1].reg.bpod);
				aic3xxx_set_bits(&var[c->ldst.dvar],
						 c->ldst.mask, data);
				pc += 2;
			}
			break;
		case CFW_CMD_STORE_VAR:
			if (c->ldst.svar != c->ldst.dvar)
				ps->ops->set_bits(ps->codec,
						  c[1].reg.bpod,
						  var[c->ldst.dvar],
						  var[c->ldst.svar]);
			else
				ps->ops->set_bits(ps->codec,
						  c[1].reg.bpod,
						  c->ldst.mask,
						  var[c->ldst.svar]);
			pc += 2;
			break;
		case CFW_CMD_COND:
			cond = var[c->cond.svar] & c->cond.mask;
			pc += 1;
			break;
		case CFW_CMD_BRANCH:
			pc = c->branch.address;
			break;
		case CFW_CMD_BRANCH_IM:
			if (c->branch.match == cond)
				pc = c->branch.address;
			else
				pc += 1;
			break;
		case CFW_CMD_BRANCH_ID:
			if (var[c->branch.match] == cond)
				pc = c->branch.address;
			else
				pc += 1;
			break;
		case CFW_CMD_PRINT:
			{
				union cfw_cmd *parglist =
				    c + CFW_CMD_PRINT_ARG(c->print);
				printk(c->print.fmt,
				     var[parglist->print_arg[0]],
				     var[parglist->print_arg[1]],
				     var[parglist->print_arg[2]],
				     var[parglist->print_arg[3]]);
				pc += CFW_CMD_PRINT_LEN(c->print);
			}
			break;
		case CFW_CMD_OP_START ... CFW_CMD_OP_END:
			aic3xxx_cfw_op(ps, var, c->op);
			pc += 1;
			break;
		default:
			warn("Unknown cmd command %x. Skipped", c->cid);
			pc += 1;
			break;
		}
	}
}

static void aic3xxx_wait(struct cfw_state *ps, unsigned int reg, u8 mask,
			 u8 data)
{
	while ((ps->ops->reg_read(ps->codec, reg) & mask) != data)
		mdelay(2);
}

static void aic3xxx_set_bits(u8 *data, u8 mask, u8 val)
{
	*data = (*data & (~mask)) | (val & mask);
}

static const struct {
	u32 mdsp;
	int buf_a, buf_b;
	u32 swap;
} csecs[] = {
	{
		.mdsp = AIC3XXX_COPS_MDSP_A,
		.swap = AIC3XXX_ABUF_MDSP_A,
		.buf_a = CFW_BLOCK_A_A_COEF,
		.buf_b = CFW_BLOCK_A_B_COEF
	},
	{
		.mdsp = AIC3XXX_COPS_MDSP_D,
		.swap = AIC3XXX_ABUF_MDSP_D1,
		.buf_a = CFW_BLOCK_D_A1_COEF,
		.buf_b = CFW_BLOCK_D_B1_COEF
	},
	{
		.mdsp = AIC3XXX_COPS_MDSP_D,
		.swap = AIC3XXX_ABUF_MDSP_D2,
		.buf_a = CFW_BLOCK_D_A2_COEF,
		.buf_b = CFW_BLOCK_D_B2_COEF
	},
};
static int aic3xxx_cfw_dlctl(struct cfw_state *ps, struct cfw_block *pb,
			     u32 mute_flags)
{
	int i, btype = pb->type;
	int run_state = ps->ops->lock(ps->codec);

	DBG("Download CTL");
	for (i = 0; i < sizeof(csecs) / sizeof(csecs[0]); ++i) {
		if (csecs[i].buf_a != btype && csecs[i].buf_b != btype)
			continue;
		DBG("\tDownload once to %d", btype);
		aic3xxx_cfw_dlcmds(ps, pb);
		if (run_state & csecs[i].mdsp) {
			DBG("\tDownload again to make sure it reaches B");
			aic3xxx_cfw_mute(ps, 1, run_state & mute_flags);
			ps->ops->bswap(ps->codec, csecs[i].swap);
			aic3xxx_cfw_mute(ps, 0, run_state & mute_flags);
			aic3xxx_cfw_dlcmds(ps, pb);
		}
		break;
	}
	ps->ops->unlock(ps->codec);
	return 0;
}

static int aic3xxx_cfw_dlcfg(struct cfw_state *ps, struct cfw_image *pim)
{
	int i, run_state, swap;

	DBG("Download CFG %s", pim->name);
	run_state = ps->ops->lock(ps->codec);
	swap = 0;
	for (i = 0; i < sizeof(csecs) / sizeof(csecs[0]); ++i) {
		if (!pim->block[csecs[i].buf_a])
			continue;
		aic3xxx_cfw_dlcmds(ps, pim->block[csecs[i].buf_a]);
		aic3xxx_cfw_dlcmds(ps, pim->block[csecs[i].buf_b]);
		if (run_state & csecs[i].mdsp)
			swap |= csecs[i].swap;
	}
	if (swap) {
		aic3xxx_cfw_mute(ps, 1, run_state & pim->mute_flags);
		ps->ops->bswap(ps->codec, swap);
		aic3xxx_cfw_mute(ps, 0, run_state & pim->mute_flags);
		for (i = 0; i < sizeof(csecs) / sizeof(csecs[0]); ++i) {
			if (!pim->block[csecs[i].buf_a])
				continue;
			if (!(run_state & csecs[i].mdsp))
				continue;
			aic3xxx_cfw_dlcmds(ps, pim->block[csecs[i].buf_a]);
			aic3xxx_cfw_dlcmds(ps, pim->block[csecs[i].buf_b]);
		}
	}
	ps->ops->unlock(ps->codec);
	return 0;
}

static int aic3xxx_cfw_dlimage(struct cfw_state *ps, struct cfw_image *pim)
{
	int i;

	if (!pim)
		return 0;
	DBG("Download IMAGE %s", pim->name);
	for (i = 0; i < CFW_BLOCK_N; ++i)
		aic3xxx_cfw_dlcmds(ps, pim->block[i]);
	return 0;
}

static int aic3xxx_cfw_mute(struct cfw_state *ps, int mute, u32 flags)
{
	if ((flags & AIC3XXX_COPS_MDSP_D) && (flags & AIC3XXX_COPS_MDSP_A))
		aic3xxx_cfw_transition_u(ps,
					 mute ? "AD_MUTE" : "AD_UNMUTE");
	else if (flags & AIC3XXX_COPS_MDSP_D)
		aic3xxx_cfw_transition_u(ps, mute ? "D_MUTE" : "D_UNMUTE");
	else if (flags & AIC3XXX_COPS_MDSP_A)
		aic3xxx_cfw_transition_u(ps, mute ? "A_MUTE" : "A_UNMUTE");
	return 0;
}

static inline void *aic3xxx_cfw_ndx2ptr(void *p, u8 *base)
{
	return &base[(int)p];
}
static inline char *aic3xxx_cfw_desc(void *p, u8 *base)
{
	if (p)
		return aic3xxx_cfw_ndx2ptr(p, base);
	return NULL;
}

static void aic3xxx_cfw_unpickle_image(struct cfw_image *im, void *p)
{
	int i;

	im->desc = aic3xxx_cfw_desc(im->desc, p);
	for (i = 0; i < CFW_BLOCK_N; ++i)
		if (im->block[i])
			im->block[i] = aic3xxx_cfw_ndx2ptr(im->block[i], p);
}

static void aic3xxx_cfw_unpickle_control(struct cfw_control *ct, void *p)
{
	int i;

	ct->output = aic3xxx_cfw_ndx2ptr(ct->output, p);
	ct->desc = aic3xxx_cfw_desc(ct->desc, p);
	for (i = 0; i <= ct->imax; ++i)
		ct->output[i] = aic3xxx_cfw_ndx2ptr(ct->output[i], p);
}

static unsigned int crc32(unsigned int *pdata, int n)
{
	u32 crc = 0, i, crc_poly = 0x04C11DB7;	/* CRC - 32 */
	u32 msb;
	u32 residue_value = 0;
	int bits;

	for (i = 0; i < (n >> 2); i++) {
		bits = 32;
		while (--bits >= 0) {
			msb = crc & 0x80000000;
			crc = (crc << 1) ^ ((*pdata >> bits) & 1);
			if (msb)
				crc = crc ^ crc_poly;
		}
		pdata++;
	}

	switch (n & 3) {
	case 0:
		break;
	case 1:
		residue_value = (*pdata & 0xFF);
		bits = 8;
		break;
	case 2:
		residue_value = (*pdata & 0xFFFF);
		bits = 16;
		break;
	case 3:
		residue_value = (*pdata & 0xFFFFFF);
		bits = 24;
		break;
	}

	if (n & 3) {
		while (--bits >= 0) {
			msb = crc & 0x80000000;
			crc = (crc << 1) ^ ((residue_value >> bits) & 1);
			if (msb)
				crc = crc ^ crc_poly;
		}
	}
	return crc;
}

static int crc_chk(void *p, int n)
{
	struct cfw_project *pjt = (void *) p;
	u32 crc = pjt->cksum, crc_comp;

	pjt->cksum = 0;
	DBG("Entering crc %d", n);
	crc_comp = crc32(p, n);
	if (crc_comp != crc) {
		DBG("CRC mismatch 0x%08X != 0x%08X", crc, crc_comp);
		return 0;
	}
	DBG("CRC pass");
	pjt->cksum = crc;
	return 1;
}

static struct cfw_project *aic3xxx_cfw_unpickle(void *p, int n)
{
	struct cfw_project *pjt = p;
	int i, j;

	if (pjt->magic != CFW_FW_MAGIC || pjt->size != n ||
	    pjt->if_id != CFW_FW_IF_ID || !crc_chk(p, n)) {
		error("Version mismatch: unable to load firmware\n");
		return NULL;
	}
	DBG("Loaded firmware inside unpickle\n");

	pjt->desc = aic3xxx_cfw_desc(pjt->desc, p);
	pjt->transition = aic3xxx_cfw_ndx2ptr(pjt->transition, p);
	for (i = 0; i < CFW_TRN_N; i++) {
		if (!pjt->transition[i])
			continue;
		pjt->transition[i] = aic3xxx_cfw_ndx2ptr(pjt->transition[i], p);
		pjt->transition[i]->desc = aic3xxx_cfw_desc(
						pjt->transition[i]->desc, p);
		pjt->transition[i]->block = aic3xxx_cfw_ndx2ptr(
						pjt->transition[i]->block, p);
	}
	pjt->pll = aic3xxx_cfw_ndx2ptr(pjt->pll, p);
	for (i = 0; i < pjt->npll; i++) {
		pjt->pll[i] = aic3xxx_cfw_ndx2ptr(pjt->pll[i], p);
		pjt->pll[i]->desc = aic3xxx_cfw_desc(pjt->pll[i]->desc, p);
		pjt->pll[i]->seq = aic3xxx_cfw_ndx2ptr(pjt->pll[i]->seq, p);
	}

	pjt->pfw = aic3xxx_cfw_ndx2ptr(pjt->pfw, p);
	for (i = 0; i < pjt->npfw; i++) {
		DBG("loading pfw %d\n", i);
		pjt->pfw[i] = aic3xxx_cfw_ndx2ptr(pjt->pfw[i], p);
		pjt->pfw[i]->desc = aic3xxx_cfw_desc(pjt->pfw[i]->desc, p);
		if (pjt->pfw[i]->base) {
			pjt->pfw[i]->base = aic3xxx_cfw_ndx2ptr(
							pjt->pfw[i]->base, p);
			aic3xxx_cfw_unpickle_image(pjt->pfw[i]->base, p);
		}
		pjt->pfw[i]->ovly_cfg = aic3xxx_cfw_ndx2ptr(
						pjt->pfw[i]->ovly_cfg, p);
		for (j = 0; j < pjt->pfw[i]->novly * pjt->pfw[i]->ncfg; ++j) {
			pjt->pfw[i]->ovly_cfg[j] = aic3xxx_cfw_ndx2ptr(
						pjt->pfw[i]->ovly_cfg[j], p);
			aic3xxx_cfw_unpickle_image(pjt->pfw[i]->ovly_cfg[j], p);
		}
		if (pjt->pfw[i]->nctrl)
			pjt->pfw[i]->ctrl = aic3xxx_cfw_ndx2ptr(
							pjt->pfw[i]->ctrl, p);
		for (j = 0; j < pjt->pfw[i]->nctrl; ++j) {
			pjt->pfw[i]->ctrl[j] = aic3xxx_cfw_ndx2ptr(
						pjt->pfw[i]->ctrl[j], p);
			aic3xxx_cfw_unpickle_control(pjt->pfw[i]->ctrl[j], p);
		}
	}

	DBG("loaded pfw's\n");
	pjt->mode = aic3xxx_cfw_ndx2ptr(pjt->mode, p);
	for (i = 0; i < pjt->nmode; i++) {
		pjt->mode[i] = aic3xxx_cfw_ndx2ptr(pjt->mode[i], p);
		pjt->mode[i]->desc = aic3xxx_cfw_desc(pjt->mode[i]->desc, p);
		if (pjt->mode[i]->entry)
			pjt->mode[i]->entry = aic3xxx_cfw_ndx2ptr(
						pjt->mode[i]->entry, p);
		if (pjt->mode[i]->exit)
			pjt->mode[i]->exit = aic3xxx_cfw_ndx2ptr(
						pjt->mode[i]->exit, p);
	}
	if (pjt->asoc_toc)
		pjt->asoc_toc = aic3xxx_cfw_ndx2ptr(pjt->asoc_toc, p);
	else {
		warn("asoc_toc not defined.  FW version mismatch?");
		return NULL;
	}
	DBG("loaded modes");
	return pjt;
}
static int aic3xxx_cfw_set_mode_id(struct cfw_state *ps)
{
	struct cfw_asoc_toc *toc = ps->pjt->asoc_toc;
	int i;

	for (i = 0; i < toc->nentries; ++i) {
		if (toc->entry[i].cfg == ps->cur_cfg &&
		    toc->entry[i].mode == ps->cur_mode) {
			ps->cur_mode_id = i;
			return 0;
		}
	}
	DBG("Unknown mode,cfg combination [%d,%d]", ps->cur_mode,
	    ps->cur_cfg);
	return -1;
}

/* **Code beyond this point is not compilable on host** */

static int aic3xxx_get_control(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct cfw_state *ps = (struct cfw_state *)kcontrol->private_value;
	struct cfw_pfw *pfw;
	int i;

	if (ps->cur_pfw >= ps->pjt->npfw) {
		DBG("Not in MiniDSP mode");
		return 0;
	}
	pfw = ps->pjt->pfw[ps->cur_pfw];
	for (i = 0; i < pfw->nctrl; ++i) {
		if (!strcasecmp(kcontrol->id.name, pfw->ctrl[i]->name)) {
			struct cfw_control *pc = pfw->ctrl[i];
			ucontrol->value.integer.value[0] = pc->icur;
			return 0;
		}
	}
	return 0;
}

static int aic3xxx_put_control(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct cfw_state *ps = (struct cfw_state *)kcontrol->private_value;

	aic3xxx_cfw_control(ps, kcontrol->id.name,
			    ucontrol->value.integer.value[0]);
	return 0;
}

static int aic3xxx_info_control(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *ucontrol)
{
	struct cfw_state *ps = (struct cfw_state *)kcontrol->private_value;
	struct cfw_pfw *pfw;
	int i;

	if (ps->cur_pfw >= ps->pjt->npfw) {
		DBG("Not in MiniDSP mode");
		return 0;
	}
	pfw = ps->pjt->pfw[ps->cur_pfw];
	for (i = 0; i < pfw->nctrl; ++i) {
		if (!strcasecmp(kcontrol->id.name, pfw->ctrl[i]->name)) {
			struct cfw_control *pc = pfw->ctrl[i];
			ucontrol->value.integer.min = 0;
			ucontrol->value.integer.max = pc->imax;
			if (pc->imax == 1)
				ucontrol->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
			else
				ucontrol->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		}
	}

	ucontrol->count = 1;
	return 0;
}
int aic3xxx_cfw_add_controls(struct snd_soc_codec *codec, struct cfw_state *ps)
{
	int i, j;
	struct cfw_pfw *pfw;

	for (j = 0; j < ps->pjt->npfw; ++j) {
		pfw = ps->pjt->pfw[j];

		for (i = 0; i < pfw->nctrl; ++i) {
			struct cfw_control *pc = pfw->ctrl[i];
			struct snd_kcontrol_new *generic_control =
			    kzalloc(sizeof(struct snd_kcontrol_new),
				    GFP_KERNEL);
			unsigned int *tlv_array =
			    kzalloc(4 * sizeof(unsigned int), GFP_KERNEL);

			if (generic_control == NULL)
				return -ENOMEM;
			generic_control->access =
			    SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			    SNDRV_CTL_ELEM_ACCESS_READWRITE;
			tlv_array[0] = SNDRV_CTL_TLVT_DB_SCALE;
			tlv_array[1] = 2 * sizeof(unsigned int);
			tlv_array[2] = pc->min;
			tlv_array[3] = ((pc->step) & TLV_DB_SCALE_MASK);
			if (pc->step > 0)
				generic_control->tlv.p = tlv_array;
			generic_control->name = pc->name;
			generic_control->private_value = (unsigned long) ps;
			generic_control->get = aic3xxx_get_control;
			generic_control->put = aic3xxx_put_control;
			generic_control->info = aic3xxx_info_control;
			generic_control->iface = SNDRV_CTL_ELEM_IFACE_MIXER;
			snd_soc_add_codec_controls(codec, generic_control, 1);
			DBG("Added control %s", pc->name);
		}
	}
	return 0;

}


static int aic3xxx_get_mode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *) kcontrol->private_value;
	struct cfw_state *ps = (struct cfw_state *) e->mask;

	ucontrol->value.enumerated.item[0] = ps->cur_mode_id;

	return 0;
}

static int aic3xxx_put_mode(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *) kcontrol->private_value;
	struct cfw_state *ps = (struct cfw_state *) e->mask;
	struct cfw_asoc_toc *toc;
	int index, ret;

	aic3xxx_cfw_lock(ps, 1);
	toc = ps->pjt->asoc_toc;

	index = ucontrol->value.enumerated.item[0];
	if (index < 0 || index >= toc->nentries) {
		aic3xxx_cfw_lock(ps, 0);
		return -EINVAL;
	}
	ret = aic3xxx_cfw_setmode_cfg_u(ps, toc->entry[index].mode,
				      toc->entry[index].cfg);
	aic3xxx_cfw_lock(ps, 0);
	return ret;
}

int aic3xxx_cfw_add_modes(struct snd_soc_codec *codec, struct cfw_state *ps)
{
	int j;
	struct cfw_asoc_toc *toc = ps->pjt->asoc_toc;
	struct soc_enum *mode_cfg_enum =
	    kzalloc(sizeof(struct soc_enum), GFP_KERNEL);
	struct snd_kcontrol_new *mode_cfg_control =
	    kzalloc(sizeof(struct snd_kcontrol_new), GFP_KERNEL);
	char **enum_texts;

	if (mode_cfg_enum == NULL)
		goto mem_err;
	if (mode_cfg_control == NULL)
		goto mem_err;

	mode_cfg_enum->texts = kzalloc(toc->nentries * sizeof(char *),
								GFP_KERNEL);
	if (mode_cfg_enum->texts == NULL)
		goto mem_err;
	/* Hack to overwrite the const * const pointer */
	enum_texts = (char **) mode_cfg_enum->texts;

	for (j = 0; j < toc->nentries; j++)
		enum_texts[j] = toc->entry[j].etext;

	mode_cfg_enum->reg = j;
	mode_cfg_enum->max = toc->nentries;
	mode_cfg_enum->mask = (unsigned int) ps;
	mode_cfg_control->name = "Codec Firmware Setmode";
	mode_cfg_control->get = aic3xxx_get_mode;
	mode_cfg_control->put = aic3xxx_put_mode;
	mode_cfg_control->info = snd_soc_info_enum_ext;
	mode_cfg_control->private_value = (unsigned long) mode_cfg_enum;
	mode_cfg_control->iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	snd_soc_add_codec_controls(codec, mode_cfg_control, 1);
	return 0;
mem_err:
	kfree(mode_cfg_control);
	kfree(mode_cfg_enum);
	kfree(mode_cfg_enum->texts);
	return -ENOMEM;

}

#if defined(CONFIG_AIC3111_CODEC) || defined(CONFIG_AIC3111_CORE)

#	define AIC3XXX_CFW_DEVICE "aic3111_cfw"
#elif defined(CONFIG_AIC3256_CODEC) || defined(CONFIG_AIC3256_CORE)

#	define AIC3XXX_CFW_DEVICE "aic3256_cfw"
#elif defined(CONFIG_AIC3262_CODEC) || defined(CONFIG_AIC3262_CORE)
#	define AIC3XXX_CFW_DEVICE "aic3262_cfw"
#else
#	define AIC3XXX_CFW_DEVICE "aic3xxx_cfw"
#endif

static int aic3xxx_cfw_open(struct inode *in, struct file *filp)
{
	struct cfw_state *ps = container_of(in->i_cdev, struct cfw_state, cdev);
	if (ps->is_open) {
		warn("driver_open: device is already open");
		return -1;
	}
	ps->is_open++;
	filp->private_data = ps;
	return 0;
}
static int aic3xxx_cfw_release(struct inode *in, struct file *filp)
{
	struct cfw_state *ps = filp->private_data;
	ps->is_open--;
	return ps->is_open;
}
static long aic3xxx_cfw_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	return 0;
}
static ssize_t aic3xxx_cfw_rw(struct file *filp, char __user *buf,
			   size_t count, loff_t *offset)
{
	struct cfw_state *ps = filp->private_data;
	struct cfw_block *kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf || copy_from_user(kbuf, buf, count)) {
		warn("dev_rw: Allocation or copy failure");
		goto err;
	}
	if (count != CFW_BLOCK_SIZE(kbuf->ncmds)) {
		warn("dev_rw: Bad packet received\n");
		goto err;
	}
	aic3xxx_cfw_dlcmds(ps, kbuf);
	if (copy_to_user(buf, kbuf, count)) {
		warn("dev_rw: copy failure");
		goto err;
	}
	kfree(kbuf);
	return count;
err:
	kfree(kbuf);
	return -EINVAL;
}

static const struct file_operations aic3xxx_cfw_fops = {
	.owner = THIS_MODULE,
	.open = aic3xxx_cfw_open,
	.release = aic3xxx_cfw_release,
	.read = aic3xxx_cfw_rw,
	.write = (ssize_t (*)(struct file *filp, const char __user *buf,
			size_t count, loff_t *offset))aic3xxx_cfw_rw,
	.unlocked_ioctl = aic3xxx_cfw_ioctl,
};
static int aic3xxx_driver_init(struct cfw_state *ps)
{
	int err;

	dev_t dev = MKDEV(0, 0);

	err = alloc_chrdev_region(&dev, 0, 1, AIC3XXX_CFW_DEVICE);
	if (err < 0) {
		warn("driver_init: Error allocating device number");
		return err;
	}
	warn("driver_init: Allocated Major Number: %d\n", MAJOR(dev));

	cdev_init(&(ps->cdev), &aic3xxx_cfw_fops);
	ps->cdev.owner = THIS_MODULE;
	ps->cdev.ops = &aic3xxx_cfw_fops;
	ps->is_open = 0;

	err = cdev_add(&(ps->cdev), dev, 1);
	if (err < 0) {
		warn("driver_init: cdev_add failed");
		unregister_chrdev_region(dev, 1);
		return err;
	}
	warn("driver_init: Registered cfw driver");
	return 0;
}

MODULE_DESCRIPTION("ASoC tlv320aic3xxx codec driver firmware functions");
MODULE_AUTHOR("Hari Rajagopala <harik@ti.com>");
MODULE_LICENSE("GPL");
