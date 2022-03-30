/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_AP_H__
#define __MTK_APU_MDW_AP_H__

#include "mdw.h"
#include "apusys_device.h"

struct mdw_ap_cmd {
	struct mdw_cmd *c;
	struct list_head sc_list;
	struct mdw_ap_sc *sc_arr;
	uint8_t *adj_matrix;
	struct list_head di_list; //for dispr item
	struct mutex mtx;
	struct kref ref;
	pid_t pid;
	pid_t tgid;
	int state;

	uint64_t sc_bitmask; // activated subcmds
	uint8_t ctx_cnt[MDW_SUBCMD_MAX]; // ctx count
	uint8_t ctx_repo[MDW_SUBCMD_MAX]; // ctx tmp storage
	uint8_t pack_cnt[MDW_SUBCMD_MAX]; // ctx count
};

struct mdw_ap_sc {
	struct mdw_ap_cmd *parent;
	struct mdw_subcmd_kinfo *hdr;
	struct mdw_subcmd_exec_info *einfo;
	struct list_head c_item;
	uint32_t idx;
	uint32_t vlm_ctx;
	uint32_t tcm_real_size;
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;
	uint32_t multi_total;
	uint32_t type;
	uint32_t boost;
	int ret;

	struct list_head ds_item; // to done sc q
	struct list_head di_item; // to dispr item

	/* norm used */
	struct list_head q_item; // to pid's priority q

	/* deadline used */
	struct rb_node node; // deadline queue
	uint64_t period;
	uint64_t deadline;
	uint64_t runtime;
	int cluster_size; // cluster size of preemption

	/* perf info */
	struct timespec64 ts_enque;
	struct timespec64 ts_deque;
	struct timespec64 ts_start;
	struct timespec64 ts_end;
};

struct mdw_parser {
	int (*end_sc)(struct mdw_ap_sc *in, struct mdw_ap_sc **out);
	int (*get_ctx)(struct mdw_ap_sc *sc);
	void (*put_ctx)(struct mdw_ap_sc *sc);
	int (*set_hnd)(struct mdw_ap_sc *sc, int d_idx, void *h);
	void (*clear_hnd)(void *h);
	bool (*is_deadline)(struct mdw_ap_sc *sc);
};
extern struct mdw_parser mdw_ap_parser;
int mdw_ap_cmd_exec(struct mdw_cmd *c);
int mdw_ap_cmd_wait(struct mdw_cmd *c, unsigned int ms);

struct mdw_queue_ops {
	int (*task_start)(struct mdw_ap_sc *sc, void *q);
	int (*task_end)(struct mdw_ap_sc *sc, void *q);
	struct mdw_ap_sc *(*pop)(void *q);
	int (*insert)(struct mdw_ap_sc *sc, void *q, int is_front);
	int (*delete)(struct mdw_ap_sc *sc, void *q);
	int (*len)(void *q);
	void (*destroy)(void *q);
};

/* deadline sched */
struct deadline_root {
	char name[32];
	struct rb_root_cached root;
	int cores;
	uint64_t total_runtime;
	uint64_t total_period;
	uint64_t total_subcmd;
	uint64_t avg_load[3];
	struct mutex lock;
	struct delayed_work work;
	bool need_timer;
	bool load_boost;
	bool trace_boost;

	atomic_t cnt;
	struct mdw_queue_ops ops;
};

int mdw_queue_deadline_init(struct deadline_root *root);

/* normal sched */
struct mdw_queue_norm {
	uint32_t cnt;
	struct mutex mtx;
	unsigned long bmp[BITS_TO_LONGS(MDW_PRIORITY_MAX)];
	struct list_head list[MDW_PRIORITY_MAX];
	struct mdw_queue_ops ops;
};

int mdw_queue_norm_init(struct mdw_queue_norm *nq);

/* mdw queue */
struct mdw_queue {
	struct mutex mtx;
	uint32_t normal_task_num;
	uint32_t deadline_task_num;
	struct mdw_queue_norm norm;
	struct deadline_root deadline;
};

int mdw_queue_task_start(struct mdw_ap_sc *sc);
int mdw_queue_task_end(struct mdw_ap_sc *sc);
struct mdw_ap_sc *mdw_queue_pop(int type);
int mdw_queue_insert(struct mdw_ap_sc *sc, int is_front);
int mdw_queue_len(int type, int is_deadline);
int mdw_queue_delete(struct mdw_ap_sc *sc);
int mdw_queue_boost(struct mdw_ap_sc *sc);
void mdw_queue_destroy(struct mdw_queue *mq);
int mdw_queue_init(struct mdw_queue *mq);

extern struct dentry *mdw_dbg_root;

#define MDW_DEV_NAME_SIZE (16)
#define MDW_RSC_TAB_DEV_MAX (16) //max device num per type
#define MDW_RSC_SET_PWR_TIMEOUT (3*1000)
#define MDW_RSC_SET_PWR_ALLON (0)

#define APUSYS_THD_TASK_FILE_PATH "/dev/stune/low_latency/tasks"

enum MDW_PREEMPT_PLCY {
	MDW_PREEMPT_PLCY_RR_SIMPLE,
	MDW_PREEMPT_PLCY_RR_PRIORITY,

	MDW_PREEMPT_PLCY_MAX,
};

enum MDW_DEV_INFO_GET_POLICY {
	MDW_DEV_INFO_GET_POLICY_SEQ,
	MDW_DEV_INFO_GET_POLICY_RR,

	MDW_DEV_INFO_GET_POLICY_MAX,
};

enum MDW_DEV_INFO_GET_MODE {
	MDW_DEV_INFO_GET_MODE_TRY,
	MDW_DEV_INFO_GET_MODE_SYNC,
	MDW_DEV_INFO_GET_MODE_ASYNC,

	MDW_DEV_INFO_GET_MODE_MAX,
};

enum MDW_DEV_INFO_STATE {
	MDW_DEV_INFO_STATE_IDLE,
	MDW_DEV_INFO_STATE_BUSY,
	MDW_DEV_INFO_STATE_LOCK,
};

struct mdw_dev_info {
	int idx;
	int type;
	int state;

	char name[32];

	struct apusys_device *dev;
	struct list_head t_item; //to rsc_tab
	struct list_head r_item; //to rsc_req
	struct list_head u_item; //to mdw_usr

	int (*exec)(struct mdw_dev_info *d, void *s);
	int (*pwr_on)(struct mdw_dev_info *d, int bst, int to);
	int (*pwr_off)(struct mdw_dev_info *d);
	int (*suspend)(struct mdw_dev_info *d);
	int (*resume)(struct mdw_dev_info *d);
	int (*fw)(struct mdw_dev_info *d, uint32_t magic, const char *name,
		uint64_t kva, uint32_t iova, uint32_t size, int op);
	int (*ucmd)(struct mdw_dev_info *d,
		uint64_t kva, uint32_t iova, uint32_t size);
	int (*sec_on)(struct mdw_dev_info *d);
	int (*sec_off)(struct mdw_dev_info *d);
	int (*lock)(struct mdw_dev_info *d);
	int (*unlock)(struct mdw_dev_info *d);

	void *sc;

	int stop;
	struct task_struct *thd;
	struct completion cmplt;
	struct completion thd_done;

	struct mutex mtx;
};

struct mdw_rsc_tab {
	int type;
	char name[MDW_DEV_NAME_SIZE];
	int dev_num;
	int avl_num;

	uint32_t norm_sc_cnt;

	struct mdw_dev_info *array[MDW_RSC_TAB_DEV_MAX]; //for mdw_dev_info
	struct list_head list; //for mdw_dev_info
	struct mutex mtx;
	struct mdw_queue q;

	struct dentry *dbg_dir;
};

struct mdw_rsc_req {
	uint8_t num[MDW_DEV_MAX]; //in
	uint8_t get_num[MDW_DEV_MAX]; //in
	uint32_t total_num; //in
	uint64_t acq_bmp; //in
	int mode; //in
	int policy; //in

	uint32_t ready_num;

	void (*cb_async)(struct mdw_rsc_req *r); //call if async
	bool in_list;
	struct kref ref;

	struct completion complt;
	struct list_head r_item; // to rsc mgr
	struct list_head d_list; // for rsc_dev
	struct mutex mtx;
};

extern struct dentry *mdw_dbg_device;

uint64_t mdw_rsc_get_avl_bmp(void);
void mdw_rsc_update_avl_bmp(int type);
struct mdw_queue *mdw_rsc_get_queue(int type);
int mdw_rsc_get_dev(struct mdw_rsc_req *req);
int mdw_rsc_put_dev(struct mdw_dev_info *d);
struct mdw_rsc_tab *mdw_rsc_get_tab(int type);
struct mdw_dev_info *mdw_rsc_get_dinfo(int type, int idx);

void mdw_rsc_dump(struct seq_file *s);
int mdw_rsc_set_preempt_plcy(uint32_t preempt_policy);
uint32_t mdw_rsc_get_preempt_plcy(void);
uint64_t mdw_rsc_get_dev_bmp(void);
int mdw_rsc_get_dev_num(int type);

#if IS_ENABLED(CONFIG_MTK_GZ_SUPPORT_SDSP)
extern int mtee_sdsp_enable(u32 on);
#endif

/* mdw_rsc.c */
int mdw_rsc_init(void);
void mdw_rsc_deinit(void);
int mdw_rsc_register_device(struct apusys_device *dev);
int mdw_rsc_unregister_device(struct apusys_device *dev);

/* mdw_dispr */
void mdw_dispr_check(void);
int mdw_dispr_norm(struct mdw_ap_sc *sc);
int mdw_dispr_pack(struct mdw_ap_sc *sc);
int mdw_dispr_init(void);
void mdw_dispr_deinit(void);

/* mdw_sched */
#define preemption_support (1)
int mdw_sched_dev_routine(void *arg);
int mdw_sched(struct mdw_ap_sc *sc);
int mdw_sched_init(void);
void mdw_sched_deinit(void);

int mdw_sched_pause(void);
void mdw_sched_restart(void);

#endif
