/*
 * Support for Medfield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _gdc_defs_h_
#define _gdc_defs_h_

#define HRT_GDC_N_BITS               6
#define HRT_GDC_N                     64

/* GDC lookup tables entries are 10 bits values, but they're
   stored 2 by 2 as 32 bit values, yielding 16 bits per entry.
   A GDC lookup table contains 64 * 4 elements */
#define HRT_GDC_LUT_BYTES             ((HRT_GDC_N * 4) * 2)

#define HRT_GDC_BLI_COEF_BITS              5
#define HRT_GDC_BLI_COEF_SHIFT             (HRT_GDC_BLI_COEF_BITS - 1)
#define HRT_GDC_BLI_COEF_ONE               (1 << HRT_GDC_BLI_COEF_SHIFT)

#define HRT_GDC_BCI_COEF_BITS             10
#define HRT_GDC_BCI_COEF_MASK              ((1 << HRT_GDC_BCI_COEF_BITS) - 1)
#define HRT_GDC_BCI_COEF_SHIFT             (HRT_GDC_BCI_COEF_BITS - 2)
#define HRT_GDC_BCI_COEF_ONE               (1 << HRT_GDC_BCI_COEF_SHIFT)

#define HRT_GDC_COORD_FRAC_BITS        4
#define HRT_GDC_COORD_ONE              (1 << HRT_GDC_COORD_FRAC_BITS)

#define HRT_GDC_MAX_GDC_IPY_16NWAY    (15*HRT_GDC_COORD_ONE - 1)
#define HRT_GDC_MAX_GDC_IPY_32NWAY    (26*HRT_GDC_COORD_ONE - 1)
#define HRT_GDC_MAX_GDC_IPY_64NWAY    (49*HRT_GDC_COORD_ONE - 1)

#define _HRT_GDC_REG_ALIGN         4

#define HRT_GDC_NND_CMD               4
#define HRT_GDC_BLI_CMD               5
#define HRT_GDC_BCI_CMD               6
#define HRT_GDC_GD_CAC_CMD            7
#define HRT_GDC_CONFIG_CMD            8

/* This is how commands are packed into one fifo token */
#define HRT_GDC_CMD_DATA_POS         16 
#define HRT_GDC_CMD_DATA_BITS        16 
#define HRT_GDC_CMD_BITS              4 
#define HRT_GDC_REG_ID_BITS           8 
#define HRT_GDC_CRUN_BIT              (HRT_GDC_CMD_BITS + HRT_GDC_REG_ID_BITS)

#define HRT_GDC_MODE_IDX              0  
#define HRT_GDC_BPP_IDX               1  
#define HRT_GDC_END_IDX               2  
#define HRT_GDC_WOIX_IDX              3  
#define HRT_GDC_WOIY_IDX              4  
#define HRT_GDC_STX_IDX               5  
#define HRT_GDC_STY_IDX               6  
#define HRT_GDC_OXDIM_IDX             7  
#define HRT_GDC_OYDIM_IDX             8  
#define HRT_GDC_SRC_ADDR_IDX          9  
#define HRT_GDC_SRC_END_ADDR_IDX     10 
#define HRT_GDC_SRC_WRAP_ADDR_IDX    11 
#define HRT_GDC_SRC_STRIDE_IDX       12 
#define HRT_GDC_DST_ADDR_IDX         13 
#define HRT_GDC_DST_STRIDE_IDX       14 
#define HRT_GDC_DX_IDX               15 
#define HRT_GDC_DY_IDX               16 
#define HRT_GDC_P0X_IDX              17 
#define HRT_GDC_P1X_IDX              18 
#define HRT_GDC_P2X_IDX              19 
#define HRT_GDC_P3X_IDX              20 
#define HRT_GDC_P0Y_IDX              21 
#define HRT_GDC_P1Y_IDX              22 
#define HRT_GDC_P2Y_IDX              23 
#define HRT_GDC_P3Y_IDX              24 
#define HRT_GDC_SOFT_RST_IDX         25 

#ifndef __HIVECC
#define HRT_GDC_ELEM_WIDTH_IDX       26 /* Only for csim */
#define HRT_GDC_ELEMS_PER_WORD_IDX   27 /* Only for csim */
#define HRT_GDC_BYTES_PER_WORD_IDX   28 /* Only for csim */
#define HRT_GDC_CRUN_IDX             29 /* Only for csim */
#endif

#define HRT_GDC_LUT_IDX              32 

#define HRT_GDC_MAX_DX		     1023
#define HRT_GDC_MAX_DY		     1023
#define HRT_GDC_MAX_PX		     (128*16)
#define HRT_GDC_MAX_PY		     (64*16)
#define HRT_GDC_MAX_WOIX	     2048
#define HRT_GDC_MAX_WOIY	     16
#define HRT_GDC_MAX_OXDIM	     4096
#define HRT_GDC_MAX_OYDIM	     64

#endif /* _gdc_defs_h_ */
