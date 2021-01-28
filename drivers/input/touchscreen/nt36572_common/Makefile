# include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
obj-y += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
obj-y   +=  tpd_setting.o
obj-y   +=  ../mtk_tpd.o
obj-y   +=  ../tpd_button.o