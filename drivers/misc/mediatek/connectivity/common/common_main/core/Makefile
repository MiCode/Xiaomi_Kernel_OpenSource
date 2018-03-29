ifeq ($(CONFIG_MTK_COMBO),y)

ccflags-y += \
	-I$(src)/../linux/include \
	-I$(src)/../core/include \
	-I$(src)/../include \
	-I$(src)/../../common_detect \
	-I$(srctree)/drivers/misc/mediatek/btif/common/inc \

obj-y += wmt_core.o wmt_ctrl.o  wmt_func.o  wmt_lib.o wmt_conf.o wmt_exp.o
obj-y	+= wmt_ic_6620.o
obj-y	+= wmt_ic_6628.o
obj-y	+= wmt_ic_6630.o
obj-y	+= wmt_ic_6632.o
obj-y	+= wmt_ic_soc.o

obj-y	+= stp_exp.o
obj-y	+= stp_core.o
obj-y	+= psm_core.o
obj-y	+= btm_core.o

endif
