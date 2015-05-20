/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2014 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  BSD LICENSE 

  Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ***********************************************************************************************
*/
/*
 *  File  : lwpmudrv_version.h
 */

#ifndef _LWPMUDRV_VERSION_H_
#define _LWPMUDRV_VERSION_H_

// SOCPERF VERSIONING

#define _STRINGIFY(x)     #x
#define STRINGIFY(x)      _STRINGIFY(x)
#define _STRINGIFY_W(x)   L#x
#define STRINGIFY_W(x)    _STRINGIFY_W(x)

#define SOCPERF_MAJOR_VERSION 1
#define SOCPERF_MINOR_VERSION 2
#define SOCPERF_API_VERSION   1

#define SOCPERF_NAME          "socperf"
#define SOCPERF_NAME_W        L"socperf"

#define SOCPERF_MSG_PREFIX    SOCPERF_NAME""STRINGIFY(SOCPERF_MAJOR_VERSION)"_"STRINGIFY(SOCPERF_MINOR_VERSION)":"
#define SOCPERF_VERSION_STR   STRINGIFY(SOCPERF_MAJOR_VERSION)"."STRINGIFY(SOCPERF_MINOR_VERSION)"."STRINGIFY(SOCPERF_API_VERSION)

#if defined(DRV_OS_WINDOWS)
#define SOCPERF_DRIVER_NAME   SOCPERF_NAME STRINGIFY(SOCPERF_MAJOR_VERSION)"_"STRINGIFY(SOCPERF_MINOR_VERSION)
#define SOCPERF_DRIVER_NAME_W SOCPERF_NAME_W STRINGIFY_W(SOCPERF_MAJOR_VERSION) L"_" STRINGIFY_W(SOCPERF_MINOR_VERSION)
#define SOCPERF_DEVICE_NAME   SOCPERF_DRIVER_NAME
#endif

#if defined(DRV_OS_LINUX) || defined(DRV_OS_SOLARIS) || defined(DRV_OS_ANDROID) || defined(DRV_OS_FREEBSD)
#define SOCPERF_DRIVER_NAME   SOCPERF_NAME""STRINGIFY(SOCPERF_MAJOR_VERSION)"_"STRINGIFY(SOCPERF_MINOR_VERSION)
#define SOCPERF_SAMPLES_NAME  SOCPERF_DRIVER_NAME"_s"
#define SOCPERF_DEVICE_NAME   "/dev/"SOCPERF_DRIVER_NAME
#endif

#if defined(DRV_OS_MAC)
#define SOCPERF_DRIVER_NAME   SOCPERF_NAME""STRINGIFY(SOCPERF_MAJOR_VERSION)"_"STRINGIFY(SOCPERF_MINOR_VERSION)
#define SOCPERF_SAMPLES_NAME  SOCPERF_DRIVER_NAME"_s"
#define SOCPERF_DEVICE_NAME   SOCPERF_DRIVER_NAME
#endif

#if defined(EMON_INTERNAL)
#define SOCPERF_DRIVER_MODE " (SOCPERF INTERNAL)"
#elif defined(EMON)
#define SOCPERF_DRIVER_MODE " (ESOCPERFMON)"
#else
#define SOCPERF_DRIVER_MODE ""
#endif

#endif

