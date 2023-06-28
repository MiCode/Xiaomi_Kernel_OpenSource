/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS, " AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#ifndef _SYNAPTICS_TCM_XIAOMI_BOARD_DATA_H_
#define _SYNAPTICS_TCM_XIAOMI_BOARD_DATA_H_

#define SYNA_GRIP_PARAMETERS_SIZE 32
#define SYNA_TOUCH_MODE_PARAMETERS_SIZE 5
#define SYNA_CORNERFILTER_AREA_STEP_SIZE 4
#define SYNA_DISPLAY_RESOLUTION_SIZE 2

struct syna_tcm_xiaomi_board_data {
	int x_max;
	int y_max;
	unsigned int game_mode[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int active_mode[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int up_threshold[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int tolerance[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int edge_filter[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int panel_orien[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int report_rate[SYNA_TOUCH_MODE_PARAMETERS_SIZE];
	unsigned int cornerfilter_area_step0;
	unsigned int cornerfilter_area_step1;
	unsigned int cornerfilter_area_step2;
	unsigned int cornerfilter_area_step3;
	unsigned int cornerzone_filter_hor1[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int cornerzone_filter_hor2[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int cornerzone_filter_ver[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int deadzone_filter_hor[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int deadzone_filter_ver[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int edgezone_filter_hor[SYNA_GRIP_PARAMETERS_SIZE];
	unsigned int edgezone_filter_ver[SYNA_GRIP_PARAMETERS_SIZE];
};

#endif
