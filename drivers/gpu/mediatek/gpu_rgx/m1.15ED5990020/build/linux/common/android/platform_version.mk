########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
#
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
#
# This License is also included in this distribution in the file called
# "MIT-COPYING".
#
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

ifeq ($(__included_platform_version_mk),)

__included_platform_version_mk := true

include ../common/android/paths.mk

# Platform autodetection is not supported in ChromeOS or ARC.
ifneq ($(SUPPORT_ARC_PLATFORM),)

ifeq ($(PLATFORM_RELEASE),)
$(error Consider setting PLATFORM_RELEASE, as platform autodetection is \
not supported.)
endif # PLATFORM_RELEASE

else

BUILD_PROP := $(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/build.prop

define newline


endef

# If there's no build.prop file in the expected location, bail out.
#
ifeq ($(wildcard $(BUILD_PROP)),)
$(warning *** $(BUILD_PROP) not found!!)
$(warning *** Did you set ANDROID_ROOT and TARGET_DEVICE in your \
environment correctly?)
$(error Error reading $(BUILD_PROP))
endif

$(eval $(subst #,$(newline),$(shell cat $(BUILD_PROP) | \
	grep '^ro.treble.enabled=\|^ro.build.version.codename=' | \
	sed -e 's,ro.treble.enabled=,SUPPORT_TREBLE=,' \
	    -e 's,ro.build.version.codename=,PLATFORM_CODENAME=,' | tr '\n' '#')))

ifeq ($(PLATFORM_RELEASE),)

$(warning PLATFORM_RELEASE was not set. Attempting to determine version using \
build.prop.)

# Extract version.release and version.codename from the build.prop file.
# If either of the values aren't in the build.prop, the Make variables won't
# be defined, and fallback handling will take place.
#
$(eval $(subst #,$(newline),$(shell cat $(BUILD_PROP) | \
	grep '^ro.build.version.release=\|^ro.build.id=' | \
	sed -e 's,ro.build.version.release=,PLATFORM_RELEASE=,' \
	    -e 's,ro.build.id=,PLATFORM_BUILDID=,' | tr '\n' '#')))

endif # ! PLATFORM_RELEASE

endif # SUPPORT_ARC_PLATFORM

define release-starts-with
$(shell echo $(PLATFORM_RELEASE) | tr '[:upper:]' '[:lower:]' | grep -q ^$(1); \
	[ "$$?" = "0" ] && echo 1 || echo 0)
endef

# ro.build.version.release contains the version number for release builds, or
# the version codename otherwise. In this case we need to assume that the
# version of Android we're building against has the features that are in the
# final release of that version, so we set PLATFORM_RELEASE to the
# corresponding release number.
#
# NOTE: It's the _string_ ordering that matters here, not the version number
# ordering. You need to make sure that strings that are sub-strings of other
# checked strings appear _later_ in this list.
#
# e.g. 'NougatMR1' starts with 'Nougat', but it is not Nougat.
#
ifeq ($(call release-starts-with,lollipopmr1),1)
override PLATFORM_RELEASE := 5.1
else ifeq ($(call release-starts-with,marshmallow),1)
override PLATFORM_RELEASE := 6.0
else ifeq ($(call release-starts-with,nougatmr),1)
override PLATFORM_RELEASE := 7.1
else ifeq ($(call release-starts-with,nougat),1)
override PLATFORM_RELEASE := 7.0
else ifeq ($(call release-starts-with,oreomr),1)
override PLATFORM_RELEASE := 8.1
else ifeq ($(call release-starts-with,oreo),1)
override PLATFORM_RELEASE := 8.0
else ifeq ($(call release-starts-with,pie),1)
override PLATFORM_RELEASE := 9.0
else ifeq ($(call release-starts-with,q),1)
override PLATFORM_RELEASE := 10.0
else ifeq ($(call release-starts-with,r),1)
override PLATFORM_RELEASE := 11.0
else ifeq ($(PLATFORM_BUILDID),AOSP.MASTER)
override PLATFORM_RELEASE := 11.0.80
else ifeq ($(shell echo $(PLATFORM_RELEASE) | grep -qE "[A-Za-z]+"; echo $$?),0)
override PLATFORM_RELEASE := 11.1
endif

# Workaround for master. Sometimes there is an AOSP version ahead of
# the current master version number, but master still has more features.
#
ifeq ($(PLATFORM_RELEASE),11.0.80)
override PLATFORM_RELEASE := 11.0
is_aosp_master := 1
endif

# Workaround for 'cut'. It doesn't give expected results when there
# is no '.' delimiter in PLATFORM_RELEASE.
#
ifneq (,$(findstring .,$(PLATFORM_RELEASE)))
PLATFORM_RELEASE_MAJ   := $(shell echo $(PLATFORM_RELEASE) | cut -f1 -d'.')
PLATFORM_RELEASE_MIN   := $(shell echo $(PLATFORM_RELEASE) | cut -f2 -d'.')
PLATFORM_RELEASE_PATCH := $(shell echo $(PLATFORM_RELEASE) | cut -f3 -d'.')
else
PLATFORM_RELEASE_MAJ   := $(PLATFORM_RELEASE)
PLATFORM_RELEASE_MIN   := 0
PLATFORM_RELEASE_PATCH := 0
endif

# Not all versions have a patchlevel; fix that up here
#
ifeq ($(PLATFORM_RELEASE_PATCH),)
PLATFORM_RELEASE_PATCH := 0
endif

# Macros to help categorize support for features and API_LEVEL for tests.
#
is_at_least_lollipop_mr1 := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -gt 5 || \
				( test $(PLATFORM_RELEASE_MAJ) -eq 5 && \
				  test $(PLATFORM_RELEASE_MIN) -gt 0 ) ) && echo 1 || echo 0)
is_at_least_marshmallow := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 6 ) && echo 1 || echo 0)
is_at_least_nougat := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 7 ) && echo 1 || echo 0)
is_at_least_nougat_mr1 := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -gt 7 || \
				( test $(PLATFORM_RELEASE_MAJ) -eq 7 && \
				  test $(PLATFORM_RELEASE_MIN) -gt 0 ) ) && echo 1 || echo 0)
is_at_least_oreo := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 8 ) && echo 1 || echo 0)
is_at_least_oreo_mr1 := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -gt 8 || \
				( test $(PLATFORM_RELEASE_MAJ) -eq 8 && \
				  test $(PLATFORM_RELEASE_MIN) -gt 0 ) ) && echo 1 || echo 0)
is_at_least_pie := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 9 ) && echo 1 || echo 0)

is_at_least_q := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 10 ) && echo 1 || echo 0)

is_at_least_r := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -ge 11 ) && echo 1 || echo 0)

# Assume "future versions" are >11.0, but we don't really know
is_future_version := \
	$(shell ( test $(PLATFORM_RELEASE_MAJ) -gt 11 || \
				( test $(PLATFORM_RELEASE_MAJ) -eq 11 && \
				  test $(PLATFORM_RELEASE_MIN) -gt 1 ) ) && echo 1 || echo 0)

# Picking an exact match of API_LEVEL for the platform we're building
# against can avoid compatibility theming and affords better integration.
#
# This is also a good place to select the right jack toolchain.
#
# Note: Android 8.1 is the last release that uses Jack. Jack toolchain is deprecated.
#
ifeq ($(is_future_version),1)
override JACK_VERSION :=
API_LEVEL := 31
else ifeq ($(is_at_least_r),1)
override JACK_VERSION :=
API_LEVEL := 30
else ifeq ($(is_at_least_q),1)
override JACK_VERSION :=
API_LEVEL := 29
else ifeq ($(is_at_least_pie),1)
override JACK_VERSION :=
API_LEVEL := 28
else ifeq ($(is_at_least_oreo_mr1),1)
JACK_VERSION ?= 4.32.CANDIDATE
API_LEVEL := 27
else ifeq ($(is_at_least_oreo),1)
JACK_VERSION ?= 4.31.CANDIDATE
API_LEVEL := 26
else ifeq ($(is_at_least_nougat_mr1),1)
JACK_VERSION ?= 3.36.CANDIDATE
API_LEVEL := 25
else ifeq ($(is_at_least_nougat),1)
JACK_VERSION ?= 3.36.CANDIDATE
API_LEVEL := 24
else ifeq ($(is_at_least_marshmallow),1)
JACK_VERSION ?= 2.21.RELEASE
API_LEVEL := 23
else ifeq ($(is_at_least_lollipop_mr1),1)
override JACK_VERSION :=
API_LEVEL := 22
else
$(error Must build against Android >= 5.1)
endif

# If the NDK is enabled, check it has API_LEVEL support for us
ifneq ($(NDK_ROOT),)
 VNDK_ROOT ?= $(NDK_ROOT)
 NDK_PLATFORMS_ROOT ?= $(VNDK_ROOT)/platforms
 NDK_SYSROOT ?= $(VNDK_ROOT)/sysroot
 ifeq ($(strip $(wildcard $(NDK_PLATFORMS_ROOT)/android-*)),)
  $(error NDK_PLATFORMS_ROOT does not point to a valid location)
 endif
 # Relax the error when Android is not yet AOSP as the NDK might not
 # be available.
 override TARGET_PLATFORM := android-$(API_LEVEL)
 ifeq ($(strip $(wildcard $(NDK_PLATFORMS_ROOT)/$(TARGET_PLATFORM))),)
   $(info NDK support for $(TARGET_PLATFORM) is missing)
   ifeq ($(strip $(wildcard $(NDK_PLATFORMS_ROOT)/android-$(PLATFORM_CODENAME)))),)
    $(error NDK support for android-$(PLATFORM_CODENAME) is missing)
   endif
   override TARGET_PLATFORM := android-$(PLATFORM_CODENAME)
 endif
endif

# Each DDK is tested against only a single version of the platform.
# Warn if a different platform version is used.
#
ifeq ($(is_future_version),1)
$(info WARNING: Android version is newer than this DDK supports)
else ifneq ($(is_at_least_marshmallow),1)
$(info WARNING: Android version is older than this DDK supports)
endif

endif # !__included_platform_version_mk
