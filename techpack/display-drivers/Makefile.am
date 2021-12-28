# SPDX-License-Identifier: GPL-2.0-only

DISPLAY_ROOT=$(ROOTDIR)vendor/qcom/opensource/display-drivers
CONFIG_DRM_MSM=$(MODULE_DRM_MSM)
KBUILD_OPTIONS := DISPLAY_ROOT=$(DISPLAY_ROOT) CONFIG_DRM_MSM=$(CONFIG_DRM_MSM)

ifeq ($(TARGET_SUPPORT),genericarmv8)
	KBUILD_OPTIONS += CONFIG_ARCH_WAIPIO=y
endif

obj-m += msm/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
