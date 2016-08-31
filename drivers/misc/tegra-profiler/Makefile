#
# drivers/misc/tegra-profiler/Makefile
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
# Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
#

obj-$(CONFIG_TEGRA_PROFILER) += tegra-profiler.o

tegra-profiler-objs := \
	main.o \
	armv7_pmu.o \
	hrt.o \
	comm.o \
	mmap.o \
	backtrace.o \
	debug.o \
	ma.o \
	power_clk.o \
	auth.o \
	quadd_proc.o \
	eh_unwind.o

ifdef CONFIG_CACHE_L2X0
tegra-profiler-objs += pl310.o
endif