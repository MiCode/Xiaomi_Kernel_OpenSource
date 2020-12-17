# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_SPECTRA_CAMERA) += top_tpg/

ifdef CONFIG_SPECTRA_CAMERA_TFE
obj-$(CONFIG_SPECTRA_CAMERA) += tfe_csid_hw/ tfe_hw/
endif

ifdef CONFIG_SPECTRA_CAMERA_IFE
obj-$(CONFIG_SPECTRA_CAMERA) += ife_csid_hw/ vfe_hw/
endif
