
MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

MTKCAM_FDVT_VERSION := 5.1
MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1/inc

#obj-y:= fdvt_dummy.o
obj-$(CONFIG_MTK_CAMERA_FD_ISP6S) += camera_fdvt_isp51.o
camera_fdvt_isp51-objs += 5.1/camera_fdvt.o


ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
  subdir-ccflags-y += -DMTK_SECURE_FD_SUPPORT
endif



