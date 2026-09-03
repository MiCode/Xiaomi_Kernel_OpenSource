# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Unisoc_CoreSight drivers.
#
obj-$(CONFIG_UNISOC_CORESIGHT) += sprd_coresight.o
sprd_coresight-y := sprd_coresight-core.o  sprd_coresight-etm-perf.o sprd_coresight-platform.o \
		sprd_coresight-sysfs.o sprd_coresight-syscfg.o sprd_coresight-config.o \
		sprd_coresight-cfg-preload.o sprd_coresight-cfg-afdo.o \
		sprd_coresight-syscfg-configfs.o

obj-$(CONFIG_UNISOC_CORESIGHT_LINK_AND_SINK_TMC) += sprd_coresight-tmc.o
sprd_coresight-tmc-y := sprd_coresight-tmc-core.o sprd_coresight-tmc-etf.o

obj-$(CONFIG_UNISOC_CORESIGHT_LINKS_AND_SINKS) += sprd_coresight-funnel.o \
					   sprd_coresight-replicator.o

obj-$(CONFIG_UNISOC_CORESIGHT_SOURCE_ETM4X) += sprd_coresight-etm4x.o
sprd_coresight-etm4x-y := sprd_coresight-etm4x-core.o sprd_coresight-etm4x-sysfs.o \
			sprd_coresight-etm4x-cfg.o

obj-$(CONFIG_UNISOC_CORESIGHT_APETB) += sprd_coresight-apetb-ctrl.o sprd_coresight-apetb-main.o

obj-$(CONFIG_UNISOC_CORESIGHT_TMC_GROUP) += sprd_coresight-tmc_group.o

obj-$(CONFIG_UNISOC_CORESIGHT_TMC_GROUP) += phy/