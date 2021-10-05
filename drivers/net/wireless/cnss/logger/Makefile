# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for cnss logger
#

obj-$(CONFIG_CNSS_LOGGER) += logger.o

logger-y += main.o	\
	    nl_service.o

logger-$(CONFIG_DEBUG_FS) += debugfs.o
