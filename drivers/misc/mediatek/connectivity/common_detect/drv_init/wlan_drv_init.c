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

int __weak mtk_wcn_wlan_soc_init(void)
{
    WMT_DETECT_ERR_FUNC("mtk_wcn_wlan_soc_init is not define!\n");
	return 0;
}

int do_wlan_drv_init(int chip_id)
{
	int i_ret = 0;
	int ret = 0;

#ifdef CONFIG_MTK_COMBO_WIFI
	WMT_DETECT_INFO_FUNC("start to do wlan module init 0x%x\n", chip_id);
	
	switch (chip_id)
	{
		case 0x6620:
#ifdef MT6620
			/* WMT-WIFI char dev init */
			ret = mtk_wcn_wmt_wifi_init();
			WMT_DETECT_INFO_FUNC("WMT-WIFI char dev init, ret:%d\n", ret);
			i_ret += ret;
			
			/* WLAN driver init*/
			ret = mtk_wcn_wlan_6620_init();
			WMT_DETECT_INFO_FUNC("WLAN driver init, ret:%d\n", ret);
			i_ret += ret;
#else
			WMT_DETECT_ERR_FUNC("MT6620 is not supported, please check kernel makefile or project config\n");
			i_ret = -1;
#endif
			break;
			
		case 0x6628:
#ifdef MT6628
			/* WMT-WIFI char dev init */
			ret = mtk_wcn_wmt_wifi_init();
			WMT_DETECT_INFO_FUNC("WMT-WIFI char dev init, ret:%d\n", ret);
			i_ret += ret;
			
			/* WLAN driver init*/
			ret = mtk_wcn_wlan_6628_init();
			WMT_DETECT_INFO_FUNC("WLAN driver init, ret:%d\n", ret);
			i_ret += ret;
#else
			WMT_DETECT_ERR_FUNC("MT6628 is not supported, please check kernel makefile or project config\n");
			i_ret = -1;
#endif
			break;
		case 0x6630:
#ifdef MT6630
			/* WMT-WIFI char dev init */
			ret = mtk_wcn_wmt_wifi_init();
			WMT_DETECT_INFO_FUNC("WMT-WIFI char dev init, ret:%d\n", ret);
			i_ret += ret;
			
			/* WLAN driver init*/
			ret = mtk_wcn_wlan_6630_init();
			WMT_DETECT_INFO_FUNC("WLAN driver init, ret:%d\n", ret);
			i_ret += ret;
#else
			WMT_DETECT_ERR_FUNC("MT6630 is not supported, please check kernel makefile or project config\n");
			i_ret = -1;
#endif
			break;		
		case 0x6572:
		case 0x6582:
		case 0x6592:
		case 0x6571:
		case 0x8127:
		case 0x6752:
        case 0x6735:
#ifdef MTK_WCN_SOC_CHIP_SUPPORT
			/* WMT-WIFI char dev init */
			ret = mtk_wcn_wmt_wifi_soc_init();
			WMT_DETECT_INFO_FUNC("WMT-WIFI char dev init, ret:%d\n", ret);
			i_ret += ret;
			
			/* WLAN driver init*/
			ret = mtk_wcn_wlan_soc_init();
			WMT_DETECT_INFO_FUNC("WLAN driver init, ret:%d\n", ret);
			i_ret += ret;
#else
			WMT_DETECT_ERR_FUNC("SOC is not supported, please check kernel makefile or project config\n");
			i_ret = -1;
#endif
			break;
	}
	
	WMT_DETECT_INFO_FUNC("finish wlan module init\n");

#else

	WMT_DETECT_INFO_FUNC("WLAN function not supported, skip\n");

#endif

	return i_ret;
}


