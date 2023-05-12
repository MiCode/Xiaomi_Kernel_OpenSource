# Makefile for the focaltech touchscreen drivers.

ifeq ($(CONFIG_BUILD_QGKI), y)
	export CONFIG_TOUCHSCREEN_FTS = y
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_core.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_ex_fun.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_ex_mode.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_gesture.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_esdcheck.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_point_report_check.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_test/
	obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_flash.o
	obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_flash/
	obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_i2c.o
else
	export CONFIG_TOUCHSCREEN_FTS = m
	obj-$(CONFIG_TOUCHSCREEN_FTS)	:= focaltech_tp.o
	focaltech_tp-objs	:= 	focaltech_core.o focaltech_ex_fun.o focaltech_ex_mode.o focaltech_gesture.o focaltech_esdcheck.o \
					focaltech_point_report_check.o focaltech_i2c.o focaltech_flash.o \
					focaltech_test/focaltech_test.o \
					focaltech_test/focaltech_test_ini.o \
					focaltech_test/supported_ic/focaltech_test_ft3519t.o \
					focaltech_test/supported_ic/focaltech_test_ft3658u.o \
					focaltech_flash/focaltech_upgrade_ft3519t.o \
					focaltech_flash/focaltech_upgrade_ft3658u.o
endif

