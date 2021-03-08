#ifndef _ARP_SCAN_H
#define _ARP_SCAN_H

#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/skbuff.h>
#include <net/netlink.h>

#define NIPQUAD_FMT "%u.%u.%u.%u"
// #define NIPQUAD(addr) \
//	((unsigned char *)&addr)[0], \
//	((unsigned char *)&addr)[1], \
//	((unsigned char *)&addr)[2], \
//	((unsigned char *)&addr)[3]
#define NIPQUAD_4(addr) \
	((unsigned char *)&addr)[3]
#define NIPQUAD_1(addr) \
	((unsigned char *)&addr)[0]
#define ARP_SCAN_THRESHOLD_COUNT 200  // 252 times
#define ARP_SCAN_THRESHOLD_TIME 100   //100 seconds
#define IPV4_ADDR_LOOPBACK 127
#define IPV4_ADDR_MC_MIN 224
#define IPV4_ADDR_MC_MAX 239

enum {
	EVENT_MILINK_CONN_SSR_SHUTDOWN_SUBSYSTEM = 0,
	EVENT_MILINK_CONN_STA_DATASTALL,
	EVENT_MILINK_CONN_POWERSAVE_WOW,
	EVENT_MILINK_CONN_STA_KICKOUT,
	EVENT_MILINK_CONN_POWERSAVE_WOW_STATS,
	EVENT_MILINK_CONN_ARP_SCAN
};

typedef struct {
	uint32_t type;
	uint32_t len;
	char     data[256];
} milink_wlan_message_t;

typedef struct {
	struct list_head	list;
	u32					arp_scan_uid;
	u32					arp_scan_nexthop;
	u32					arp_scan_list_size;
	u8					arp_scan_list_map[256];
} arp_scan_list_t;

typedef struct {
	struct list_head	list;
	u32					arp_scan_uid;
} arp_scan_uid_list_t;


void arp_scan_create_neigh(u32 nexthop, u32 uid);
void arp_scan_list_update_map(arp_scan_list_t *entry, u32 nexthop);
arp_scan_list_t *arp_scan_list_entry_kmalloc(u32 uid);

void milink_srv_init(void);
void milink_srv_exit(void);
void milink_srv_ucast(milink_wlan_message_t *msg);

#endif	/* _ARP_SCAN_H */
