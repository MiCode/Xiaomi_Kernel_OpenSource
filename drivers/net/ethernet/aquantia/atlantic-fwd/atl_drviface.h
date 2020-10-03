/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_DRVIFACE_H_
#define _ATL_DRVIFACE_H_

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct __attribute__((packed)) offloadKAv4 {
    uint32_t timeout;
    in_port_t local_port;
    in_port_t remote_port;
    uint8_t remote_mac_addr[6];
    uint16_t win_size;
    uint32_t seq_num;
    uint32_t ack_num;
    in_addr_t local_ip;
    in_addr_t remote_ip;
};

struct __attribute__((packed)) offloadKAv6 {
    uint32_t timeout;
    in_port_t local_port;
    in_port_t remote_port;
    uint8_t remote_mac_addr[6];
    uint16_t win_size;
    uint32_t seq_num;
    uint32_t ack_num;
    struct in6_addr local_ip;
    struct in6_addr remote_ip;
};

struct __attribute__((packed)) offloadIPInfo {
    uint8_t v4LocalAddrCount;
    uint8_t v4AddrCount;
    uint8_t v6LocalAddrCount;
    uint8_t v6AddrCount;
    // FW will add the base to the following offset fields and will treat them as pointers.
    // The offsets are relative to the start of this struct so that the struct is pretty much self-contained
    // in_addr_t *
    uint32_t v4AddrOfft;
    // uint8_t *
    uint32_t v4PrefixOfft;
    // in6_addr *
    uint32_t v6AddrOfft;
    // uint8_t *
    uint32_t v6PrefixOfft;
};

struct __attribute__((packed)) offloadPortInfo {
    uint16_t UDPPortCount;
    uint16_t TCPPortCount;
    // in_port_t *
    uint32_t UDPPortOfft;       // See the comment in the offloadIPInfo struct
                                // in_port_t *
    uint32_t TCPPortOfft;
};

struct __attribute__((packed))  offloadKAInfo {
    uint16_t v4KACount;
    uint16_t v6KACount;
    uint32_t retryCount;
    uint32_t retryInterval;
    // struct offloadKAv4 *
    uint32_t v4KAOfft;          // See the comment in the offloadIPInfo struct
                                // struct offloadKAv6 *
    uint32_t v6KAOfft;
};

struct  __attribute__((packed)) offloadRRInfo {
    uint32_t RRCount;
    uint32_t RRBufLen;
    // Offset to RR index table relative to the start of offloadRRInfo struct. The indices
    // themselves are relative to the start of RR buffer. FW will add the buffer address
    // and will treat them as pointers.
    // uint8_t **
    uint32_t RRIdxOfft;
    // Offset to the RR buffer relative to the start of offloadRRInfo struct.
    // uint8_t *
    uint32_t RRBufOfft;
};

struct __attribute__((packed)) offloadInfo {
    uint32_t version;               // = 0 till it stabilizes some
    uint32_t len;                   // The whole structure length including the variable-size buf
    uint8_t macAddr[8];
    struct offloadIPInfo ips;
    struct offloadPortInfo ports;
    struct offloadKAInfo kas;
    struct offloadRRInfo rrs;
    uint8_t buf[0];
};

#define FW_PACK_STRUCT __attribute__((packed))

#define DRV_REQUEST_SIZE 3072
#define DRV_MSG_PING            0x01
#define DRV_MSG_ARP             0x02
#define DRV_MSG_INJECT          0x03
#define DRV_MSG_WOL_ADD         0x04
#define DRV_MSG_WOL_REMOVE      0x05
#define DRV_MSG_ENABLE_WAKEUP   0x06
#define DRV_MSG_MSM             0x07
#define DRV_MSG_PROVISIONING    0x08
#define DRV_MSG_OFFLOAD_ADD     0x09
#define DRV_MSG_OFFLOAD_REMOVE 	0x0A
#define DRV_MSG_MSM_EX          0x0B
#define DRV_MSG_SMBUS_PROXY     0x0C

#define DRV_PROV_APPLY         1
#define DRV_PROV_REPLACE       2
#define DRV_PROV_ADD           3

#define FW_RPC_INJECT_PACKET_LEN 1514U

typedef enum {
    EVENT_DRIVER_ENABLE_WOL
} eDriverEvent;

//typedef enum {
//    HOST_UNINIT = 0,
//    HOST_RESET,
//    HOST_INIT,
//    HOST_RESERVED,
//    HOST_SLEEP,
//    HOST_INVALID
//} hostState_t;

struct drvMsgPing {
    uint32_t ping;
} FW_PACK_STRUCT;

union IPAddr {
    struct
    {
        uint8_t addr[16];
    } FW_PACK_STRUCT v6;
    struct
    {
        uint8_t padding[12];
        uint8_t addr[4];
    } FW_PACK_STRUCT v4;
} FW_PACK_STRUCT;

struct drvMsgArp {
    uint8_t macAddr[6];
    uint32_t uIpAddrCnt;
    struct
    {
        union IPAddr addr;
        union IPAddr mask;
    } FW_PACK_STRUCT ip[1];
} FW_PACK_STRUCT;

struct drvMsgInject {
    uint32_t len;
    uint8_t packet[FW_RPC_INJECT_PACKET_LEN];
} FW_PACK_STRUCT;

enum ndisPmWoLPacket {
    ndisPMWoLPacketUnspecified = 0,
    ndisPMWoLPacketBitmapPattern,
    ndisPMWoLPacketMagicPacket,
    ndisPMWoLPacketIPv4TcpSyn,
    ndisPMWoLPacketIPv6TcpSyn,
    ndisPMWoLPacketEapolRequestIdMessage,
    ndisPMWoLPacketMaximum
};

enum aqPmWoLPacket {
    aqPMWoLPacketUnspecified = 0x10000,
    aqPMWoLPacketArp,
    aqPMWoLPacketIPv4Ping,
    aqPMWoLPacketIpv6NsPacket,
    aqPMWoLPacketIpv6Ping,
    aqPMWoLReasonLinkUp,
    aqPMWoLReasonLinkDown,
    aqPMWoLPacketMaximum
};

enum ndisPmProtocolOffloadType {
    ndisPMProtocolOffloadIdUnspecified,
    ndisPMProtocolOffloadIdIPv4ARP,
    ndisPMProtocolOffloadIdIPv6NS,
    ndisPMProtocolOffload80211RSNRekey,
    ndisPMProtocolOffloadIdMaximum
};

struct drvMsgEnableWakeup {
    uint32_t patternMaskWindows;
    uint32_t patternMaskAquantia;
    uint32_t patternMaskOther;
    uint32_t offloadsMaskWindows;
    uint32_t offloadsMaskAquantia;
} FW_PACK_STRUCT;

struct drvMsgWoLAddIpv4TcpSynWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4SourceAddress;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4DestAddress;
    union {
        uint8_t v8[2];
        uint16_t v16;
    } TCPSourcePortNumber;
    union {
        uint8_t v8[2];
        uint16_t v16;
    } TCPDestPortNumber;
} FW_PACK_STRUCT;

struct drvMsgWoLAddIpv6TcpSynWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } IPv6SourceAddress;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } IPv6DestAddress;
    union {
        uint8_t v8[2];
        uint16_t v16;
    } TCPSourcePortNumber;
    union {
        uint8_t v8[2];
        uint16_t v16;
    } TCPDestPortNumber;
} FW_PACK_STRUCT;

struct drvMsgWoLAddIpv4PingWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4SourceAddress;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4DestAddress;
} FW_PACK_STRUCT;

struct drvMsgWoLAddIpv6PingWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } IPv6SourceAddress;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } IPv6DestAddress;
} FW_PACK_STRUCT;

struct drvMsgWoLAddEapolRequestIdMessageWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4SourceAddress;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4DestAddress;
} FW_PACK_STRUCT;

struct drvMsgWoLAddBitmapPattern {
    uint32_t Flags;
    uint32_t MaskOffset;
    uint32_t MaskSize;
    uint32_t PatternOffset;
    uint32_t PatternSize;
} FW_PACK_STRUCT;

struct drvMsgWoLAddMagicPacketPattern {
    uint8_t macAddr[6];
} FW_PACK_STRUCT;

struct drvMsgWoLAddArpWoLPacketParameters {
    uint32_t flags;
    union {
        uint8_t v8[4];
        uint32_t v32;
    } IPv4Address;
} FW_PACK_STRUCT;

struct drvMsgWoLAddLinkUpWoLParameters {
    uint32_t timeout;
} FW_PACK_STRUCT;

struct drvMsgWoLAddLinkDownWoLParameters {
    uint32_t timeout;
} FW_PACK_STRUCT;

struct drvMsgWoLAdd {
    uint32_t priority; // Currently not used
    uint32_t packetType; // One of ndisPmWoLPacket or aqPmWoLPacket
    uint32_t patternId; // Id to save - will be used in remove message
    uint32_t nextWoLPatternOffset; // For chaining multiple additions in one request

    // Depends on `parrernId`
    union _WOL_PATTERN {
        struct drvMsgWoLAddIpv4TcpSynWoLPacketParameters wolIpv4TcpSyn;
        struct drvMsgWoLAddIpv6TcpSynWoLPacketParameters wolIpv6TcpSyn;
        struct drvMsgWoLAddEapolRequestIdMessageWoLPacketParameters wolEapolRequestIdMessage;
        struct drvMsgWoLAddBitmapPattern wolBitmap;
        struct drvMsgWoLAddMagicPacketPattern wolMagicPacket;
        struct drvMsgWoLAddIpv4PingWoLPacketParameters wolIpv4Ping;
        struct drvMsgWoLAddIpv6PingWoLPacketParameters wolIpv6Ping;
        struct drvMsgWoLAddArpWoLPacketParameters wolArp;
        struct drvMsgWoLAddLinkUpWoLParameters wolLinkUpReason;
        struct drvMsgWoLAddLinkDownWoLParameters wolLinkDownReason;
    } wolPattern;
} FW_PACK_STRUCT;

struct drvMsgWoLRemove {
    uint32_t id;
} FW_PACK_STRUCT;

struct ipv4ArpParameters {
    uint32_t flags;
    uint8_t remoteIPv4Address[4];
    uint8_t hostIPv4Address[4];
    uint8_t macAddress[6];
} FW_PACK_STRUCT;

struct ipv6NsParameters {
    uint32_t flags;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } remoteIPv6Address;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } solicitedNodeIPv6Address;
    union {
        uint8_t v8[16];
        uint32_t v32[4];
    } targetIPv6Addresses[2];
    uint8_t macAddress[6];
} FW_PACK_STRUCT;

struct drvMsgOffloadAdd {
    uint32_t priority;
    uint32_t protocolOffloadType;
    uint32_t protocolOffloadId;
    uint32_t nextProtocolOffloadOffset;
    union {
        struct ipv4ArpParameters ipv4Arp;
        struct ipv6NsParameters ipv6Ns;
    } wolOffload;
} FW_PACK_STRUCT;

struct drvMsgOffloadRemove {
    uint32_t id;
} FW_PACK_STRUCT;

struct drvMsmSettings {
    uint32_t msmReg054;
    uint32_t msmReg058;
    uint32_t msmReg05c;
    uint32_t msmReg060;
    uint32_t msmReg064;
    uint32_t msmReg068;
    uint32_t msmReg06c;
    uint32_t msmReg070;
    uint32_t flags;     // Valid for message DRV_MSG_MSM_EX only
} FW_PACK_STRUCT;

//struct drvMsgProvisioning {
//    uint32_t command;
//    uint32_t len;
//    provList_t list;
//} FW_PACK_STRUCT;

//struct drvMsgSmbusProxy {
//    uint32_t typeMsg;
//    union {
//        struct smbusProxyWrite smbWrite;
//        struct smbusProxyRead smbRead;
//        struct smbusProxyGetStatus smbStatus;
//        struct smbusProxyReadResp smbReadResp;
//    } FW_PACK_STRUCT;
//} FW_PACK_STRUCT;

struct drvIface {
    uint32_t msgId;

    union {
        struct drvMsgPing msgPing;
        struct drvMsgArp msgArp;
        struct drvMsgInject msgInject;
        struct drvMsgWoLAdd msgWoLAdd;
        struct drvMsgWoLRemove msgWoLRemove;
        struct drvMsgEnableWakeup msgEnableWakeup;
        struct drvMsmSettings msgMsm;
//        struct drvMsgProvisioning msgProvisioning;
        struct drvMsgOffloadAdd msgOffloadAdd;
        struct drvMsgOffloadRemove msgOffloadRemove;
//        struct drvMsgSmbusProxy msgSmbusProxy;
        struct offloadInfo fw2xOffloads;
    } FW_PACK_STRUCT;
} FW_PACK_STRUCT;

#endif
