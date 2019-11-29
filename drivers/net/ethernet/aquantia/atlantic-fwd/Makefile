################################################################################
#
# aQuantia Ethernet Controller AQtion Linux Driver
# Copyright(c) 2014-2017 aQuantia Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.
#
# The full GNU General Public License is included in this distribution in
# the file called "COPYING".
#
# Contact Information: <rdc-drv@aquantia.com>
# aQuantia Corporation, 105 E. Tasman Dr. San Jose, CA 95134, USA
#
################################################################################

#
# Makefile for the AQtion(tm) Ethernet driver
#

obj-$(CONFIG_AQFWD) += atlantic-fwd.o

atlantic-fwd-objs := atl_fw.o \
		     atl2_fw.o \
		     atl_hw.o \
		     atl_main.o \
		     atl_ring.o \
		     atl_ethtool.o \
		     atl_trace.o \
		     atl_compat.o \
		     atl_hwmon.o

atlantic-fwd-$(CONFIG_ATLFWD_FWD) += atl_fwd.o
atlantic-fwd-$(CONFIG_ATLFWD_FWD_NETLINK) += atl_fwdnl.o \
					      atl_fwdnl_params.o
atlantic-fwd-$(CONFIG_MACSEC) += atl_macsec.o macsec/macsec_api.o

atlantic-fwd-$(CONFIG_AQFWD_QCOM) += atl_qcom.o
atlantic-fwd-$(CONFIG_AQFWD_QCOM_IPA) += atl_qcom_ipa.o

ccflags-y += -I$(src)
