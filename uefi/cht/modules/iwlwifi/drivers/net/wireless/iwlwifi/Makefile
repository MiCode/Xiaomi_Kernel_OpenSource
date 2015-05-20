# common
obj-$(CPTCFG_IWLWIFI)	+= iwlwifi.o
iwlwifi-objs		+= iwl-io.o
iwlwifi-objs		+= iwl-drv.o
iwlwifi-objs		+= iwl-debug.o
iwlwifi-objs		+= iwl-notif-wait.o
iwlwifi-objs		+= iwl-eeprom-read.o iwl-eeprom-parse.o
iwlwifi-objs		+= iwl-phy-db.o iwl-nvm-parse.o

# Bus
iwlwifi-$(CPTCFG_IWLWIFI_PCIE)	+= pcie/drv.o pcie/rx.o pcie/tx.o pcie/trans.o
iwlwifi-$(CPTCFG_IWLWIFI_PCIE)-$(CPTCFG_IWLMVM) += iwl-7000.o iwl-8000.o
iwlwifi-$(CPTCFG_IWLWIFI_SDIO)	+= iwl-8000.o iwl-7000.o trans_slave/shared.o
iwlwifi-$(CPTCFG_IWLWIFI_SDIO)	+= trans_slave/sdio_drv.o trans_slave/sdio_trans.o trans_slave/sdio_rx.o trans_slave/sdio_tx_policy.o trans_slave/sdio_tx.o
iwlwifi-$(CPTCFG_IWLWIFI_SUPPORT_DEBUG_OVERRIDES) += iwl-dbg-cfg.o
iwlwifi-$(CPTCFG_IWLWIFI_MINI_PM_RUNTIME) += trans_slave/mini_rpm.o

iwlwifi-objs += $(iwlwifi-y-y) $(iwlwifi-y-m)

iwlwifi-$(CPTCFG_IWLWIFI_DEVICE_TRACING) += iwl-devtrace.o
iwlwifi-$(CPTCFG_IWLWIFI_DEVICE_TESTMODE) += iwl-tm-gnl.o iwl-dnt-cfg.o iwl-dnt-dispatch.o iwl-dnt-dev-if.o

ccflags-y += -D__CHECK_ENDIAN__ -I$(src)

obj-$(CPTCFG_IWLMVM)	+= mvm/
obj-$(CPTCFG_IWLXVT)	+= xvt/

CFLAGS_iwl-devtrace.o := -I$(src)
