#ifndef _LINUX_PM_QOS_H
#define _LINUX_PM_QOS_H
/* interface for the pm_qos_power infrastructure of the linux kernel.
 *
 * Mark Gross <mgross@linux.intel.com>
 *
 * Support added for bounded constraints by Sai Gurrappadi
 * <sgurrappad@nvidia.com>
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 */
#include <linux/plist.h>
#include <linux/notifier.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/workqueue.h>

enum {
	PM_QOS_RESERVED = 0,
	PM_QOS_CPU_DMA_LATENCY,
	PM_QOS_NETWORK_LATENCY,
	PM_QOS_NETWORK_THROUGHPUT,
	PM_QOS_MIN_ONLINE_CPUS,
	PM_QOS_MAX_ONLINE_CPUS,
	PM_QOS_CPU_FREQ_MIN,
	PM_QOS_CPU_FREQ_MAX,
	PM_QOS_GPU_FREQ_MIN,
	PM_QOS_GPU_FREQ_MAX,
	PM_QOS_EMC_FREQ_MIN,

	/* insert new class ID */

	PM_QOS_NUM_CLASSES,
};

/**
 * enum pm_qos_bounded_classes - Class ID's for bounded constraints
 *
 * Each class wraps around a corresponding min and max pm qos node
 * and binds the two constraints in one.
 */
enum pm_qos_bounded_classes {
	PM_QOS_RESERVED_BOUNDS = 0,
	PM_QOS_CPU_FREQ_BOUNDS,	/* requests should be in KHz to not exceed s32*/
	PM_QOS_GPU_FREQ_BOUNDS,	/* requests should be in KHz to not exceed s32*/
	PM_QOS_ONLINE_CPUS_BOUNDS,
	/* insert new bounded class ids here */
	PM_QOS_NUM_BOUNDED_CLASSES,
};

enum pm_qos_flags_status {
	PM_QOS_FLAGS_UNDEFINED = -1,
	PM_QOS_FLAGS_NONE,
	PM_QOS_FLAGS_SOME,
	PM_QOS_FLAGS_ALL,
};

#define PM_QOS_DEFAULT_VALUE -1

#define PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
#define PM_QOS_NETWORK_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
#define PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_MIN_ONLINE_CPUS_DEFAULT_VALUE	0
#define PM_QOS_MAX_ONLINE_CPUS_DEFAULT_VALUE	LONG_MAX
#define PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE	LONG_MAX
#define PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE	LONG_MAX
#define PM_QOS_EMC_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_EMC_FREQ_MAX_DEFAULT_VALUE	LONG_MAX
#define PM_QOS_DEV_LAT_DEFAULT_VALUE		0

#define PM_QOS_FLAG_NO_POWER_OFF	(1 << 0)
#define PM_QOS_FLAG_REMOTE_WAKEUP	(1 << 1)

struct pm_qos_request {
	struct plist_node node;
	int pm_qos_class;
	int priority;
	struct delayed_work work; /* for pm_qos_update_request_timeout */
};

struct pm_qos_flags_request {
	struct list_head node;
	s32 flags;	/* Do not change to 64 bit */
};

enum dev_pm_qos_req_type {
	DEV_PM_QOS_LATENCY = 1,
	DEV_PM_QOS_FLAGS,
};

struct dev_pm_qos_request {
	enum dev_pm_qos_req_type type;
	union {
		struct plist_node pnode;
		struct pm_qos_flags_request flr;
	} data;
	struct device *dev;
	struct delayed_work work; /* for pm_qos_update_request_timeout */
};

enum pm_qos_type {
	PM_QOS_UNITIALIZED,
	PM_QOS_MAX,		/* return the largest value */
	PM_QOS_MIN		/* return the smallest value */
};

/**
 * enum pm_qos_bound_priority - priority value of the given bound request
 *
 * Kernel space clients can request any priority level; priorities
 * PM_QOS_PRIO_TRUSTED and higher should be used when the client wants to
 * ensure that no userspace client can override it's request
 *
 * Userspace clients can request any priority as high as PM_QOS_PRIO_UNTRUSTED
 * Default userspace request which don't have any priority specified will have
 * PM_QOS_PRIO_DEFAULT_UNTRUSTED priority.
 *
 * General rule of thumb - request as low (numerically larger) a priority
 * as you can
 */
enum pm_qos_bound_priority {
	/* kernel clients */
	PM_QOS_PRIO_HIGHEST = 0,
	PM_QOS_PRIO_TRUSTED = 10,
	/* userspace clients */
	PM_QOS_PRIO_UNTRUSTED,
	PM_QOS_PRIO_DEFAULT_UNTRUSTED = 50,
	PM_QOS_NUM_PRIO = 100,
};

/**
 * struct pm_qos_prio - priority node for bounded constraints
 * @max_list: List of upper bound requests
 * @min_list: list of lower bound requests
 * @node: node to queue in the list of priorities
 */
struct pm_qos_prio {
	struct plist_head max_list;
	struct plist_head min_list;
	struct plist_node node;
};

/**
 * struct pm_qos_bounded_constraint - binds two pm_qos_constraints
 * @prio_list: list of priorities
 * @max_class: Class id of the upper bound
 * @min_class: Class id of the lower bound
 * @min_wins: Pick min if min > max
 *
 * Requests are added at their corresponding priority level. Target
 * values for a bounded constraint will be the intersection of all the
 * (min, max) ranges specified by each priority level. If the intersection
 * is null at any priority level, the higher priority level's requests are
 * honoured.
 */
struct pm_qos_bounded_constraint {
	struct plist_head prio_list;
	int max_class;
	int min_class;
	bool min_wins;
};

/*
 * Note: The lockless read path depends on the CPU accessing target_value
 * or effective_flags atomically.  Atomic access is only guaranteed on all CPU
 * types linux supports for 32 bit quantites
 */
struct pm_qos_constraints {
	struct plist_head list;
	s32 target_value;	/* Do not change to 64 bit */
	s32 default_value;
	enum pm_qos_type type;
	struct blocking_notifier_head *notifiers;
	int parent_class;
};

struct pm_qos_flags {
	struct list_head list;
	s32 effective_flags;	/* Do not change to 64 bit */
	struct blocking_notifier_head *notifiers;
};

struct dev_pm_qos {
	struct pm_qos_constraints latency;
	struct pm_qos_flags flags;
	struct dev_pm_qos_request *latency_req;
	struct dev_pm_qos_request *flags_req;
};

/* Action requested to pm_qos_update_target */
enum pm_qos_req_action {
	PM_QOS_ADD_REQ,		/* Add a new request */
	PM_QOS_UPDATE_REQ,	/* Update an existing request */
	PM_QOS_REMOVE_REQ	/* Remove an existing request */
};

static inline int dev_pm_qos_request_active(struct dev_pm_qos_request *req)
{
	return req->dev != NULL;
}

int pm_qos_update_target(struct pm_qos_constraints *c, struct plist_node *node,
			 enum pm_qos_req_action action, int value);
bool pm_qos_update_flags(struct pm_qos_flags *pqf,
			 struct pm_qos_flags_request *req,
			 enum pm_qos_req_action action, s32 val);
void pm_qos_add_request(struct pm_qos_request *req, int pm_qos_class,
			s32 value);
void pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value);
void pm_qos_update_request_timeout(struct pm_qos_request *req,
				   s32 new_value, unsigned long timeout_us);
void pm_qos_remove_request(struct pm_qos_request *req);

/* Interface for bounded constraints */
void pm_qos_add_min_bound_req(struct pm_qos_request *req, int priority,
			      int pm_qos_bounded_class, s32 val);
void pm_qos_add_max_bound_req(struct pm_qos_request *req, int priority,
			      int pm_qos_bounded_class, s32 val);
void pm_qos_update_bounded_req(struct pm_qos_request *req, int priority,
			       s32 val);
void pm_qos_update_bounded_req_timeout(struct pm_qos_request *req,
				       unsigned long timeout_us);
void pm_qos_remove_bounded_req(struct pm_qos_request *req);
void pm_qos_add_min_notifier(int pm_qos_bounded_class,
			     struct notifier_block *notifer);
void pm_qos_add_max_notifier(int pm_qos_bounded_class,
			     struct notifier_block *notifier);
void pm_qos_remove_min_notifier(int pm_qos_bounded_class,
				struct notifier_block *notifier);
void pm_qos_remove_max_notifier(int pm_qos_bounded_class,
				struct notifier_block *notifier);
s32 pm_qos_read_min_bound(int pm_qos_bounded_class);
s32 pm_qos_read_max_bound(int pm_qos_bounded_class);

int pm_qos_request(int pm_qos_class);
int pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier);
int pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier);
int pm_qos_request_active(struct pm_qos_request *req);
s32 pm_qos_read_value(struct pm_qos_constraints *c);

#ifdef CONFIG_PM
enum pm_qos_flags_status __dev_pm_qos_flags(struct device *dev, s32 mask);
enum pm_qos_flags_status dev_pm_qos_flags(struct device *dev, s32 mask);
s32 __dev_pm_qos_read_value(struct device *dev);
s32 dev_pm_qos_read_value(struct device *dev);
int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req,
			   enum dev_pm_qos_req_type type, s32 value);
int dev_pm_qos_update_request(struct dev_pm_qos_request *req, s32 new_value);
int dev_pm_qos_update_request_timeout(struct dev_pm_qos_request *req,
				      s32 new_value,
				      unsigned long timeout_us);
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req);
int dev_pm_qos_add_notifier(struct device *dev,
			    struct notifier_block *notifier);
int dev_pm_qos_remove_notifier(struct device *dev,
			       struct notifier_block *notifier);
int dev_pm_qos_add_global_notifier(struct notifier_block *notifier);
int dev_pm_qos_remove_global_notifier(struct notifier_block *notifier);
void dev_pm_qos_constraints_init(struct device *dev);
void dev_pm_qos_constraints_destroy(struct device *dev);
int dev_pm_qos_add_ancestor_request(struct device *dev,
				    struct dev_pm_qos_request *req, s32 value);
#else
static inline enum pm_qos_flags_status __dev_pm_qos_flags(struct device *dev,
							  s32 mask)
			{ return PM_QOS_FLAGS_UNDEFINED; }
static inline enum pm_qos_flags_status dev_pm_qos_flags(struct device *dev,
							s32 mask)
			{ return PM_QOS_FLAGS_UNDEFINED; }
static inline s32 __dev_pm_qos_read_value(struct device *dev)
			{ return 0; }
static inline s32 dev_pm_qos_read_value(struct device *dev)
			{ return 0; }
static inline int dev_pm_qos_add_request(struct device *dev,
					 struct dev_pm_qos_request *req,
					 enum dev_pm_qos_req_type type,
					 s32 value)
			{ return 0; }
static inline int dev_pm_qos_update_request(struct dev_pm_qos_request *req,
					    s32 new_value)
			{ return 0; }
static inline int dev_pm_qos_remove_request(struct dev_pm_qos_request *req)
			{ return 0; }
static inline int dev_pm_qos_add_notifier(struct device *dev,
					  struct notifier_block *notifier)
			{ return 0; }
static inline int dev_pm_qos_remove_notifier(struct device *dev,
					     struct notifier_block *notifier)
			{ return 0; }
static inline int dev_pm_qos_add_global_notifier(
					struct notifier_block *notifier)
			{ return 0; }
static inline int dev_pm_qos_remove_global_notifier(
					struct notifier_block *notifier)
			{ return 0; }
static inline void dev_pm_qos_constraints_init(struct device *dev)
{
	dev->power.power_state = PMSG_ON;
}
static inline void dev_pm_qos_constraints_destroy(struct device *dev)
{
	dev->power.power_state = PMSG_INVALID;
}
static inline int dev_pm_qos_add_ancestor_request(struct device *dev,
				    struct dev_pm_qos_request *req, s32 value)
			{ return 0; }
#endif

#ifdef CONFIG_PM_RUNTIME
int dev_pm_qos_expose_latency_limit(struct device *dev, s32 value);
void dev_pm_qos_hide_latency_limit(struct device *dev);
int dev_pm_qos_expose_flags(struct device *dev, s32 value);
void dev_pm_qos_hide_flags(struct device *dev);
int dev_pm_qos_update_flags(struct device *dev, s32 mask, bool set);

static inline s32 dev_pm_qos_requested_latency(struct device *dev)
{
	return dev->power.qos->latency_req->data.pnode.prio;
}

static inline s32 dev_pm_qos_requested_flags(struct device *dev)
{
	return dev->power.qos->flags_req->data.flr.flags;
}
#else
static inline int dev_pm_qos_expose_latency_limit(struct device *dev, s32 value)
			{ return 0; }
static inline void dev_pm_qos_hide_latency_limit(struct device *dev) {}
static inline int dev_pm_qos_expose_flags(struct device *dev, s32 value)
			{ return 0; }
static inline void dev_pm_qos_hide_flags(struct device *dev) {}
static inline int dev_pm_qos_update_flags(struct device *dev, s32 m, bool set)
			{ return 0; }

static inline s32 dev_pm_qos_requested_latency(struct device *dev) { return 0; }
static inline s32 dev_pm_qos_requested_flags(struct device *dev) { return 0; }
#endif

#endif
