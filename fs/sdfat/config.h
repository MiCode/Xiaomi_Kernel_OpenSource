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
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SDFAT_CONFIG_H
#define _SDFAT_CONFIG_H
/*======================================================================*/
/*                                                                      */
/*                        FFS CONFIGURATIONS                            */
/*                  (CHANGE THIS PART IF REQUIRED)                      */
/*                                                                      */
/*======================================================================*/

/*----------------------------------------------------------------------*/
/* Feature Config                                                       */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Debug/Experimental Config                                            */
/*----------------------------------------------------------------------*/



/*----------------------------------------------------------------------*/
/* Defragmentation Config                                               */
/*----------------------------------------------------------------------*/




/*----------------------------------------------------------------------*/
/* Config for Kernel equal or newer than 3.7                            */
/*----------------------------------------------------------------------*/
#ifndef CONFIG_SDFAT_WRITE_SB_INTERVAL_CSECS
#define CONFIG_SDFAT_WRITE_SB_INTERVAL_CSECS	(dirty_writeback_interval)
#endif

/*----------------------------------------------------------------------*/
/* Default Kconfig                                                      */
/*----------------------------------------------------------------------*/
/* default mount options                            */
#ifndef CONFIG_SDFAT_DEFAULT_CODEPAGE /* if Kconfig lacked codepage */
#define CONFIG_SDFAT_DEFAULT_CODEPAGE   437
#endif

#ifndef CONFIG_SDFAT_DEFAULT_IOCHARSET /* if Kconfig lacked iocharset */
#define CONFIG_SDFAT_DEFAULT_IOCHARSET  "utf8"
#endif

#ifndef CONFIG_SDFAT_FAT32_SHORTNAME_SEQ /* Shortname ~1, ... ~9 have higher
					  * priority (WIN32/VFAT-like)
					  */

#endif

#ifndef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE

#endif

#ifndef CONFIG_SDFAT_FAT_MIRRORING /* if Kconfig lacked fat-mirroring option */
#define CONFIG_SDFAT_FAT_MIRRORING /* Write FAT 1, FAT 2 simultaneously */
#endif

#ifndef CONFIG_SDFAT_DELAYED_META_DIRTY

#endif

#ifndef CONFIG_SDFAT_SUPPORT_DIR_SYNC

#endif

#ifndef CONFIG_SDFAT_CHECK_RO_ATTR

#endif

#ifndef CONFIG_SDFAT_RESTRICT_EXT_ONLY_SFN
#define CONFIG_SDFAT_RESTRICT_EXT_ONLY_SFN
#endif

#ifndef CONFIG_SDFAT_ALLOW_LOOKUP_LOSSY_SFN

#endif

#ifndef CONFIG_SDFAT_DBG_SHOW_PID

#endif

#ifndef CONFIG_SDFAT_VIRTUAL_XATTR

#endif

#ifndef CONFIG_SDFAT_SUPPORT_STLOG

#endif

#ifndef CONFIG_SDFAT_DEBUG



#ifndef CONFIG_SDFAT_DBG_IOCTL

#endif

#ifndef CONFIG_SDFAT_DBG_MSG

#endif

#ifndef CONFIG_SDFAT_DBG_CAREFUL

#endif

#ifndef CONFIG_SDFAT_DBG_BUGON

#endif

#ifndef CONFIG_SDFAT_DBG_WARNON

#endif

#endif /* CONFIG_SDFAT_DEBUG */


#ifndef	CONFIG_SDFAT_TRACE_SB_LOCK

#endif

#ifndef	CONFIG_SDFAT_TRACE_ELAPSED_TIME

#endif

#endif /* _SDFAT_CONFIG_H */

/* end of config.h */
