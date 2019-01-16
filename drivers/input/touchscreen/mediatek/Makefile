include $(srctree)/drivers/misc/mediatek/Makefile.custom


# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_init.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o

ifeq ($(CONFIG_TOUCHSCREEN_MTK_MAX1187X),y)
obj-y	+=  max1187x/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_FT5346),y)
obj-y	+=  ft5346/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_ATMEL336T),y)
obj-y	+=  atmel336t/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT9XX),y)
obj-y	+=  GT9XX/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_FT5346_CX865),y)
obj-y	+=  ft5346_cx865/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_ATMEL336T_CX865),y)
obj-y	+=  atmel336t_cx865/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT9XX_AIO),y)
obj-y	+=  GT9XX_aio/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT1151),y)
obj-y	+=  GT1151/
endif

ifeq ($(CONFIG_MTK_MMS244),y)
obj-y	+=  mms244/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT9XX_HOTKNOT),y)
obj-y	+=  GT9XX_hotknot/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT818B_FPGA),y)
obj-y	+=  GT818B-fpga/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT9XXTB_HOTKNOT),y)
obj-y	+=  GT9XXTB_hotknot/
endif

ifeq ($(CONFIG_TOUCHSCREEN_S7020),y)
obj-y	+=  S7020/
endif

obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX_HOTKNOT_SCP)	+=  GT9XX_hotknot_SCP/

ifeq ($(CONFIG_GT9XX_HOTKNOT_FLASHLESS),y)
obj-y	+=  GT9XX_hotknot_FLASHLESS/
endif

ifeq ($(CONFIG_MTK_GT9XX),y)
obj-y	+=  GT9XX/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_FTS2A052),y)
obj-y	+=  FTS2A052/
endif

ifeq ($(CONFIG_MTK_S2200),y)
obj-y   +=  synaptics_2200/
endif

ifeq ($(CONFIG_MTK_S3320),y)
obj-y	+=  synaptics_3320/
endif

ifeq ($(CONFIG_MTK_S3320_47),y)
obj-y	+=  synaptics_3320_47/
endif

ifeq ($(CONFIG_MTK_MMS134),y)
obj-y	+=  mms134/
endif

ifeq ($(CONFIG_TOUCHSCREEN_SYNAPTICS_S2202), y)
obj-y   +=  synaptics_2202/
endif

ifeq ($(CONFIG_TOUCHSCREEN_SYNAPTICS_S7020), y)
obj-y   +=  synaptics_7020/
endif

ifeq ($(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528),y)
obj-y   +=  synaptics_3528/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_S3528_2),y)
obj-y   +=  synaptics_3528_2/
endif

ifeq ($(CONFIG_MTK_S7020),y)
obj-y   +=  synaptics_7020_s7/
endif

ifeq ($(CONFIG_MTK_FT5406),y)
obj-y	+=  ft5406/
endif

ifeq ($(CONFIG_MTK_MMS128),y)
obj-y	+=  mms128/
endif

ifeq ($(CONFIG_MTK_FT5406_82),y)
obj-y	+=  ft5406_82/
endif

ifeq ($(CONFIG_MTK_FT5206),y)
obj-y	+=  ft5206/
endif

ifeq ($(CONFIG_TOUCHSCREEN_MTK_GT9XX_2),y)
obj-y	+=  GT9XX_2/
endif

ifeq ($(CONFIG_MTK_MIT200),y)
obj-y	+=  melfas_mit200/
endif
