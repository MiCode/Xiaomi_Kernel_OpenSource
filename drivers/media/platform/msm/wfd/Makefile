ifeq ($(CONFIG_MSM_WFD),y)
  obj-y += wfd-ioctl.o
  obj-y += wfd-util.o
  obj-y += vsg-subdev.o
  ifeq ($(CONFIG_FB_MSM_WRITEBACK_MSM_PANEL),y)
    obj-y += mdp-4-subdev.o
  else ifeq ($(CONFIG_FB_MSM_MDSS_WRITEBACK),y)
    obj-y += mdp-5-subdev.o
  else
    obj-y += mdp-dummy-subdev.o
  endif
  obj-$(CONFIG_MSM_VIDC_1080P) += enc-mfc-subdev.o
  obj-$(CONFIG_MSM_VIDC_V4L2) += enc-venus-subdev.o
endif
