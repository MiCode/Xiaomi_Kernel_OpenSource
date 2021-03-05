# Goodix Touchscreen Makefile
# This Makefile is only for tempory compiling use
# Use xxxproject/Makefile-release when release driver
#
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX9896_I2C)           += goodix_core.o
goodix_core-y					:= goodix_ts_i2c.o goodix_ts_core.o goodix_cfg_bin.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX9896_UPDATE)    += goodix_gtx8_update.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX9896_GESTURE)   += goodix_ts_gesture.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX9896_TOOLS)     += goodix_ts_tools.o
obj-$(CONFIG_TOUCHSCREEN_GOODIX_GTX9896_TOOLS)     += goodix_ts_test.o

