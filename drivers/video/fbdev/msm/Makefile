ifeq ($(CONFIG_FB_MSM_MDSS_COMMON),y)
obj-y += ../../msm/
else
obj-y := msm_fb.o

# MDP DMA/PPP engine
#
obj-y += mdp.o mdp_scale_tables.o mdp_ppp.o

# MDDI interface
#
obj-y += mddi.o

# MDDI client/panel drivers
#
obj-y += mddi_client_dummy.o
obj-y += mddi_client_toshiba.o
obj-y += mddi_client_nt35399.o
endif
