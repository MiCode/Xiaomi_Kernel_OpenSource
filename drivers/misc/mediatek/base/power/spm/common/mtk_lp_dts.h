/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_LP_DTS_H__
#define __MTK_LP_DTS_H__

#include <linux/of.h>
#include <linux/of_device.h>
#include "mtk_lp_dts_def.h"

/* Parsing property of status in parent node*/
#define GET_MTK_IDLE_OF_PROPERTY_STATUS(_parentNode, _procName)\
	({ const char *pro_string; struct device_node *state_node = NULL;\
		int pValue = 0, err;\
		do {\
			state_node =\
				of_get_child_by_name(_parentNode, _procName);\
			if (!state_node)\
				break;\
			err = of_property_read_string(state_node, "status"\
				, &pro_string);\
			if (err)\
				break;\
			if (!pro_string)\
				break;\
			pValue |= MTK_OF_PROPERTY_STATUS_FOUND;\
			if ((strncmp(pro_string, "okay", 4) == 0) ||\
				(strncmp(pro_string, "ok", 2) == 0)\
				) {\
				pValue |= MTK_OF_PROPERTY_VALUE_ENABLE;\
			} of_node_put(state_node);\
		} while (0); pValue; })

/*Parsing idle-state's status about sodi*/
#define GET_MTK_OF_PROPERTY_STATUS_SODI(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, MTK_LP_FEATURE_DTS_NAME_SODI)

/*Parsing idle-state's status about sodi3*/
#define GET_MTK_OF_PROPERTY_STATUS_SODI3(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, MTK_LP_FEATURE_DTS_NAME_SODI3)

/*Parsing idle-state's status about dp*/
#define GET_MTK_OF_PROPERTY_STATUS_DP(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, MTK_LP_FEATURE_DTS_NAME_DP)

/*Parsing idle-state's status about suspend*/
#define GET_MTK_OF_PROPERTY_STATUS_SUSPEND(_parentNode)\
	GET_MTK_IDLE_OF_PROPERTY_STATUS(\
		_parentNode, MTK_LP_FEATURE_DTS_NAME_SUSPEND)

/*Get idle-state node in dts*/
#define GET_MTK_IDLE_STATES_DTS_NODE() ({\
	struct device_node *_node = of_find_node_by_name(NULL\
		, MTK_LP_FEATURE_DTS_PROPERTY_IDLE_NODE);\
	_node; })

#define GET_MTK_SPM_DTS_NODE() ({\
	struct device_node *_node = of_find_compatible_node(NULL\
		, NULL, MTK_LP_SPM_DTS_COMPATIABLE_NODE);\
	_node; })
#endif
