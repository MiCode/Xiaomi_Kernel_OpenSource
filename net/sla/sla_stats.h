/*
    Copyright (c) 2018, The Linux Foundation. All rights reserved.

    This program is free software; you can redistribute it and/or modify it under the terms
    of the GNU General Public License version 2 and only version 2 as published by the Free
    Software Foundation.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
    without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
*/

#ifndef __NET_SLA_STATS_H__
#define __NET_SLA_STATS_H__

/* config */
#define CMD_ENABLE          "enable="
#define CMD_RATE_ON         "rate_on="
#define CMD_PORTS           "ports="
#define CMD_RESET_TRAFFIC   "reset_traffic="
#define CMD_HASH_PRINT      "hash_print"
#define CMD_HASH_CLEAR      "hash_clear"
#define CMD_MAX_SIZE        "max_size="
#define CMD_LIMIT_TIME      "limit_time="
#define CMD_WLAN_IFNAME     "wlan_interface="
#define CMD_WWAN_IFNAME     "data_interface="
#define CMD_IFACE_ADD       "iface="
#define PORTS_MAX           127
#define CONFIG_CMD_MAX      (PORTS_MAX*6 + 18)  // 6 is max char num. strlen("xxxxx,"). 18 used for CMD_PORTS
#define IF_NAME_LEN         16  //equal to IFNAMSIZ

/* sla_stats_entry */
#define STATS_MAGIC         0x686A7379
#define ENTRY_TOTAL         1279

/* link_type */
#define LT_INVALID       0
#define LT_UPLINK        1
#define LT_DOWNLINK     2

struct sla_config
{
    u8 enable;      // 0:disable, 1:enable
    u8 rate_on;     // 0:off, 1:on
    u16 ports[PORTS_MAX];   // the local ports, sla will statistics packets under these ports
    u16 max_size;   // The max size of the consolidated packet
    u16 limit_time;   // The max time between consolidated packets
};

struct sla_stats_entry //16 bytes
{
    u64 timestamp;  //Packet arrival timestamp in ms
    u32 size;       //Size of packet in Bytes
    u16 port;       //Source port of the packet to identify the socket flow it belongs to
    u8 link_type;  //Uplink or downlink. LT_INVALID:invalid, LT_UPLINK:uplink, LT_DOWNLINK:downlink
    u8 discard;    //Tagged discarded, used only as a timestamp reference. 1:true, 0:false
};

struct sla_interface_stats //16 bytes + circular buffer
{
    u32 magic_num;   //magic_num is 0x686A7379, Used to indicate that the buffer status is ready
    u32 total;      //The total number of entries
    u32 cur_idx;    //The index of the entry currently being written or next. Starting from 0.
    u32 reserve;    //reserve field
    struct sla_stats_entry entries[ENTRY_TOTAL];  //pointer of first entry, alse circular buffer's head pointer
};

struct sla_interface
{
   struct list_head interface_list_hook;
   struct proc_dir_entry *mmap_file;
   struct proc_dir_entry *total_stats_file;
   u64    total_trfc;
   struct sla_interface_stats *mmap_struct;
   char   interface_name[IF_NAME_LEN];

};

#endif

