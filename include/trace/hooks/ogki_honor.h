/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ogki_honor
#ifdef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_PATH
#endif
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_OGKI_HONOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_OGKI_HONOR_H

#include <trace/hooks/vendor_hooks.h>

struct task_struct;
struct sock;
struct sk_buff;
struct ufs_hba;
struct tcp_sock;
struct net_device;
struct cfg80211_registered_device;
struct dentry;
struct inode;
struct page;
struct bio;
DECLARE_HOOK(android_vh_ogki_async_psi_bypass,
	TP_PROTO(bool *bypass),
	TP_ARGS(bypass));
DECLARE_HOOK(android_vh_ogki_ufs_clock_scaling,
	TP_PROTO(struct ufs_hba *hba, bool *force_out, bool *force_scaling, bool *scale_up),
	TP_ARGS(hba, force_out, force_scaling, scale_up));
DECLARE_HOOK(android_vh_ogki_ufs_dsm,
	TP_PROTO(struct ufs_hba *hba, unsigned long code, char *err_msg),
	TP_ARGS(hba, code, err_msg));
DECLARE_HOOK(android_vh_ogki_audit_log_setid,
	TP_PROTO(u32 type, u32 old_id, u32 new_id),
	TP_ARGS(type, old_id, new_id));
DECLARE_HOOK(android_vh_ogki_audit_log_cfi,
	TP_PROTO(unsigned long addr, unsigned long* target),
	TP_ARGS(addr, target));
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_audit_log_usercopy,
	TP_PROTO(bool to_user, const char* name, unsigned long len),
	TP_ARGS(to_user, name, len), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_audit_log_module_sign,
	TP_PROTO(int err),
	TP_ARGS(err), 1);
DECLARE_HOOK(android_vh_ogki_check_vip_status,
	TP_PROTO(int cur_pid, int cur_tgid, struct task_struct* task, int* ret),
	TP_ARGS(cur_pid, cur_tgid, task, ret));
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_task_util,
	TP_PROTO(struct task_struct* p, unsigned long* ret),
	TP_ARGS(p, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_uclamp_task_util,
	TP_PROTO(struct task_struct* p, unsigned long* ret),
	TP_ARGS(p, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_get_task_tags,
	TP_PROTO(struct task_struct* p, unsigned long long* ret),
	TP_ARGS(p, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_get_task_rsum,
	TP_PROTO(struct task_struct* p, unsigned long long* ret),
	TP_ARGS(p, ret), 1);
DECLARE_HOOK(android_rvh_ogki_check_task_tags,
	TP_PROTO(struct task_struct *p, int *ret),
	TP_ARGS(p, ret));
DECLARE_HOOK(android_vh_ogki_tcp_srtt_estimator,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_ogki_tcp_rcv_estab_fastpath,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_ogki_tcp_rcv_estab_slowpath,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_ogki_set_wifi_state_connect,
	TP_PROTO(const char *name, struct cfg80211_registered_device *rdev, struct net_device *dev, u8 *mac_addr),
	TP_ARGS(name, rdev, dev, mac_addr));
DECLARE_HOOK(android_vh_ogki_set_wifi_state_disconnect,
	TP_PROTO(const char *name), TP_ARGS(name));
DECLARE_HOOK(android_vh_ogki_tcp_rcv_rtt_update,
	TP_PROTO(struct tcp_sock *tp, u32 sample, int win_dep), TP_ARGS(tp, sample, win_dep));
DECLARE_HOOK(android_vh_ogki_tcp_retransmit_timer,
	TP_PROTO(struct sock *sk), TP_ARGS(sk));
DECLARE_HOOK(android_vh_ogki_udp_unicast_rcv_skb,
	TP_PROTO(struct sk_buff *skb, struct sock *sk),
	TP_ARGS(skb, sk));
DECLARE_HOOK(android_vh_ogki_udp6_unicast_rcv_skb,
	TP_PROTO(struct sk_buff *skb, struct sock *sk),
	TP_ARGS(skb, sk));
DECLARE_HOOK(android_vh_ogki_get_log_usertype,
	TP_PROTO(unsigned int *type),
	TP_ARGS(type));
DECLARE_HOOK(android_vh_ogki_hievent_to_jank,
	TP_PROTO(int tag, int prio, const char *buf, int *ret),
	TP_ARGS(tag, prio, buf, ret));
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_hievent_create,
	TP_PROTO(unsigned int event_id, void **event),
	TP_ARGS(event_id, event), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_hievent_put_string,
	TP_PROTO(void *event, const char *key, const char *value, int *ret),
	TP_ARGS(event, key, value, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_hievent_put_integral,
	TP_PROTO(void *event, const char *key, long long value, int *ret),
	TP_ARGS(event, key, value, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_hievent_report,
	TP_PROTO(void *event, int *ret),
	TP_ARGS(event, ret), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_hievent_destroy,
	TP_PROTO(void *event),
	TP_ARGS(event), 1);
DECLARE_HOOK(android_vh_ogki_f2fs_dsm,
	TP_PROTO(char *name, int len),
	TP_ARGS(name, len));
DECLARE_HOOK(android_vh_ogki_f2fs_dsm_get,
	TP_PROTO(unsigned long code, char *err_msg),
	TP_ARGS(code, err_msg));
DECLARE_HOOK(android_vh_ogki_f2fs_create,
	TP_PROTO(struct inode *inode, struct dentry *dentry),
	TP_ARGS(inode, dentry));
DECLARE_HOOK(android_vh_ogki_f2fs_submit_write_page,
	TP_PROTO(struct page *page, struct bio *bio),
	TP_ARGS(page, bio));
DECLARE_HOOK(android_vh_ogki_cma_alloc_retry,
	TP_PROTO(char *name, int *retry),
	TP_ARGS(name, retry));
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_vmalloc_node_bypass,
	TP_PROTO(unsigned long size, gfp_t gfp_mask, void **addr),
	TP_ARGS(size, gfp_mask, addr), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_ogki_vfree_bypass,
	TP_PROTO(const void *addr, bool *bypass),
	TP_ARGS(addr, bypass), 1);
DECLARE_HOOK(android_vh_ogki_kmem_cache_create_usercopy,
	TP_PROTO(unsigned int flags),
	TP_ARGS(flags));
#endif /* _TRACE_HOOK_OGKI_ogki_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

