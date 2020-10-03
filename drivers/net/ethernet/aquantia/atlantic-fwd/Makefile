# SPDX-License-Identifier: GPL-2.0-only
# Atlantic Network Driver
#
# Copyright(c) 2014-2019 aQuantia Corporation.
# Copyright(c) 2019-2020 Marvell International Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

obj-$(CONFIG_AQFWD) += atlantic-fwd.o

atlantic-fwd-objs := atl_fw.o \
		     atl2_fw.o \
		     atl_hw.o \
		     atl_main.o \
		     atl_ring.o \
		     atl_ethtool.o \
		     atl_trace.o \
		     atl_compat.o \
		     atl_hwmon.o \
		     atl_ptp.o \
		     atl_hw_ptp.o

atlantic-fwd-$(CONFIG_ATLFWD_FWD) += atl_fwd.o
atlantic-fwd-$(CONFIG_ATLFWD_FWD_NETLINK) += atl_fwdnl.o \
					      atl_fwdnl_params.o
atlantic-fwd-$(CONFIG_MACSEC) += atl_macsec.o macsec/macsec_api.o

ccflags-y += -I$(src)
