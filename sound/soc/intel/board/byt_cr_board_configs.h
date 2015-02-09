/*
 *  byt_cr_board_configs.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2014 Intel Corporation
 *  Authors:	Ola Lilja <ola.lilja@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  Board-specific hardware-configurations
 */

#ifndef __BYT_CR_BOARD_CONFIGS_H__
#define __BYT_CR_BOARD_CONFIGS_H__

enum jack_int_select {
	JACK_INT1, /* AUDIO_INT */
	JACK_INT2, /* JACK_DET */
};

enum jack_bp_select {
	JACK_BP_CODEC,
	JACK_BP_MICBIAS,
};

struct board_config {
	char *name;
	int idx;
	int i2s_port;
	int mic_input;
	int jack_active_low;
	enum jack_int_select jack_int_sel;
	enum jack_bp_select jack_bp_sel;
};

struct mach_codec_link {
	const char const *codec_hid;
	struct platform_device *mach_dev;
	struct dmi_system_id const *dmi_system_ids;
	struct board_config const *board_configs;
	struct board_config const *board_config_default;
};

/* Configurations - RT5640 */

enum board_id_rt5640 {
	RT5640_DEFAULT = -1,
	RT5640_MRD7,
	RT5640_T100,
	RT5640_MALATA,
	RT5640_CHIPHD,
};

static const char codec_hid0[] = "10EC5640";

static struct platform_device mach_dev0 = {
	.name		= "byt_rt5640",
	.id		= -1,
	.num_resources	= 0,
};

static const struct board_config board_config_default0 = {
	.name = "bytcr-rt5640",
	.idx = RT5640_DEFAULT,
	.i2s_port = 0,
	.mic_input = 3,
	.jack_active_low = 0,
	.jack_int_sel = JACK_INT1,
	.jack_bp_sel = JACK_BP_MICBIAS,
};

static const struct board_config board_configs0[] = {
	[RT5640_MRD7] = {
		.name = "bytcr-rt5640",
		.idx = RT5640_MRD7,
		.i2s_port = 0,
		.mic_input = 3,
		.jack_active_low = 0,
		.jack_int_sel = JACK_INT1,
		.jack_bp_sel = JACK_BP_MICBIAS,
	},
	[RT5640_T100] = {
		.name = "bytcr-rt5642-t100",
		.idx = RT5640_T100,
		.i2s_port = 2,
		.mic_input = 1,
		.jack_active_low = 0,
		.jack_int_sel = JACK_INT1,
		.jack_bp_sel = JACK_BP_MICBIAS,
	},
	[RT5640_MALATA] = {
		.name = "bytcr-rt5640",
		.idx = RT5640_MALATA,
		.i2s_port = 0,
		.mic_input = 3,
		.jack_active_low = 1,
		.jack_int_sel = JACK_INT2,
		.jack_bp_sel = JACK_BP_CODEC,
	},
	[RT5640_CHIPHD] = {
		.name = "bytcr-rt5640",
		.idx = RT5640_CHIPHD,
		.i2s_port = 0,
		.mic_input = 3,
		.jack_active_low = 1,
		.jack_int_sel = JACK_INT2,
		.jack_bp_sel = JACK_BP_CODEC,
	},
	{}
};

static const struct dmi_system_id dmi_system_ids0[] = {
	[RT5640_MRD7] = {
		.ident = "MRD7",
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "TABLET"),
			DMI_MATCH(DMI_BOARD_VERSION, "MRD 7"),
		},
		.driver_data = (void *)&board_configs0[RT5640_MRD7],
	},
	[RT5640_T100] = {
		.ident = "T100",
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "T100TA"),
			DMI_MATCH(DMI_BOARD_VERSION, "1.0"),
		},
		.driver_data = (void *)&board_configs0[RT5640_T100],
	},
	[RT5640_MALATA] = {
		.ident = "MALATA",
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "MALATA8"),
			DMI_MATCH(DMI_BOARD_VERSION, "0"),
		},
		.driver_data = (void *)&board_configs0[RT5640_MALATA],
	},
	[RT5640_CHIPHD] = {
		.ident = "CHIPHD",
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "CHIPHD8"),
			DMI_MATCH(DMI_BOARD_VERSION, "0"),
		},
		.driver_data = (void *)&board_configs0[RT5640_CHIPHD],
	},
	{}
};

/* Configurations - RT5651 */

enum board_id_rt5651 {
	RT5651_DEFAULT = -1,
	RT5651_ANCHOR8,
};

static const char codec_hid1[] = "10EC5651";

static struct platform_device mach_dev1 = {
	.name		= "byt_rt5651",
	.id		= -1,
	.num_resources	= 0,
};

static const struct board_config board_config_default1 = {
	.idx = RT5651_DEFAULT,
	.i2s_port = 0,
	.mic_input = 3,
	.jack_active_low = 0,
	.jack_int_sel = JACK_INT2,
	.jack_bp_sel = JACK_BP_MICBIAS,
};

static const struct board_config board_configs1[] = {
	[RT5651_ANCHOR8] = {
		.idx = RT5651_ANCHOR8,
		.i2s_port = 0,
		.mic_input = 3,
		.jack_active_low = 0,
		.jack_int_sel = JACK_INT2,
		.jack_bp_sel = JACK_BP_MICBIAS,
	},
	{}
};

static const struct dmi_system_id dmi_system_ids1[] = {
	[RT5651_ANCHOR8] = {
		.ident = "ANCHOR8",
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "TABLET"),
			DMI_MATCH(DMI_BOARD_VERSION, "MRD 7"),
		},
		.driver_data = (void *)&board_configs1[RT5651_ANCHOR8],
	},
	{}
};

static const struct mach_codec_link mach_codec_links[] = {
	{
		.codec_hid = &codec_hid0[0],
		.mach_dev = &mach_dev0,
		.dmi_system_ids = &dmi_system_ids0[0],
		.board_configs = &board_configs0[0],
		.board_config_default = &board_config_default0,
	},
	{
		.codec_hid = &codec_hid1[0],
		.mach_dev = &mach_dev1,
		.dmi_system_ids = &dmi_system_ids1[0],
		.board_configs = &board_configs1[0],
		.board_config_default = &board_config_default1,
	},
};

int set_mc_link(void);
const struct mach_codec_link *get_mc_link(void);
const struct board_config *get_board_config(
				const struct mach_codec_link *mc_link);

#endif
