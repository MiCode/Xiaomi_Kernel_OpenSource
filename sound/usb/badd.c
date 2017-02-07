/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>

struct uac3_input_terminal_descriptor badd_baif_in_term_desc = {
	.bLength = UAC3_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_INPUT_TERMINAL,
	.bTerminalID = BADD_IN_TERM_ID_BAIF,
	.bCSourceID = BADD_CLOCK_SOURCE,
	.wExTerminalDescrID = 0x0000,
	.wTerminalDescrStr = 0x0000
};

struct uac3_input_terminal_descriptor badd_baof_in_term_desc = {
	.bLength = UAC3_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_INPUT_TERMINAL,
	.bTerminalID = BADD_IN_TERM_ID_BAOF,
	.wTerminalType = UAC_TERMINAL_STREAMING,
	.bAssocTerminal = 0x00,
	.bCSourceID = BADD_CLOCK_SOURCE,
	.bmControls = 0x00000000,
	.wExTerminalDescrID = 0x0000,
	.wConnectorsDescrID = 0x0000,
	.wTerminalDescrStr = 0x0000
};

struct uac3_output_terminal_descriptor badd_baif_out_term_desc = {
	.bLength = UAC3_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_OUTPUT_TERMINAL,
	.bTerminalID = BADD_OUT_TERM_ID_BAIF,
	.wTerminalType = UAC_TERMINAL_STREAMING,
	.bAssocTerminal = 0x00,		/* No associated terminal */
	.bSourceID = BADD_FU_ID_BAIF,
	.bCSourceID = BADD_CLOCK_SOURCE,
	.bmControls = 0x00000000,	/* No controls */
	.wExTerminalDescrID = 0x0000,
	.wConnectorsDescrID = 0x0000,
	.wTerminalDescrStr = 0x0000
};

struct uac3_output_terminal_descriptor badd_baof_out_term_desc = {
	.bLength = UAC3_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_OUTPUT_TERMINAL,
	.bTerminalID = BADD_OUT_TERM_ID_BAOF,
	.bSourceID = BADD_FU_ID_BAOF,
	.bCSourceID = BADD_CLOCK_SOURCE,
	.wExTerminalDescrID = 0x0000,
	.wTerminalDescrStr = 0x0000
};

__u8 monoControls[] = {
	0x03, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x00, 0x00};

__u8 stereoControls[] = {
	0x03, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x00, 0x00
};

__u8 badd_mu_src_ids[] = {BADD_IN_TERM_ID_BAOF, BADD_FU_ID_BAIOF};

struct uac3_mixer_unit_descriptor badd_baiof_mu_desc = {
	.bLength = UAC3_DT_MIXER_UNIT_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC3_MIXER_UNIT_V3,
	.bUnitID = BADD_MU_ID_BAIOF,
	.bNrInPins = 0x02,
	.baSourceID = badd_mu_src_ids,
	.bmMixerControls = 0x00,
	.bmControls = 0x00000000,
	.wMixerDescrStr = 0x0000
};

struct uac3_feature_unit_descriptor badd_baif_fu_desc = {
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC3_FEATURE_UNIT_V3,
	.bUnitID = BADD_FU_ID_BAIF,
	.bSourceID = BADD_IN_TERM_ID_BAIF,
	.wFeatureDescrStr = 0x0000
};

struct uac3_feature_unit_descriptor badd_baof_fu_desc = {
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC3_FEATURE_UNIT_V3,
	.bUnitID = BADD_FU_ID_BAOF,
	.wFeatureDescrStr = 0x0000
};

struct uac3_feature_unit_descriptor badd_baiof_fu_desc = {
	.bLength = 0x0f,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC3_FEATURE_UNIT_V3,
	.bUnitID = BADD_FU_ID_BAIOF,
	.bSourceID = BADD_IN_TERM_ID_BAIF,
	.bmaControls = monoControls,
	.wFeatureDescrStr = 0x0000
};

struct uac3_clock_source_descriptor badd_clock_desc = {
	.bLength = UAC3_DT_CLOCK_SRC_SIZE,
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC3_CLOCK_SOURCE,
	.bClockID = BADD_CLOCK_SOURCE,
	.bmControls = 0x00000001,
	.bReferenceTerminal = 0x00,
	.wClockSourceStr = 0x0000
};

void *badd_desc_list[] = {
	&badd_baif_in_term_desc,
	&badd_baof_in_term_desc,
	&badd_baiof_mu_desc,
	&badd_baif_fu_desc,
	&badd_baof_fu_desc,
	&badd_baiof_fu_desc,
	&badd_clock_desc
};

