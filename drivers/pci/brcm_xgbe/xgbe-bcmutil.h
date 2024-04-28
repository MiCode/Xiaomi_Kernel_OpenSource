/*
 * Broadcom BCM8956X / BCM8957X / BCM8989X 10Gb Ethernet driver
 *
 * Copyright (c) 2022 Broadcom. The term "Broadcom" refers solely to the
 * Broadcom Inc. subsidiary that distributes the Licensed Product, as defined
 * below.
 *
 * The following copyright statements and licenses apply to open source software
 * ("OSS") distributed with the BCM8956X / BCM8957X / BCM8989X product (the "Licensed Product").The
 * Licensed Product does not necessarily use all the OSS referred to below and
 * may also only use portions of a given OSS component.
 *
 * To the extent required under an applicable open source license, Broadcom
 * will make source code available for applicable OSS upon request. Please send
 * an inquiry to opensource@broadcom.com including your name, address, the
 * product name and version, operating system, and the place of purchase.
 *
 * To the extent the Licensed Product includes OSS, the OSS is typically no
 * owned by Broadcom. THE OSS IS PROVIDED AS IS WITHOUT WARRANTY OR CONDITION
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * To the full extent permitted under applicable law, Broadcom disclaims all
 * warranties and liability arising from or related to any use of the OSS.
 *
 * To the extent the Licensed Product includes OSS licensed under the GNU
 * General Public License ("GPL") or the GNU Lesser General Public License
 * ("LGPL"), the use, copying, distribution and modification of the GPL OSS or
 * LGPL OSS is governed, respectively, by the GPL or LGPL.  A copy of the GPL
 * or LGPL license may be found with the applicable OSS.  Additionally, a copy
 * of the GPL License or LGPL License can be found a
 * https://www.gnu.org/licenses or obtained by writing to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2 License
 *
 * Copyright (c) 2021 Broadcom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * License 2: Modified BSD License
 *
 * Copyright (c) 2021 Broadcom
 *
 * Redistribution and use in source and binary forms, with or withou
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyrigh
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyrigh
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __XGBE_BCMUTIL_H__
#define __XGBE_BCMUTIL_H__

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/log2.h>
#include <linux/version.h>

struct xgbe_ioctlcmd {
    uint32_t addr;
    uint32_t width;
    uint32_t len;
    uint8_t *data;
};

#define XGBE_IOCTL_RDMEM_SOCK    SIOCDEVPRIVATE+1
#define XGBE_IOCTL_WRMEM_SOCK    SIOCDEVPRIVATE+2

#define XGBE_IOCTL_RDMEM    _IOW('b', 1, struct xgbe_ioctlcmd)
#define XGBE_IOCTL_WRMEM    _IOR('b', 2, struct xgbe_ioctlcmd)

int xgbe_bcmutil_misc_driver_register(struct xgbe_prv_data *pdata);
void xgbe_bcmutil_fixup(struct xgbe_prv_data *pdata);
void xgbe_bcmutil_misc_driver_deregister(struct xgbe_prv_data* pdata);

int xgbe_bcmutil_pci_ioctl(struct xgbe_prv_data* pdata, unsigned int cmd, unsigned long ioctl_arg);

#endif /* __XGBE_BCMUTIL_H__ */
