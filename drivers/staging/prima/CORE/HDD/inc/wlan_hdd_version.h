/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

//==================================================================
//
//  File:         hdd_Version.h
//
//  Description:  Miniport driver version information
//
//  Author:       Larry Cawley
// 
//  Copyright 2007, QUALCOMM, Inc.  All rights reserved.
//
//===================================================================
#if !defined( __hddVersion_h__ )
#define __hddVersion_h__

// force string expansion from chars                               
#define strEXPAND(x) #x
#define strSTRING(x) strEXPAND(x)
#define strVERSION( _mj, _mn, _sfx, _build ) strSTRING(_mj) "." strSTRING(_mn) "." strSTRING(_sfx) "." strSTRING(_build)


#if defined( BLD_REL )
#define HDD_DRIVER_MAJOR_VERSION BLD_REL
#else 
#define HDD_DRIVER_MAJOR_VERSION             0   
#endif

#if defined( BLD_VER )
#define HDD_DRIVER_MINOR_VERSION BLD_VER
#else 
#define HDD_DRIVER_MINOR_VERSION             0
#endif

#if defined( BLD_SFX )
#define HDD_DRIVER_SUFFIX BLD_SFX 
#else
#define HDD_DRIVER_SUFFIX                    0
#endif 

#if defined( BLD_NUM )
#define HDD_DRIVER_BUILD BLD_NUM
#else 
#define HDD_DRIVER_BUILD                     0000  
#endif 
#define HDD_BUILD_DATETIME __DATE__ " " __TIME__  

#define HDD_DRIVER_VERSION WNI_DRIVER_MAJOR_VERSION,WNI_DRIVER_MINOR_VERSION

#define HDD_DRIVER_VERSION_STR strVERSION( WNI_DRIVER_MAJOR_VERSION, WNI_DRIVER_MINOR_VERSION, WNI_DRIVER_SUFFIX, WNI_DRIVER_BUILD )
                                            
#define HDD_COMPANYNAME_FULL        "QUALCOMM, Inc."
#define HDD_DRIVER_DESCRIPTION      "QUALCOMM Gen6 802.11n Wireless Adapter"
#define OEM_FILEDESCRIPTION_STR     ANI_DRIVER_DESCRIPTION 
                        
#define OEM_COMPANYNAME_STR         ANI_COMPANYNAME_FULL 

#define OEM_INTERNALNAME_STR        "WLAN_QCT_DRV.dll"
#define OEM_INTERNALNAME_STR2       "WLAN_QCT_DRV.dll"
#define OEM_ORIGINALFILENAME_STR    "WLAN_QCT_DRV.dll"

#define OEM_LEGALCOPYRIGHT_YEARS    "2008"
#define OEM_LEGALCOPYRIGHT_STR      "Copyright \251 " OEM_COMPANYNAME_STR "," OEM_LEGALCOPYRIGHT_YEARS
#define OEM_PRODUCTNAME_STR         HDD_DRIVER_DESCRIPTION

#define OEM_PRODUCTVERSION          HDD_DRIVER_VERSION
#define OEM_FILEVERSION_STR         HDD_DRIVER_VERSION_STR
#define OEM_FILEVERSION             HDD_DRIVER_MAJOR_VERSION,HDD_DRIVER_MINOR_VERSION,HDD_DRIVER_SUFFIX,HDD_DRIVER_BUILD
#define OEM_PRODUCTVERSION_STR      HDD_DRIVER_VERSION_STR


#endif  // __hddVersion_h__
