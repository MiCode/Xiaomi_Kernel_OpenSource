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

########################################################################### ###
# Display class drivers
########################################################################### ###

ifeq ($(DISPLAY_CONTROLLER),dc_example)
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_WIDTH,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_HEIGHT,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_DPI,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_BIT_DEPTH,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_FBC_FORMAT,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_MEMORY_LAYOUT,))
endif

ifeq ($(DISPLAY_CONTROLLER),dc_fbdev)
$(eval $(call TunableKernelConfigC,DC_FBDEV_REFRESH,))

$(eval $(call TunableKernelConfigC,DC_FBDEV_FORCE_XRGB8888,,\
Force the dc_fbdev display driver to use XRGB8888. This is necessary_\
when the underlying Linux framebuffer driver does not ignore alpha_\
meaning an alpha value of 0 results in nothing being displayed._\
))

$(eval $(call TunableBothConfigC,DC_FBDEV_NUM_PREFERRED_BUFFERS,,\
The maximum number of display buffers the dc_fbdev display driver_\
supports. The underlying Linux framebuffer driver must be capable_\
of allocating sufficient memory for the number of buffers chosen._\
))

$(eval $(call TunableKernelConfigC,DC_FBDEV_USE_SCREEN_BASE,,\
Use the framebuffer virtual address found in the framebuffer info_\
screen base instead of the physical address found in the framebuffer_\
fix screen info._\
))

$(eval $(call TunableKernelConfigC,DC_FBDEV_FORCE_CONTEXT_CLEAN,,\
Before each framebuffer flip force a context clean by writing out_\
to memory the contents of the framebuffer display buffers. Certain_\
Linux framebuffer drivers like udldrmfb for displaylink USB-to-VGA_\
adapters make use of cached buffers which require userspace support_\
to write out to memory only those regions that are dirty. So in the_\
absence of such a userspace support in certain window systems like_\
NULLWS we force clean the entire display buffers before each flip._\
))
endif

ifeq ($(DISPLAY_CONTROLLER),dc_pdp)
 $(eval $(call TunableKernelConfigC,DCPDP_WIDTH,))
 $(eval $(call TunableKernelConfigC,DCPDP_HEIGHT,))
 $(eval $(call TunableKernelConfigC,DCPDP_DPI,))
 ifneq ($(_CLANG),true)
  $(eval $(call TunableKernelConfigC,DCPDP_DYNAMIC_GTF_TIMING,1))
 endif
endif

ifeq ($(DISPLAY_CONTROLLER),dc_pdp2)
$(eval $(call TunableKernelConfigC,DCPDP_WIDTH,))
$(eval $(call TunableKernelConfigC,DCPDP_HEIGHT,))
endif

ifeq ($(DISPLAY_CONTROLLER),adf_pdp)
$(eval $(call TunableKernelConfigC,ADF_PDP_WIDTH,))
$(eval $(call TunableKernelConfigC,ADF_PDP_HEIGHT,))
endif

ifeq ($(DISPLAY_CONTROLLER),drm_pdp)
 ifeq ($(SUPPORT_KMS),1)
  ifneq ($(call kernel-version-at-least,4,3),true)
   $(eval $(call TunableKernelConfigC,SUPPORT_DRM_FBDEV_EMULATION,,\
Enables legacy framebuffer device support in those DRM/KMS drivers \
that support it when using kernel 4.2 and below. When using later \
kernels this support must be enabled in the kernel via the \
CONFIG_DRM_FBDEV_EMULATION option.))
  else ifeq ($(call kernel-version-at-least,4,3),true)
   ifneq ($(filter command line environment,$(origin SUPPORT_DRM_FBDEV_EMULATION)),)
    $(warning CONFIG_DRM_FBDEV_EMULATION must be set as part of the Linux kernel build)
    $(warning SUPPORT_DRM_FBDEV_EMULATION will be ignored)
   endif
   override undefine SUPPORT_DRM_FBDEV_EMULATION
  endif
 endif
endif
