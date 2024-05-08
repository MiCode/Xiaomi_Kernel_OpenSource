# SPDX-License-Identifier: GPL-2.0-only

### Menuconfig info
###
### CONFIG_XM_ISP
### CONFIG_XM_ISP_DEBUG

### Common set
subdir-ccflags-y := -I $(srctree)/drivers/media/platform/xiaomi/inc
subdir-ccflags-y += -I $(srctree)/drivers/remoteproc
subdir-ccflags-y += -I $(srctree)/drivers/mailbox

$(warning CONFIG_MIISP = $(CONFIG_MIISP))
$(warning CONFIG_MIISP_CHIP = $(CONFIG_MIISP_CHIP))
### Global VAR
ISP_VER := 4

### Switch to config
# [common] Version of PCIE.
subdir-ccflags-y += -Dispv=$(ISP_VER)
# [bus/pcie] Use spi to bringup v400 when pcie failed.
# subdir-ccflags-y += -DPCI_FAILED_USE_SPI
# [bus/pcie] Try to update pci-phy firmware when pcie failed.
# subdir-ccflags-y += -DPCI_FAILED_UPDATE_PHY
# [bus/pcie] Remain v400 qcom-driver to ensure camera always avalid.
subdir-ccflags-y += -DALWAYS_EXIST_QCOMDRV
# [bus/pcie] Not check pcie-domain rw success(for fpga debug).
subdir-ccflags-y += -DNO_FPGA_CHECK
subdir-ccflags-y += -DFORCE_NOT_USE_HW_MSI

### Modules name header
MNAMEH := xm_ispv$(ISP_VER)
# Modules
PMIC := $(MNAMEH)_pmic
PCIE := $(MNAMEH)_pcie
BASE_SPI_TEST := $(MNAMEH)_base_spi_test
SPI := $(MNAMEH)_regops
SPI_TEST := $(MNAMEH)_regops_test
RPROC := $(MNAMEH)_rproc
RPROC_FAKEDEV := $(MNAMEH)_rproc_fakedev
RPROC_THERMAL := $(MNAMEH)_thermal
PCIE_IONMAP := $(MNAMEH)_ionmap
PCIE_MEMDUMP := $(MNAMEH)_memdump
MANAGER := $(MNAMEH)_manager
CTRL := $(MNAMEH)_ctrl
MBOX := $(MNAMEH)_mbox
MBOX_TEST := $(MNAMEH)_mbox_test
MBOX_FAKEDEV := $(MNAMEH)_mbox_fakedev
LED := $(MNAMEH)_led
BUSMON := $(MNAMEH)_busmon
### Module compile list
obj-$(CONFIG_MIISP) += $(PMIC).o
obj-$(CONFIG_MIISP) += $(PCIE).o
# obj-(CONFIG_MIISP) += $(BASE_SPI_TEST).o
obj-$(CONFIG_MIISP) += $(SPI).o
# obj-$(CONFIG_MIISP) += $(SPI_TEST).o
obj-$(CONFIG_MIISP) += $(RPROC).o
# obj-$(CONFIG_MIISP) += $(RPROC_FAKEDEV).o
obj-$(CONFIG_MIISP) += $(RPROC_THERMAL).o
obj-$(CONFIG_MIISP) += $(PCIE_IONMAP).o
obj-$(CONFIG_MIISP) += $(PCIE_MEMDUMP).o
obj-$(CONFIG_MIISP) += $(MANAGER).o
obj-$(CONFIG_MIISP) += $(CTRL).o
obj-$(CONFIG_MIISP) += $(MBOX).o
obj-$(CONFIG_MIISP) += $(MBOX_TEST).o
# obj-$(CONFIG_MIISP) += $(MBOX_FAKEDEV).o
# obj-$(CONFIG_MIISP) += $(LED).o
obj-$(CONFIG_MIISP) += $(BUSMON).o
### Module details
#------ for ispv4
ifeq ($(ISP_VER), 4)

$(PMIC)-y := pmic/ispv4_pmic.o

$(PCIE)-y := bus/pcie/ispv4_dev.o
$(PCIE)-y += bus/pcie/ispv4_pcie_hdma.o
$(PCIE)-y += bus/pcie/ispv4_pcie_pm.o
$(PCIE)-y += bus/pcie/ispv4_debugfs.o
$(PCIE)-y += bus/pcie/ispv4_boot.o
$(PCIE)-y += bus/pcie/ispv4_notify.o

$(PCIE_IONMAP)-y += bus/pcie/ispv4_ionmap.o

$(PCIE_MEMDUMP)-y += bus/pcie/ispv4_memdump.o

$(SPI)-y := bus/spi/ispv4_regops.o

$(BASE_SPI_TEST)-y := bus/spi/ispv4_base_spi_test.o

# $(SPI_TEST)-y := bus/spi/ispv4_regops_test.o

$(RPROC_FAKEDEV)-y := ipc/rproc/ispv4_fake_dev.o

$(RPROC)-y := ipc/rproc/ispv4_rproc.o
$(RPROC)-y += ipc/rproc/ispv4_fake.o
$(RPROC)-y += ipc/rproc/ispv4_pci.o
$(RPROC)-y += ipc/rproc/ispv4_spi.o
$(RPROC)-y += ipc/rproc/ispv4_log.o
$(RPROC)-y += ipc/rproc/ispv4_exception.o
$(RPROC)-y += ipc/rpmsg/ispv4_rpmsg.o
$(RPROC)-y += ipc/rpmsg/ispv4_rpepts.o

$(RPROC_THERMAL)-y += ipc/rproc/ispv4_thermal.o

$(MANAGER)-y += manager/ispv4_manager.o
$(CTRL)-y += manager/ispv4_ctrl.o

$(MBOX)-y := ipc/mailbox/ispv4_mailbox.o

$(MBOX_TEST)-y := ipc/mailbox/ispv4_mailbox_test.o

$(MBOX_FAKEDEV)-y := ipc/mailbox/ispv4_mailbox_fakedev.o
# $(LED)-y := tools/ispv4_led.o
$(BUSMON)-y := tools/ispv4_busmon.o
#------ for ispv4
endif
