ifeq ($(CONFIG_MTK_COMBO),y)

ccflags-y += \
	-I$(src)/../../linux/include \
	-I$(src)/../../core/include \
	-I$(src)/../../include \
	-I$(src)/../../../common_detect

PLAT := alps
obj-y	+= wmt_plat_$(PLAT).o
obj-y	+= wmt_plat_stub.o
obj-y	+= mtk_wcn_cmb_hw.o

endif
