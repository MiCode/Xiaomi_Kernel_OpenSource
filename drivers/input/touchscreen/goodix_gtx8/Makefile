# Goodix Touchscreen Makefile
# This Makefile is only for tempory compiling use
# Use xxxproject/Makefile-release when release driver
#
ifeq ($(CONFIG_QGKI), y)
	export CONFIG_TOUCHSCREEN_GOODIX_GTX8=y
	obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX8)  		+= goodix_ts_i2c.o
	obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX8)           += goodix_core.o
	goodix_core-y					:=  goodix_ts_core.o goodix_cfg_bin.o goodix_gtx8_update.o goodix_ts_test.o goodix_ts_tools.o goodix_ts_gesture.o wt_tp_palm.o wt_tp_grip_area.o
	obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX8_GESTURE)   += goodix_ts_gesture.o
else
	export CONFIG_TOUCHSCREEN_GOODIX_GTX8=m
	obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX8) += goodix_core.o
	goodix_core-y := \
				goodix_ts_i2c.o \
				goodix_ts_core.o \
				goodix_cfg_bin.o \
				goodix_gtx8_update.o \
				goodix_ts_gesture.o \
				goodix_ts_test.o \
				goodix_ts_tools.o \
				wt_tp_palm.o \
				wt_tp_grip_area.o
endif
