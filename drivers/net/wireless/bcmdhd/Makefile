# bcmdhd
DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER               \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE            \
	-DDHDTHREAD -DDHD_DEBUG -DSDTEST -DBDC -DTOE                          \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DPROP_TXSTATUS -DBCMDBG                \
	-DCUSTOMER_HW2 -DUSE_KTHREAD_API                                      \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                                \
	-DKEEP_ALIVE -DGET_CUSTOM_MAC_ENABLE -DPKT_FILTER_SUPPORT             \
	-DEMBEDDED_PLATFORM -DPNO_SUPPORT          \
	-DDHD_USE_IDLECOUNT -DSET_RANDOM_MAC_SOFTAP -DROAM_ENABLE -DVSDB      \
	-DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST -DSDIO_CRC_ERROR_FIX       \
	-DESCAN_RESULT_PATCH -DHT40_GO -DPASS_ARP_PACKET -DSUPPORT_PM2_ONLY   \
	-DCUSTOM_SDIO_F2_BLKSIZE=128 -DSET_LOW_LATENCY                        \
	-Idrivers/net/wireless/bcmdhd -Idrivers/net/wireless/bcmdhd/include

# for supporting different type of bcm943341_wbfgn_x board.
DHDCFLAGS += -DNV_BCM943341_WBFGN_MULTI_MODULE_SUPPORT

ifeq ($(CONFIG_BCMDHD_WIFI_CONTROL_FUNC),y)
DHDCFLAGS += -DCONFIG_WIFI_CONTROL_FUNC
else
DHDCFLAGS += -DCUSTOM_OOB_GPIO_NUM=2
endif
#ifeq ($(CONFIG_BCM4334),y)
#DHDCFLAGS += -DPROP_TXSTATUS_VSDB
#DHDCFLAGS += -DCUSTOM_GLOM_SETTING=5
#endif

#ifeq ($(CONFIG_BCM43341),y)
# for supporting different type of bcm943341_wbfgn_x board on NV reference design.
#DHDCFLAGS += -DNV_BCM943341_WBFGN_MULTI_MODULE_SUPPORT
#endif

#ifeq ($(CONFIG_BCM43241),y)
DHDCFLAGS += -DCUSTOM_SDIO_F2_BLKSIZE=128 -DSUPPORT_PM2_ONLY
DHDCFLAGS += -DAMPDU_HOSTREORDER
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-65
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=15
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=28000
#DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=7
#DHDCFLAGS += -DQUEUE_BW
#DHDCFLAGS += -DVSDB_BW_ALLOCATE_ENABLE
DHDCFLAGS += -DP2P_DISCOVERY_WAR
#endif

## Set dhd_dpd_thread priority to MAX to avoid starvation
DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=99

ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
DHDCFLAGS += -DHW_OOB -DOOB_INTR_ONLY
else
DHDCFLAGS += -DSDIO_ISR_THREAD
endif

ifeq ($(CONFIG_BCMDHD_INSMOD_NO_FW_LOAD),y)
DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD
endif

ifeq ($(CONFIG_BCMDHD_EDP_SUPPORT),y)
DHDCFLAGS += -DWIFIEDP
endif

ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif

DHDOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o   \
	dhd_linux_sched.o dhd_sdio.o bcmwifi_channels.o bcmevent.o hndpmu.o   \
	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o linux_osl.o            \
	bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wldev_common.o wl_android.o

obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)

ifeq ($(CONFIG_BCMDHD_WEXT),y)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT -DUSE_IW
endif

ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += wl_cfg80211.o wl_cfgp2p.o wl_linux_mon.o dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211 -DWL_CFG80211_STA_EVENT -DWL_ENABLE_P2P_IF
endif

EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif
