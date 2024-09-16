
/*****************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/
/*! \file   gl_dependency.h
 * \brief  List the os-dependent structure/API that need to implement
 * to align with common part
 *
 * not to modify *.c while put current used linux-type strucutre/API here
 * For porting to new OS, the API listed in this file needs to be
 * implemented
 */

#ifndef _GL_DEPENDENT_H
#define _GL_DEPENDENT_H

#ifndef IS_ENABLED
#define IS_ENABLED(_val) defined(_val)
#endif
/*
 * TODO: implement defined structure to let
 * other OS aligned to linux style for common part logic workaround
 */
/*
 * TODO: os-related?, CHN_DIRTY_WEIGHT_UPPERBOUND
 * should we remove gl_*_ioctl.h in os/none?
 * defined in os/linux/include/gl_p2p_ioctl.h
 * used in
 * 1) os/linux/gl_p2p_cfg80211.c
 * 2) mgmt/cnm.c
 * > is it related to ioctl?, or should it put in wlan_p2p.h
 */
#if 0
#define CHN_DIRTY_WEIGHT_UPPERBOUND     4
#endif

/* some arch's have a small-data section that can be accessed register-relative
 * but that can only take up to, say, 4-byte variables. jiffies being part of
 * an 8-byte variable may not be correctly accessed unless we force the issue
 * #define __jiffy_data  __attribute__((section(".data")))
 *
 * The 64-bit value is not atomic - you MUST NOT read it
 * without sampling the sequence number in jiffies_lock.

 * extern u64 __jiffy_data jiffies_64;
 * extern unsigned long volatile __jiffy_data jiffies;
 * TODO: no idea how to implement jiffies here
 */
#ifndef HZ
#define HZ (1000)
#endif
#define jiffies (0)

/*
 * comment: cipher type should not related to os
 * defined in os/linux/gl_wext.h,
 * while it looks like also defined in linux/wireless.h
 * are we trying to be an self-fulfilled driver then
 * should not put it under os folder? or we should just include from linux
 */
/* IW_AUTH_PAIRWISE_CIPHER and IW_AUTH_GROUP_CIPHER values (bit field) */
#define IW_AUTH_CIPHER_NONE     0x00000001
#define IW_AUTH_CIPHER_WEP40    0x00000002
#define IW_AUTH_CIPHER_TKIP     0x00000004
#define IW_AUTH_CIPHER_CCMP     0x00000008
#define IW_AUTH_CIPHER_WEP104   0x00000010

/*
 * comment:
 * 1) access GlueInfo member from core logic
 * 2) IW_AUTH_WPA_VERSION_DISABLED is defined in os/linux/include/gl_wext.h
 * > is the an os-dependent value/ protocol value.
 * should implement depends on OS/ its WiFi
 * needed by,
 * mgmt/ais_fsm.c
 * mgmt/assoc.c
 */
/* IW_AUTH_WPA_VERSION values (bit field) */
#define IW_AUTH_WPA_VERSION_DISABLED    0x00000001
#define IW_AUTH_WPA_VERSION_WPA         0x00000002
#define IW_AUTH_WPA_VERSION_WPA2        0x00000004

#define IW_AUTH_ALG_FT			0x00000008

#define IW_PMKID_LEN        16
/*
 * this highly depends on kernel version
 * why can't we just use kalGetTimeTick (?)
 * needed by
 * wmm.c
 */
#if 0
struct timespec {
	__kernel_time_t	tv_sec;			/* seconds */
	long tv_nsec;		/* nanoseconds */
};
#endif

/*
 * defined in linux/time64.h
 * in cnm_timer.h
 * we do (?!), we may just add undef NSEC_PER_MSEC then define ours
 * #undef MSEC_PER_SEC
 * #define MSEC_PER_SEC            1000
 * #undef USEC_PER_MSEC
 * #define USEC_PER_MSEC           1000
 * #undef USEC_PER_SEC
 * #define USEC_PER_SEC            1000000
 */
#define NSEC_PER_MSEC	1000000L

/*
 * needed by nic/nic_cmd_event.c
 * defined in uapi/asm-generic/fcntl.h
 */
#define O_RDONLY	00000000

/*
 * needed by que_mgt.c
 * ETH_P_IP, include/linux/if_ether.h
 */
#define ETH_P_IP    0x0800      /* Internet Protocol packet */

/*
 * needed by cmm_asic_connac.c
 *	Potential risk in this function
 *	#ifdef CONFIG_PHYS_ADDR_T_64BIT
 *	typedef u64 phys_addr_t;
 *	#else
 *	typedef u32 phys_addr_t;
 *	#endif
 *	while anyway the rDmaAddr is transfer to u8Addr still u64
 *	, and filled into u4Ptr0 which is uint32_t (?)
 */
#define phys_addr_t uint32_t

/*
 * needed by nic_tx.h
 * defined in linux/types.h
 * #ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
 * typedef u64 dma_addr_t;
 * #else
 * typedef u32 dma_addr_t;
 * #endif
 */
#define dma_addr_t uint32_t

/* needed by:
 * common/debug.c
 * source/include/linux/printk.h
 */
enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

/* needed by
 * common/wlan_lib.c
 * include/mgmt/rsn.h
 */
#define LINUX_VERSION_CODE 199947
#define CFG80211_VERSION_CODE LINUX_VERSION_CODE
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

/* needed by nic/nic_cmd_event.c */
struct wireless_dev {

	struct wiphy *wiphy;
	struct net_device *netdev;

};
/* needed by
 * common/wlan_lib.c & wlan_lib.h
 * mgmt/tdls.c
 * mgmt/p2p_role_fsm.c
 * nic/nic_cmd_event.c
 */
struct net_device {
	/*
	 * access member of member of GlueInfo directly
	 * in mgmt/p2p_role_fsm.c
	 */
	unsigned char *dev_addr;
	/* needed by nic/nic_cmd_event.c */
	struct wireless_dev	*ieee80211_ptr;
};

struct device {
};

/* needed by
 * common/debug.c
 * mgmt/stats.c
 * include/nic/nic_tx.h, nic/nic_tx.c
 * cb for driver private for data packet
 * dev needed by mgmt/tdls.c
 * mgmt/tkip_mic.c, and do dev_alloc_skb
 * nic/nic_rx.c
 * nic/que_mgt.c
 */
struct sk_buff {
	char cb[48];
	unsigned char *data;
	unsigned int len;
	struct net_device *dev;
};

/*
 * needed by common/wlan_oid.c
 * struct cfg80211_update_ft_ies_params - FT IE Information
 * This structure provides information needed to update the fast transition IE
 *
 * @md: The Mobility Domain ID, 2 Octet value
 * @ie: Fast Transition IEs
 * @ie_len: Length of ft_ie in octets
 */
struct cfg80211_update_ft_ies_params {
	u16 md;
	const u8 *ie;
	size_t ie_len;
};

/*
 * needed by
 * include/mgmt/rlm_domain.h
 * mgmt/p2p_func.c
 *
 * enum nl80211_dfs_regions - regulatory DFS regions
 *
 * @NL80211_DFS_UNSET: Country has no DFS master region specified
 * @NL80211_DFS_FCC: Country follows DFS master rules from FCC
 * @NL80211_DFS_ETSI: Country follows DFS master rules from ETSI
 * @NL80211_DFS_JP: Country follows DFS master rules from JP/MKK/Telec
 */
enum nl80211_dfs_regions {
	NL80211_DFS_UNSET	= 0,
	NL80211_DFS_FCC		= 1,
	NL80211_DFS_ETSI	= 2,
	NL80211_DFS_JP		= 3,
};

/*
 * needed by mgmt/rlm_domain.c
 * enum ieee80211_channel_flags - channel flags
 *
 * Channel flags set by the regulatory control code.
 *
 * @IEEE80211_CHAN_DISABLED: This channel is disabled.
 * @IEEE80211_CHAN_NO_IR: do not initiate radiation, this includes
 *      sending probe requests or beaconing.
 * @IEEE80211_CHAN_RADAR: Radar detection is required on this channel.
 * @IEEE80211_CHAN_NO_HT40PLUS: extension channel above this channel
 *      is not permitted.
 * @IEEE80211_CHAN_NO_HT40MINUS: extension channel below this channel
 *      is not permitted.
 * @IEEE80211_CHAN_NO_OFDM: OFDM is not allowed on this channel.
 * @IEEE80211_CHAN_NO_80MHZ: If the driver supports 80 MHz on the band,
 *      this flag indicates that an 80 MHz channel cannot use this
 *      channel as the control or any of the secondary channels.
 *      This may be due to the driver or due to regulatory bandwidth
 *      restrictions.
 * @IEEE80211_CHAN_NO_160MHZ: If the driver supports 160 MHz on the band,
 *      this flag indicates that an 160 MHz channel cannot use this
 *      channel as the control or any of the secondary channels.
 *      This may be due to the driver or due to regulatory bandwidth
 *      restrictions.
 * @IEEE80211_CHAN_INDOOR_ONLY: see %NL80211_FREQUENCY_ATTR_INDOOR_ONLY
 * @IEEE80211_CHAN_GO_CONCURRENT: see %NL80211_FREQUENCY_ATTR_GO_CONCURRENT
 * @IEEE80211_CHAN_NO_20MHZ: 20 MHz bandwidth is not permitted
 *      on this channel.
 * @IEEE80211_CHAN_NO_10MHZ: 10 MHz bandwidth is not permitted
 *      on this channel.
 *
 */
enum ieee80211_channel_flags {
	IEEE80211_CHAN_DISABLED         = 1<<0,
	IEEE80211_CHAN_NO_IR            = 1<<1,
	/* hole at 1<<2 */
	IEEE80211_CHAN_RADAR            = 1<<3,
	IEEE80211_CHAN_NO_HT40PLUS      = 1<<4,
	IEEE80211_CHAN_NO_HT40MINUS     = 1<<5,
	IEEE80211_CHAN_NO_OFDM          = 1<<6,
	IEEE80211_CHAN_NO_80MHZ         = 1<<7,
	IEEE80211_CHAN_NO_160MHZ        = 1<<8,
	IEEE80211_CHAN_INDOOR_ONLY      = 1<<9,
	IEEE80211_CHAN_GO_CONCURRENT    = 1<<10,
	IEEE80211_CHAN_NO_20MHZ         = 1<<11,
	IEEE80211_CHAN_NO_10MHZ         = 1<<12,
};

/* needed by mgmt/rlm_domain.c */
#define IEEE80211_CHAN_NO_HT40 \
	(IEEE80211_CHAN_NO_HT40PLUS | IEEE80211_CHAN_NO_HT40MINUS)

/*
 * needed by
 * mgmt/p2p_func.c
 *
 * enum nl80211_dfs_state - DFS states for channels
 *
 * Channel states used by the DFS code.
 *
 * @NL80211_DFS_USABLE: The channel can be used, but channel availability
 *	check (CAC) must be performed before using it for AP or IBSS.
 * @NL80211_DFS_UNAVAILABLE: A radar has been detected on this channel, it
 *	is therefore marked as not available.
 * @NL80211_DFS_AVAILABLE: The channel has been CAC checked and is available.
 */
enum nl80211_dfs_state {
	NL80211_DFS_USABLE,
	NL80211_DFS_UNAVAILABLE,
	NL80211_DFS_AVAILABLE,
};
/*
 * needed by mgmt/rlm_domain.c
 * needed by mgmt/p2p_func.c
 * enum nl80211_band - Frequency band
 * @NL80211_BAND_2GHZ: 2.4 GHz ISM band
 * @NL80211_BAND_5GHZ: around 5 GHz band (4.9 - 5.7 GHz)
 * @NL80211_BAND_60GHZ: around 60 GHz band (58.32 - 69.12 GHz)
 * @NUM_NL80211_BANDS: number of bands, avoid using this in userspace
 * since newer kernel versions may support more bands
 */
enum nl80211_band {
	NL80211_BAND_2GHZ,
	NL80211_BAND_5GHZ,
	NL80211_BAND_60GHZ,
	NUM_NL80211_BANDS,
};

/*
 * enum nl80211_initiator - Indicates the initiator of a reg domain request
 * @NL80211_REGDOM_SET_BY_CORE: Core queried CRDA for a dynamic world
 *	regulatory domain.
 * @NL80211_REGDOM_SET_BY_USER: User asked the wireless core to set the
 *	regulatory domain.
 * @NL80211_REGDOM_SET_BY_DRIVER: a wireless drivers has hinted to the
 *	wireless core it thinks its knows the regulatory domain we should be in.
 * @NL80211_REGDOM_SET_BY_COUNTRY_IE: the wireless core has received an
 *	802.11 country information element with regulatory information it
 *	thinks we should consider. cfg80211 only processes the country
 *	code from the IE, and relies on the regulatory domain information
 *	structure passed by userspace (CRDA) from our wireless-regdb.
 *	If a channel is enabled but the country code indicates it should
 *	be disabled we disable the channel and re-enable it upon disassociation.
 */
enum nl80211_reg_initiator {
	L80211_REGDOM_SET_BY_CORE,
	NL80211_REGDOM_SET_BY_USER,
	NL80211_REGDOM_SET_BY_DRIVER,
	NL80211_REGDOM_SET_BY_COUNTRY_IE,
};
/* needed by
 * mgmt/rlm_domain.c
 * mgmt/cnm.c
 * mgmt/p2p_func.c
 */
struct ieee80211_channel {
	enum nl80211_band band;
	u32 center_freq;
	u16 hw_value;
	u32 flags;
	enum nl80211_dfs_state dfs_state;
};

/*
 * needed by mgmt/cnm.c
 * enum nl80211_chan_width - channel width definitions
 *
 * These values are used with the %NL80211_ATTR_CHANNEL_WIDTH
 * attribute.
 *
 * @NL80211_CHAN_WIDTH_20_NOHT: 20 MHz, non-HT channel
 * @NL80211_CHAN_WIDTH_20: 20 MHz HT channel
 * @NL80211_CHAN_WIDTH_40: 40 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @NL80211_CHAN_WIDTH_80: 80 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @NL80211_CHAN_WIDTH_80P80: 80+80 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *	and %NL80211_ATTR_CENTER_FREQ2 attributes must be provided as well
 * @NL80211_CHAN_WIDTH_160: 160 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *	attribute must be provided as well
 * @NL80211_CHAN_WIDTH_5: 5 MHz OFDM channel
 * @NL80211_CHAN_WIDTH_10: 10 MHz OFDM channel
 */
enum nl80211_chan_width {
	NL80211_CHAN_WIDTH_20_NOHT,
	NL80211_CHAN_WIDTH_20,
	NL80211_CHAN_WIDTH_40,
	NL80211_CHAN_WIDTH_80,
	NL80211_CHAN_WIDTH_80P80,
	NL80211_CHAN_WIDTH_160,
	NL80211_CHAN_WIDTH_5,
	NL80211_CHAN_WIDTH_10,
};

/* needed by mgmt/rlm_domain.c */
struct ieee80211_supported_band {
	struct ieee80211_channel *channels;
	int n_channels;
};

/*
 * too many os dependent in regular domain
 * on/off fail
 */
#if CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1
/* at leat 7 is needed for regdom_jp */
#define MAX_NUMER_REG_RULES	7

struct ieee80211_power_rule {
	u32 max_antenna_gain;
	u32 max_eirp;
};

struct ieee80211_freq_range {
	u32 start_freq_khz;
	u32 end_freq_khz;
	u32 max_bandwidth_khz;
};

struct ieee80211_reg_rule {
	struct ieee80211_freq_range freq_range;
	struct ieee80211_power_rule power_rule;
	u32 flags; /* enum reg_flags */
	u32 dfs_cac_ms;
};

struct ieee80211_regdomain {
	char alpha2[3];
	u32 n_reg_rules;
	enum nl80211_dfs_regions dfs_region;
	struct ieee80211_reg_rule reg_rules[MAX_NUMER_REG_RULES];
};

#define MHZ_TO_KHZ(freq) ((freq) * 1000)
#define KHZ_TO_MHZ(freq) ((freq) / 1000)
#define DBI_TO_MBI(gain) ((gain) * 100)
#define MBI_TO_DBI(gain) ((gain) / 100)
#define DBM_TO_MBM(gain) ((gain) * 100)
#define MBM_TO_DBM(gain) ((gain) / 100)

#define REG_RULE_EXT(start, end, bw, gain, eirp, dfs_cac, reg_flags)    \
{                                                                       \
	.freq_range.start_freq_khz = MHZ_TO_KHZ(start),                 \
	.freq_range.end_freq_khz = MHZ_TO_KHZ(end),                     \
	.freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw),                 \
	.power_rule.max_eirp = DBM_TO_MBM(eirp),                        \
	.flags = reg_flags,                                             \
	.dfs_cac_ms = dfs_cac,                                          \
}

#define REG_RULE(start, end, bw, gain, eirp, reg_flags) \
{                                                       \
	.freq_range.start_freq_khz = MHZ_TO_KHZ(start), \
	.freq_range.end_freq_khz = MHZ_TO_KHZ(end),     \
	.freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw), \
	.power_rule.max_antenna_gain = DBI_TO_MBI(gain),\
	.power_rule.max_eirp = DBM_TO_MBM(eirp),        \
	.flags = reg_flags,                             \
}
#endif
/* needed by
 * mgmt/rlm_domain.c
 * nic/nic_cmd_event.c
 */
struct wiphy {
	struct ieee80211_supported_band *bands[NUM_NL80211_BANDS];
};

/*
 * needed by include/mgmt/rlm_domain.h & mgmt/rlm_domain.c
 * but parameter not used
 */
struct regulatory_request {
	enum nl80211_reg_initiator initiator;
};

/* needed by mgmt/stats.c */
struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#define NSEC_PER_USEC	1000L

/*
 * needed by
 * mgmt/tdls.c
 * mgmt/saa_fsm.c, saaFsmSteps
 *
 * enum nl80211_tdls_operation - values for %NL80211_ATTR_TDLS_OPERATION
 * @NL80211_TDLS_DISCOVERY_REQ: Send a TDLS discovery request
 * @NL80211_TDLS_SETUP: Setup TDLS link
 * @NL80211_TDLS_TEARDOWN: Teardown a TDLS link which is already established
 * @NL80211_TDLS_ENABLE_LINK: Enable TDLS link
 * @NL80211_TDLS_DISABLE_LINK: Disable TDLS link
 */
enum nl80211_tdls_operation {
	NL80211_TDLS_DISCOVERY_REQ,
	NL80211_TDLS_SETUP,
	NL80211_TDLS_TEARDOWN,
	NL80211_TDLS_ENABLE_LINK,
	NL80211_TDLS_DISABLE_LINK,
};

/*
 * needed by mgmt/tdls.c
 */
enum gfp_t {
	GFP_KERNEL,
	GFP_ATOMIC,
	__GFP_HIGHMEM,
	__GFP_HIGH
};

/* needed by include/nic/adapter.h
 * for DIRECT_TX implementation in os folder
 */
struct timer_list {
	/* TODO: from the comment in linux/timer.h. Its difficult to implement
	 * All fields that change during normal runtime grouped to the
	 * same cacheline
	 */
};

/* needed by include/nic/adapter.h
 * for DIRECT_TX implementation in os folder
 */
struct sk_buff_head {
	/* These two members must be first. */
	/* struct sk_buff	*next; */
	/* struct sk_buff	*prev; */
	uint32_t qlen;
};

/*
 * should be defined as lock in corresponding os
 * access directly by
 * nic/nic_tx.c
 * nic/nic_rx.c
 * nic/que_mgt.c
 *
 * reference:
 * typedef struct spinlock {
 * } spinlock_t;
 */
#define spinlock_t uint32_t

/*
 * needed by mgmt/auth.c
 * struct cfg80211_ft_event - FT Information Elements
 * @ies: FT IEs
 * @ies_len: length of the FT IE in bytes
 * @target_ap: target AP's MAC address
 * @ric_ies: RIC IE
 * @ric_ies_len: length of the RIC IE in bytes
 */
struct cfg80211_ft_event_params {
	const u8 *ies;
	size_t ies_len;
	const u8 *target_ap;
	const u8 *ric_ies;
	size_t ric_ies_len;
};

/*
 * needed by
 * mgmt/cnm.c, which access struct GL_P2P_INFO directly
 * mgmt/p2p_func.c, do cnmMemAlloc
 * mgmt/p2p_role_fsm.c
 *
 * struct cfg80211_chan_def - channel definition
 * @chan: the (control) channel
 * @width: channel width
 * @center_freq1: center frequency of first segment
 * @center_freq2: center frequency of second segment
 * (only with 80+80 MHz)
 */
struct cfg80211_chan_def {
	struct ieee80211_channel *chan;
	enum nl80211_chan_width width;
	u32 center_freq1;
	u32 center_freq2;
};

/*
 * needed by
 * mgmt/p2p_scan.c
 */
struct cfg80211_scan_request {
};

/* need by include/hal.h, halDeAggRxPktWorker
 * comment: use os-related structure directly outside headers of gl layer
 * while the implementation is in os/linux/hif*
 * possible actions:
 * 1) should we just move the function prototype to os gl layer?
 */
struct work_struct {
	/* atomic_long_t data; */
	/* struct list_head entry; */
	/* work_func_t func; */
};

/* needed by include/nic/mt66xx_reg.h
 * struct mt66xx_chip_info
 * comment: use with #if CFG_MTK_ANDROID_WMT
 * implementation: extern void connectivity_export_show_stack
 * which is outside our driver, but defined in chips/connac*
 * should we move to glue layer ? other os can also show StakInfo
 */
struct task_struct {
};
/*
 * TODO: Functions need implementation
 */

/****************************************************************************
 * TODO: Functions prototype, which could be realized as follows
 * 1) inline function
 * 2) os API with same functionality
 * 3) implemented in gl_dependent.c
 ****************************************************************************
 */
/*
 * KAL_NEED_IMPLEMENT: wrapper to caution user to implement func when porting
 * @file: from which file
 * @func: called by which func
 * @line: at which line
 */
long KAL_NEED_IMPLEMENT(const char *file, const char *func, int line, ...);

/*
 * kal_dbg_print: print debug message to screen
 *
 * needed by common/debug.c
 * source/include/linux/printk.h
 */
int kal_dbg_print(const char *s, ...);
#define pr_info(fmt, ...) printf(fmt)

/*
 * kal_hex_dump_to_buffer: convert a blob of data to "hex ASCII" in memory
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 * @rowsize: number of bytes to print per line; must be 16 or 32
 * @groupsize: number of bytes to print at a time (1, 2, 4, 8; default = 1)
 * @linebuf: where to put the converted data
 * @linebuflen: total size of @linebuf, including space for terminating NUL
 * @ascii: include ASCII after the hex output
 *
 * needed by common/debug.c
 * source/include/linux/printk.h
 */
int kal_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	int groupsize, char *linebuf, size_t linebuflen,
	bool ascii);
#define hex_dump_to_buffer(_buf, _len, _rowsize, _groupsize, _linebuf, \
	_linebuflen, _ascii) \
	kal_hex_dump_to_buffer(_buf, _len, _rowsize, _groupsize, _linebuf, \
	_linebuflen, _ascii)

/*
 * purpose:
 * polite version of BUG_ON() - WARN_ON() which doesn't
 * kill the machine, replace panic() to dump_stack()
 * needed by mgmt/rlm_domain.c
 */
void kal_warn_on(uint8_t condition);
#define WARN_ON(_condition) kal_warn_on(_condition)

/*
 * kal_do_gettimeofday - Returns the time of day in a timeval
 * @tv: pointer to the timeval to be set
 * needed by
 * common/debug.c
 * mgmt/stats.c
 */
void kal_do_gettimeofday(struct timeval *tv);
#define do_gettimeofday(_tv) kal_do_gettimeofday(_tv)

/*
 * needed by: mgmt/wmm.c
 * anything on kalGetTimeTick (?)
 * Timespec interfaces utilizing the ktime based ones
 * ktime_to_timespec(ktime_get_boottime());
 */
void kal_get_monotonic_boottime(struct timespec *ts);
#define get_monotonic_boottime(_ts) kal_get_monotonic_boottime(_ts)

/*
 * needed by nic_tx.c
 * kal_mod_timer - modify a timer's timeout
 * @timer: the timer to be modified
 * @expires: new timeout in jiffies
 * The function returns whether it has modified a pending timer or not.
 * (ie. mod_timer() of an inactive timer returns 0, mod_timer() of an
 * active timer returns 1.)
 */
int kal_mod_timer(struct timer_list *timer, unsigned long expires);
#define mod_timer(_timer, _expires) kal_mod_timer(_timer, _expires)

/*
 * kstrto* - convert a string to an *
 * @s: The start of the string. The string must be null-terminated, and may also
 * include a single newline before its terminating null. The first character
 * may also be a plus sign or a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 * given as 0, then the base of the string is automatically detected with the
 * conventional semantics - If it begins with 0x the number will be parsed as a
 * hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 * parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kal_strtoint(const char *s, unsigned int base, int *res);
int kal_strtou8(const char *s, unsigned int base, uint8_t *res);
int kal_strtou16(const char *s, unsigned int base, uint16_t *res);
int kal_strtou32(const char *s, unsigned int base, uint32_t *res);
int kal_strtos32(const char *s, unsigned int base, int32_t *res);

/*
 * kstrtoul - convert a string to an unsigned long
 * @s: The start of the string. The string must be null-terminated, and may also
 * include a single newline before its terminating null. The first character
 * may also be a plus sign, but not a minus sign.
 * @base: The number base to use. The maximum supported base is 16. If base is
 * given as 0, then the base of the string is automatically detected with the
 * conventional semantics - If it begins with 0x the number will be parsed as a
 * hexadecimal (case insensitive), if it otherwise begins with 0, it will be
 * parsed as an octal number. Otherwise it will be parsed as a decimal.
 * @res: Where to write the result of the conversion on success.
 *
 * Returns 0 on success, -ERANGE on overflow and -EINVAL on parsing error.
 * Used as a replacement for the obsolete simple_strtoull. Return code must
 * be checked.
 */
int kal_strtoul(const char *s, unsigned int base, unsigned long *res);

/*
 * scnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is == 0 the function returns 0.
 */
int kal_scnprintf(char *buf, size_t size, const char *fmt, ...);

void *kal_kmalloc(size_t size, enum gfp_t type);
void *kal_vmalloc(size_t size);

void kal_kfree(void *addr);
void kal_vfree(void *addr);
/*
 * kal_spin_lock_bh: lock in bottom half, os-dependent
 * nic/nic_tx.c
 * nic/nic_rx.c
 * nic/que_mgt.c
 */
void kal_spin_lock_bh(spinlock_t *lock);
#define spin_lock_bh(_lock) kal_spin_lock_bh(_lock)

/*
 * kal_spin_unlock_bh: unlock in bottom half, os-dependent
 * paired with kal_spin_lock_bh
 * nic/nic_tx.c
 * nic/nic_rx.c
 * nic/que_mgt.c
 */
void kal_spin_unlock_bh(spinlock_t *lock);
#define spin_unlock_bh(_lock) kal_spin_unlock_bh(_lock)

/*
 * kal_spin_lock_irqsave: lock exclude irq, os-dependent
 * nic/nic_tx.c
 * nic/nic_rx.c
 * nic/que_mgt.c
 */
void kal_spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
#define spin_lock_irqsave(_lock, _flag) kal_spin_lock_irqsave(_lock, _flag)

/*
 * kal_spin_unlock_irqsave: unlock, os-dependent
 * paired with kal_spin_lock_irqsave
 * nic/nic_tx.c
 * nic/nic_rx.c
 * nic/que_mgt.c
 */
void kal_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
#define spin_unlock_irqrestore(_lock, _flag) \
		kal_spin_unlock_irqrestore(_lock, _flag)
/*
 * kal_skb_queue_len - get queue length
 * @list_: list to measure
 * Return the length of an &sk_buff queue.
 *
 * needed by nic_tx.c
 */
uint32_t kal_skb_queue_len(const struct sk_buff_head *list);
#define skb_queue_len(_list) kal_skb_queue_len(_list)

/* kal_skb_queue_tail - queue a buffer at the list tail
 * @list: list to use
 * @newsk: buffer to queue
 * Queue a buffer at the end of a list. This function takes no locks
 * and you must therefore hold required locks before calling it.
 * A buffer cannot be placed on two lists at the same time.
 *
 * needed by nic_tx.c, what's wrong with struct QUE?
 */
void kal_skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk);
#define skb_queue_tail(_list, _newsk)  kal_skb_queue_tail(_list, _newsk)

/*
 * skb_put - add data to a buffer
 * @skb: buffer to use
 * @len: amount of data to add
 * This function extends the used data area of the buffer. If this would
 * exceed the total buffer size the kernel will panic. A pointer to the
 * first byte of the extra data is returned.
 *
 * needed by nic_rx.c
 */
unsigned char *kal_skb_put(struct sk_buff *skb, unsigned int len);
#define skb_put(_skb, _len) kal_skb_put(_skb, _len)

/*
 * kal_skb_push - add data to the start of a buffer
 * @skb: buffer to use
 * @len: amount of data to add
 * This function extends the used data area of the buffer at the buffer
 * start. If this would exceed the total buffer headroom the kernel will
 * panic. A pointer to the first byte of the extra data is returned.
 *
 * needed by nic_tx.c
 */
void *kal_skb_push(struct sk_buff *skb, unsigned int len);
#define skb_push(_skb, _len) kal_skb_push(_skb, _len)

/*
 * needed by nic_tx.c
 * __skb_dequeue - remove from the head of the queue
 * @list: list to dequeue from
 *
 *	Remove the head of the list. This function does not take any locks
 *	so must be used with appropriate locks held only. The head item is
 *	returned or %NULL if the list is empty.
 */
struct sk_buff *kal_skb_dequeue_tail(struct sk_buff_head *list);
#define skb_dequeue(_list) kal_skb_dequeue_tail(_list)

/*
 * needed by nic_tx.c, what's wrong with struct QUE?
 * __skb_queue_head - queue a buffer at the list head
 * @list: list to use
 * @newsk: buffer to queue
 *
 *	Queue a buffer at the start of a list. This function takes no locks
 *	and you must therefore hold required locks before calling it.
 *
 *	A buffer cannot be placed on two lists at the same time.
 */
void kal_skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk);
#define skb_queue_head(_list, _newsk) kal_skb_queue_head(_list, _newsk)

/*
 * needed by nic_rx.c
 * make the tail pointer in skb point to beginning of data
 */
void kal_skb_reset_tail_pointer(struct sk_buff *skb);
#define skb_reset_tail_pointer(_skb) kal_skb_reset_tail_pointer(_skb)

/*
 * needed by nic_rx.c
 * remove end from a buffer
 * @len: len of skb after trim
 */
void kal_skb_trim(struct sk_buff *skb, unsigned int len);
#define skb_trim(_skb, _len) kal_skb_trim(_skb, _len)

/*
 * needed by mgmt/tkip_mic.c
 * legacy helper around netdev_alloc_skb()
 */
struct sk_buff *kal_dev_alloc_skb(unsigned int length);
#define dev_alloc_skb(_length) kal_dev_alloc_skb(_length)

/* needed by mgmt/tkip_mic.c */
void kal_kfree_skb(struct sk_buff *skb);
#define kfree_skb(_skb) kal_kfree_skb(_skb)

/****************************************************************************
 * TODO: Functions need implementation
 ****************************************************************************
 */
/* needed by
 * common/debug.c
 * common/wlan_lib.c
 * mgmt/stats.c
 * ais_fsm.c
 */
#define kal_sched_clock() KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define sched_clock() kal_sched_clock()

/* looks like min/max not in C
 * https://stackoverflow.com/questions/3437404/min-and-max-in-c
 * needed by:
 * common/debug.c
 * mgmt/scan.c
 */
#define min(_a, _b) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)


/* needed by mgmt/stats.c */
#define rtc_time_to_tm(_time, _tm) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/*	implemented: os/linux/gl_wext.c
 *	used: common/wlan_oid.c, wlanoidSetWapiAssocInfo
 *	why function called "search WPAIIE" has been implement under wext
 *	first used in wext_get_scan. Should it be part of wlan_oid?
 */
#if CFG_SUPPORT_WAPI
#define wextSrchDesiredWAPIIE(_pucIEStart, _i4TotalIeLen, \
	_ppucDesiredIE) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#endif

/* common/wlan_lib.c */
#define netdev_priv(_ndev) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _ndev)

/*
 * needed by
 * mgmt/rlm_domain.c
 * nic/nic_cmd_event.c
 */
#define priv_to_wiphy(_priv) \
((void *) KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__, _priv))


/* needed by mgmt/rlm_domain.c
 * implementation in linux is in gl_cfg80211.c
 * while other operating system may also need to notify reg change
 */
#define mtk_reg_notify(_pWiphy, _pRequest) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* needed by mgmt/rlm_domain.c */
#define regulatory_hint(_wiphy, _alpha2) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/* needed by mgmt/tdls.c */
#define cfg80211_tdls_oper_request(_dev, _peer, _oper, \
	_reason_code, _gfp) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/*
 * needed by
 * mgmt/p2p_func.c
 *
 * cfg80211_ch_switch_notify - update wdev channel and notify userspace
 * @dev: the device which switched channels
 * @chandef: the new channel definition
 *
 * Caller must acquire wdev_lock, therefore must only be called from sleepable
 * driver context!
 */
#define cfg80211_ch_switch_notify(_dev, _chandef) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
/*
 * needed by mgmt/rlm.c
 */
#define get_random_bytes(_buf, _nbytes) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
/*
 * needed by: nic/nic_cmd_event.c
 * 0: no error !0: error
 * defeined in include/linux/err.h
 */
#define IS_ERR(_ptr) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

/*
 * os endian related, need to find corresponding API in OS
 * eg.
 * #if __BYTE_ORDER == __BIG_ENDIAN
 * #define cpu_to_le16 bswap_16
 * #else
 * #define cpu_to_le16
 * #endif
 */
#define cpu_to_le16(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define cpu_to_le32(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define cpu_to_le64(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define le16_to_cpu(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define le32_to_cpu(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define le64_to_cpu(_val) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

#define div_u64(_val, _div) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)

int kal_test_and_clear_bit(unsigned long bit, unsigned long *p);
#ifndef test_and_clear_bit
#define test_and_clear_bit(_offset, _val) \
	kal_test_and_clear_bit(_offset, _val)
#endif

void kal_clear_bit(unsigned long bit, unsigned long *p);
#ifndef clear_bit
#define clear_bit(_offset, _val) kal_clear_bit(_offset, _val)
#endif

/*
 * kal_set_bit: set bit atomically
 * @nr: bit to set
 * @addr: addr to set bit
 */
void kal_set_bit(unsigned long bit, unsigned long *p);
#ifndef set_bit
#define set_bit(_offset, _val) kal_set_bit(_offset, _val)
#endif

/* needed by mgmt/scan.c */
int kal_test_bit(unsigned long bit, unsigned long *p);
#ifndef test_bit
#define test_bit(_offset, _val) kal_test_bit(_offset, _val)
#endif
#endif
