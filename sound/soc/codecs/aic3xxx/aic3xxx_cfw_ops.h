/*
 * aic3xxx_cfw_ops.h  --  SoC audio for TI OMAP44XX SDP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef AIC3XXX_CFW_OPS_H_
#define AIC3XXX_CFW_OPS_H_

#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/cdev.h>

struct cfw_project;
struct aic3xxx_codec_ops;

struct cfw_state {
	struct cfw_project *pjt;
	const struct aic3xxx_codec_ops  *ops;
	struct snd_soc_codec *codec;
	struct mutex mutex;
	int cur_mode_id;
	int cur_pll;
	int cur_mode;
	int cur_pfw;
	int cur_ovly;
	int cur_cfg;
	struct cdev cdev;
	int is_open;
};

int aic3xxx_cfw_init(struct cfw_state *ps, const struct aic3xxx_codec_ops *ops,
		     struct snd_soc_codec *codec);
int aic3xxx_cfw_lock(struct cfw_state *ps, int lock);
int aic3xxx_cfw_reload(struct cfw_state *ps, void *pcfw, int n);
int aic3xxx_cfw_setmode(struct cfw_state *ps, int mode);
int aic3xxx_cfw_setmode_cfg(struct cfw_state *ps, int mode, int cfg);
int aic3xxx_cfw_setcfg(struct cfw_state *ps, int cfg);
int aic3xxx_cfw_transition(struct cfw_state *ps, char *ttype);
int aic3xxx_cfw_set_pll(struct cfw_state *ps, int asi);
int aic3xxx_cfw_control(struct cfw_state *ps, char *cname, int param);
int aic3xxx_cfw_add_controls(struct snd_soc_codec *codec, struct cfw_state *ps);
int aic3xxx_cfw_add_modes(struct snd_soc_codec *codec, struct cfw_state *ps);


#define AIC3XXX_COPS_MDSP_D_L    (0x00000002u)
#define AIC3XXX_COPS_MDSP_D_R    (0x00000001u)
#define AIC3XXX_COPS_MDSP_D      (AIC3XXX_COPS_MDSP_D_L|AIC3XXX_COPS_MDSP_D_R)

#define AIC3XXX_COPS_MDSP_A_L    (0x00000020u)
#define AIC3XXX_COPS_MDSP_A_R    (0x00000010u)
#define AIC3XXX_COPS_MDSP_A      (AIC3XXX_COPS_MDSP_A_L|AIC3XXX_COPS_MDSP_A_R)

#define AIC3XXX_COPS_MDSP_ALL    (AIC3XXX_COPS_MDSP_D|AIC3XXX_COPS_MDSP_A)

#define AIC3XXX_ABUF_MDSP_D1 (0x00000001u)
#define AIC3XXX_ABUF_MDSP_D2 (0x00000002u)
#define AIC3XXX_ABUF_MDSP_A  (0x00000010u)
#define AIC3XXX_ABUF_MDSP_ALL \
		(AIC3XXX_ABUF_MDSP_D1|AIC3XXX_ABUF_MDSP_D2|AIC3XXX_ABUF_MDSP_A)


struct aic3xxx_codec_ops {
	int (*reg_read) (struct snd_soc_codec *codec, unsigned int reg);
	int (*reg_write) (struct snd_soc_codec *codec, unsigned int reg,
					unsigned char val);
	int (*set_bits) (struct snd_soc_codec *codec, unsigned int reg,
					unsigned char mask, unsigned char val);
	int (*bulk_read) (struct snd_soc_codec *codec, unsigned int reg,
					int count, u8 *buf);
	int (*bulk_write) (struct snd_soc_codec *codec, unsigned int reg,
					int count, const u8 *buf);
	int (*lock) (struct snd_soc_codec *codec);
	int (*unlock) (struct snd_soc_codec *codec);
	int (*stop) (struct snd_soc_codec *codec, int mask);
	int (*restore) (struct snd_soc_codec *codec, int runstate);
	int (*bswap) (struct snd_soc_codec *codec, int mask);
};

MODULE_LICENSE("GPL");

#endif
