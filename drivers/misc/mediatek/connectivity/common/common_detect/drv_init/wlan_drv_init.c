/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WLAN-MOD-INIT]"

#include "wmt_detect.h"
#include "wlan_drv_init.h"

int __attribute__((weak)) mtk_wcn_wlan_gen4_init()
{
	WMT_DETECT_PR_INFO("no impl. mtk_wcn_wlan_gen4_init\n");
	return 0;
}

int __attribute__((weak)) mtk_wcn_wlan_gen3_init()
{
	WMT_DETECT_PR_INFO("no impl. mtk_wcn_wlan_gen3_init\n");
	return 0;
}

int __attribute__((weak)) mtk_wcn_wlan_gen2_init()
{
	WMT_DETECT_PR_INFO("no impl. mtk_wcn_wlan_gen2_init\n");
	return 0;
}

int __attribute__((weak)) mtk_wcn_wmt_wifi_init()
{
	WMT_DETECT_PR_INFO("no impl. mtk_wcn_wmt_wifi_init\n");
	return 0;
}

int do_wlan_drv_init(int chip_id)
{
	int i_ret = 0;
	int ret = 0;

	WMT_DETECT_PR_INFO("start to do wlan module init 0x%x\n", chip_id);

	/* WMT-WIFI char dev init */
	ret = mtk_wcn_wmt_wifi_init();
	WMT_DETECT_PR_INFO("WMT-WIFI char dev init, ret:%d\n", ret);
	i_ret += ret;

	switch (chip_id) {
	case 0x6580:
	case 0x6739:
		ret = mtk_wcn_wlan_gen2_init();
		WMT_DETECT_PR_INFO("WLAN-GEN2 driver init, ret:%d\n", ret);
		break;

	case 0x6630:
	case 0x6797:
	case 0x6758:
	case 0x6759:
	case 0x6775:
	case 0x6771:
		/* WLAN driver init */
		ret = mtk_wcn_wlan_gen3_init();
		WMT_DETECT_PR_INFO("WLAN-GEN3 driver init, ret:%d\n", ret);
		break;

	default:
		/* WLAN driver init */
		ret = mtk_wcn_wlan_gen4_init();
		WMT_DETECT_PR_INFO("WLAN-GEN4 driver init, ret:%d\n", ret);
		break;
	}

	i_ret += ret;

	WMT_DETECT_PR_INFO("finish wlan module init\n");

	return i_ret;
}
