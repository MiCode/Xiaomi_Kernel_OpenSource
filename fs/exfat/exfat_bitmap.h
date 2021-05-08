/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : exfat_global.h                                            */
/*  PURPOSE : Header File for exFAT Global Definitions & Misc Functions */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  REVISION HISTORY (Ver 0.9)                                          */
/*                                                                      */
/*  - 2010.11.15 [Joosun Hahn] : first writing                          */
/*                                                                      */
/************************************************************************/

#ifndef _EXFAT_BITMAP_H
#define _EXFAT_BITMAP_H

#include <linux/types.h>

/*======================================================================*/
/*                                                                      */
/*       LIBRARY FUNCTION DECLARATIONS -- OTHER UTILITY FUNCTIONS       */
/*                    (DO NOT CHANGE THIS PART !!)                      */
/*                                                                      */
/*======================================================================*/

/*----------------------------------------------------------------------*/
/*  Bitmap Manipulation Functions                                       */
/*----------------------------------------------------------------------*/

s32	exfat_bitmap_test(u8 *bitmap, int i);
void	exfat_bitmap_set(u8 *bitmap, int i);
void	exfat_bitmap_clear(u8 *bitmpa, int i);

#endif /* _EXFAT_BITMAP_H */
