/*
 * soc-fw.c  --  ALSA SoC Firmware
 *
 * Copyright (C) 2012 Texas Instruments Inc.
 *
 * Author: Liam Girdwood <lrg@ti.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Support for audio fimrware to contain kcontrols, DAPM graphs, widgets,
 *  DAIs, equalizers, firmware, coefficienst etc.
 *
 *  This file only manages the DAPM and Kcontrol components, all other firmware
 *  data is passed to component drivers for bespoke handling.
 */

#define DEBUG

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-fw.h>

/*
 * We make several passes over the data (since it wont necessarily be ordered)
 * and process objects in the following order. This guarantees the component
 * drivers will be ready with any vendor data before the mixers and DAPM objects
 * are loaded (that may make use of the vendor data).
 */
#define SOC_FW_PASS_VENDOR	0
#define SOC_FW_PASS_MIXER	1
#define SOC_FW_PASS_COEFF	SOC_FW_PASS_MIXER
#define SOC_FW_PASS_WIDGET	2
#define SOC_FW_PASS_GRAPH	3
#define SOC_FW_PASS_PINS	4
#define SOC_FW_PASS_DAI		5

#define SOC_FW_PASS_START	SOC_FW_PASS_VENDOR
#define SOC_FW_PASS_END	SOC_FW_PASS_DAI

struct soc_fw {
	const char *file;
	const struct firmware *fw;

	/* runtime FW parsing */
	const u8 *pos;		/* read postion */
	const u8 *hdr_pos;	/* header position */
	unsigned int pass;	/* pass number */

	/* component caller */
	struct device *dev;
	struct snd_soc_codec *codec;
	struct snd_soc_platform *platform;
	struct snd_soc_card *card;
	u32 index;

	/* kcontrol operations */
	const struct snd_soc_fw_kcontrol_ops *io_ops;
	int io_ops_count;

	/* dai operations */
	const struct snd_soc_fw_dai_ops *dai_ops;
	int dai_ops_count;

	/* optional fw loading callbacks to component drivers */
	union {
		struct snd_soc_fw_codec_ops *codec_ops;
		struct snd_soc_fw_platform_ops *platform_ops;
		struct snd_soc_fw_card_ops *card_ops;
	};
};

static int soc_fw_process_headers(struct soc_fw *sfw);
static void soc_fw_complete(struct soc_fw *sfw);

/* List of Kcontrol types and associated operations. */
static const struct snd_soc_fw_kcontrol_ops io_ops[] = {
	{SOC_CONTROL_IO_VOLSW, snd_soc_get_volsw,
		snd_soc_put_volsw, snd_soc_info_volsw},
	{SOC_CONTROL_IO_VOLSW_SX, snd_soc_get_volsw_sx,
		snd_soc_put_volsw_sx, NULL},
	{SOC_CONTROL_IO_VOLSW_S8, snd_soc_get_volsw_s8,
		snd_soc_put_volsw_s8, snd_soc_info_volsw_s8},
	{SOC_CONTROL_IO_ENUM, snd_soc_get_enum_double,
		snd_soc_put_enum_double, snd_soc_info_enum_double},
	{SOC_CONTROL_IO_ENUM_EXT, NULL,
		NULL, snd_soc_info_enum_double},
	{SOC_CONTROL_IO_BYTES, snd_soc_bytes_get,
		snd_soc_bytes_put, snd_soc_bytes_info},
	{SOC_CONTROL_IO_BOOL_EXT, NULL,
		NULL, snd_ctl_boolean_mono_info},
	{SOC_CONTROL_IO_BYTES_EXT, NULL,
		NULL, snd_soc_info_bytes_ext},
	{SOC_CONTROL_IO_RANGE, snd_soc_get_volsw_range,
		snd_soc_put_volsw_range, snd_soc_info_volsw_range},
	{SOC_CONTROL_IO_VOLSW_XR_SX, snd_soc_get_xr_sx,
		snd_soc_put_xr_sx, snd_soc_info_xr_sx},
	{SOC_CONTROL_IO_STROBE, snd_soc_get_strobe,
		snd_soc_put_strobe, NULL},

	{SOC_DAPM_IO_VOLSW, snd_soc_dapm_get_volsw,
		snd_soc_dapm_put_volsw, NULL},
	{SOC_DAPM_IO_ENUM_DOUBLE, snd_soc_dapm_get_enum_double,
		snd_soc_dapm_put_enum_double, snd_soc_info_enum_double},
	{SOC_DAPM_IO_PIN, snd_soc_dapm_get_pin_switch,
		snd_soc_dapm_put_pin_switch, snd_soc_dapm_info_pin_switch},
};

static inline void soc_fw_list_add_enum(struct soc_fw *sfw, struct soc_enum *se)
{
	if (sfw->codec)
		list_add(&se->list, &sfw->codec->denums);
	else if (sfw->platform)
		list_add(&se->list, &sfw->platform->denums);
	else if (sfw->card)
		list_add(&se->list, &sfw->card->denums);
	else
		BUG();
}

static inline void soc_fw_list_add_mixer(struct soc_fw *sfw,
	struct soc_mixer_control *mc)
{
	if (sfw->codec)
		list_add(&mc->list, &sfw->codec->dmixers);
	else if (sfw->platform)
		list_add(&mc->list, &sfw->platform->dmixers);
	else if (sfw->card)
		list_add(&mc->list, &sfw->card->dmixers);
	else
		BUG();
}

static inline void soc_fw_list_add_bytes(struct soc_fw *sfw,
	struct soc_bytes_ext *sb)
{
	if (sfw->codec)
		list_add(&sb->list, &sfw->codec->dbytes);
	else if (sfw->platform)
		list_add(&sb->list, &sfw->platform->dbytes);
	else if (sfw->card)
		list_add(&sb->list, &sfw->card->dbytes);
	else
		dev_err(sfw->dev, "Cannot add dbytes no valid type\n");
}
static inline struct snd_soc_dapm_context *soc_fw_dapm_get(struct soc_fw *sfw)
{
	if (sfw->codec)
		return &sfw->codec->dapm;
	else if (sfw->platform)
		return &sfw->platform->dapm;
	else if (sfw->card)
		return &sfw->card->dapm;
	BUG();
}

static inline struct snd_soc_card *soc_fw_card_get(struct soc_fw *sfw)
{
	if (sfw->codec)
		return sfw->codec->card;
	else if (sfw->platform)
		return sfw->platform->card;
	else if (sfw->card)
		return sfw->card;
	BUG();
}

/* check we dont overflow the data for this control chunk */
static int soc_fw_check_control_count(struct soc_fw *sfw, size_t elem_size,
	unsigned int count, size_t bytes)
{
	const u8 *end = sfw->pos + elem_size * count;

	if (end > sfw->fw->data + sfw->fw->size) {
		dev_err(sfw->dev, "ASoC: controls overflow end of data\n");
		return -EINVAL;
	}

	/* check there is enough room in chunk for control.
	   extra bytes at the end of control are for vendor data here  */
	if (elem_size * count > bytes) {
		dev_err(sfw->dev,
			"ASoC: controls count %d of elem size %d are bigger than chunk %d\n",
			count, elem_size, bytes);
		return -EINVAL;
	}

	return 0;
}

static inline int soc_fw_is_eof(struct soc_fw *sfw)
{
	const u8 *end = sfw->hdr_pos;

	if (end >= sfw->fw->data + sfw->fw->size)
		return 1;
	return 0;
}

static inline unsigned int soc_fw_get_hdr_offset(struct soc_fw *sfw)
{
	return (unsigned int)(sfw->hdr_pos - sfw->fw->data);
}

static inline unsigned int soc_fw_get_offset(struct soc_fw *sfw)
{
	return (unsigned int)(sfw->pos - sfw->fw->data);
}

/* pass vendor data to component driver for processing */
static int soc_fw_vendor_load_(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	int ret = 0;

	if (sfw->codec && sfw->codec_ops && sfw->codec_ops->vendor_load)
		ret = sfw->codec_ops->vendor_load(sfw->codec, hdr);

	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->vendor_load)
		ret = sfw->platform_ops->vendor_load(sfw->platform, hdr);

	if (sfw->card && sfw->card_ops && sfw->card_ops->vendor_load)
		ret = sfw->card_ops->vendor_load(sfw->card, hdr);

	if (ret < 0)
		dev_err(sfw->dev, "ASoC: vendor load failed at hdr offset %d/0x%x for type %d:%d\n",
			soc_fw_get_hdr_offset(sfw), soc_fw_get_hdr_offset(sfw),
			hdr->type, hdr->vendor_type);
	return ret;
}

/* pass vendor data to component driver for processing */
static int soc_fw_vendor_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	if (sfw->pass != SOC_FW_PASS_VENDOR)
		return 0;

	return soc_fw_vendor_load_(sfw, hdr);
}

/* optionally pass new dynamic widget to component driver. mainly for external
 widgets where we can assign private data/ops */
static int soc_fw_widget_load(struct soc_fw *sfw, struct snd_soc_dapm_widget *w)
{
	if (sfw->codec && sfw->codec_ops && sfw->codec_ops->widget_load)
		return sfw->codec_ops->widget_load(sfw->codec, w);

	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->widget_load)
		return sfw->platform_ops->widget_load(sfw->platform, w);

	if (sfw->card && sfw->card_ops && sfw->card_ops->widget_load)
		return sfw->card_ops->widget_load(sfw->card, w);

	dev_dbg(sfw->dev, "ASoC: no handler specified for ext widget %s\n",
		w->name);
	return 0;
}

/* pass new dynamic dais to component driver for dai registration */
static int soc_fw_dai_load(struct soc_fw *sfw, struct snd_soc_dai_driver *dai_drv, int n)
{
	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->dai_load)
		return sfw->platform_ops->dai_load(sfw->platform, dai_drv, n);

	dev_dbg(sfw->dev, "ASoC: no handler specified for dai registration\n");

	return 0;
}
/* tell the component driver that all firmware has been loaded in this request */
static void soc_fw_complete(struct soc_fw *sfw)
{
	if (sfw->codec && sfw->codec_ops && sfw->codec_ops->complete)
		sfw->codec_ops->complete(sfw->codec);
	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->complete)
		sfw->platform_ops->complete(sfw->platform);
	if (sfw->card && sfw->card_ops && sfw->card_ops->complete)
		sfw->card_ops->complete(sfw->card);
}

/* add a dynamic kcontrol */
static int soc_fw_add_dcontrol(struct snd_card *card, struct device *dev,
	const struct snd_kcontrol_new *control_new, const char *prefix,
	void *data, struct snd_kcontrol **kcontrol)
{
	int err;

	*kcontrol = snd_soc_cnew(control_new, data, control_new->name, prefix);
	if (*kcontrol == NULL) {
		dev_err(dev, "ASoC: Failed to create new kcontrol %s\n",
		control_new->name);
		return -ENOMEM;
	}

	err = snd_ctl_add(card, *kcontrol);
	if (err < 0) {
		kfree(*kcontrol);
		dev_err(dev, "ASoC: Failed to add %s: %d\n", control_new->name,
			err);
		return err;
	}

	return 0;
}

/* add a dynamic kcontrol for component driver */
static int soc_fw_add_kcontrol(struct soc_fw *sfw, struct snd_kcontrol_new *k,
	struct snd_kcontrol **kcontrol)
{
	if (sfw->codec) {
		struct snd_soc_codec *codec = sfw->codec;

		return soc_fw_add_dcontrol(codec->card->snd_card, codec->dev,
				k, codec->name_prefix, codec, kcontrol);
	} else if (sfw->platform) {
		struct snd_soc_platform *platform = sfw->platform;

		return soc_fw_add_dcontrol(platform->card->snd_card,
				platform->dev, k, NULL, platform, kcontrol);
	} else if (sfw->card) {
		struct snd_soc_card *card = sfw->card;

		return soc_fw_add_dcontrol(card->snd_card, card->dev,
				k, NULL, card, kcontrol);
	} else
		dev_dbg(sfw->dev,
			"ASoC: no handler specified for kcontrol %s\n", k->name);
	return 0;
}

/* bind a kcontrol to it's IO handlers */
static int soc_fw_kcontrol_bind_io(u32 io_type, struct snd_kcontrol_new *k,
	const struct snd_soc_fw_kcontrol_ops *ops, int num_ops)
{
	int i;

	for (i = 0; i < num_ops; i++) {

		if (SOC_CONTROL_GET_ID_PUT(ops[i].id) ==
			SOC_CONTROL_GET_ID_PUT(io_type) && ops[i].put)
			k->put = ops[i].put;
		if (SOC_CONTROL_GET_ID_GET(ops[i].id) ==
			SOC_CONTROL_GET_ID_GET(io_type) && ops[i].get)
			k->get = ops[i].get;
		if (SOC_CONTROL_GET_ID_INFO(ops[i].id) ==
			SOC_CONTROL_GET_ID_INFO(io_type) && ops[i].info)
			k->info = ops[i].info;
	}

	/* let the caller know if we need to bind external kcontrols */
	if (!k->put || !k->get || !k->info)
		return 1;

	return 0;
}

int snd_soc_fw_widget_bind_event(u16 event_type, struct snd_soc_dapm_widget *w,
		const struct snd_soc_fw_widget_events *events, int num_events)
{
	int i;

	w->event = NULL;

	if (event_type == 0) {
		pr_debug("ASoC: No event type registered\n");
		return 0;
	}

	for (i = 0; i < num_events; i++) {
		if (event_type == events[i].type) {
			w->event = events[i].event_handler;
			break;
		}
	}
	if (!w->event)
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_fw_widget_bind_event);

/* optionally pass new dynamic kcontrol to component driver. */
static int soc_fw_init_kcontrol(struct soc_fw *sfw, struct snd_kcontrol_new *k)
{
	if (sfw->codec && sfw->codec_ops && sfw->codec_ops->control_load)
		return sfw->codec_ops->control_load(sfw->codec, k);

	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->control_load)
		return sfw->platform_ops->control_load(sfw->platform, k);

	if (sfw->card && sfw->card_ops && sfw->card_ops->control_load)
		return sfw->card_ops->control_load(sfw->card, k);

	dev_dbg(sfw->dev, "ASoC: no handler specified for kcontrol %s\n",
		k->name);
	return 0;
}

/* optionally pass private data to be habdled by component driver. */
static int soc_fw_init_pvt_data(struct soc_fw *sfw, u32 io_type, unsigned long sm,
			unsigned long mc)
{
	if (sfw->codec && sfw->codec_ops && sfw->codec_ops->pvt_load)
		return sfw->codec_ops->pvt_load(sfw->codec, io_type, sm, mc);

	if (sfw->platform && sfw->platform_ops && sfw->platform_ops->pvt_load)
		return sfw->platform_ops->pvt_load(sfw->platform, io_type, sm, mc);

	if (sfw->card && sfw->card_ops && sfw->card_ops->pvt_load)
		return sfw->card_ops->pvt_load(sfw->card, io_type, sm, mc);

	dev_dbg(sfw->dev, "ASoC: no handler specified for pvt data copy\n");
	return 0;
}

static int soc_fw_create_tlv(struct soc_fw *sfw, struct snd_kcontrol_new *kc,
	u32 tlv_size)
{
	struct snd_soc_fw_ctl_tlv *fw_tlv;
	struct snd_ctl_tlv *tlv;

	if (tlv_size == 0)
		return 0;

	fw_tlv = (struct snd_soc_fw_ctl_tlv *) sfw->pos;
	sfw->pos += tlv_size;

	tlv = kzalloc(sizeof(*tlv) + tlv_size, GFP_KERNEL);
	if (tlv == NULL)
		return -ENOMEM;

	dev_dbg(sfw->dev, " created TLV type %d size %d bytes\n",
		fw_tlv->numid, fw_tlv->length);
	tlv->numid = fw_tlv->numid;
	tlv->length = fw_tlv->length;
	memcpy(tlv->tlv, fw_tlv + 1, fw_tlv->length);
	kc->tlv.p = (void *)tlv;

	return 0;
}

static inline void soc_fw_free_tlv(struct soc_fw *sfw,
	struct snd_kcontrol_new *kc)
{
	kfree(kc->tlv.p);
}

static int soc_fw_dbytes_create(struct soc_fw *sfw, unsigned int count,
	size_t size)
{
	struct snd_soc_fw_bytes_ext *be;
	struct soc_bytes_ext  *sbe;
	struct snd_kcontrol_new kc;
	int i, err, ext;

	if (soc_fw_check_control_count(sfw,
		sizeof(struct snd_soc_fw_bytes_ext), count, size)) {
		dev_err(sfw->dev, "Asoc: Invalid count %d for byte control\n",
				count);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		be = (struct snd_soc_fw_bytes_ext *)sfw->pos;

		/* validate kcontrol */
		if (strnlen(be->hdr.name, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;

		sbe = kzalloc(sizeof(*sbe) + be->pvt_data_len, GFP_KERNEL);
		if (!sbe)
			return -ENOMEM;

		sfw->pos += (sizeof(struct snd_soc_fw_bytes_ext) + be->pvt_data_len);

		dev_dbg(sfw->dev,
			"ASoC: adding bytes kcontrol %s with access 0x%x\n",
			be->hdr.name, be->hdr.access);

		memset(&kc, 0, sizeof(kc));
		kc.name = be->hdr.name;
		kc.private_value = (long)sbe;
		kc.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		kc.access = be->hdr.access;

		sbe->max = be->max;

		if (be->pvt_data_len)
			soc_fw_init_pvt_data(sfw, be->hdr.index, (unsigned long)sbe, (unsigned long)be);

		INIT_LIST_HEAD(&sbe->list);

		/* map standard io handlers and check for external handlers */
		ext = soc_fw_kcontrol_bind_io(be->hdr.index, &kc, io_ops,
			ARRAY_SIZE(io_ops));

		if (ext) {
			/* none exist, so now try and map ext handlers */
			ext = soc_fw_kcontrol_bind_io(be->hdr.index, &kc,
				sfw->io_ops, sfw->io_ops_count);
			if (ext) {
				dev_err(sfw->dev,
					"ASoC: no complete mixer IO handler for %s type (g,p,i) %d:%d:%d\n",
					be->hdr.name,
					SOC_CONTROL_GET_ID_GET(be->hdr.index),
					SOC_CONTROL_GET_ID_PUT(be->hdr.index),
					SOC_CONTROL_GET_ID_INFO(be->hdr.index));
				kfree(sbe);
				continue;
			}

			err = soc_fw_init_kcontrol(sfw, &kc);
			if (err < 0) {
				dev_err(sfw->dev, "ASoC: failed to init %s\n",
					be->hdr.name);
				kfree(sbe);
				continue;
			}
		}

		/* register control here */
		err = soc_fw_add_kcontrol(sfw, &kc, &sbe->dcontrol);
		if (err < 0) {
			dev_err(sfw->dev, "ASoC: failed to add %s\n", be->hdr.name);
			kfree(sbe);
			continue;
		}
		/* This needs to  be change to a widget which would not work
		 * unless we made changes to snd_soc_code, snd_soc_platform as
		 * that has only enumns and mixer everywhere that list is used*/
		soc_fw_list_add_bytes(sfw, sbe);
	}
	return 0;

}
static int soc_fw_dmixer_create(struct soc_fw *sfw, unsigned int count,
	size_t size)
{
	struct snd_soc_fw_mixer_control *mc;
	struct soc_mixer_control *sm;
	struct snd_kcontrol_new kc;
	int i, err, ext;

	if (soc_fw_check_control_count(sfw,
		sizeof(struct snd_soc_fw_mixer_control), count, size)) {
		dev_err(sfw->dev, "ASoC: invalid count %d for controls\n",
			count);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		mc = (struct snd_soc_fw_mixer_control *)sfw->pos;
		sfw->pos += (sizeof(struct snd_soc_fw_mixer_control) + mc->pvt_data_len);


		/* validate kcontrol */
		if (strnlen(mc->hdr.name, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;

		sm = kzalloc(sizeof(*sm) + mc->pvt_data_len, GFP_KERNEL);
		if (!sm)
			return -ENOMEM;

		dev_dbg(sfw->dev,
			"ASoC: adding mixer kcontrol %s with access 0x%x\n",
			mc->hdr.name, mc->hdr.access);

		memset(&kc, 0, sizeof(kc));
		kc.name = mc->hdr.name;
		kc.private_value = (long)sm;
		kc.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		kc.access = mc->hdr.access;

		sm->reg = mc->reg;
		sm->rreg = mc->rreg;
		sm->shift = mc->shift;
		sm->rshift = mc->rshift;
		sm->max = mc->max;
		sm->min = mc->min;
		sm->invert = mc->invert;
		sm->platform_max = mc->platform_max;
		sm->index = sfw->index;
		if (mc->pvt_data_len)
			soc_fw_init_pvt_data(sfw, mc->hdr.index, (unsigned long)sm, (unsigned long)mc);

		INIT_LIST_HEAD(&sm->list);

		/* map standard io handlers and check for external handlers */
		ext = soc_fw_kcontrol_bind_io(mc->hdr.index, &kc, io_ops,
			ARRAY_SIZE(io_ops));
		if (ext) {
			/* none exist, so now try and map ext handlers */
			ext = soc_fw_kcontrol_bind_io(mc->hdr.index, &kc,
				sfw->io_ops, sfw->io_ops_count);
			if (ext) {
				dev_err(sfw->dev,
					"ASoC: no complete mixer IO handler for %s type (g,p,i) %d:%d:%d\n",
					mc->hdr.name,
					SOC_CONTROL_GET_ID_GET(mc->hdr.index),
					SOC_CONTROL_GET_ID_PUT(mc->hdr.index),
					SOC_CONTROL_GET_ID_INFO(mc->hdr.index));
				kfree(sm);
				continue;
			}

			err = soc_fw_init_kcontrol(sfw, &kc);
			if (err < 0) {
				dev_err(sfw->dev, "ASoC: failed to init %s\n",
					mc->hdr.name);
				kfree(sm);
				continue;
			}
		}

		/* create any TLV data */
		soc_fw_create_tlv(sfw, &kc, mc->hdr.tlv_size);

		/* register control here */
		err = soc_fw_add_kcontrol(sfw, &kc, &sm->dcontrol);
		if (err < 0) {
			dev_err(sfw->dev, "ASoC: failed to add %s\n", mc->hdr.name);
			soc_fw_free_tlv(sfw, &kc);
			kfree(sm);
			continue;
		}

		soc_fw_list_add_mixer(sfw, sm);
	}

	return 0;
}

static inline void soc_fw_denum_free_data(struct soc_enum *se)
{
	int i;

	kfree(se->dvalues);
	for (i = 0; i < se->items - 1; i++)
		kfree(se->dtexts[i]);
}

static int soc_fw_denum_create_texts(struct soc_enum *se,
	struct snd_soc_fw_enum_control *ec)
{
	int i, ret;

	se->dtexts = kzalloc(sizeof(char *) * ec->items, GFP_KERNEL);
	if (se->dtexts == NULL)
		return -ENOMEM;

	for (i = 0; i < ec->items; i++) {

		if (strnlen(ec->texts[i], SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE) {
			ret = -EINVAL;
			goto err;
		}

		se->dtexts[i] = kstrdup(ec->texts[i], GFP_KERNEL);
		if (!se->dtexts[i]) {
			ret = -ENOMEM;
			goto err;
		}
	}

	return 0;

err:
	for (--i; i >= 0; i--)
		kfree(se->dtexts[i]);
	kfree(se->dtexts);
	return ret;
}

static int soc_fw_denum_create(struct soc_fw *sfw, unsigned int count,
	size_t size)
{
	struct snd_soc_fw_enum_control *ec;
	struct soc_enum *se;
	struct snd_kcontrol_new kc;
	int i, ret, err, ext;

	if (soc_fw_check_control_count(sfw,
		sizeof(struct snd_soc_fw_enum_control), count, size)) {
		dev_err(sfw->dev, "ASoC: invalid count %d for enum controls\n",
			count);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		ec = (struct snd_soc_fw_enum_control *)sfw->pos;
		sfw->pos += (sizeof(struct snd_soc_fw_enum_control) + ec->pvt_data_len);

		/* validate kcontrol */
		if (strnlen(ec->hdr.name, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;

		se = kzalloc((sizeof(*se) + ec->pvt_data_len), GFP_KERNEL);
		if (!se)
			return -ENOMEM;

		dev_dbg(sfw->dev, "ASoC: adding enum kcontrol %s size %d\n",
			ec->hdr.name, ec->items);

		memset(&kc, 0, sizeof(kc));
		kc.name = ec->hdr.name;
		kc.private_value = (long)se;
		kc.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		kc.access = ec->hdr.access;

		se->reg = ec->reg;
		se->shift_l = ec->shift_l;
		se->shift_r = ec->shift_r;
		se->items = ec->items;
		se->mask = ec->mask;
		se->index = sfw->index;
		se->pvt_data_len = ec->pvt_data_len;
		if (ec->pvt_data_len)
			soc_fw_init_pvt_data(sfw, ec->hdr.index, (unsigned long)se, (unsigned long)ec);

		INIT_LIST_HEAD(&se->list);

		switch (SOC_CONTROL_GET_ID_INFO(ec->hdr.index)) {
		case SOC_CONTROL_TYPE_ENUM:
		case SOC_CONTROL_TYPE_ENUM_EXT:
		case SOC_DAPM_TYPE_ENUM_EXT:
		case SOC_DAPM_TYPE_ENUM_DOUBLE:
			err = soc_fw_denum_create_texts(se, ec);
			if (err < 0) {
				dev_err(sfw->dev,
					"ASoC: could not create texts for %s\n",
					ec->hdr.name);
				kfree(se);
				continue;
			}
			break;
		default:
			dev_err(sfw->dev,
				"ASoC: invalid enum control type %d for %s\n",
				ec->hdr.index, ec->hdr.name);
			kfree(se);
			continue;
		}

		/* map standard io handlers and check for external handlers */
		ext = soc_fw_kcontrol_bind_io(ec->hdr.index, &kc, io_ops,
			ARRAY_SIZE(io_ops));
		if (ext) {
			/* none exist, so now try and map ext handlers */
			ext = soc_fw_kcontrol_bind_io(ec->hdr.index, &kc,
				sfw->io_ops, sfw->io_ops_count);
			if (ext) {
				dev_err(sfw->dev, "ASoC: no complete enum IO handler for %s type (g,p,i) %d:%d:%d\n",
					ec->hdr.name,
					SOC_CONTROL_GET_ID_GET(ec->hdr.index),
					SOC_CONTROL_GET_ID_PUT(ec->hdr.index),
					SOC_CONTROL_GET_ID_INFO(ec->hdr.index));
				kfree(se);
				continue;
			}

			err = soc_fw_init_kcontrol(sfw, &kc);
			if (err < 0) {
				dev_err(sfw->dev, "ASoC: failed to init %s\n",
					ec->hdr.name);
				kfree(se);
				continue;
			}
		}

		/* register control here */
		ret = soc_fw_add_kcontrol(sfw, &kc, &se->dcontrol);
		if (ret < 0) {
			dev_err(sfw->dev, "ASoC: could not add kcontrol %s\n",
				ec->hdr.name);
			kfree(se);
			continue;
		}

		soc_fw_list_add_enum(sfw, se);
	}

	return 0;
}

static int soc_fw_kcontrol_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	struct snd_soc_fw_kcontrol *sfwk =
		(struct snd_soc_fw_kcontrol *)sfw->pos;
	struct snd_soc_fw_control_hdr *control_hdr;
	int i;

	if (sfw->pass != SOC_FW_PASS_MIXER) {
		sfw->pos += sizeof(struct snd_soc_fw_kcontrol) + hdr->size;
		return 0;
	}

	sfw->pos += sizeof(struct snd_soc_fw_kcontrol);
	control_hdr = (struct snd_soc_fw_control_hdr *)sfw->pos;

	dev_dbg(sfw->dev, "ASoC: adding %d kcontrols\n", sfwk->count);

	for (i = 0; i < sfwk->count; i++) {
		switch (SOC_CONTROL_GET_ID_INFO(control_hdr->index)) {
		case SOC_CONTROL_TYPE_VOLSW:
		case SOC_CONTROL_TYPE_STROBE:
		case SOC_CONTROL_TYPE_VOLSW_SX:
		case SOC_CONTROL_TYPE_VOLSW_S8:
		case SOC_CONTROL_TYPE_VOLSW_XR_SX:
		case SOC_CONTROL_TYPE_BYTES:
		case SOC_CONTROL_TYPE_BOOL_EXT:
		case SOC_CONTROL_TYPE_RANGE:
		case SOC_DAPM_TYPE_VOLSW:
		case SOC_DAPM_TYPE_PIN:
			soc_fw_dmixer_create(sfw, 1, hdr->size);
			break;
		case SOC_CONTROL_TYPE_ENUM:
		case SOC_CONTROL_TYPE_ENUM_EXT:
		case SOC_DAPM_TYPE_ENUM_DOUBLE:
		case SOC_DAPM_TYPE_ENUM_EXT:
			soc_fw_denum_create(sfw, 1, hdr->size);
			break;
		case SOC_CONTROL_TYPE_BYTES_EXT:
			soc_fw_dbytes_create(sfw, 1, hdr->size);
			break;
		default:
			dev_err(sfw->dev, "ASoC: invalid control type %d:%d:%d count %d\n",
				SOC_CONTROL_GET_ID_GET(control_hdr->index),
				SOC_CONTROL_GET_ID_PUT(control_hdr->index),
				SOC_CONTROL_GET_ID_INFO(control_hdr->index),
				sfwk->count);
		}
	}

	return 0;
}

static int soc_fw_dapm_graph_load(struct soc_fw *sfw,
	struct snd_soc_fw_hdr *hdr)
{
	struct snd_soc_dapm_context *dapm = soc_fw_dapm_get(sfw);
	struct snd_soc_dapm_route route;
	struct snd_soc_fw_dapm_elems *elem_info =
		(struct snd_soc_fw_dapm_elems *)sfw->pos;
	struct snd_soc_fw_dapm_graph_elem *elem;
	int count = elem_info->count, i;

	if (sfw->pass != SOC_FW_PASS_GRAPH) {
		sfw->pos += sizeof(struct snd_soc_fw_dapm_elems) + hdr->size;
		return 0;
	}

	sfw->pos += sizeof(struct snd_soc_fw_dapm_elems);

	if (soc_fw_check_control_count(sfw,
		sizeof(struct snd_soc_fw_dapm_graph_elem), count, hdr->size)) {
		dev_err(sfw->dev, "ASoC: invalid count %d for DAPM routes\n",
			count);
		return -EINVAL;
	}

	dev_dbg(sfw->dev, "ASoC: adding %d DAPM routes\n", count);

	memset(&route, 0, sizeof(route));

	for (i = 0; i < count; i++) {
		elem = (struct snd_soc_fw_dapm_graph_elem *)sfw->pos;
		sfw->pos += sizeof(struct snd_soc_fw_dapm_graph_elem);

		/* validate routes */
		if (strnlen(elem->source, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;
		if (strnlen(elem->sink, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;
		if (strnlen(elem->control, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			return -EINVAL;

		route.source = elem->source;
		route.sink = elem->sink;
		if (strnlen(elem->control, SND_SOC_FW_TEXT_SIZE))
			route.control = elem->control;
		else
			route.control = NULL;

		/* add route, but keep going if some fail */
		snd_soc_dapm_add_routes(dapm, &route, 1);
	}

	return 0;
}

static struct snd_kcontrol_new *soc_fw_dapm_widget_dmixer_create(struct soc_fw *sfw,
	int num_kcontrols)
{
	struct snd_kcontrol_new *kc;
	struct soc_mixer_control *sm;
	struct snd_soc_fw_mixer_control *mc;
	int i, err, ext;

	kc = kzalloc(sizeof(*kc) * num_kcontrols, GFP_KERNEL);
	if (!kc)
		return NULL;

	for (i = 0; i < num_kcontrols; i++) {
		mc = (struct snd_soc_fw_mixer_control *)sfw->pos;
		sm = kzalloc(sizeof(*sm) + mc->pvt_data_len, GFP_KERNEL);
		if (!sm)
			goto err;

		mc = (struct snd_soc_fw_mixer_control *)sfw->pos;
		sfw->pos += (sizeof(struct snd_soc_fw_mixer_control) + mc->pvt_data_len);

		/* validate kcontrol */
		if (strnlen(mc->hdr.name, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
			goto err_str;

		dev_dbg(sfw->dev, " adding DAPM widget mixer control %s at %d\n",
			mc->hdr.name, i);

		kc[i].name = mc->hdr.name;
		kc[i].private_value = (long)sm;
		kc[i].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		kc[i].access = mc->hdr.access;

		sm->reg = mc->reg;
		sm->rreg = mc->rreg;
		sm->shift = mc->shift;
		sm->rshift = mc->rshift;
		sm->max = mc->max;
		sm->min = mc->min;
		sm->invert = mc->invert;
		sm->platform_max = mc->platform_max;
		sm->index = sfw->index;

		if (mc->pvt_data_len)
			soc_fw_init_pvt_data(sfw, mc->hdr.index, (unsigned long)sm, (unsigned long)mc);

		INIT_LIST_HEAD(&sm->list);

		/* map standard io handlers and check for external handlers */
		ext = soc_fw_kcontrol_bind_io(mc->hdr.index, &kc[i], io_ops,
			ARRAY_SIZE(io_ops));
		if (ext) {
			/* none exist, so now try and map ext handlers */
			ext = soc_fw_kcontrol_bind_io(mc->hdr.index, &kc[i],
				sfw->io_ops, sfw->io_ops_count);
			if (ext) {
				dev_err(sfw->dev,
					"ASoC: no complete widget mixer IO handler for %s type (g,p,i) %d:%d:%d\n",
					mc->hdr.name,
					SOC_CONTROL_GET_ID_GET(mc->hdr.index),
					SOC_CONTROL_GET_ID_PUT(mc->hdr.index),
					SOC_CONTROL_GET_ID_INFO(mc->hdr.index));
				kfree(sm);
				continue;
			}

			err = soc_fw_init_kcontrol(sfw, &kc[i]);
			if (err < 0) {
				dev_err(sfw->dev, "ASoC: failed to init %s\n",
					mc->hdr.name);
				kfree(sm);
				continue;
			}
		}
	}
	return kc;
err_str:
	kfree(sm);
err:
	for (--i; i >= 0; i--)
		kfree((void *)kc[i].private_value);
	kfree(kc);
	return NULL;
}

static struct snd_kcontrol_new *soc_fw_dapm_widget_denum_create(struct soc_fw *sfw)
{
	struct snd_kcontrol_new *kc;
	struct snd_soc_fw_enum_control *ec;
	struct soc_enum *se;
	int i, err, ext;

	ec = (struct snd_soc_fw_enum_control *)sfw->pos;
	sfw->pos += (sizeof(struct snd_soc_fw_enum_control) + ec->pvt_data_len);

	/* validate kcontrol */
	if (strnlen(ec->hdr.name, SND_SOC_FW_TEXT_SIZE) ==
		SND_SOC_FW_TEXT_SIZE)
		return NULL;

	kc = kzalloc(sizeof(*kc), GFP_KERNEL);
	if (!kc)
		return NULL;

	se = kzalloc((sizeof(*se) + ec->pvt_data_len), GFP_KERNEL);
	if (!se)
		goto err_se;

	dev_dbg(sfw->dev, " adding DAPM widget enum control %s\n",
		ec->hdr.name);

	kc->name = ec->hdr.name;
	kc->private_value = (long)se;
	kc->iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	kc->access = ec->hdr.access;

	se->reg = ec->reg;
	se->shift_l = ec->shift_l;
	se->shift_r = ec->shift_r;
	se->items = ec->items;
	se->mask = ec->mask;
	se->index = sfw->index;
	se->pvt_data_len = ec->pvt_data_len;
	if (se->pvt_data_len)
		soc_fw_init_pvt_data(sfw, ec->hdr.index, (unsigned long)se, (unsigned long)ec);

	switch (SOC_CONTROL_GET_ID_INFO(ec->hdr.index)) {
	case SOC_CONTROL_TYPE_ENUM:
	case SOC_CONTROL_TYPE_ENUM_EXT:
	case SOC_DAPM_TYPE_ENUM_EXT:
	case SOC_DAPM_TYPE_ENUM_DOUBLE:
		err = soc_fw_denum_create_texts(se, ec);
		if (err < 0) {
			dev_err(sfw->dev, "ASoC: could not create"
				" texts for %s\n", ec->hdr.name);
			goto err_se;
		}
		break;
	default:
		dev_err(sfw->dev, "ASoC: invalid enum control type %d for %s\n",
			ec->hdr.index, ec->hdr.name);
		goto err_se;
	}

	/* map standard io handlers and check for external handlers */
	ext = soc_fw_kcontrol_bind_io(ec->hdr.index, kc, io_ops,
		ARRAY_SIZE(io_ops));
	if (ext) {
		/* none exist, so now try and map ext handlers */
		ext = soc_fw_kcontrol_bind_io(ec->hdr.index, kc,
			sfw->io_ops, sfw->io_ops_count);
		if (ext) {
			dev_err(sfw->dev,
				"ASoC: no complete widget enum IO handler for %s type (g,p,i) %d:%d:%d\n",
				ec->hdr.name,
				SOC_CONTROL_GET_ID_GET(ec->hdr.index),
				SOC_CONTROL_GET_ID_PUT(ec->hdr.index),
				SOC_CONTROL_GET_ID_INFO(ec->hdr.index));
			goto err_se;
		}

		err = soc_fw_init_kcontrol(sfw, kc);
		if (err < 0) {
			dev_err(sfw->dev, "ASoC: failed to init %s\n",
				ec->hdr.name);
			goto err_se;
		}
	}
	return kc;

err_se:
	kfree(kc);

	/* free values and texts */
	kfree(se->dvalues);
	for (i = 0; i < ec->items; i++)
		kfree(se->dtexts[i]);

	kfree(se);

	return NULL;
}

static int soc_fw_dapm_widget_create(struct soc_fw *sfw,
	struct snd_soc_fw_dapm_widget *w)
{
	struct snd_soc_dapm_context *dapm = soc_fw_dapm_get(sfw);
	struct snd_soc_dapm_widget widget;
	struct snd_soc_fw_control_hdr *control_hdr;
	int ret = 0;

	if (strnlen(w->name, SND_SOC_FW_TEXT_SIZE) ==
		SND_SOC_FW_TEXT_SIZE)
		return -EINVAL;
	if (strnlen(w->sname, SND_SOC_FW_TEXT_SIZE) ==
		SND_SOC_FW_TEXT_SIZE)
		return -EINVAL;

	dev_dbg(sfw->dev, "ASoC: creating DAPM widget %s id %d\n",
		w->name, w->id);

	memset(&widget, 0, sizeof(widget));
	widget.id = w->id;
	widget.name = kstrdup(w->name, GFP_KERNEL);
	if (!widget.name)
		return -ENOMEM;
	widget.sname = kstrdup(w->sname, GFP_KERNEL);
	if (!widget.sname) {
		ret = -ENOMEM;
		goto err;
	}
	widget.reg = w->reg;
	widget.shift = w->shift;
	widget.mask = w->mask;
	widget.on_val = w->invert ? 0 : 1;
	widget.off_val = w->invert ? 1 : 0;
	widget.ignore_suspend = w->ignore_suspend;
	widget.event_flags = w->event_flags;
	widget.index = sfw->index;

	sfw->pos += sizeof(struct snd_soc_fw_dapm_widget);
	if (w->kcontrol.count == 0) {
		widget.num_kcontrols = 0;
		goto widget;
	}

	control_hdr = (struct snd_soc_fw_control_hdr *)sfw->pos;
	dev_dbg(sfw->dev, "ASoC: widget %s has %d controls of type %x\n",
		w->name, w->kcontrol.count, control_hdr->index);

	switch (SOC_CONTROL_GET_ID_INFO(control_hdr->index)) {
	case SOC_CONTROL_TYPE_VOLSW:
	case SOC_CONTROL_TYPE_STROBE:
	case SOC_CONTROL_TYPE_VOLSW_SX:
	case SOC_CONTROL_TYPE_VOLSW_S8:
	case SOC_CONTROL_TYPE_VOLSW_XR_SX:
	case SOC_CONTROL_TYPE_BYTES:
	case SOC_CONTROL_TYPE_BOOL_EXT:
	case SOC_CONTROL_TYPE_RANGE:
	case SOC_DAPM_TYPE_VOLSW:
		widget.num_kcontrols = w->kcontrol.count;
		widget.kcontrol_news = soc_fw_dapm_widget_dmixer_create(sfw,
			widget.num_kcontrols);
		if (!widget.kcontrol_news) {
			ret = -ENOMEM;
			goto hdr_err;
		}
		break;
	case SOC_CONTROL_TYPE_ENUM:
	case SOC_CONTROL_TYPE_ENUM_EXT:
	case SOC_DAPM_TYPE_ENUM_DOUBLE:
	case SOC_DAPM_TYPE_ENUM_EXT:
		widget.num_kcontrols = 1;
		widget.kcontrol_enum = 1;
		widget.kcontrol_news = soc_fw_dapm_widget_denum_create(sfw);
		if (!widget.kcontrol_news) {
			ret = -ENOMEM;
			goto hdr_err;
		}
		break;
	default:
		dev_err(sfw->dev, "ASoC: invalid widget control type %d:%d:%d\n",
			SOC_CONTROL_GET_ID_GET(control_hdr->index),
			SOC_CONTROL_GET_ID_PUT(control_hdr->index),
			SOC_CONTROL_GET_ID_INFO(control_hdr->index));
		ret = -EINVAL;
		goto hdr_err;
	}

widget:
	ret = soc_fw_widget_load(sfw, &widget);
		if (ret < 0)
			goto hdr_err;

	ret = snd_soc_dapm_new_controls(dapm, &widget, 1);
	if (ret < 0) {
		dev_err(sfw->dev, "ASoC: failed to create widget %s controls\n",
			w->name);
		goto hdr_err;
	}

hdr_err:
	kfree(widget.sname);
err:
	kfree(widget.name);
	return ret;
}

static int soc_fw_dapm_widget_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	struct snd_soc_fw_dapm_elems *elem_info =
		(struct snd_soc_fw_dapm_elems *)sfw->pos;
	struct snd_soc_fw_dapm_widget *widget;
	int ret, count = elem_info->count, i;

	if (sfw->pass != SOC_FW_PASS_WIDGET)
		return 0;

	sfw->pos += sizeof(struct snd_soc_fw_dapm_elems);

	if (soc_fw_check_control_count(sfw,
		sizeof(struct snd_soc_fw_dapm_graph_elem), count, hdr->size)) {
		dev_err(sfw->dev, "ASoC: invalid count %d for widgets\n", count);
		return -EINVAL;
	}

	dev_dbg(sfw->dev, "ASoC: adding %d DAPM widgets\n", count);

	for (i = 0; i < count; i++) {
		widget = (struct snd_soc_fw_dapm_widget *) sfw->pos;
		ret = soc_fw_dapm_widget_create(sfw, widget);
		if (ret < 0)
			dev_err(sfw->dev, "ASoC: failed to load widget %s\n",
				widget->name);
	}

	return 0;
}

/* Coefficients with mixer header */
static int soc_fw_coeff_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	struct snd_soc_fw_kcontrol *sfwk =
		(struct snd_soc_fw_kcontrol *)sfw->pos;
	struct snd_soc_fw_control_hdr *control_hdr;
	struct snd_soc_fw_hdr *vhdr;
	int ret;

	if (sfw->pass != SOC_FW_PASS_COEFF)
		return 0;

	/* vendor coefficient data is encapsulated with hdrs in generic
	  coefficient controls */
	if (hdr->vendor_type != 0)
		return 0;

	dev_dbg(sfw->dev, "ASoC: got %d new coefficients\n", sfwk->count);

	sfw->pos += sizeof(struct snd_soc_fw_kcontrol);
	control_hdr = (struct snd_soc_fw_control_hdr *)sfw->pos;

	switch (SOC_CONTROL_GET_ID_INFO(control_hdr->index)) {
	case SOC_CONTROL_TYPE_ENUM:
	case SOC_CONTROL_TYPE_ENUM_EXT:
		ret = soc_fw_denum_create(sfw, 1, hdr->size);
		if (ret < 0) {
			dev_err(sfw->dev, "ASoC: failed to create coeff enum %d\n",
				ret);
			return ret;
		}
		break;
	default:
		dev_err(sfw->dev, "ASoC: invalid coeff control type %d count %d\n",
			SOC_CONTROL_GET_ID_INFO(control_hdr->index),
			sfwk->count);
		return -EINVAL;
	}

	vhdr = (struct snd_soc_fw_hdr *)sfw->pos;

	ret = soc_fw_vendor_load_(sfw, vhdr);
	if (ret < 0) {
		dev_err(sfw->dev, "ASoC: unabled to load coeff data %d\n", ret);
		return ret;
	}
	sfw->pos += sizeof(*vhdr) + vhdr->size;
	vhdr = (struct snd_soc_fw_hdr *)sfw->pos;

	return 0;
}

static int soc_fw_dai_link_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	/* TODO: add DAI links based on FW routing between components */
	dev_err(sfw->dev, "ASoC: Firmware DAIs not supported\n");
	return 0;
}

/* bind a dai to its ops */
static int soc_fw_dai_bind_ops(unsigned int dai_type, struct snd_soc_dai_driver *dai_drv,
		const struct snd_soc_fw_dai_ops *ops, int num_ops)
{
	int i;

	dai_drv->ops = NULL;
	for (i = 0; i < num_ops; i++) {
		if (dai_type == ops[i].id) {
			dai_drv->ops = ops[i].ops;
			break;
		}
	}

	return 0;
}

static inline int soc_fw_set_dai_caps(struct snd_soc_pcm_stream *stream,
		struct	snd_soc_fw_dai_caps *caps)
{
	/* validate stream names */
	if (strnlen(caps->stream_name, SND_SOC_FW_TEXT_SIZE) ==
			SND_SOC_FW_TEXT_SIZE)
		return -EINVAL;

	stream->stream_name = caps->stream_name;
	stream->formats = caps->formats;
	stream->rates = caps->rates;
	stream->rate_min = caps->rate_min;
	stream->rate_max = caps->rate_max;
	stream->channels_min = caps->channels_min;
	stream->channels_max = caps->channels_max;
	return 0;
}

static int soc_fw_ddai_load(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	struct snd_soc_fw_dai_data *dai_data =
		(struct snd_soc_fw_dai_data *)sfw->pos;
	struct snd_soc_fw_dai_elem *elem;
	int count = dai_data->count, i;
	struct snd_soc_dai_driver *dai_drv;

	if (sfw->pass != SOC_FW_PASS_DAI)
		return 0;

	sfw->pos += sizeof(struct snd_soc_fw_dai_data);
	/* The following function for control count has been reused to
	 * validate the dai count as well
	 */
	if (soc_fw_check_control_count(sfw,
				sizeof(struct snd_soc_fw_dai_elem),
				count, hdr->size)) {
		dev_err(sfw->dev, "ASoC: invalid count %d for DAI elems\n",
				count);
		return -EINVAL;
	}

	dev_dbg(sfw->dev, "ASoC: adding %d DAIs\n", count);
	dai_drv = devm_kzalloc(sfw->dev,
			sizeof(struct snd_soc_dai_driver) * count, GFP_KERNEL);
	if (!dai_drv) {
		dev_err(sfw->dev, "ASoC: Mem alloc failue\n");
		return -ENOMEM;
	}
	for (i = 0; i < count; i++) {
		elem = (struct snd_soc_fw_dai_elem *)sfw->pos;
		sfw->pos += sizeof(struct snd_soc_fw_dai_elem);

		/* validate dai name */
		if (strnlen(elem->name, SND_SOC_FW_TEXT_SIZE) ==
				SND_SOC_FW_TEXT_SIZE) {
			dev_err(sfw->dev, "ASoC: invalid dai name\n");
			return -EINVAL;
		}
		dai_drv[i].name = elem->name;
		dai_drv[i].compress_dai = elem->compress_dai;

		if (elem->pb_stream) {
			if (soc_fw_set_dai_caps(&dai_drv[i].playback,
					&elem->playback_caps)) {
				dev_err(sfw->dev, "ASoC: invalid playback stream name\n");
				return -EINVAL;
			}

		}
		if (elem->cp_stream) {
			if (soc_fw_set_dai_caps(&dai_drv[i].capture,
					&elem->capture_caps)) {
				dev_err(sfw->dev, "ASoC: invalid capture stream name\n");
				return -EINVAL;
			}
		}

		soc_fw_dai_bind_ops(elem->dai_type, &dai_drv[i],
				sfw->dai_ops, sfw->dai_ops_count);
	}

	/* Call the platform driver call back to register the dais */
	return soc_fw_dai_load(sfw, dai_drv,  count);
}

static int soc_valid_header(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	if (soc_fw_get_hdr_offset(sfw) >= sfw->fw->size)
		return 0;

	/* big endian firmware objects not supported atm */
	if (hdr->magic == cpu_to_be32(SND_SOC_FW_MAGIC)) {
		dev_err(sfw->dev,
			"ASoC: %s at pass %d big endian not supported header got %x at offset 0x%x size 0x%x.\n",
			sfw->file, sfw->pass, hdr->magic,
			soc_fw_get_hdr_offset(sfw), sfw->fw->size);
		return -EINVAL;
	}

	if (hdr->magic != SND_SOC_FW_MAGIC) {
		dev_err(sfw->dev,
			"ASoC: %s at pass %d does not have a valid header got %x at offset 0x%x size 0x%x.\n",
			sfw->file, sfw->pass, hdr->magic,
			soc_fw_get_hdr_offset(sfw), sfw->fw->size);
		return -EINVAL;
	}

	if (hdr->abi != SND_SOC_FW_ABI_VERSION) {
		dev_err(sfw->dev,
			"ASoC: %s at pass %d invalid ABI version got 0x%x need 0x%x at offset 0x%x size 0x%x.\n",
			sfw->file, sfw->pass, hdr->abi, SND_SOC_FW_ABI_VERSION,
			soc_fw_get_hdr_offset(sfw), sfw->fw->size);
		return -EINVAL;
	}

	if (hdr->size == 0) {
		dev_err(sfw->dev, "ASoC: %s header has 0 size at offset 0x%x.\n",
			sfw->file, soc_fw_get_hdr_offset(sfw));
		return -EINVAL;
	}

	if (sfw->pass == hdr->type)
		dev_dbg(sfw->dev,
			"ASoC: Got 0x%x bytes of type %d version %d vendor %d at pass %d\n",
			hdr->size, hdr->type, hdr->version, hdr->vendor_type,
			sfw->pass);

	return 1;
}

static int soc_fw_load_header(struct soc_fw *sfw, struct snd_soc_fw_hdr *hdr)
{
	sfw->pos = sfw->hdr_pos + sizeof(struct snd_soc_fw_hdr);

	switch (hdr->type) {
	case SND_SOC_FW_MIXER:
		return soc_fw_kcontrol_load(sfw, hdr);
	case SND_SOC_FW_DAPM_GRAPH:
		return soc_fw_dapm_graph_load(sfw, hdr);
	case SND_SOC_FW_DAPM_WIDGET:
		return soc_fw_dapm_widget_load(sfw, hdr);
	case SND_SOC_FW_DAI_LINK:
		return soc_fw_dai_link_load(sfw, hdr);
	case SND_SOC_FW_COEFF:
		return soc_fw_coeff_load(sfw, hdr);
	case SND_SOC_FW_DAI:
		return soc_fw_ddai_load(sfw, hdr);
	default:
		return soc_fw_vendor_load(sfw, hdr);
	}

	return 0;
}

static int soc_fw_process_headers(struct soc_fw *sfw)
{
	struct snd_soc_fw_hdr *hdr;
	int ret;

	sfw->pass = SOC_FW_PASS_START;

	while (sfw->pass <= SOC_FW_PASS_END) {

		sfw->hdr_pos = sfw->fw->data;
		hdr = (struct snd_soc_fw_hdr *)sfw->hdr_pos;

		while (!soc_fw_is_eof(sfw)) {

			ret = soc_valid_header(sfw, hdr);
			if (ret < 0)
				return ret;
			else if (ret == 0)
				break;

			ret = soc_fw_load_header(sfw, hdr);
			if (ret < 0)
				return ret;

			sfw->hdr_pos += hdr->size + sizeof(struct snd_soc_fw_hdr);
			hdr = (struct snd_soc_fw_hdr *)sfw->hdr_pos;
		}
		sfw->pass++;
	}

	return 0;
}

static int soc_fw_load(struct soc_fw *sfw)
{
	int ret;

	ret = soc_fw_process_headers(sfw);
	if (ret == 0)
		soc_fw_complete(sfw);

	return ret;
}

int snd_soc_fw_load_codec(struct snd_soc_codec *codec,
	struct snd_soc_fw_codec_ops *ops, const struct firmware *fw,
	u32 index)
{
	struct soc_fw sfw;

	memset(&sfw, 0, sizeof(sfw));

	sfw.fw = fw;
	sfw.dev = codec->dev;
	sfw.codec = codec;
	sfw.codec_ops = ops;
	sfw.index = index;
	sfw.io_ops = ops->io_ops;
	sfw.io_ops_count = ops->io_ops_count;

	return soc_fw_load(&sfw);
}
EXPORT_SYMBOL_GPL(snd_soc_fw_load_codec);

int snd_soc_fw_load_platform(struct snd_soc_platform *platform,
	struct snd_soc_fw_platform_ops *ops, const struct firmware *fw,
	u32 index)
{
	struct soc_fw sfw;

	memset(&sfw, 0, sizeof(sfw));

	sfw.fw = fw;
	sfw.dev = platform->dev;
	sfw.platform = platform;
	sfw.platform_ops = ops;
	sfw.index = index;
	sfw.io_ops = ops->io_ops;
	sfw.io_ops_count = ops->io_ops_count;
	sfw.dai_ops = ops->dai_ops;
	sfw.dai_ops_count = ops->dai_ops_count;

	return soc_fw_load(&sfw);
}
EXPORT_SYMBOL_GPL(snd_soc_fw_load_platform);

int snd_soc_fw_load_card(struct snd_soc_card *card,
	struct snd_soc_fw_card_ops *ops, const struct firmware *fw,
	u32 index)
{
	struct soc_fw sfw;

	memset(&sfw, 0, sizeof(sfw));

	sfw.fw = fw;
	sfw.dev = card->dev;
	sfw.card = card;
	sfw.card_ops = ops;
	sfw.index = index;
	sfw.io_ops = ops->io_ops;
	sfw.io_ops_count = ops->io_ops_count;

	return soc_fw_load(&sfw);
}
EXPORT_SYMBOL_GPL(snd_soc_fw_load_card);

/* remove this dynamic widget */
void snd_soc_fw_dcontrols_remove_widget(struct snd_soc_dapm_widget *w)
{
	struct snd_card *card = w->dapm->card->snd_card;
	int i;

	/*
	 * Dynamic Widgets either have 1 enum kcontrol or 1..N mixers.
	 * The enumm may either have an array of values or strings.
	 */
	if (w->kcontrol_enum) {
		struct soc_enum *se =
			(struct soc_enum *)w->kcontrols[0]->private_value;

		snd_ctl_remove(card, w->kcontrols[0]);

		kfree(se->dvalues);
		for (i = 0; i < se->items; i++)
			kfree(se->dtexts[i]);

		kfree(se);
		kfree(w->kcontrol_news);
	} else {
		for (i = 0; i < w->num_kcontrols; i++) {
			struct snd_kcontrol *kcontrol = w->kcontrols[i];
			struct soc_mixer_control *sm =
			(struct soc_mixer_control *) kcontrol->private_value;

			kfree(w->kcontrols[i]->tlv.p);

			snd_ctl_remove(card, w->kcontrols[i]);
			kfree(sm);
		}
		kfree(w->kcontrol_news);
	}

}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_widget);

/* remove all dynamic widgets from this context */
void snd_soc_fw_dcontrols_remove_widgets(struct snd_soc_dapm_context *dapm,
	u32 index)
{
	struct snd_soc_dapm_widget *w, *next_w;
	struct snd_soc_dapm_path *p, *next_p;

	list_for_each_entry_safe(w, next_w, &dapm->card->widgets, list) {
		if (w->index != index || w->dapm != dapm)
			continue;

		list_del(&w->list);
		/*
		 * remove source and sink paths associated to this widget.
		 * While removing the path, remove reference to it from both
		 * source and sink widgets so that path is removed only once.
		 */
		list_for_each_entry_safe(p, next_p, &w->sources, list_sink) {
			list_del(&p->list_sink);
			list_del(&p->list_source);
			list_del(&p->list);
			kfree(p);
		}
		list_for_each_entry_safe(p, next_p, &w->sinks, list_source) {
			list_del(&p->list_sink);
			list_del(&p->list_source);
			list_del(&p->list);
			kfree(p);
		}
		/* check and free and dynamic widget kcontrols */
		snd_soc_fw_dcontrols_remove_widget(w);
		kfree(w->kcontrols);
		kfree(w->name);
		kfree(w);
	}
}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_widgets);

/* remove dynamic controls from the codec driver only */
void snd_soc_fw_dcontrols_remove_codec(struct snd_soc_codec *codec,
	u32 index)
{
	struct soc_mixer_control *sm, *next_sm;
	struct soc_enum *se, *next_se;
	struct soc_bytes_ext *sb, *next_sb;
	struct snd_card *card = codec->card->snd_card;
	const unsigned int *p = NULL;
	int i;

	list_for_each_entry_safe(sm, next_sm, &codec->dmixers, list) {

		if (sm->index != index)
			continue;

		if (sm->dcontrol->tlv.p)
			p = sm->dcontrol->tlv.p;
		snd_ctl_remove(card, sm->dcontrol);
		list_del(&sm->list);
		kfree(sm);
		kfree(p);
	}

	list_for_each_entry_safe(se, next_se, &codec->denums, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, se->dcontrol);
		list_del(&se->list);

		kfree(se->dvalues);
		for (i = 0; i < se->items; i++)
			kfree(se->dtexts[i]);
		kfree(se);
	}

	list_for_each_entry_safe(sb, next_sb, &codec->dbytes, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, sb->dcontrol);
		list_del(&sb->list);
		kfree(sb);
	}
}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_codec);

/* remove dynamic controls from the platform driver only */
void snd_soc_fw_dcontrols_remove_platform(struct snd_soc_platform *platform,
	u32 index)
{
	struct soc_mixer_control *sm, *next_sm;
	struct soc_enum *se, *next_se;
	struct soc_bytes_ext *sb, *next_sb;
	struct snd_card *card = platform->card->snd_card;
	const unsigned int *p = NULL;
	int i;

	list_for_each_entry_safe(sm, next_sm, &platform->dmixers, list) {

		if (sm->index != index)
			continue;

		if (sm->dcontrol->tlv.p)
			p = sm->dcontrol->tlv.p;
		snd_ctl_remove(card, sm->dcontrol);
		list_del(&sm->list);
		kfree(sm);
		kfree(p);
	}

	list_for_each_entry_safe(se, next_se, &platform->denums, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, se->dcontrol);
		list_del(&se->list);

		kfree(se->dvalues);
		for (i = 0; i < se->items; i++)
			kfree(se->dtexts[i]);
		kfree(se);
	}

	list_for_each_entry_safe(sb, next_sb, &platform->dbytes, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, sb->dcontrol);
		list_del(&sb->list);
		kfree(sb);
	}
}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_platform);

/* remove dynamic controls from the card driver only */
void snd_soc_fw_dcontrols_remove_card(struct snd_soc_card *soc_card,
	u32 index)
{
	struct soc_mixer_control *sm, *next_sm;
	struct soc_enum *se, *next_se;
	struct soc_bytes_ext *sb, *next_sb;
	struct snd_card *card = soc_card->snd_card;
	const unsigned int *p = NULL;
	int i;

	list_for_each_entry_safe(sm, next_sm, &soc_card->dmixers, list) {

		if (sm->index != index)
			continue;

		if (sm->dcontrol->tlv.p)
			p = sm->dcontrol->tlv.p;
		snd_ctl_remove(card, sm->dcontrol);
		list_del(&sm->list);
		kfree(sm);
		kfree(p);
	}

	list_for_each_entry_safe(se, next_se, &soc_card->denums, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, se->dcontrol);
		list_del(&se->list);

		kfree(se->dvalues);
		for (i = 0; i < se->items; i++)
			kfree(se->dtexts[i]);
		kfree(se);
	}

	list_for_each_entry_safe(sb, next_sb, &soc_card->dbytes, list) {

		if (sm->index != index)
			continue;

		snd_ctl_remove(card, sb->dcontrol);
		list_del(&sb->list);
		kfree(sb);
	}
}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_card);

/* remove all dynamic controls from sound card and components */
int snd_soc_fw_dcontrols_remove_all(struct snd_soc_card *card, u32 index)
{
	struct snd_soc_codec *codec;
	struct snd_soc_platform *platform;

	list_for_each_entry(codec, &card->codec_dev_list, card_list)
		snd_soc_fw_dcontrols_remove_codec(codec, index);

	list_for_each_entry(platform, &card->platform_dev_list, card_list)
		snd_soc_fw_dcontrols_remove_platform(platform, index);

	snd_soc_fw_dcontrols_remove_card(card, index);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_fw_dcontrols_remove_all);
