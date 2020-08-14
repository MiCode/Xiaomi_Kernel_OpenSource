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

ccflags-y := \
 -I$(TOP)/kernel/drivers/staging/imgtec \
 -I$(TOP)/kernel/drivers/staging/imgtec/tc \
 -I$(TOP)/kernel/drivers/staging/imgtec/rk3368 \
 -I$(TOP)/kernel/drivers/staging/imgtec/plato \
 -I$(TOP)/kernel/drivers/staging/imgtec/plato/hdmi \
 -I$(TOP)/kernel/drivers/staging/imgtec/sunxi \
 -I$(TOP)/include/$(PVR_ARCH)/system/rgx_tc -I$(TOP)/include/system/rgx_tc \
 -I$(TOP)/include/drm \
 -I$(TOP)/hwdefs/$(PVR_ARCH) \
 $(ccflags-y)

adf_fbdev-y += \
 kernel/drivers/staging/imgtec/adf_fbdev.o \
 kernel/drivers/staging/imgtec/adf_common.o

adf_pdp-y += \
 kernel/drivers/staging/imgtec/tc/adf_pdp.o \
 kernel/drivers/staging/imgtec/tc/pdp_apollo.o \
 kernel/drivers/staging/imgtec/tc/pdp_odin.o \
 kernel/drivers/staging/imgtec/adf_common.o \
 kernel/drivers/staging/imgtec/debugfs_dma_buf.o

tc-y += \
 kernel/drivers/staging/imgtec/tc/tc_apollo.o \
 kernel/drivers/staging/imgtec/tc/tc_odin.o \
 kernel/drivers/staging/imgtec/tc/tc_drv.o

ifeq ($(SUPPORT_APOLLO_FPGA),1)
tc-y += \
 kernel/drivers/staging/imgtec/tc/tc_apollo_debugfs.o
endif

ifeq ($(SUPPORT_ION),1)
tc-y += \
 kernel/drivers/staging/imgtec/tc/tc_ion.o \
 kernel/drivers/staging/imgtec/tc/ion_lma_heap.o \
 kernel/drivers/staging/imgtec/ion_fbcdc_clear.o
endif

adf_sunxi-y += \
 kernel/drivers/staging/imgtec/sunxi/adf_sunxi.o \
 kernel/drivers/staging/imgtec/adf_common.o

drm_nulldisp-y += \
 kernel/drivers/staging/imgtec/drm_nulldisp_drv.o \
 kernel/drivers/staging/imgtec/drm_nulldisp_netlink.o \
 kernel/drivers/staging/imgtec/drm_netlink_gem.o

ifeq ($(LMA),1)
drm_nulldisp-y += \
 kernel/drivers/staging/imgtec/tc/drm_pdp_gem.o
else
drm_nulldisp-y += \
 kernel/drivers/staging/imgtec/drm_nulldisp_gem.o
endif

drm_pdp-y += \
 kernel/drivers/staging/imgtec/tc/drm_pdp_debugfs.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_drv.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_gem.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_modeset.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_plane.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_crtc.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_dvi.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_tmds.o \
 kernel/drivers/staging/imgtec/tc/drm_pdp_fb.o \
 kernel/drivers/staging/imgtec/tc/pdp_apollo.o \
 kernel/drivers/staging/imgtec/tc/pdp_odin.o \
 kernel/drivers/staging/imgtec/tc/pdp_plato.o

plato-y += \
 kernel/drivers/staging/imgtec/plato/plato_drv.o \
 kernel/drivers/staging/imgtec/plato/plato_init.o

drm_pdp2_hdmi-y += \
 kernel/drivers/staging/imgtec/plato/hdmi/hdmi_core.o \
 kernel/drivers/staging/imgtec/plato/hdmi/hdmi_video.o \
 kernel/drivers/staging/imgtec/plato/hdmi/hdmi_i2c.o \
 kernel/drivers/staging/imgtec/plato/hdmi/hdmi_phy.o

drm_rk-y += \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_drv.o \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_gem.o \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_modeset.o \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_crtc.o \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_hdmi.o \
  kernel/drivers/staging/imgtec/rk3368/drm_rk_encoder.o
