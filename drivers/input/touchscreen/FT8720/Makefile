# Makefile for the focaltech touchscreen drivers.

obj-$(CONFIG_TOUCHSCREEN_FT8720) += fts_8720.o
fts_8720-y := focaltech_core.o focaltech_ex_fun.o focaltech_ex_mode.o \
        focaltech_gesture.o focaltech_esdcheck.o focaltech_point_report_check.o \
        focaltech_spi.o focaltech_flash.o \
        focaltech_test/focaltech_test.o focaltech_test/focaltech_test_ini.o \
	focaltech_test/supported_ic/focaltech_test_ft8720.o
