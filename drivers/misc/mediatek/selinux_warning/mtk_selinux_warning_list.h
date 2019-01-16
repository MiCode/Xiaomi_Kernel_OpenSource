#ifndef __MTK_SELINUX_WARNING_LIST_H__
#define __MTK_SELINUX_WARNING_LIST_H__

#ifdef SELINUX_WARNING_C
#define AUTOEXT
#else				/* SELINUX_WARNING_C */
#define AUTOEXT  extern
#endif				/* SELINUX_WARNING_C */

#define AEE_FILTER_NUM 70
AUTOEXT const char *aee_filter_list[AEE_FILTER_NUM] = {
//	"u:r:adbd:s0",
	"u:r:bootanim:s0",
	"u:r:bluetooth:s0",
	"u:r:binderservicedomain:s0",
	"u:r:clatd:s0",
	"u:r:dex2oat:s0",
	"u:r:debuggerd:s0",
	"u:r:dhcp:s0",
	"u:r:dnsmasq:s0",
	"u:r:drmserver:s0",
	"u:r:dumpstate:s0",
	"u:r:gpsd:s0",
	"u:r:healthd:s0",
	"u:r:hci_attach:s0",
	"u:r:hostapd:s0",
	"u:r:inputflinger:s0",
	"u:r:installd:s0",
	"u:r:isolated_app:s0",
	"u:r:keystore:s0",
	"u:r:lmkd:s0",
	"u:r:mdnsd:s0",
	"u:r:logd:s0",
	"u:r:mediaserver:s0",
	"u:r:mtp:s0",
	"u:r:netd:s0",
	"u:r:nfc:s0",
	"u:r:ppp:s0",
	"u:r:platform_app:s0",
	"u:r:racoon:s0",
//	"u:r:radio:s0",
	"u:r:recovery:s0",
	"u:r:rild:s0",
	"u:r:runas:s0",
	"u:r:sdcardd:s0",
	"u:r:servicemanager:s0",
	"u:r:shared_relro:s0",
//	"u:r:shell:s0",
 	"u:r:system_app:s0",
// 	"u:r:system_server:s0",
//	"u:r:surfaceflinger:s0",
	"u:r:tee:s0",
	"u:r:uncrypt:s0",
	"u:r:watchdogd:s0",
	"u:r:wpa:s0",
	"u:r:ueventd:s0",
	"u:r:vold:s0",
	"u:r:vdc:s0",
//	"u:r:untrusted_app:s0",
	"u:r:zygote:s0",
	"u:r:mobile_log_d:s0",
//	"u:r:guiext-server:s0",
	"u:r:mtkrild:s0",
	"u:r:mtkrildmd2:s0",
	"u:r:nvram_agent_binder:s0",
	"u:r:nvram_daemon:s0",
};

#endif
