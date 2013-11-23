ifeq ($(CONFIG_MSM_WFD),y)
  obj-y += wfd-ioctl.o
  obj-y += wfd-util.o
  obj-y += vsg-subdev.o

  #pick up the MDP subdevice
  ifeq ($(CONFIG_FB_MSM_WRITEBACK_MSM_PANEL),y)
  else ifeq ($(CONFIG_FB_MSM_MDSS_WRITEBACK),y)
    obj-y += mdp-5-subdev.o
  else
  endif

  #pick up the Encoder subdevice
  ifeq ($(CONFIG_MSM_VIDC_1080P),y)
  else ifeq ($(CONFIG_MSM_VIDC_V4L2),y)
    obj-y += enc-venus-subdev.o
  else
  endif

endif
