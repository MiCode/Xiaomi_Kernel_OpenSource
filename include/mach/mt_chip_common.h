 /*
  *
  *
  * Copyright (C) 2008,2009 MediaTek <www.mediatek.com>
  * Authors: Infinity Chen <infinity.chen@mediatek.com>
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  */

#ifndef __MT_CHIP_COMMON_H__
#define __MT_CHIP_COMMON_H__

typedef enum {
    CHIP_INFO_NONE = 0,
    CHIP_INFO_HW_CODE,
    CHIP_INFO_HW_SUBCODE,
    CHIP_INFO_HW_VER,
    CHIP_INFO_SW_VER,

    CHIP_INFO_REG_HW_CODE,
    CHIP_INFO_REG_HW_SUBCODE,
    CHIP_INFO_REG_HW_VER,
    CHIP_INFO_REG_SW_VER,
    
    CHIP_INFO_FUNCTION_CODE,
    CHIP_INFO_PROJECT_CODE,
    CHIP_INFO_DATE_CODE,
    CHIP_INFO_FAB_CODE,
    CHIP_INFO_WAFER_BIG_VER,

    CHIP_INFO_MAX,
    CHIP_INFO_ALL,
} chip_info_t;

#define CHIP_INFO_BIT(ID) (1 << ID)
#define CHIP_INFO_SUP(MASK,ID) ((MASK & CHIP_INFO_BIT(ID)) ? (1) : (0))

#define C_UNKNOWN_CHIP_ID (0x0000FFFF)

struct mt_chip_drv
{
    /* raw information */
    unsigned int info_bit_mask;
    unsigned int (*get_chip_info)(chip_info_t id);
};

typedef unsigned int (*chip_info_cb)(void);
struct mt_chip_drv* get_mt_chip_drv(void);


#endif
