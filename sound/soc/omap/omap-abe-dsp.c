/*
 * omap-abe-dsp.c
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
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Authors: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
 *
 */

#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c/twl.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/dma.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/omap-abe-dsp.h>

#include "omap-abe-dsp.h"
#include "omap-abe.h"
#include "abe/abe_main.h"
#include "abe/port_mgr.h"

#warning need omap_device_set_rate
#define omap_device_set_rate(x, y, z)

static const char *abe_memory_bank[5] = {
	"dmem",
	"cmem",
	"smem",
	"pmem",
	"mpu"
};


/*
 * ABE loadable coefficients.
 * The coefficient and their mixer configurations are loaded with the firmware
 * blob duing probe().
 */

struct coeff_config {
	char name[ABE_COEFF_NAME_SIZE];
	u32 count;
	u32 coeff;
	char texts[ABE_COEFF_NUM_TEXTS][ABE_COEFF_TEXT_SIZE];
};

/*
 * ABE Firmware Header.
 * The ABE firmware blob has a header that describes each data section. This
 * way we can store coefficients etc in the firmware.
 */
struct fw_header {
	u32 magic;			/* magic number */
	u32 crc;			/* optional crc */
	u32 firmware_size;	/* payload size */
	u32 coeff_size;		/* payload size */
	u32 coeff_version;	/* coefficent version */
	u32 firmware_version;	/* min version of ABE firmware required */
	u32 num_equ;		/* number of equalizers */
};

/*
 * ABE private data.
 */
struct abe_data {
	struct omap4_abe_dsp_pdata *abe_pdata;
	struct device *dev;
	struct snd_soc_platform *platform;
	struct delayed_work delayed_work;
	struct mutex mutex;
	struct mutex opp_mutex;
	struct clk *clk;
	void __iomem *io_base[5];
	int irq;
	int opp;
	int active;

	/* coefficients */
	struct fw_header hdr;
	u32 *firmware;
	s32 *equ[ABE_MAX_EQU];
	int equ_profile[ABE_MAX_EQU];
	struct soc_enum equalizer_enum[ABE_MAX_EQU];
	struct snd_kcontrol_new equalizer_control[ABE_MAX_EQU];
	struct coeff_config *equ_texts;

	/* DAPM mixer config - TODO: some of this can be replaced with HAL update */
	u32 widget_opp[ABE_NUM_DAPM_REG + 1];

	u16 router[16];
	int loss_count;

	struct snd_pcm_substream *ping_pong_substream;
	int first_irq;

	struct snd_pcm_substream *psubs;

#ifdef CONFIG_DEBUG_FS
	/* ABE runtime debug config */

	/* its intended we can switch on/off individual debug items */
	u32 dbg_format1; /* TODO: match flag names here to debug format flags */
	u32 dbg_format2;
	u32 dbg_format3;

	u32 dbg_buffer_bytes;
	u32 dbg_circular;
	u32 dbg_buffer_msecs;  /* size of buffer in secs */
	u32 dbg_elem_bytes;
	dma_addr_t dbg_buffer_addr;
	wait_queue_head_t wait;
	int dbg_reader_offset;
	int dbg_dma_offset;
	int dbg_complete;
	struct dentry *debugfs_root;
	struct dentry *debugfs_fmt1;
	struct dentry *debugfs_fmt2;
	struct dentry *debugfs_fmt3;
	struct dentry *debugfs_size;
	struct dentry *debugfs_data;
	struct dentry *debugfs_circ;
	struct dentry *debugfs_elem_bytes;
	struct dentry *debugfs_opp_level;
	char *dbg_buffer;
	struct omap_pcm_dma_data *dma_data;
	int dma_ch;
	int dma_req;
#endif
};

static struct abe_data *the_abe;

// TODO: map to the new version of HAL
static unsigned int abe_dsp_read(struct snd_soc_platform *platform,
		unsigned int reg)
{
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);

	BUG_ON(reg > ABE_NUM_DAPM_REG);
	return abe->widget_opp[reg];
}

static int abe_dsp_write(struct snd_soc_platform *platform, unsigned int reg,
		unsigned int val)
{
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);

	BUG_ON(reg > ABE_NUM_DAPM_REG);
	abe->widget_opp[reg] = val;
	return 0;
}

static void abe_irq_pingpong_subroutine(u32 *data)
{
	u32 dst, n_bytes;

	abe_read_next_ping_pong_buffer(MM_DL_PORT, &dst, &n_bytes);
	abe_set_ping_pong_buffer(MM_DL_PORT, n_bytes);

	/* Do not call ALSA function for first IRQ */
	if (the_abe->first_irq) {
		the_abe->first_irq = 0;
	} else {
		if (the_abe->ping_pong_substream)
			snd_pcm_period_elapsed(the_abe->ping_pong_substream);
	}
}

static irqreturn_t abe_irq_handler(int irq, void *dev_id)
{
	struct abe_data *abe = dev_id;

	/* TODO: handle underruns/overruns/errors */
	pm_runtime_get_sync(abe->dev);
	abe_clear_irq();  // TODO: why is IRQ not cleared after processing ?
	abe_irq_processing();
	pm_runtime_put_sync(abe->dev);
	return IRQ_HANDLED;
}

// TODO: these should really be called internally since we will know the McPDM state
void abe_dsp_pm_get(void)
{
	pm_runtime_get_sync(the_abe->dev);
}
EXPORT_SYMBOL_GPL(abe_dsp_pm_get);

void abe_dsp_pm_put(void)
{
	pm_runtime_put_sync(the_abe->dev);
}
EXPORT_SYMBOL_GPL(abe_dsp_pm_put);

void abe_dsp_shutdown(void)
{
	if (!the_abe->active && !abe_check_activity()) {
		abe_set_opp_processing(ABE_OPP25);
		the_abe->opp = 25;
		abe_stop_event_generator();
		udelay(250);
		omap_device_set_rate(the_abe->dev, the_abe->dev, 0);
	}
}
EXPORT_SYMBOL_GPL(abe_dsp_shutdown);

/*
 * These TLV settings will need fine tuned for each individual control
 */

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl1_tlv, -12000, 100, 3000);

/* Media DL1 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl1_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(mm_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(tones_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(voice_dl2_tlv, -12000, 100, 3000);

/* Media DL2 volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(capture_dl2_tlv, -12000, 100, 3000);

/* SDT volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(sdt_ul_tlv, -12000, 100, 3000);

/* SDT volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(sdt_dl_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_mm_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_tones_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_vx_ul_tlv, -12000, 100, 3000);

/* AUDUL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(audul_vx_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_mm_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_tones_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_vx_dl_tlv, -12000, 100, 3000);

/* VXREC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(vxrec_vx_ul_tlv, -12000, 100, 3000);

/* DMIC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(dmic_tlv, -12000, 100, 3000);

/* BT UL volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(btul_tlv, -12000, 100, 3000);

/* AMIC volume control from -120 to 30 dB in 1 dB steps */
static DECLARE_TLV_DB_SCALE(amic_tlv, -12000, 100, 3000);

//TODO: we have to use the shift value atm to represent register id due to current HAL
static int dl1_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	// TODO: optimise all of these to call HAL abe_enable_gain(mixer, enable)
	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		abe_enable_gain(MIXDL1, mc->reg);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		abe_disable_gain(MIXDL1, mc->reg);
	}
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int dl2_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		abe_enable_gain(MIXDL2, mc->reg);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		abe_disable_gain(MIXDL2, mc->reg);
	}

	pm_runtime_put_sync(the_abe->dev);
	return 1;
}

static int audio_ul_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		abe_enable_gain(MIXAUDUL, mc->reg);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		abe_disable_gain(MIXAUDUL, mc->reg);
	}
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int vxrec_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		abe_enable_gain(MIXVXREC, mc->reg);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		abe_disable_gain(MIXVXREC, mc->reg);
	}
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int sdt_put_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
		abe_enable_gain(MIXSDT, mc->reg);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
		abe_disable_gain(MIXSDT, mc->reg);
	}
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int abe_get_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	ucontrol->value.integer.value[0] = the_abe->widget_opp[mc->shift];
	return 0;
}

/* router IDs that match our mixer strings */
static const abe_router_t router[] = {
		ZERO_labelID, /* strangely this is not 0 */
		DMIC1_L_labelID, DMIC1_R_labelID,
		DMIC2_L_labelID, DMIC2_R_labelID,
		DMIC3_L_labelID, DMIC3_R_labelID,
		BT_UL_L_labelID, BT_UL_R_labelID,
		MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID,
		AMIC_L_labelID, AMIC_R_labelID,
		VX_REC_L_labelID, VX_REC_R_labelID,
};

static int ul_mux_put_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mux = ucontrol->value.enumerated.item[0];
	int reg = e->reg - ABE_MUX(0);

	pm_runtime_get_sync(the_abe->dev);

	if (mux > ABE_ROUTES_UL)
		return 0;

	// TODO: get all this via firmware
	if (reg < 8) {
		/* 0  .. 9   = MM_UL */
		the_abe->router[reg] = router[mux];
	} else if (reg < 12) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		the_abe->router[reg + 2] = router[mux];
	}

	/* 2nd arg here is unused */
	abe_set_router_configuration(UPROUTE, 0, (u32 *)the_abe->router);

	if (router[mux] != ZERO_labelID)
		the_abe->widget_opp[e->reg] = e->shift_l;
	else
		the_abe->widget_opp[e->reg] = 0;

	snd_soc_dapm_mux_update_power(widget, kcontrol, 1, mux, e);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int ul_mux_get_route(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	int reg = e->reg - ABE_MUX(0), i, rval = 0;

	// TODO: get all this via firmware
	if (reg < 8) {
		/* 0  .. 9   = MM_UL */
		rval = the_abe->router[reg];
	} else if (reg < 12) {
		/* 10 .. 11  = MM_UL2 */
		/* 12 .. 13  = VX_UL */
		rval = the_abe->router[reg + 2];
	}

	for (i = 0; i < ARRAY_SIZE(router); i++) {
		if (router[i] == rval) {
			ucontrol->value.integer.value[0] = i;
			return 0;
		}
	}

	return 1;
}


static int abe_put_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	if (ucontrol->value.integer.value[0]) {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 1);
	} else {
		the_abe->widget_opp[mc->shift] = ucontrol->value.integer.value[0];
		snd_soc_dapm_mixer_update_power(widget, kcontrol, 0);
	}
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}


static int volume_put_sdt_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);

	abe_write_mixer(MIXSDT, abe_val_to_gain(ucontrol->value.integer.value[0]),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_put_audul_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_mixer(MIXAUDUL, abe_val_to_gain(ucontrol->value.integer.value[0]),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_put_vxrec_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_mixer(MIXVXREC, abe_val_to_gain(ucontrol->value.integer.value[0]),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_put_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_mixer(MIXDL1, abe_val_to_gain(ucontrol->value.integer.value[0]),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_put_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_mixer(MIXDL2, abe_val_to_gain(ucontrol->value.integer.value[0]),
				RAMP_0MS, mc->reg);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_put_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_gain(mc->reg,
		       abe_val_to_gain(ucontrol->value.integer.value[0]),
		       RAMP_20MS, mc->shift);
	abe_write_gain(mc->reg,
		       -12000 + (ucontrol->value.integer.value[1] * 100),
		       RAMP_20MS, mc->rshift);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

static int volume_get_dl1_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_mixer(MIXDL1, &val, mc->reg);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int volume_get_dl2_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_mixer(MIXDL2, &val, mc->reg);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int volume_get_audul_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_mixer(MIXAUDUL, &val, mc->reg);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int volume_get_vxrec_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_mixer(MIXVXREC, &val, mc->reg);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int volume_get_sdt_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_mixer(MIXSDT, &val, mc->reg);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int volume_get_gain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;

	pm_runtime_get_sync(the_abe->dev);
	abe_read_gain(mc->reg, &val, mc->shift);
	ucontrol->value.integer.value[0] = abe_gain_to_val(val);
	abe_read_gain(mc->reg, &val, mc->rshift);
	ucontrol->value.integer.value[1] = abe_gain_to_val(val);
	pm_runtime_put_sync(the_abe->dev);

	return 0;
}

static int abe_get_equalizer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *eqc = (struct soc_enum *)kcontrol->private_value;

	ucontrol->value.integer.value[0] = the_abe->equ_profile[eqc->reg];
	return 0;
}

static int abe_put_equalizer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *eqc = (struct soc_enum *)kcontrol->private_value;
	u16 val = ucontrol->value.enumerated.item[0];
	abe_equ_t equ_params;
	int len;

	if (eqc->reg >= the_abe->hdr.num_equ)
		return -EINVAL;

	if (val >= the_abe->equ_texts[eqc->reg].count)
		return -EINVAL;

	len = the_abe->equ_texts[eqc->reg].coeff;
	equ_params.equ_length = len;
	memcpy(equ_params.coef.type1, the_abe->equ[eqc->reg] + val * len,
		len * sizeof(u32));
	the_abe->equ_profile[eqc->reg] = val;

	pm_runtime_get_sync(the_abe->dev);
	abe_write_equalizer(eqc->reg + 1, &equ_params);
	pm_runtime_put_sync(the_abe->dev);

	return 1;
}

int snd_soc_info_enum_ext1(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
		snd_soc_get_enum_text(e, uinfo->value.enumerated.item));

	return 0;
}

static const char *route_ul_texts[] = {
	"None", "DMic0L", "DMic0R", "DMic1L", "DMic1R", "DMic2L", "DMic2R",
	"BT Left", "BT Right", "MMExt Left", "MMExt Right", "AMic0", "AMic1",
	"VX Left", "VX Right"
};

/* ROUTE_UL Mux table */
static const struct soc_enum abe_enum[] = {
		SOC_ENUM_SINGLE(MUX_MM_UL10, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL11, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL12, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL13, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL14, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL15, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL16, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL17, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL20, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_MM_UL21, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_VX_UL0, 0, 15, route_ul_texts),
		SOC_ENUM_SINGLE(MUX_VX_UL1, 0, 15, route_ul_texts),
};

static const struct snd_kcontrol_new mm_ul00_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[0],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul01_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[1],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul02_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[2],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul03_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[3],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul04_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[4],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul05_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[5],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul06_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[6],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul07_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[7],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul10_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[8],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_ul11_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[9],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_vx0_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[10],
	ul_mux_get_route, ul_mux_put_route);

static const struct snd_kcontrol_new mm_vx1_control =
	SOC_DAPM_ENUM_EXT("Route", abe_enum[11],
	ul_mux_get_route, ul_mux_put_route);

/* DL1 mixer paths */
static const struct snd_kcontrol_new dl1_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", MIX_DL1_INPUT_TONES, MIX_DL1_TONES, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Voice", MIX_DL1_INPUT_VX_DL, MIX_DL1_VOICE, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_DL1_INPUT_MM_UL2, MIX_DL1_CAPTURE, 1, 0,
		abe_get_mixer, dl1_put_mixer),
	SOC_SINGLE_EXT("Multimedia", MIX_DL1_INPUT_MM_DL, MIX_DL1_MEDIA, 1, 0,
		abe_get_mixer, dl1_put_mixer),
};

/* DL2 mixer paths */
static const struct snd_kcontrol_new dl2_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", MIX_DL2_INPUT_TONES, MIX_DL2_TONES, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Voice", MIX_DL2_INPUT_VX_DL, MIX_DL2_VOICE, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_DL2_INPUT_MM_UL2, MIX_DL2_CAPTURE, 1, 0,
		abe_get_mixer, dl2_put_mixer),
	SOC_SINGLE_EXT("Multimedia", MIX_DL2_INPUT_MM_DL, MIX_DL2_MEDIA, 1, 0,
		abe_get_mixer, dl2_put_mixer),
};

/* AUDUL ("Voice Capture Mixer") mixer paths */
static const struct snd_kcontrol_new audio_ul_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones Playback", MIX_AUDUL_INPUT_TONES, MIX_AUDUL_TONES, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
	SOC_SINGLE_EXT("Media Playback", MIX_AUDUL_INPUT_MM_DL, MIX_AUDUL_MEDIA, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
	SOC_SINGLE_EXT("Capture", MIX_AUDUL_INPUT_UPLINK, MIX_AUDUL_CAPTURE, 1, 0,
		abe_get_mixer, audio_ul_put_mixer),
};

/* VXREC ("Capture Mixer")  mixer paths */
static const struct snd_kcontrol_new vx_rec_mixer_controls[] = {
	SOC_SINGLE_EXT("Tones", MIX_VXREC_INPUT_TONES, MIX_VXREC_TONES, 1, 0,
		abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Voice Playback", MIX_VXREC_INPUT_VX_DL,
		MIX_VXREC_VOICE_PLAYBACK, 1, 0, abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Voice Capture", MIX_VXREC_INPUT_VX_UL,
		MIX_VXREC_VOICE_CAPTURE, 1, 0, abe_get_mixer, vxrec_put_mixer),
	SOC_SINGLE_EXT("Media Playback", MIX_VXREC_INPUT_MM_DL,
		MIX_VXREC_MEDIA, 1, 0, abe_get_mixer, vxrec_put_mixer),
};

/* SDT ("Sidetone Mixer") mixer paths */
static const struct snd_kcontrol_new sdt_mixer_controls[] = {
	SOC_SINGLE_EXT("Capture", MIX_SDT_INPUT_UP_MIXER, MIX_SDT_CAPTURE, 1, 0,
		abe_get_mixer, sdt_put_mixer),
	SOC_SINGLE_EXT("Playback", MIX_SDT_INPUT_DL1_MIXER, MIX_SDT_PLAYBACK, 1, 0,
		abe_get_mixer, sdt_put_mixer),
};

/* Virtual PDM_DL Switch */
static const struct snd_kcontrol_new pdm_dl1_switch_controls =
	SOC_SINGLE_EXT("Switch", ABE_VIRTUAL_SWITCH, MIX_SWITCH_PDM_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual BT_VX_DL Switch */
static const struct snd_kcontrol_new bt_vx_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", ABE_VIRTUAL_SWITCH, MIX_SWITCH_BT_VX_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

/* Virtual MM_EXT_DL Switch */
static const struct snd_kcontrol_new mm_ext_dl_switch_controls =
	SOC_SINGLE_EXT("Switch", ABE_VIRTUAL_SWITCH, MIX_SWITCH_MM_EXT_DL, 1, 0,
			abe_get_mixer, abe_put_switch);

static const struct snd_kcontrol_new abe_controls[] = {
	/* DL1 mixer gains */
	SOC_SINGLE_EXT_TLV("DL1 Media Playback Volume",
		MIX_DL1_INPUT_MM_DL, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, mm_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Tones Playback Volume",
		MIX_DL1_INPUT_TONES, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, tones_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Voice Playback Volume",
		MIX_DL1_INPUT_VX_DL, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, voice_dl1_tlv),
	SOC_SINGLE_EXT_TLV("DL1 Capture Playback Volume",
		MIX_DL1_INPUT_MM_UL2, 0, 149, 0,
		volume_get_dl1_mixer, volume_put_dl1_mixer, capture_dl1_tlv),

	/* DL2 mixer gains */
	SOC_SINGLE_EXT_TLV("DL2 Media Playback Volume",
		MIX_DL2_INPUT_MM_DL, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, mm_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Tones Playback Volume",
		MIX_DL2_INPUT_TONES, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, tones_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Voice Playback Volume",
		MIX_DL2_INPUT_VX_DL, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, voice_dl2_tlv),
	SOC_SINGLE_EXT_TLV("DL2 Capture Playback Volume",
		MIX_DL2_INPUT_MM_UL2, 0, 149, 0,
		volume_get_dl2_mixer, volume_put_dl2_mixer, capture_dl2_tlv),

	/* VXREC mixer gains */
	SOC_SINGLE_EXT_TLV("VXREC Media Volume",
		MIX_VXREC_INPUT_MM_DL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_mm_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Tones Volume",
		MIX_VXREC_INPUT_TONES, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_tones_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice DL Volume",
		MIX_VXREC_INPUT_VX_UL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_vx_dl_tlv),
	SOC_SINGLE_EXT_TLV("VXREC Voice UL Volume",
		MIX_VXREC_INPUT_VX_DL, 0, 149, 0,
		volume_get_vxrec_mixer, volume_put_vxrec_mixer, vxrec_vx_ul_tlv),

	/* AUDUL mixer gains */
	SOC_SINGLE_EXT_TLV("AUDUL Media Volume",
		MIX_AUDUL_INPUT_MM_DL, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_mm_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Tones Volume",
		MIX_AUDUL_INPUT_TONES, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_tones_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice UL Volume",
		MIX_AUDUL_INPUT_UPLINK, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_vx_ul_tlv),
	SOC_SINGLE_EXT_TLV("AUDUL Voice DL Volume",
		MIX_AUDUL_INPUT_VX_DL, 0, 149, 0,
		volume_get_audul_mixer, volume_put_audul_mixer, audul_vx_dl_tlv),

	/* SDT mixer gains */
	SOC_SINGLE_EXT_TLV("SDT UL Volume",
		MIX_SDT_INPUT_UP_MIXER, 0, 149, 0,
		volume_get_sdt_mixer, volume_put_sdt_mixer, sdt_ul_tlv),
	SOC_SINGLE_EXT_TLV("SDT DL Volume",
		MIX_SDT_INPUT_DL1_MIXER, 0, 149, 0,
		volume_get_sdt_mixer, volume_put_sdt_mixer, sdt_dl_tlv),

	/* DMIC gains */
	SOC_DOUBLE_EXT_TLV("DMIC1 UL Volume",
		GAINS_DMIC1, GAIN_LEFT_OFFSET, GAIN_RIGHT_OFFSET, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("DMIC2 UL Volume",
		GAINS_DMIC2, GAIN_LEFT_OFFSET, GAIN_RIGHT_OFFSET, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("DMIC3 UL Volume",
		GAINS_DMIC3, GAIN_LEFT_OFFSET, GAIN_RIGHT_OFFSET, 149, 0,
		volume_get_gain, volume_put_gain, dmic_tlv),

	SOC_DOUBLE_EXT_TLV("AMIC UL Volume",
		GAINS_AMIC, GAIN_LEFT_OFFSET, GAIN_RIGHT_OFFSET, 149, 0,
		volume_get_gain, volume_put_gain, amic_tlv),

	SOC_DOUBLE_EXT_TLV("BT UL Volume",
		GAINS_BTUL, GAIN_LEFT_OFFSET, GAIN_RIGHT_OFFSET, 149, 0,
		volume_get_gain, volume_put_gain, btul_tlv),
};

static const struct snd_soc_dapm_widget abe_dapm_widgets[] = {

	/* Frontend AIFs */
	SND_SOC_DAPM_AIF_IN("TONES_DL", "Tones Playback", 0,
			W_AIF_TONES_DL, ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VX_DL", "Voice Playback", 0,
			W_AIF_VX_DL, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("VX_UL", "Voice Capture", 0,
			W_AIF_VX_UL, ABE_OPP_50, 0),
	/* the MM_UL mapping is intentional */
	SND_SOC_DAPM_AIF_OUT("MM_UL1", "MultiMedia1 Capture", 0,
			W_AIF_MM_UL1, ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("MM_UL2", "MultiMedia2 Capture", 0,
			W_AIF_MM_UL2, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL", " MultiMedia1 Playback", 0,
			W_AIF_MM_DL, ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("MM_DL_LP", " MultiMedia1 LP Playback", 0,
			W_AIF_MM_DL_LP, ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("VIB_DL", "Vibra Playback", 0,
			W_AIF_VIB_DL, ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("MODEM_DL", "MODEM Playback", 0,
			W_AIF_MODEM_DL, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MODEM_UL", "MODEM Capture", 0,
			W_AIF_MODEM_UL, ABE_OPP_50, 0),

	/* Backend DAIs  */
	SND_SOC_DAPM_AIF_IN("PDM_UL1", "Analog Capture", 0,
			W_AIF_PDM_UL1, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL1", "HS Playback", 0,
			W_AIF_PDM_DL1, ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_DL2", "HF Playback", 0,
			W_AIF_PDM_DL2, ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_OUT("PDM_VIB", "Vibra Playback", 0,
			W_AIF_PDM_VIB, ABE_OPP_100, 0),
	SND_SOC_DAPM_AIF_IN("BT_VX_UL", "BT Capture", 0,
			W_AIF_BT_VX_UL, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("BT_VX_DL", "BT Playback", 0,
			W_AIF_BT_VX_DL, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("MM_EXT_UL", "FM Capture", 0,
			W_AIF_MM_EXT_UL, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_OUT("MM_EXT_DL", "FM Playback", 0,
			W_AIF_MM_EXT_DL, ABE_OPP_25, 0),
	SND_SOC_DAPM_AIF_IN("DMIC0", "DMIC0 Capture", 0,
			W_AIF_DMIC0, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC1", "DMIC1 Capture", 0,
			W_AIF_DMIC1, ABE_OPP_50, 0),
	SND_SOC_DAPM_AIF_IN("DMIC2", "DMIC2 Capture", 0,
			W_AIF_DMIC2, ABE_OPP_50, 0),

	/* ROUTE_UL Capture Muxes */
	SND_SOC_DAPM_MUX("MUX_UL00",
			W_MUX_UL00, ABE_OPP_50, 0, &mm_ul00_control),
	SND_SOC_DAPM_MUX("MUX_UL01",
			W_MUX_UL01, ABE_OPP_50, 0, &mm_ul01_control),
	SND_SOC_DAPM_MUX("MUX_UL02",
			W_MUX_UL02, ABE_OPP_50, 0, &mm_ul02_control),
	SND_SOC_DAPM_MUX("MUX_UL03",
			W_MUX_UL03, ABE_OPP_50, 0, &mm_ul03_control),
	SND_SOC_DAPM_MUX("MUX_UL04",
			W_MUX_UL04, ABE_OPP_50, 0, &mm_ul04_control),
	SND_SOC_DAPM_MUX("MUX_UL05",
			W_MUX_UL05, ABE_OPP_50, 0, &mm_ul05_control),
	SND_SOC_DAPM_MUX("MUX_UL06",
			W_MUX_UL06, ABE_OPP_50, 0, &mm_ul06_control),
	SND_SOC_DAPM_MUX("MUX_UL07",
			W_MUX_UL07, ABE_OPP_50, 0, &mm_ul07_control),
	SND_SOC_DAPM_MUX("MUX_UL10",
			W_MUX_UL10, ABE_OPP_50, 0, &mm_ul10_control),
	SND_SOC_DAPM_MUX("MUX_UL11",
			W_MUX_UL11, ABE_OPP_50, 0, &mm_ul11_control),
	SND_SOC_DAPM_MUX("MUX_VX0",
			W_MUX_VX00, ABE_OPP_50, 0, &mm_vx0_control),
	SND_SOC_DAPM_MUX("MUX_VX1",
			W_MUX_VX01, ABE_OPP_50, 0, &mm_vx1_control),

	/* DL1 & DL2 Playback Mixers */
	SND_SOC_DAPM_MIXER("DL1 Mixer",
			W_MIXER_DL1, ABE_OPP_25, 0, dl1_mixer_controls,
			ARRAY_SIZE(dl1_mixer_controls)),
	SND_SOC_DAPM_MIXER("DL2 Mixer",
			W_MIXER_DL2, ABE_OPP_100, 0, dl2_mixer_controls,
			ARRAY_SIZE(dl2_mixer_controls)),

	/* DL1 Mixer Input volumes ?????*/
	SND_SOC_DAPM_PGA("DL1 Media Volume",
			W_VOLUME_DL1, 0, 0, NULL, 0),

	/* AUDIO_UL_MIXER */
	SND_SOC_DAPM_MIXER("Voice Capture Mixer",
			W_MIXER_AUDIO_UL, ABE_OPP_50, 0, audio_ul_mixer_controls,
			ARRAY_SIZE(audio_ul_mixer_controls)),

	/* VX_REC_MIXER */
	SND_SOC_DAPM_MIXER("Capture Mixer",
			W_MIXER_VX_REC, ABE_OPP_50, 0, vx_rec_mixer_controls,
			ARRAY_SIZE(vx_rec_mixer_controls)),

	/* SDT_MIXER  - TODO: shoult this not be OPP25 ??? */
	SND_SOC_DAPM_MIXER("Sidetone Mixer",
			W_MIXER_SDT, ABE_OPP_25, 0, sdt_mixer_controls,
			ARRAY_SIZE(sdt_mixer_controls)),

	/*
	 * The Following three are virtual switches to select the output port
	 * after DL1 Gain.
	 */

	/* Virtual PDM_DL1 Switch */
	SND_SOC_DAPM_MIXER("DL1 PDM",
			W_VSWITCH_DL1_PDM, ABE_OPP_25, 0, &pdm_dl1_switch_controls, 1),

	/* Virtual BT_VX_DL Switch */
	SND_SOC_DAPM_MIXER("DL1 BT_VX",
			W_VSWITCH_DL1_BT_VX, ABE_OPP_50, 0, &bt_vx_dl_switch_controls, 1),

	/* Virtual MM_EXT_DL Switch TODO: confrm OPP level here */
	SND_SOC_DAPM_MIXER("DL1 MM_EXT",
			W_VSWITCH_DL1_MM_EXT, ABE_OPP_50, 0, &mm_ext_dl_switch_controls, 1),

	/* Virtuals to join our capture sources */
	SND_SOC_DAPM_MIXER("Sidetone Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Voice Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DL1 Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DL2 Capture VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Join our MM_DL and MM_DL_LP playback */
	SND_SOC_DAPM_MIXER("MM_DL VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Virtual MODEM and VX_UL mixer */
	SND_SOC_DAPM_MIXER("VX UL VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VX DL VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Virtual Pins to force backends ON atm */
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("BE_IN"),
};

static const struct snd_soc_dapm_route intercon[] = {

	/* MUX_UL00 - ROUTE_UL - Chan 0  */
	{"MUX_UL00", "DMic0L", "DMIC0"},
	{"MUX_UL00", "DMic0R", "DMIC0"},
	{"MUX_UL00", "DMic1L", "DMIC1"},
	{"MUX_UL00", "DMic1R", "DMIC1"},
	{"MUX_UL00", "DMic2L", "DMIC2"},
	{"MUX_UL00", "DMic2R", "DMIC2"},
	{"MUX_UL00", "BT Left", "BT_VX_UL"},
	{"MUX_UL00", "BT Right", "BT_VX_UL"},
	{"MUX_UL00", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL00", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL00", "AMic0", "PDM_UL1"},
	{"MUX_UL00", "AMic1", "PDM_UL1"},
	{"MUX_UL00", "VX Left", "Capture Mixer"},
	{"MUX_UL00", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL00"},

	/* MUX_UL01 - ROUTE_UL - Chan 1  */
	{"MUX_UL01", "DMic0L", "DMIC0"},
	{"MUX_UL01", "DMic0R", "DMIC0"},
	{"MUX_UL01", "DMic1L", "DMIC1"},
	{"MUX_UL01", "DMic1R", "DMIC1"},
	{"MUX_UL01", "DMic2L", "DMIC2"},
	{"MUX_UL01", "DMic2R", "DMIC2"},
	{"MUX_UL01", "BT Left", "BT_VX_UL"},
	{"MUX_UL01", "BT Right", "BT_VX_UL"},
	{"MUX_UL01", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL01", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL01", "AMic0", "PDM_UL1"},
	{"MUX_UL01", "AMic1", "PDM_UL1"},
	{"MUX_UL01", "VX Left", "Capture Mixer"},
	{"MUX_UL01", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL01"},

	/* MUX_UL02 - ROUTE_UL - Chan 2  */
	{"MUX_UL02", "DMic0L", "DMIC0"},
	{"MUX_UL02", "DMic0R", "DMIC0"},
	{"MUX_UL02", "DMic1L", "DMIC1"},
	{"MUX_UL02", "DMic1R", "DMIC1"},
	{"MUX_UL02", "DMic2L", "DMIC2"},
	{"MUX_UL02", "DMic2R", "DMIC2"},
	{"MUX_UL02", "BT Left", "BT_VX_UL"},
	{"MUX_UL02", "BT Right", "BT_VX_UL"},
	{"MUX_UL02", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL02", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL02", "AMic0", "PDM_UL1"},
	{"MUX_UL02", "AMic1", "PDM_UL1"},
	{"MUX_UL02", "VX Left", "Capture Mixer"},
	{"MUX_UL02", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL02"},

	/* MUX_UL03 - ROUTE_UL - Chan 3  */
	{"MUX_UL03", "DMic0L", "DMIC0"},
	{"MUX_UL03", "DMic0R", "DMIC0"},
	{"MUX_UL03", "DMic1L", "DMIC1"},
	{"MUX_UL03", "DMic1R", "DMIC1"},
	{"MUX_UL03", "DMic2L", "DMIC2"},
	{"MUX_UL03", "DMic2R", "DMIC2"},
	{"MUX_UL03", "BT Left", "BT_VX_UL"},
	{"MUX_UL03", "BT Right", "BT_VX_UL"},
	{"MUX_UL03", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL03", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL03", "AMic0", "PDM_UL1"},
	{"MUX_UL03", "AMic1", "PDM_UL1"},
	{"MUX_UL03", "VX Left", "Capture Mixer"},
	{"MUX_UL03", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL03"},

	/* MUX_UL04 - ROUTE_UL - Chan 4  */
	{"MUX_UL04", "DMic0L", "DMIC0"},
	{"MUX_UL04", "DMic0R", "DMIC0"},
	{"MUX_UL04", "DMic1L", "DMIC1"},
	{"MUX_UL04", "DMic1R", "DMIC1"},
	{"MUX_UL04", "DMic2L", "DMIC2"},
	{"MUX_UL04", "DMic2R", "DMIC2"},
	{"MUX_UL04", "BT Left", "BT_VX_UL"},
	{"MUX_UL04", "BT Right", "BT_VX_UL"},
	{"MUX_UL04", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL04", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL04", "AMic0", "PDM_UL1"},
	{"MUX_UL04", "AMic1", "PDM_UL1"},
	{"MUX_UL04", "VX Left", "Capture Mixer"},
	{"MUX_UL04", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL04"},

	/* MUX_UL05 - ROUTE_UL - Chan 5  */
	{"MUX_UL05", "DMic0L", "DMIC0"},
	{"MUX_UL05", "DMic0R", "DMIC0"},
	{"MUX_UL05", "DMic1L", "DMIC1"},
	{"MUX_UL05", "DMic1R", "DMIC1"},
	{"MUX_UL05", "DMic2L", "DMIC2"},
	{"MUX_UL05", "DMic2R", "DMIC2"},
	{"MUX_UL05", "BT Left", "BT_VX_UL"},
	{"MUX_UL05", "BT Right", "BT_VX_UL"},
	{"MUX_UL05", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL05", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL05", "AMic0", "PDM_UL1"},
	{"MUX_UL05", "AMic1", "PDM_UL1"},
	{"MUX_UL05", "VX Left", "Capture Mixer"},
	{"MUX_UL05", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL05"},

	/* MUX_UL06 - ROUTE_UL - Chan 6  */
	{"MUX_UL06", "DMic0L", "DMIC0"},
	{"MUX_UL06", "DMic0R", "DMIC0"},
	{"MUX_UL06", "DMic1L", "DMIC1"},
	{"MUX_UL06", "DMic1R", "DMIC1"},
	{"MUX_UL06", "DMic2L", "DMIC2"},
	{"MUX_UL06", "DMic2R", "DMIC2"},
	{"MUX_UL06", "BT Left", "BT_VX_UL"},
	{"MUX_UL06", "BT Right", "BT_VX_UL"},
	{"MUX_UL06", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL06", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL06", "AMic0", "PDM_UL1"},
	{"MUX_UL06", "AMic1", "PDM_UL1"},
	{"MUX_UL06", "VX Left", "Capture Mixer"},
	{"MUX_UL06", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL06"},

	/* MUX_UL07 - ROUTE_UL - Chan 7  */
	{"MUX_UL07", "DMic0L", "DMIC0"},
	{"MUX_UL07", "DMic0R", "DMIC0"},
	{"MUX_UL07", "DMic1L", "DMIC1"},
	{"MUX_UL07", "DMic1R", "DMIC1"},
	{"MUX_UL07", "DMic2L", "DMIC2"},
	{"MUX_UL07", "DMic2R", "DMIC2"},
	{"MUX_UL07", "BT Left", "BT_VX_UL"},
	{"MUX_UL07", "BT Right", "BT_VX_UL"},
	{"MUX_UL07", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL07", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL07", "AMic0", "PDM_UL1"},
	{"MUX_UL07", "AMic1", "PDM_UL1"},
	{"MUX_UL07", "VX Left", "Capture Mixer"},
	{"MUX_UL07", "VX Right", "Capture Mixer"},
	{"MM_UL1", NULL, "MUX_UL07"},

	/* MUX_UL10 - ROUTE_UL - Chan 10  */
	{"MUX_UL10", "DMic0L", "DMIC0"},
	{"MUX_UL10", "DMic0R", "DMIC0"},
	{"MUX_UL10", "DMic1L", "DMIC1"},
	{"MUX_UL10", "DMic1R", "DMIC1"},
	{"MUX_UL10", "DMic2L", "DMIC2"},
	{"MUX_UL10", "DMic2R", "DMIC2"},
	{"MUX_UL10", "BT Left", "BT_VX_UL"},
	{"MUX_UL10", "BT Right", "BT_VX_UL"},
	{"MUX_UL10", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL10", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL10", "AMic0", "PDM_UL1"},
	{"MUX_UL10", "AMic1", "PDM_UL1"},
	{"MUX_UL10", "VX Left", "Capture Mixer"},
	{"MUX_UL10", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL10"},

	/* MUX_UL11 - ROUTE_UL - Chan 11  */
	{"MUX_UL11", "DMic0L", "DMIC0"},
	{"MUX_UL11", "DMic0R", "DMIC0"},
	{"MUX_UL11", "DMic1L", "DMIC1"},
	{"MUX_UL11", "DMic1R", "DMIC1"},
	{"MUX_UL11", "DMic2L", "DMIC2"},
	{"MUX_UL11", "DMic2R", "DMIC2"},
	{"MUX_UL11", "BT Left", "BT_VX_UL"},
	{"MUX_UL11", "BT Right", "BT_VX_UL"},
	{"MUX_UL11", "MMExt Left", "MM_EXT_UL"},
	{"MUX_UL11", "MMExt Right", "MM_EXT_UL"},
	{"MUX_UL11", "AMic0", "PDM_UL1"},
	{"MUX_UL11", "AMic1", "PDM_UL1"},
	{"MUX_UL11", "VX Left", "Capture Mixer"},
	{"MUX_UL11", "VX Right", "Capture Mixer"},
	{"MM_UL2", NULL, "MUX_UL11"},

	/* MUX_VX0 - ROUTE_UL - Chan 20  */
	{"MUX_VX0", "DMic0L", "DMIC0"},
	{"MUX_VX0", "DMic0R", "DMIC0"},
	{"MUX_VX0", "DMic1L", "DMIC1"},
	{"MUX_VX0", "DMic1R", "DMIC1"},
	{"MUX_VX0", "DMic2L", "DMIC2"},
	{"MUX_VX0", "DMic2R", "DMIC2"},
	{"MUX_VX0", "BT Left", "BT_VX_UL"},
	{"MUX_VX0", "BT Right", "BT_VX_UL"},
	{"MUX_VX0", "MMExt Left", "MM_EXT_UL"},
	{"MUX_VX0", "MMExt Right", "MM_EXT_UL"},
	{"MUX_VX0", "AMic0", "PDM_UL1"},
	{"MUX_VX0", "AMic1", "PDM_UL1"},
	{"MUX_VX0", "VX Left", "Capture Mixer"},
	{"MUX_VX0", "VX Right", "Capture Mixer"},

	/* MUX_VX1 - ROUTE_UL - Chan 20  */
	{"MUX_VX1", "DMic0L", "DMIC0"},
	{"MUX_VX1", "DMic0R", "DMIC0"},
	{"MUX_VX1", "DMic1L", "DMIC1"},
	{"MUX_VX1", "DMic1R", "DMIC1"},
	{"MUX_VX1", "DMic2L", "DMIC2"},
	{"MUX_VX1", "DMic2R", "DMIC2"},
	{"MUX_VX1", "BT Left", "BT_VX_UL"},
	{"MUX_VX1", "BT Right", "BT_VX_UL"},
	{"MUX_VX1", "MMExt Left", "MM_EXT_UL"},
	{"MUX_VX1", "MMExt Right", "MM_EXT_UL"},
	{"MUX_VX1", "AMic0", "PDM_UL1"},
	{"MUX_VX1", "AMic1", "PDM_UL1"},
	{"MUX_VX1", "VX Left", "Capture Mixer"},
	{"MUX_VX1", "VX Right", "Capture Mixer"},

	/* Headset (DL1)  playback path */
	{"DL1 Mixer", "Tones", "TONES_DL"},
	{"DL1 Mixer", "Voice", "VX DL VMixer"},
	{"DL1 Mixer", "Capture", "DL1 Capture VMixer"},
	{"DL1 Capture VMixer", NULL, "MUX_UL10"},
	{"DL1 Capture VMixer", NULL, "MUX_UL11"},
	{"DL1 Mixer", "Multimedia", "MM_DL VMixer"},
	{"MM_DL VMixer", NULL, "MM_DL"},
	{"MM_DL VMixer", NULL, "MM_DL_LP"},

	/* Sidetone Mixer */
	{"Sidetone Mixer", "Playback", "DL1 Mixer"},
	{"Sidetone Mixer", "Capture", "Sidetone Capture VMixer"},
	{"Sidetone Capture VMixer", NULL, "MUX_VX0"},
	{"Sidetone Capture VMixer", NULL, "MUX_VX1"},

	/* Playback Output selection after DL1 Gain */
	{"DL1 BT_VX", "Switch", "Sidetone Mixer"},
	{"DL1 MM_EXT", "Switch", "Sidetone Mixer"},
	{"DL1 PDM", "Switch", "Sidetone Mixer"},
	{"PDM_DL1", NULL, "DL1 PDM"},
	{"BT_VX_DL", NULL, "DL1 BT_VX"},
	{"MM_EXT_DL", NULL, "DL1 MM_EXT"},

	/* Handsfree (DL2) playback path */
	{"DL2 Mixer", "Tones", "TONES_DL"},
	{"DL2 Mixer", "Voice", "VX DL VMixer"},
	{"DL2 Mixer", "Capture", "DL2 Capture VMixer"},
	{"DL2 Capture VMixer", NULL, "MUX_UL10"},
	{"DL2 Capture VMixer", NULL, "MUX_UL11"},
	{"DL2 Mixer", "Multimedia", "MM_DL VMixer"},
	{"MM_DL VMixer", NULL, "MM_DL"},
	{"MM_DL VMixer", NULL, "MM_DL_LP"},
	{"PDM_DL2", NULL, "DL2 Mixer"},

	/* VxREC Mixer */
	{"Capture Mixer", "Tones", "TONES_DL"},
	{"Capture Mixer", "Voice Playback", "VX DL VMixer"},
	{"Capture Mixer", "Voice Capture", "VX UL VMixer"},
	{"Capture Mixer", "Media Playback", "MM_DL VMixer"},
	{"MM_DL VMixer", NULL, "MM_DL"},
	{"MM_DL VMixer", NULL, "MM_DL_LP"},

	/* Audio UL mixer */
	{"Voice Capture Mixer", "Tones Playback", "TONES_DL"},
	{"Voice Capture Mixer", "Media Playback", "MM_DL VMixer"},
	{"MM_DL VMixer", NULL, "MM_DL"},
	{"MM_DL VMixer", NULL, "MM_DL_LP"},
	{"Voice Capture Mixer", "Capture", "Voice Capture VMixer"},
	{"Voice Capture VMixer", NULL, "MUX_VX0"},
	{"Voice Capture VMixer", NULL, "MUX_VX1"},

	/* BT */
	{"VX UL VMixer", NULL, "Voice Capture Mixer"},

	/* Vibra */
	{"PDM_VIB", NULL, "VIB_DL"},

	/* VX and MODEM */
	{"VX_UL", NULL, "VX UL VMixer"},
	{"MODEM_UL", NULL, "VX UL VMixer"},
	{"VX DL VMixer", NULL, "VX_DL"},
	{"VX DL VMixer", NULL, "MODEM_DL"},

	/* Backend Enablement - TODO: maybe re-work*/
	{"BE_OUT", NULL, "PDM_DL1"},
	{"BE_OUT", NULL, "PDM_DL2"},
	{"BE_OUT", NULL, "PDM_VIB"},
	{"BE_OUT", NULL, "MM_EXT_DL"},
	{"BE_OUT", NULL, "BT_VX_DL"},
	{"PDM_UL1", NULL, "BE_IN"},
	{"BT_VX_UL", NULL, "BE_IN"},
	{"MM_EXT_UL", NULL, "BE_IN"},
	{"DMIC0", NULL, "BE_IN"},
	{"DMIC1", NULL, "BE_IN"},
	{"DMIC2", NULL, "BE_IN"},
};

#ifdef CONFIG_DEBUG_FS

static int abe_dbg_get_dma_pos(struct abe_data *abe)
{
	return omap_get_dma_dst_pos(abe->dma_ch) - abe->dbg_buffer_addr;
}

static void abe_dbg_dma_irq(int ch, u16 stat, void *data)
{
}

static int abe_dbg_start_dma(struct abe_data *abe, int circular)
{
	struct omap_dma_channel_params dma_params;
	int err;

	/* TODO: start the DMA in either :-
	 *
	 * 1) circular buffer mode where the DMA will restart when it get to
	 *    the end of the buffer.
	 * 2) default mode, where DMA stops at the end of the buffer.
	 */

	abe->dma_req = OMAP44XX_DMA_ABE_REQ_7;
	err = omap_request_dma(abe->dma_req, "ABE debug",
			       abe_dbg_dma_irq, abe, &abe->dma_ch);
	if (abe->dbg_circular) {
		/*
		 * Link channel with itself so DMA doesn't need any
		 * reprogramming while looping the buffer
		 */
		omap_dma_link_lch(abe->dma_ch, abe->dma_ch);
	}

	memset(&dma_params, 0, sizeof(dma_params));
	dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
	dma_params.trigger = abe->dma_req;
	dma_params.sync_mode = OMAP_DMA_SYNC_FRAME;
	dma_params.src_amode = OMAP_DMA_AMODE_DOUBLE_IDX;
	dma_params.dst_amode = OMAP_DMA_AMODE_POST_INC;
	dma_params.src_or_dst_synch = OMAP_DMA_SRC_SYNC;
	dma_params.src_start = D_DEBUG_FIFO_ADDR + ABE_DMEM_BASE_ADDRESS_L3;
	dma_params.dst_start = abe->dbg_buffer_addr;
	dma_params.src_port = OMAP_DMA_PORT_MPUI;
	dma_params.src_ei = 1;
	dma_params.src_fi = 1 - abe->dbg_elem_bytes;

	dma_params.elem_count = abe->dbg_elem_bytes >> 2; /* 128 bytes shifted into words */
	dma_params.frame_count = abe->dbg_buffer_bytes / abe->dbg_elem_bytes;
	omap_set_dma_params(abe->dma_ch, &dma_params);

	omap_enable_dma_irq(abe->dma_ch, OMAP_DMA_FRAME_IRQ);
	omap_set_dma_src_burst_mode(abe->dma_ch, OMAP_DMA_DATA_BURST_16);
	omap_set_dma_dest_burst_mode(abe->dma_ch, OMAP_DMA_DATA_BURST_16);

	abe->dbg_reader_offset = 0;

	pm_runtime_get_sync(abe->dev);
	omap_start_dma(abe->dma_ch);
	return 0;
}

static void abe_dbg_stop_dma(struct abe_data *abe)
{
	while (omap_get_dma_active_status(abe->dma_ch))
		omap_stop_dma(abe->dma_ch);

	if (abe->dbg_circular)
		omap_dma_unlink_lch(abe->dma_ch, abe->dma_ch);
	omap_free_dma(abe->dma_ch);
	pm_runtime_put_sync(abe->dev);
}

static int abe_open_data(struct inode *inode, struct file *file)
{
	struct abe_data *abe = inode->i_private;

	abe->dbg_elem_bytes = 128; /* size of debug data per tick */

	if (abe->dbg_format1)
		abe->dbg_elem_bytes += ABE_DBG_FLAG1_SIZE;
	if (abe->dbg_format2)
		abe->dbg_elem_bytes += ABE_DBG_FLAG2_SIZE;
	if (abe->dbg_format3)
		abe->dbg_elem_bytes += ABE_DBG_FLAG3_SIZE;

	abe->dbg_buffer_bytes = abe->dbg_elem_bytes * 4 *
							abe->dbg_buffer_msecs;

	abe->dbg_buffer = dma_alloc_writecombine(abe->dev,
			abe->dbg_buffer_bytes, &abe->dbg_buffer_addr, GFP_KERNEL);
	if (abe->dbg_buffer == NULL)
		return -ENOMEM;

	file->private_data = inode->i_private;
	abe->dbg_complete = 0;
	abe_dbg_start_dma(abe, abe->dbg_circular);

	return 0;
}

static int abe_release_data(struct inode *inode, struct file *file)
{
	struct abe_data *abe = inode->i_private;

	abe_dbg_stop_dma(abe);

	dma_free_writecombine(abe->dev, abe->dbg_buffer_bytes,
				      abe->dbg_buffer, abe->dbg_buffer_addr);
	return 0;
}

static ssize_t abe_copy_to_user(struct abe_data *abe, char __user *user_buf,
			       size_t count)
{
	/* check for reader buffer wrap */
	if (abe->dbg_reader_offset + count > abe->dbg_buffer_bytes) {
		int size = abe->dbg_buffer_bytes - abe->dbg_reader_offset;

		/* wrap */
		if (copy_to_user(user_buf,
			abe->dbg_buffer + abe->dbg_reader_offset, size))
			return -EFAULT;

		/* need to just return if non circular */
		if (!abe->dbg_circular) {
			abe->dbg_complete = 1;
			return count;
		}

		if (copy_to_user(user_buf,
			abe->dbg_buffer, count - size))
			return -EFAULT;
		abe->dbg_reader_offset = count - size;
		return count;
	} else {
		/* no wrap */
		if (copy_to_user(user_buf,
			abe->dbg_buffer + abe->dbg_reader_offset, count))
			return -EFAULT;
		abe->dbg_reader_offset += count;

		if (!abe->dbg_circular &&
				abe->dbg_reader_offset == abe->dbg_buffer_bytes)
			abe->dbg_complete = 1;

		return count;
	}
}

static ssize_t abe_read_data(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	struct abe_data *abe = file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	int dma_offset, bytes;

	add_wait_queue(&abe->wait, &wait);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		/* TODO: Check if really needed. Or adjust sleep delay
		 * If not delay trace is not working */
		msleep_interruptible(1);
		dma_offset = abe_dbg_get_dma_pos(abe);

		/* is DMA finished ? */
		if (abe->dbg_complete)
			break;

		/* get maximum amount of debug bytes we can read */
		if (dma_offset >= abe->dbg_reader_offset) {
			/* dma ptr is ahead of reader */
			bytes = dma_offset - abe->dbg_reader_offset;
		} else {
			/* dma ptr is behind reader */
			bytes = dma_offset + abe->dbg_buffer_bytes -
				abe->dbg_reader_offset;
		}

		if (count > bytes)
			count = bytes;

		if (count > 0) {
			ret = abe_copy_to_user(abe, user_buf, count);
			break;
		}

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		schedule();

	} while (1);

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&abe->wait, &wait);

	return ret;
}

static const struct file_operations abe_data_fops = {
	.open = abe_open_data,
	.read = abe_read_data,
	.release = abe_release_data,
};

static void abe_init_debugfs(struct abe_data *abe)
{
	abe->debugfs_root = debugfs_create_dir("omap4-abe", NULL);
	if (!abe->debugfs_root) {
		printk(KERN_WARNING "ABE: Failed to create debugfs directory\n");
		return;
	}

	abe->debugfs_fmt1 = debugfs_create_bool("format1", 0644,
						 abe->debugfs_root,
						 &abe->dbg_format1);
	if (!abe->debugfs_fmt1)
		printk(KERN_WARNING "ABE: Failed to create format1 debugfs file\n");

	abe->debugfs_fmt2 = debugfs_create_bool("format2", 0644,
						 abe->debugfs_root,
						 &abe->dbg_format2);
	if (!abe->debugfs_fmt2)
		printk(KERN_WARNING "ABE: Failed to create format2 debugfs file\n");

	abe->debugfs_fmt3 = debugfs_create_bool("format3", 0644,
						 abe->debugfs_root,
						 &abe->dbg_format3);
	if (!abe->debugfs_fmt3)
		printk(KERN_WARNING "ABE: Failed to create format3 debugfs file\n");

	abe->debugfs_elem_bytes = debugfs_create_u32("element_bytes", 0604,
						 abe->debugfs_root,
						 &abe->dbg_elem_bytes);
	if (!abe->debugfs_elem_bytes)
		printk(KERN_WARNING "ABE: Failed to create element size debugfs file\n");

	abe->debugfs_size = debugfs_create_u32("msecs", 0644,
						 abe->debugfs_root,
						 &abe->dbg_buffer_msecs);
	if (!abe->debugfs_size)
		printk(KERN_WARNING "ABE: Failed to create buffer size debugfs file\n");

	abe->debugfs_circ = debugfs_create_bool("circular", 0644,
						 abe->debugfs_root,
						 &abe->dbg_circular);
	if (!abe->debugfs_size)
		printk(KERN_WARNING "ABE: Failed to create circular mode debugfs file\n");

	abe->debugfs_data = debugfs_create_file("debug", 0644,
						 abe->debugfs_root,
						 abe, &abe_data_fops);
	if (!abe->debugfs_data)
		printk(KERN_WARNING "ABE: Failed to create data debugfs file\n");

	abe->debugfs_opp_level = debugfs_create_u32("opp_level", 0604,
						 abe->debugfs_root,
						 &abe->opp);
	if (!abe->debugfs_opp_level)
		printk(KERN_WARNING "ABE: Failed to create OPP level debugfs file\n");

	abe->dbg_buffer_msecs = 500;
	init_waitqueue_head(&abe->wait);
}

static void abe_cleanup_debugfs(struct abe_data *abe)
{
	debugfs_remove_recursive(abe->debugfs_root);
}

#else

static inline void abe_init_debugfs(struct abe_data *abe)
{
}

static inline void abe_cleanup_debugfs(struct abe_data *abe)
{
}
#endif

static const struct snd_pcm_hardware omap_abe_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= 4 * 1024,
	.period_bytes_max	= 24 * 1024,
	.periods_min		= 2,
	.periods_max		= 2,
	.buffer_bytes_max	= 24 * 1024 * 2,
};


static int abe_set_opp_mode(struct abe_data *abe)
{
	int i, opp = 0;

	/* now calculate OPP level based upon DAPM widget status */
	for (i = 0; i < ABE_NUM_WIDGETS; i++) {
		if (abe->widget_opp[ABE_WIDGET(i)]) {
			dev_dbg(abe->dev, "OPP: id %d = %d%%\n", i,
					abe->widget_opp[ABE_WIDGET(i)] * 25);
			opp |= abe->widget_opp[ABE_WIDGET(i)];
		}
	}
	opp = (1 << (fls(opp) - 1)) * 25;

	if (abe->opp > opp) {
		/* Decrease OPP mode - no need of OPP100% */
		switch (opp) {
		case 25:
			abe_set_opp_processing(ABE_OPP25);
			udelay(250);
			omap_device_set_rate(abe->dev, abe->dev, 49150000);
			break;
		case 50:
		default:
			abe_set_opp_processing(ABE_OPP50);
			udelay(250);
			omap_device_set_rate(abe->dev, abe->dev, 98300000);
			break;
		}
	} else if (abe->opp < opp) {
		/* Increase OPP mode */
		switch (opp) {
		case 25:
			omap_device_set_rate(abe->dev, abe->dev, 49000000);
			abe_set_opp_processing(ABE_OPP25);
			break;
		case 50:
			omap_device_set_rate(abe->dev, abe->dev, 98300000);
			abe_set_opp_processing(ABE_OPP50);
			break;
		case 100:
		default:
			omap_device_set_rate(abe->dev, abe->dev, 196600000);
			abe_set_opp_processing(ABE_OPP100);
			break;
		}
	}
	abe->opp = opp;
	dev_dbg(abe->dev, "new OPP level is %d\n", opp);

	return 0;
}

static int aess_set_runtime_opp_level(struct abe_data *abe)
{
	mutex_lock(&abe->opp_mutex);

	pm_runtime_get_sync(abe->dev);
	abe_set_opp_mode(abe);
	pm_runtime_put_sync(abe->dev);

	mutex_unlock(&abe->opp_mutex);

	return 0;
}

static int aess_save_context(struct abe_data *abe)
{
	struct omap4_abe_dsp_pdata *pdata = abe->abe_pdata;

	/* TODO: Find a better way to save/retore gains after OFF mode */

	abe_mute_gain(MIXSDT, MIX_SDT_INPUT_UP_MIXER);
	abe_mute_gain(MIXSDT, MIX_SDT_INPUT_DL1_MIXER);
	abe_mute_gain(MIXAUDUL, MIX_AUDUL_INPUT_MM_DL);
	abe_mute_gain(MIXAUDUL, MIX_AUDUL_INPUT_TONES);
	abe_mute_gain(MIXAUDUL, MIX_AUDUL_INPUT_UPLINK);
	abe_mute_gain(MIXAUDUL, MIX_AUDUL_INPUT_VX_DL);
	abe_mute_gain(MIXVXREC, MIX_VXREC_INPUT_TONES);
	abe_mute_gain(MIXVXREC, MIX_VXREC_INPUT_VX_DL);
	abe_mute_gain(MIXVXREC, MIX_VXREC_INPUT_MM_DL);
	abe_mute_gain(MIXVXREC, MIX_VXREC_INPUT_VX_UL);
	abe_mute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
	abe_mute_gain(MIXDL1, MIX_DL1_INPUT_MM_UL2);
	abe_mute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
	abe_mute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
	abe_mute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
	abe_mute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
	abe_mute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
	abe_mute_gain(MIXDL2, MIX_DL2_INPUT_MM_UL2);
	abe_mute_gain(MIXECHO, MIX_ECHO_DL1);
	abe_mute_gain(MIXECHO, MIX_ECHO_DL2);
	abe_mute_gain(GAINS_DMIC1, GAIN_LEFT_OFFSET);
	abe_mute_gain(GAINS_DMIC1, GAIN_RIGHT_OFFSET);
	abe_mute_gain(GAINS_DMIC2, GAIN_LEFT_OFFSET);
	abe_mute_gain(GAINS_DMIC2, GAIN_RIGHT_OFFSET);
	abe_mute_gain(GAINS_DMIC3, GAIN_LEFT_OFFSET);
	abe_mute_gain(GAINS_DMIC3, GAIN_RIGHT_OFFSET);
	abe_mute_gain(GAINS_AMIC, GAIN_LEFT_OFFSET);
	abe_mute_gain(GAINS_AMIC, GAIN_RIGHT_OFFSET);

	if (pdata->get_context_loss_count)
	        abe->loss_count = pdata->get_context_loss_count(abe->dev);

	return 0;
}

static int aess_restore_context(struct abe_data *abe)
{
	struct omap4_abe_dsp_pdata *pdata = abe->abe_pdata;
	int loss_count = 0;

	omap_device_set_rate(&abe->dev, &abe->dev, 98000000);

	if (pdata->get_context_loss_count)
		loss_count = pdata->get_context_loss_count(abe->dev);

	if  (loss_count != the_abe->loss_count)
	        abe_reload_fw(abe->firmware);

	/* TODO: Find a better way to save/retore gains after dor OFF mode */
	abe_unmute_gain(MIXSDT, MIX_SDT_INPUT_UP_MIXER);
	abe_unmute_gain(MIXSDT, MIX_SDT_INPUT_DL1_MIXER);
	abe_unmute_gain(MIXAUDUL, MIX_AUDUL_INPUT_MM_DL);
	abe_unmute_gain(MIXAUDUL, MIX_AUDUL_INPUT_TONES);
	abe_unmute_gain(MIXAUDUL, MIX_AUDUL_INPUT_UPLINK);
	abe_unmute_gain(MIXAUDUL, MIX_AUDUL_INPUT_VX_DL);
	abe_unmute_gain(MIXVXREC, MIX_VXREC_INPUT_TONES);
	abe_unmute_gain(MIXVXREC, MIX_VXREC_INPUT_VX_DL);
	abe_unmute_gain(MIXVXREC, MIX_VXREC_INPUT_MM_DL);
	abe_unmute_gain(MIXVXREC, MIX_VXREC_INPUT_VX_UL);
	abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
	abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_MM_UL2);
	abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
	abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
	abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
	abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
	abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
	abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_MM_UL2);
	abe_unmute_gain(MIXECHO, MIX_ECHO_DL1);
	abe_unmute_gain(MIXECHO, MIX_ECHO_DL2);
	abe_unmute_gain(GAINS_DMIC1, GAIN_LEFT_OFFSET);
	abe_unmute_gain(GAINS_DMIC1, GAIN_RIGHT_OFFSET);
	abe_unmute_gain(GAINS_DMIC2, GAIN_LEFT_OFFSET);
	abe_unmute_gain(GAINS_DMIC2, GAIN_RIGHT_OFFSET);
	abe_unmute_gain(GAINS_DMIC3, GAIN_LEFT_OFFSET);
	abe_unmute_gain(GAINS_DMIC3, GAIN_RIGHT_OFFSET);
	abe_unmute_gain(GAINS_AMIC, GAIN_LEFT_OFFSET);
	abe_unmute_gain(GAINS_AMIC, GAIN_RIGHT_OFFSET);
/*
	abe_dsp_set_equalizer(EQ1, abe->dl1_equ_profile);
	abe_dsp_set_equalizer(EQ2L, abe->dl20_equ_profile);
	abe_dsp_set_equalizer(EQ2R, abe->dl21_equ_profile);
	abe_dsp_set_equalizer(EQAMIC, abe->amic_equ_profile);
	abe_dsp_set_equalizer(EQDMIC, abe->dmic_equ_profile);
	abe_dsp_set_equalizer(EQSDT, abe->sdt_equ_profile);
*/
	abe_set_router_configuration(UPROUTE, 0, (u32 *)abe->router);

       return 0;
}

static int aess_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;
	int ret = 0;

	mutex_lock(&abe->mutex);

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	pm_runtime_get_sync(abe->dev);

	if (!abe->active++) {
		abe->opp = 0;
		aess_restore_context(abe);
		abe_set_opp_mode(abe);
		abe_wakeup();
	}

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MODEM:
		break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		snd_soc_set_runtime_hwparams(substream, &omap_abe_hardware);
		ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 1024);
		break;
	default:
		break;
	}

	mutex_unlock(&abe->mutex);
	return ret;
}

static int aess_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_platform *platform = rtd->platform;
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;
	abe_data_format_t format;
	size_t period_size;
	u32 dst;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id != ABE_FRONTEND_DAI_LP_MEDIA)
		return 0;

	/*Storing substream pointer for irq*/
	abe->ping_pong_substream = substream;

	format.f = params_rate(params);
	if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE)
		format.samp_format = STEREO_MSB;
	else
		format.samp_format = STEREO_16_16;

	if (format.f == 44100)
		abe_write_event_generator(EVENT_44100);

	period_size = params_period_bytes(params);

	/*Adding ping pong buffer subroutine*/
	abe_plug_subroutine(&abe_irq_pingpong_player_id,
				(abe_subroutine2) abe_irq_pingpong_subroutine,
				SUB_1_PARAM, (u32 *)abe);

	/* Connect a Ping-Pong cache-flush protocol to MM_DL port */
	abe_connect_irq_ping_pong_port(MM_DL_PORT, &format,
				abe_irq_pingpong_player_id,
				period_size, &dst,
				PING_PONG_WITH_MCU_IRQ);

	/* Memory mapping for hw params */
	runtime->dma_area  = abe->io_base[0] + dst;
	runtime->dma_addr  = 0;
	runtime->dma_bytes = period_size * 2;

	/* Need to set the first buffer in order to get interrupt */
	abe_set_ping_pong_buffer(MM_DL_PORT, period_size);
	abe->first_irq = 1;

	return 0;
}

static int aess_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;

	mutex_lock(&abe->mutex);
	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);
	aess_set_runtime_opp_level(abe);
	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_platform *platform = rtd->platform;
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	struct snd_soc_dai *dai = rtd->cpu_dai;

	mutex_lock(&abe->mutex);
	aess_set_runtime_opp_level(abe);

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (!--abe->active) {
		abe_disable_irq();
		aess_save_context(abe);
		abe_dsp_shutdown();
	}

	pm_runtime_put_sync(abe->dev);

	mutex_unlock(&abe->mutex);
	return 0;
}

static int aess_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime  *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	int offset, size, err;

	if (dai->id != ABE_FRONTEND_DAI_LP_MEDIA)
		return -EINVAL;

	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;

	err = io_remap_pfn_range(vma, vma->vm_start,
			(ABE_DMEM_BASE_ADDRESS_MPU +
			ABE_DMEM_BASE_OFFSET_PING_PONG + offset) >> PAGE_SHIFT,
			size, vma->vm_page_prot);

	if (err)
		return -EAGAIN;

	return 0;
}

static snd_pcm_uframes_t aess_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t offset;
	u32 pingpong;

	abe_read_offset_from_ping_buffer(MM_DL_PORT, &pingpong);
	offset = (snd_pcm_uframes_t)pingpong;

	return offset;
}

static struct snd_pcm_ops omap_aess_pcm_ops = {
	.open           = aess_open,
	.hw_params	= aess_hw_params,
	.prepare	= aess_prepare,
	.close	        = aess_close,
	.pointer	= aess_pointer,
	.mmap		= aess_mmap,
};

#if CONFIG_PM
static int aess_suspend(struct device *dev)
{
	struct abe_data *abe = dev_get_drvdata(dev);

	pm_runtime_get_sync(abe->dev);

	aess_save_context(abe);

	pm_runtime_put_sync(abe->dev);

	return 0;
}

static int aess_resume(struct device *dev)
{
	struct abe_data *abe = dev_get_drvdata(dev);

	pm_runtime_get_sync(abe->dev);

	aess_restore_context(abe);

	pm_runtime_put_sync(abe->dev);

	return 0;
}

#else
#define aess_suspend	NULL
#define aess_resume	NULL
#endif

static const struct dev_pm_ops aess_pm_ops = {
	.suspend = aess_suspend,
	.resume = aess_resume,
};

static int aess_stream_event(struct snd_soc_dapm_context *dapm)
{
	struct snd_soc_platform *platform = dapm->platform;
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);

	pm_runtime_get_sync(abe->dev);

	if (abe->active)
		aess_set_runtime_opp_level(abe);

	pm_runtime_put_sync(abe->dev);

	return 0;
}

static int abe_add_widgets(struct snd_soc_platform *platform)
{
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	struct fw_header *hdr = &abe->hdr;
	int i, j;

	/* create equalizer controls */
	for (i = 0; i < hdr->num_equ; i++) {
		struct soc_enum *equalizer_enum = &abe->equalizer_enum[i];
		struct snd_kcontrol_new *equalizer_control =
				&abe->equalizer_control[i];

		equalizer_enum->reg = i;
		equalizer_enum->max = abe->equ_texts[i].count;
		for (j = 0; j < abe->equ_texts[i].count; j++)
			equalizer_enum->dtexts[j] = abe->equ_texts[i].texts[j];

		equalizer_control->name = abe->equ_texts[i].name;
		equalizer_control->private_value = (unsigned long)equalizer_enum;
		equalizer_control->get = abe_get_equalizer;
		equalizer_control->put = abe_put_equalizer;
		equalizer_control->info = snd_soc_info_enum_ext1;
		equalizer_control->iface = SNDRV_CTL_ELEM_IFACE_MIXER;

		dev_dbg(platform->dev, "added EQU mixer: %s profiles %d\n",
				abe->equ_texts[i].name, abe->equ_texts[i].count);

		for (j = 0; j < abe->equ_texts[i].count; j++)
			dev_dbg(platform->dev, " %s\n", equalizer_enum->dtexts[j]);
	}

	snd_soc_add_platform_controls(platform, abe->equalizer_control,
			hdr->num_equ);

	snd_soc_add_platform_controls(platform, abe_controls,
			ARRAY_SIZE(abe_controls));

	snd_soc_dapm_new_controls(&platform->dapm, abe_dapm_widgets,
				 ARRAY_SIZE(abe_dapm_widgets));

	snd_soc_dapm_add_routes(&platform->dapm, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(&platform->dapm);

	return 0;
}

static int abe_probe(struct snd_soc_platform *platform)
{
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	u8 *fw_data;
	int i, offset = 0;
	int ret = 0;
#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
	const struct firmware *fw;
#endif

	abe->platform = platform;

	pm_runtime_enable(abe->dev);

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
	/* request firmware & coefficients */
	ret = request_firmware(&fw, "omap4_abe", platform->dev);
	if (ret != 0) {
		dev_err(abe->dev, "Failed to load firmware: %d\n", ret);
		return ret;
	}
	fw_data = fw->data;
#else
	fw_data = (u8 *)abe_get_default_fw();
#endif

	/* get firmware and coefficients header info */
	memcpy(&abe->hdr, fw_data, sizeof(struct fw_header));
	if (abe->hdr.firmware_size > ABE_MAX_FW_SIZE) {
			dev_err(abe->dev, "Firmware too large at %d bytes: %d\n",
					abe->hdr.firmware_size, ret);
			ret = -EINVAL;
			goto err_fw;
	}
	dev_dbg(abe->dev, "ABE firmware size %d bytes\n", abe->hdr.firmware_size);

	if (abe->hdr.coeff_size > ABE_MAX_COEFF_SIZE) {
		dev_err(abe->dev, "Coefficients too large at %d bytes: %d\n",
					abe->hdr.coeff_size, ret);
			ret = -EINVAL;
			goto err_fw;
	}
	dev_dbg(abe->dev, "ABE coefficients size %d bytes\n", abe->hdr.coeff_size);

	/* get coefficient EQU mixer strings */
	if (abe->hdr.num_equ >= ABE_MAX_EQU) {
		dev_err(abe->dev, "Too many equalizers got %d\n", abe->hdr.num_equ);
		ret = -EINVAL;
		goto err_fw;
	}
	abe->equ_texts = kzalloc(abe->hdr.num_equ * sizeof(struct coeff_config),
			GFP_KERNEL);
	if (abe->equ_texts == NULL) {
		ret = -ENOMEM;
		goto err_fw;
	}
	offset = sizeof(struct fw_header);
	memcpy(abe->equ_texts, fw_data + offset,
			abe->hdr.num_equ * sizeof(struct coeff_config));

	/* get coefficients from firmware */
	abe->equ[0] = kmalloc(abe->hdr.coeff_size, GFP_KERNEL);
	if (abe->equ[0] == NULL) {
		ret = -ENOMEM;
		goto err_equ;
	}
	offset += abe->hdr.num_equ * sizeof(struct coeff_config);
	memcpy(abe->equ[0], fw_data + offset, abe->hdr.coeff_size);

	/* allocate coefficient mixer texts */
	dev_dbg(abe->dev, "loaded %d equalizers\n", abe->hdr.num_equ);
	for (i = 0; i < abe->hdr.num_equ; i++) {
		dev_dbg(abe->dev, "equ %d: %s profiles %d\n", i,
				abe->equ_texts[i].name, abe->equ_texts[i].count);
		if (abe->equ_texts[i].count >= ABE_MAX_PROFILES) {
			dev_err(abe->dev, "Too many profiles got %d for equ %d\n",
					abe->equ_texts[i].count, i);
			ret = -EINVAL;
			goto err_texts;
		}
		abe->equalizer_enum[i].dtexts =
				kzalloc(abe->equ_texts[i].count * sizeof(char *), GFP_KERNEL);
		if (abe->equalizer_enum[i].dtexts == NULL) {
			ret = -ENOMEM;
			goto err_texts;
		}
	}

	/* initialise coefficient equalizers */
	for (i = 1; i < abe->hdr.num_equ; i++) {
		abe->equ[i] = abe->equ[i - 1] +
			abe->equ_texts[i - 1].count * abe->equ_texts[i - 1].coeff;
	}

	/* store ABE firmware for later context restore */
	abe->firmware = kzalloc(abe->hdr.firmware_size, GFP_KERNEL);
	memcpy(abe->firmware,
		fw_data + sizeof(struct fw_header) + abe->hdr.coeff_size,
		abe->hdr.firmware_size);

	ret = request_irq(abe->irq, abe_irq_handler, 0, "ABE", (void *)abe);
	if (ret) {
		dev_err(platform->dev, "request for ABE IRQ %d failed %d\n",
				abe->irq, ret);
		goto err_texts;
	}

	/* aess_clk has to be enabled to access hal register.
	 * Disable the clk after it has been used.
	 */
	pm_runtime_get_sync(abe->dev);

	abe_init_mem(abe->io_base);

	abe_reset_hal();

	abe_load_fw(abe->firmware);

	/* Config OPP 100 for now */
	abe_set_opp_processing(ABE_OPP100);

	/* "tick" of the audio engine */
	abe_write_event_generator(EVENT_TIMER);
	/* Stop the engine */
	abe_stop_event_generator();
	abe_disable_irq();

	pm_runtime_put_sync(abe->dev);
	abe_add_widgets(platform);

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
	release_firmware(fw);
#endif
	return ret;

err_texts:
	kfree(abe->firmware);
	for (i = 0; i < abe->hdr.num_equ; i++)
		kfree(abe->equalizer_enum[i].texts);
	kfree(abe->equ[0]);
err_equ:
	kfree(abe->equ_texts);
err_fw:
#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
	release_firmware(fw);
#endif
	return ret;
}

static int abe_remove(struct snd_soc_platform *platform)
{
	struct abe_data *abe = snd_soc_platform_get_drvdata(platform);
	int i;

	free_irq(abe->irq, (void *)abe);

	for (i = 0; i < abe->hdr.num_equ; i++)
		kfree(abe->equalizer_enum[i].texts);

	kfree(abe->equ[0]);
	kfree(abe->equ_texts);
	kfree(abe->firmware);

	pm_runtime_disable(abe->dev);

	return 0;
}

static struct snd_soc_platform_driver omap_aess_platform = {
	.ops		= &omap_aess_pcm_ops,
	.probe		= abe_probe,
	.remove		= abe_remove,
	.read		= abe_dsp_read,
	.write		= abe_dsp_write,
	.stream_event = aess_stream_event,
};

static int __devinit abe_engine_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct omap4_abe_dsp_pdata *pdata = pdev->dev.platform_data;
	struct abe_data *abe;
	int ret = -EINVAL, i;

	abe = kzalloc(sizeof(struct abe_data), GFP_KERNEL);
	if (abe == NULL)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, abe);
	the_abe = abe;

	/* ZERO_labelID should really be 0 */
	for (i = 0; i < ABE_ROUTES_UL + 2; i++)
		abe->router[i] = ZERO_labelID;

	for (i = 0; i < 5; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   abe_memory_bank[i]);
		if (res == NULL) {
			dev_err(&pdev->dev, "no resource %s\n",
				abe_memory_bank[i]);
			goto err;
		}
		abe->io_base[i] = ioremap(res->start, resource_size(res));
		if (!abe->io_base[i]) {
			ret = -ENOMEM;
			goto err;
		}
	}

	abe->irq = platform_get_irq(pdev, 0);
	if (abe->irq < 0) {
		ret = abe->irq;
		goto err;
	}

	abe->abe_pdata = pdata;
	abe->dev = &pdev->dev;
	mutex_init(&abe->mutex);
	mutex_init(&abe->opp_mutex);

	ret = snd_soc_register_platform(abe->dev,
			&omap_aess_platform);
	if (ret < 0)
		return ret;

	abe_init_debugfs(abe);
	return ret;

err:
	for (--i; i >= 0; i--)
		iounmap(abe->io_base[i]);
	kfree(abe);
	return ret;
}

static int __devexit abe_engine_remove(struct platform_device *pdev)
{
	struct abe_data *abe = dev_get_drvdata(&pdev->dev);
	int i;

	abe_cleanup_debugfs(abe);
	snd_soc_unregister_platform(&pdev->dev);
	for (i = 0; i < 5; i++)
		iounmap(abe->io_base[i]);
	kfree(abe);
	return 0;
}

static struct platform_driver omap_aess_driver = {
	.driver = {
		.name = "aess",
		.owner = THIS_MODULE,
		.pm = &aess_pm_ops,
	},
	.probe = abe_engine_probe,
	.remove = __devexit_p(abe_engine_remove),
};

static int __init abe_engine_init(void)
{
	return platform_driver_register(&omap_aess_driver);
}
module_init(abe_engine_init);

static void __exit abe_engine_exit(void)
{
	platform_driver_unregister(&omap_aess_driver);
}
module_exit(abe_engine_exit);

MODULE_DESCRIPTION("ASoC OMAP4 ABE");
MODULE_AUTHOR("Liam Girdwood <lrg@ti.com>");
MODULE_LICENSE("GPL");
