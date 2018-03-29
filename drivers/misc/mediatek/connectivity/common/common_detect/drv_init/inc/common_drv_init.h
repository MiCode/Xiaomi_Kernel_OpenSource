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

#ifndef _COMMON_DRV_INIT_H_
#define _COMMON_DRV_INIT_H_
extern int do_common_drv_init(int chip_id);

extern int mtk_wcn_common_drv_init(void);
extern int mtk_wcn_hif_sdio_drv_init(void);
extern int mtk_wcn_stp_uart_drv_init(void);
extern int mtk_wcn_stp_sdio_drv_init(void);


#endif
