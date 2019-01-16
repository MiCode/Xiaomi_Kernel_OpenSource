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

#ifndef _WLAN_DRV_INIT_H_
#define _WLAN_DRV_INIT_H_
extern int do_wlan_drv_init(int chip_id);


#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
extern int mtk_wcn_wmt_wifi_init(void);
#ifdef MT6620
extern int mtk_wcn_wlan_6620_init(void);
#endif
#ifdef MT6628
extern int mtk_wcn_wlan_6628_init(void);
#endif
#ifdef MT6630
extern int mtk_wcn_wlan_6630_init(void);
#endif
#endif

#ifdef MTK_WCN_SOC_CHIP_SUPPORT
extern int mtk_wcn_wmt_wifi_soc_init(void);
extern int mtk_wcn_wlan_soc_init(void);
#endif
#endif
