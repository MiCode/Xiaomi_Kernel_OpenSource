# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/MTK1151
ccflags-y += -I$(srctree)/drivers/input/touchscreen/MTK1151/include/$(CONFIG_GT1151_FIRMWARE)/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/MTK1151/include/$(CONFIG_GT1151_CONFIG)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors
obj-m += mtk1151.o
mtk1151-y   +=  mtk_tpd.o
mtk1151-y   +=  tpd_button.o
mtk1151-y   +=  tpd_calibrate.o
mtk1151-y   +=  tpd_debug.o
mtk1151-y   +=  tpd_default.o
mtk1151-y   +=  tpd_misc.o
mtk1151-y   +=  tpd_setting.o
mtk1151-y   +=  gt1x_generic.o
mtk1151-n   +=  gt1x_tools.o
mtk1151-y   +=  gt1x_tpd.o
mtk1151-y   +=  gt1x_update.o
mtk1151-y   +=  gt1x_extents.o
