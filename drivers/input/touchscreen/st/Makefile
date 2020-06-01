# SPDX-License-Identifier: GPL-2.0-only
#
## Makefile for the STMicroelectronics touchscreen driver.
#

obj-$(CONFIG_TOUCHSCREEN_ST) += st_fts.o

st_fts-y := fts.o fts_gui.o fts_driver_test.o fts_aoi_event.o \
	fts_lib/ftsCompensation.o fts_lib/ftsCrossCompile.o fts_lib/ftsError.o \
	fts_lib/ftsFrame.o fts_lib/ftsIO.o fts_lib/ftsTest.o fts_lib/ftsTime.o \
	fts_lib/ftsTool.o fts_lib/ftsFlash.o fts_lib/ftsGesture.o
