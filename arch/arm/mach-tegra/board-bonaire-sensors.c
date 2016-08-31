/*
 * arch/arm/mach-tegra/board-bonaire-sensors.c
 *
 * Copyright (c) 2011-2013 NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <media/imx091.h>
#include <media/ov9772.h>
#include "board.h"
#include "board-bonaire.h"

static int bonaire_imx091_power_on(struct nvc_regulator *vreg)
{
	return 0;
}

static int bonaire_imx091_power_off(struct nvc_regulator *vreg)
{
	return 0;
}

static struct nvc_imager_cap imx091_cap = {
	.identifier		= "IMX091",
	.sensor_nvc_interface	= NVC_IMAGER_SENSOR_INTERFACE_SERIAL_A,
	.pixel_types[0]		= 0x100,
	.orientation		= 0,
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 10416667, /* value / 1,000,000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge		= 0,
	.v_sync_edge		= 0,
	.mclk_on_vgp0		= 0,
	.csi_port		= 0,
	.data_lanes		= 4,
	.virtual_channel_id	= 0,
	.discontinuous_clk_mode	= 1,
	.cil_threshold_settle	= 0xc,
	.min_blank_time_width	= 16,
	.min_blank_time_height	= 16,
	.preferred_mode_index	= 0,
	.focuser_guid		= 0,
	.torch_guid		= 0,
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct imx091_platform_data bonaire_imx091_data = {
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.gpio_count		= 0,
	.gpio			= 0,
	.cap			= &imx091_cap,
	.power_on		= bonaire_imx091_power_on,
	.power_off		= bonaire_imx091_power_off,
};

static int bonaire_ov9772_power_on(struct nvc_regulator *vreg)
{
	return 0;
}

static int bonaire_ov9772_power_off(struct nvc_regulator *vreg)
{
	return 0;
}

static struct nvc_imager_cap ov9772_cap = {
	.identifier		= "OV9772",
	/* refer to NvOdmImagerSensorInterface enum in ODM nvodm_imager.h */
	.sensor_nvc_interface	= NVC_IMAGER_SENSOR_INTERFACE_SERIAL_C,
	/* refer to NvOdmImagerPixelType enum in ODM nvodm_imager.h */
	.pixel_types[0]		= 0x103,
	/* refer to NvOdmImagerOrientation enum in ODM nvodm_imager.h */
	.orientation		= 0,
	/* refer to NvOdmImagerDirection enum in ODM nvodm_imager.h */
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 1162020, /* value / 1,000,000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge		= 0,
	.v_sync_edge		= 0,
	.mclk_on_vgp0		= 0,
	.csi_port		= 1,
	.data_lanes		= 1,
	.virtual_channel_id	= 0,
	.discontinuous_clk_mode	= 0, /* use continuous clock */
	.cil_threshold_settle	= 0xc,
	.min_blank_time_width	= 16,
	.min_blank_time_height	= 16,
	.preferred_mode_index	= 0,
	.focuser_guid		= 0,
	.torch_guid		= 0,
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct ov9772_platform_data bonaire_ov9772_data = {
	.num			= 1,
	.sync			= 0,
	.dev_name		= "camera",
	.gpio_count		= 0,
	.gpio			= 0,
	.cap			= &ov9772_cap,
	.power_on		= bonaire_ov9772_power_on,
	.power_off		= bonaire_ov9772_power_off,
};


static struct i2c_board_info bonaire_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("imx091", 0x36),
		.platform_data = &bonaire_imx091_data,
	},
	{
		I2C_BOARD_INFO("ov9772", 0x10),
		.platform_data = &bonaire_ov9772_data,
	},
};

int __init bonaire_sensors_init(void)
{
	i2c_register_board_info(4, bonaire_i2c_board_info,
		ARRAY_SIZE(bonaire_i2c_board_info));
	return 0;
}
