# Makefile for the Himax touchscreen drivers.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

MODULE_NAME := himax_mmi

PREFIX_SRC = $(srctree)/$(src)/

HX_SRC := $(wildcard $(srctree)/$(src)/*.c)

# string definition
STR_HXINCELL := himax_ic_incell_core
STR_HX83102 := himax_ic_HX83102
STR_HX83108 := himax_ic_HX83108
STR_HX83112 := himax_ic_HX83112
STR_HX83121 := himax_ic_HX83121

STR_HXONCELL := himax_ic_oncell_core
STR_HX852XJ := himax_ic_HX852xJ

STR_HXDEBUG := himax_debug
STR_HXINSPT := himax_inspection

ifneq ($(filter y, $(CONFIG_KALLSYMS_ALL)),)
	ccflags-y += -D__KERNEL_KALLSYMS_ALL_ENABLED__
endif

ifneq ($(filter y, $(CONFIG_FB)),)
	ccflags-y += -DHX_CONFIG_FB
endif

ifneq ($(filter y, $(CONFIG_DRM)),)
	ccflags-y += -DHX_CONFIG_DRM
endif

ifneq ($(filter y, $(CONFIG_HIMAX_WAKE_UP)),)
	ccflags-y += -DHX_SMART_WAKEUP
endif

#DEBUG function
ifeq ($(STR_HXDEBUG), $(findstring $(STR_HXDEBUG), $(HX_SRC)))
  ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_DEBUG
endif

#INSPECTION function
ifeq ($(STR_HXINSPT), $(findstring $(STR_HXINSPT), $(HX_SRC)))
  ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_INSPECT
endif

# INCELL
ifeq ($(STR_HXINCELL), $(findstring $(STR_HXINCELL), $(HX_SRC)))
  ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_INCELL
  ifeq ($(STR_HX83102), $(findstring $(STR_HX83102), $(HX_SRC)))
    ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_IC_HX83102
    ccflags-y += -D__HIMAX_HX83102_MOD__
  endif
  ifeq ($(STR_HX83108), $(findstring $(STR_HX83108), $(HX_SRC)))
     ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_IC_HX83108
     ccflags-y += -D__HIMAX_HX83108_MOD__
  endif
  ifeq ($(STR_HX83112), $(findstring $(STR_HX83112), $(HX_SRC)))
     ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_IC_HX83112
     ccflags-y += -D__HIMAX_HX83112_MOD__
  endif
  ifeq ($(STR_HX83121), $(findstring $(STR_HX83121), $(HX_SRC)))
     ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_IC_HX83121
     ccflags-y += -D__HIMAX_HX83121_MOD__
  endif
endif

# ONCELL
ifeq ($(STR_HXONCELL), $(findstring $(STR_HXONCELL), $(HX_SRC)))
  ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_ONCELL
  ifeq ($(STR_HX852XJ), $(findstring $(STR_HX852XJ), $(HX_SRC)))
    ccflags-y += -D__HIMAX_HX852xJ_MOD__
    ccflags-y += -DCONFIG_TOUCHSCREEN_HIMAX_IC_HX852xJ
  endif
endif

ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
  ccflags-y += -D__HIMAX_MOD__
endif
# obj-y += himax_modular_table.o

# BUILD := $(shell find $(srctree)/$(src) -name "*.c" | xargs -n 1 basename | sed --expression='s/\.c/.o/g')
BUILD := $(subst $(PREFIX_SRC),,$(patsubst %.c,%.o,$(HX_SRC)))



$(MODULE_NAME)-objs += $(BUILD)
ifneq ($(filter m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
   ccflags-y += -D__HIMAX_MOD__
   obj-m += $(MODULE_NAME).o
endif
ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
   obj-y += $(MODULE_NAME).o
endif

ifneq ($(filter y, $(CONFIG_TOUCHSCREEN_HIMAX_EMBEDDED_FIRMWARE)),)
ccflags-y += -D__EMBEDDED_FW__

ld_array_start_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_start
ld_array_start_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_start_str))))
obj_array_start_sym = _binary___Himax_firmware_bin_start

ld_array_size_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_size
ld_array_size_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_size_str))))
obj_array_size_sym = _binary___Himax_firmware_bin_size

ld_array_end_str = _binary_$(srctree)/$(src)_Himax_firmware_bin_end
ld_array_end_sym = $(subst -,_,$(subst .,_,$(subst /,_,$(ld_array_end_str))))
obj_array_end_sym = _binary___Himax_firmware_bin_end

$(src)/Himax_firmware.o: $(src)/Himax_firmware.bin FORCE
	$(LD) $(LDFLAGS) -r -b binary $(srctree)/$(src)/Himax_firmware.bin -o $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_start_sym)=$(obj_array_start_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_size_sym)=$(obj_array_size_sym) $(objtree)/$(obj)/Himax_firmware.o
	$(OBJCOPY) --redefine-sym $(ld_array_end_sym)=$(obj_array_end_sym) $(objtree)/$(obj)/Himax_firmware.o

endif
