include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(src)/../linux/include \
						 -I$(src)/../core/include \
						 -I$(src)/../include \
						 -I$(src)/ \
						 -I$(srctree)/drivers/misc/mediatek/connectivity/common_detect

ifeq ($(CONFIG_MTK_COMBO),y)
obj-y += wmt_core.o wmt_ctrl.o  wmt_func.o  wmt_lib.o wmt_conf.o wmt_dbg.o wmt_exp.o
obj-y	+= wmt_ic_6620.o
obj-y	+= wmt_ic_6628.o
obj-y	+= wmt_ic_6630.o

obj-y	+= stp_exp.o
obj-y	+= stp_core.o
obj-y	+= psm_core.o
obj-y	+= btm_core.o

endif
