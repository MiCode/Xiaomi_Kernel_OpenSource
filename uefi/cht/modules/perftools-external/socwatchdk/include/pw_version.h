/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2013 Intel Corporation. All rights reserved.

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

  Contact Information:
  SOCWatch Developer Team <socwatchdevelopers@intel.com>

  BSD LICENSE 

  Copyright(c) 2013 Intel Corporation. All rights reserved.
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

#ifndef _PW_VERSION_H_
#define _PW_VERSION_H_ 1

/*
 * SOCWatch driver version
 * Current driver version is 1.0.0
 * Current driver version is 1.1.0
 */
#define PW_DRV_VERSION_MAJOR 1
#define PW_DRV_VERSION_MINOR 5
#define PW_DRV_VERSION_OTHER 0
#define PW_DRV_VERSION_STRING "1.5" // used by matrix
#define PW_DRV_NAME "socwatch1_5"

/*
 * Every SOCWatch component shares the same version number.
 */
#define SOCWATCH_VERSION_MAJOR 1
#define SOCWATCH_VERSION_MINOR 5
#define SOCWATCH_VERSION_OTHER 1

/*
 * WUWatch driver version
 */
#define PW_DRV_VERSION 3
#define PW_DRV_INTERFACE 1
#define PW_DRV_OTHER 9

/*
 * Every wuwatch component shares the same version number.
 * THIS WILL BE REMOVED WHEN NOT USED IN WUWATCH
 */
#define WUWATCH_VERSION_VERSION 3
#define WUWATCH_VERSION_INTERFACE 1
#define WUWATCH_VERSION_OTHER 9

/*
 * Power interface version
 * Current interface version is 0.1.0
 * Current interface version is 0.2.0
 */
#define PW_INT_VERSION_VERSION 0
#define PW_INT_VERSION_INTERFACE 2
#define PW_INT_VERSION_OTHER 0

#endif // _PW_VERSION_H_
