mediatek-drm-y += mi_disp/mi_disp_core.o \
       mi_disp/mi_disp_feature.o \
       mi_disp/mi_disp_file.o \
       mi_disp/mi_dsi_display.o \
       mi_disp/mi_dsi_panel.o \
       mi_disp/mi_disp_print.o

ifeq ($(CONFIG_MI_DISP_DEBUGFS), y)
mediatek-drm-y += mi_disp/mi_disp_debugfs.o
endif

ifeq ($(CONFIG_MI_DISP_SYSFS), y)
mediatek-drm-y += mi_disp/mi_disp_sysfs.o
endif

ifeq ($(CONFIG_MI_DISP_PROCFS), y)
mediatek-drm-y += mi_disp/mi_disp_procfs.o
endif

ifeq ($(CONFIG_MI_DISP_ESD_CHECK), y)
mediatek-drm-y += mi_disp/mi_disp_esd_check.o
endif

ifeq ($(CONFIG_MI_DISP_BRIGTHNESS_CLONE_COOLER), y)
mediatek-drm-y += mi_disp/mi_cooler_brightness_clone.o
endif

ifeq ($(CONFIG_MI_DISP_NOTIFIER), y)
mediatek-drm-y += mi_disp/mi_disp_notifier.o
endif

ifeq ($(CONFIG_MI_DISP_LOG), y)
mediatek-drm-y += mi_disp/mi_disp_log.o
endif

ifeq ($(CONFIG_MI_DISP_LHBM), y)
mediatek-drm-y += mi_disp/mi_disp_lhbm.o
endif

ifeq ($(CONFIG_MI_DISP_INPUT_HANDLER), y)
mediatek-drm-y += mi_disp/mi_disp_input_handler.o
endif

ifeq ($(CONFIG_MI_DISP_BOOST), y)
mediatek-drm-y += mi_disp/mi_disp_boost.o
endif

ifeq ($(CONFIG_MI_DISP_FOD_SYNC), y)
mediatek-drm-y += mi_disp/mi_drm_crtc.o
endif
