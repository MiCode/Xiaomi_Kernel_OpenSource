# Makefile for the focaltech touchscreen drivers.
# N17 code for HQ-291711 by liunianliang at 2023/06/03 start
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
# N17 code for HQ-291711 by liunianliang at 2023/06/03 end

# N17 code for HQ-291091 by liunianliang at 2023/5/31 start
obj-m += ft3683g.o
ft3683g-y += focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_spi_v2.o \
		focaltech_flash.o \
		focaltech_upgrade_ft3683g.o \
		focaltech_test.o \
		focaltech_test_ini.o \
		focaltech_test_ft3683g.o
# N17 code for HQ-291091 by liunianliang at 2023/5/31 end
