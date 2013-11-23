ifeq ($(CONFIG_FB_MSM_MDSS_COMMON),y)
obj-y += mdss/
else
obj-y := msm_fb.o


ifeq ($(CONFIG_FB_MSM_MDP_HW),y)
# MDP
obj-y += mdp.o


ifeq ($(CONFIG_FB_MSM_MDP40),y)
else
obj-y += mdp_ppp.o
ifeq ($(CONFIG_FB_MSM_MDP31),y)
endif
endif
ifeq ($(CONFIG_FB_MSM_OVERLAY),y)
ifeq ($(CONFIG_FB_MSM_MIPI_DSI),y)
endif
else
endif



# EBI2

# LCDC

# LVDS

# MDDI
msm_mddi-objs := mddi.o
obj-$(CONFIG_FB_MSM_MDDI) += msm_mddi.o

# External MDDI
obj-$(CONFIG_FB_MSM_EXTMDDI) += msm_mddi_ext.o

# MIPI gereric
obj-$(CONFIG_FB_MSM_MIPI_DSI) += msm_mipi.o

# MIPI manufacture

# MIPI Bridge

# TVEnc

# MSM FB Panel



ccflags-y := -I$(src)/mhl
obj-$(CONFIG_FB_MSM_HDMI_MHL_8334) += mhl-8334.o


else
endif
endif

obj-$(CONFIG_MSM_VIDC_1080P) += vidc/
obj-$(CONFIG_MSM_VIDC_720P) += vidc/
clean:
	rm *.o .*cmd
