# include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
obj-m += nt36672c_i2c.o
nt36672c_i2c-y += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
nt36672c_i2c-y   +=  tpd_setting.o
nt36672c_i2c-y   +=  mtk_tpd.o
nt36672c_i2c-y   +=  tpd_button.o