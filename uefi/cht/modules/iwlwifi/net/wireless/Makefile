obj-$(CPTCFG_CFG80211) += iwl-cfg80211.o
obj-$(CPTCFG_LIB80211) += lib80211.o
obj-$(CPTCFG_LIB80211_CRYPT_WEP) += lib80211_crypt_wep.o
obj-$(CPTCFG_LIB80211_CRYPT_CCMP) += lib80211_crypt_ccmp.o
obj-$(CPTCFG_LIB80211_CRYPT_TKIP) += lib80211_crypt_tkip.o

obj-$(CONFIG_WEXT_CORE) += wext-core.o
obj-$(CONFIG_WEXT_PROC) += wext-proc.o
obj-$(CONFIG_WEXT_SPY) += wext-spy.o
obj-$(CONFIG_WEXT_PRIV) += wext-priv.o

iwl-cfg80211-y += core.o sysfs.o radiotap.o util.o reg.o scan.o nl80211.o
iwl-cfg80211-y += mlme.o ibss.o sme.o chan.o ethtool.o mesh.o ap.o trace.o ocb.o
iwl-cfg80211-$(CPTCFG_CFG80211_DEBUGFS) += debugfs.o
iwl-cfg80211-$(CPTCFG_CFG80211_WEXT) += wext-compat.o wext-sme.o
iwl-cfg80211-$(CPTCFG_CFG80211_INTERNAL_REGDB) += regdb.o
iwl-cfg80211-$(CPTCFG_CFG80211_ANDROID_P2P_HACK) += android.o

CFLAGS_trace.o := -I$(src)

ccflags-y += -D__CHECK_ENDIAN__

$(obj)/regdb.c: $(src)/db.txt $(src)/genregdb.awk
	@$(AWK) -f $(src)/genregdb.awk < $< > $@

clean-files := regdb.c
