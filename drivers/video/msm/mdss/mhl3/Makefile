ccflags-y += -I$(src)

ccflags-y += -DANSI_COLORS
ccflags-y += -DDEBUG
ccflags-y += -DMHL_BUILD_NUM=1
ccflags-y += -DDEVELOPER_BUILD_ID=1
ccflags-y += -DBUILD_NUM_STRING=\"$(MHL_BUILD_NUM)$(DEVELOPER_BUILD_ID)\"
ccflags-y += -DMHL_PRODUCT_NUM=8620
ccflags-y += -DMHL_DRIVER_NAME=\"sii$(MHL_PRODUCT_NUM)drv\"
ccflags-y += -DMHL_DEVICE_NAME=\"sii-$(MHL_PRODUCT_NUM)\"
ccflags-y += $(MHL_HOST_PLATFORM)
ccflags-y += -DMANUAL_EDID_FETCH
ccflags-y += -DPRINT_DDC_ABORTS
ccflags-y += -DSIMG_USE_DTS
ccflags-y += -DINCLUDE_HID=0
ccflags-y += -DINCLUDE_RBP=0
ccflags-y += -DLINUX_KERNEL_VER=310
ccflags-y += -DINCLUDE_SII6031=0

mdss-mhl3-objs := platform.o
mdss-mhl3-objs += mhl_linux_tx.o
mdss-mhl3-objs += mhl_rcp_inputdev.o
mdss-mhl3-objs += mhl_supp.o
mdss-mhl3-objs += si_8620_drv.o
mdss-mhl3-objs += si_mhl2_edid_3d.o
mdss-mhl3-objs += si_mdt_inputdev.o
obj-y += mdss-mhl3.o
