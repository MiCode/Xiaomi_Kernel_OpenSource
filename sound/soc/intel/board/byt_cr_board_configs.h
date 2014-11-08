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

struct board_config {
	int idx;
	int i2s_port;
	int speaker_input;
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
};

static const char codec_hid0[] = "10EC5640";

static struct platform_device mach_dev0 = {
	.name		= "byt_rt5640",
	.id		= -1,
	.num_resources	= 0,
};

static const struct board_config board_config_default0 = {
	.idx = RT5640_DEFAULT,
	.i2s_port = 0,
	.speaker_input = 1,
};

static const struct board_config board_configs0[] = {
	[RT5640_MRD7] = {
		.idx = RT5640_MRD7,
		.i2s_port = 0,
		.speaker_input = 3,
	},
	[RT5640_T100] = {
		.idx = RT5640_T100,
		.i2s_port = 2,
		.speaker_input = 1,
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
	.speaker_input = 3,
};

static const struct board_config board_configs1[] = {
	[RT5651_ANCHOR8] = {
		.idx = RT5651_ANCHOR8,
		.i2s_port = 0,
		.speaker_input = 3,
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
