# SPDX-License-Identifier: GPL-2.0-only

#
# Makefile for the MSM specific device drivers.
#

obj-$(CONFIG_IPA3) += ipa_fmwk/
CFLAGS_msm-geni-se.o := -D__DISABLE_TRACE_MMIO__
obj-$(CONFIG_MSM_GENI_SE) += msm-geni-se.o
obj-$(CONFIG_USB_BAM) += usb_bam.o
obj-$(CONFIG_QPNP_REVID) += qpnp-revid.o
obj-$(CONFIG_SPS) += sps/
obj-$(CONFIG_MSM_11AD) += msm_11ad/
obj-$(CONFIG_EP_PCIE) += ep_pcie/
obj-$(CONFIG_MSM_MHI_DEV) += mhi_dev/
