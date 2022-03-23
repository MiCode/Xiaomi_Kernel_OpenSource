/*
 * This module exposes the interface to kernel space for specifying
 * QoS dependencies.  It provides infrastructure for registration of:
 *
 * Dependents on a QoS value : register requests
 * Watchers of QoS value : get notified when target QoS value changes
 *
 * This QoS design is best effort based.  Dependents register their QoS needs.
 * Watchers register to keep track of the current QoS needs of the system.
 *
 * There are 3 basic classes of QoS parameter: latency, timeout, throughput
 * each have defined units:
 * latency: usec
 * timeout: usec <-- currently not used.
 * throughput: kbs (kilo byte / sec)
 *
 * There are lists of pm_qos_objects each one wrapping requests, notifiers
 *
 * User mode requests on a QOS parameter register themselves to the
 * subsystem by opening the device node /dev/... and writing there request to
 * the node.  As long as the process holds a file handle open to the node the
 * client continues to be accounted for.  Upon file release the usermode
 * request is removed and a new qos target is computed.  This way when the
 * request that the application has is cleaned up when closes the file
 * pointer or exits the pm_qos_object will get an opportunity to clean up.
 *
 * Mark Gross <mgross@linux.intel.com>
 */

/*#define DEBUG*/

#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/uaccess.h>
#include <linux/export.h>
#include <trace/events/power.h>

/*
 * locking rule: all changes to constraints or notifiers lists
 * or pm_qos_object list and pm_qos_objects need to happen with pm_qos_lock
 * held, taken with _irqsave.  One lock to rule them all
 */
struct pm_qos_object {
	struct pm_qos_constraints *constraints;
	struct miscdevice pm_qos_power_miscdev;
	char *name;
};

static DEFINE_SPINLOCK(pm_qos_lock);

static struct pm_qos_object null_pm_qos;

static BLOCKING_NOTIFIER_HEAD(cpu_dma_lat_notifier);
static struct pm_qos_constraints cpu_dma_constraints = {
	.req_list = LIST_HEAD_INIT(cpu_dma_constraints.req_list),
	.list = PLIST_HEAD_INIT(cpu_dma_constraints.list),
	.target_value = PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(cpu_dma_constraints.qos_lock),
	.notifiers = &cpu_dma_lat_notifier,
};
static struct pm_qos_object cpu_dma_pm_qos = {
	.constraints = &cpu_dma_constraints,
	.name = "cpu_dma_latency",
};

static BLOCKING_NOTIFIER_HEAD(network_lat_notifier);
static struct pm_qos_constraints network_lat_constraints = {
	.req_list = LIST_HEAD_INIT(network_lat_constraints.req_list),
	.list = PLIST_HEAD_INIT(network_lat_constraints.list),
	.target_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(network_lat_constraints.qos_lock),
	.notifiers = &network_lat_notifier,
};
static struct pm_qos_object network_lat_pm_qos = {
	.constraints = &network_lat_constraints,
	.name = "network_latency",
};


static BLOCKING_NOTIFIER_HEAD(network_throughput_notifier);
static struct pm_qos_constraints network_tput_constraints = {
	.req_list = LIST_HEAD_INIT(network_tput_constraints.req_list),
	.list = PLIST_HEAD_INIT(network_tput_constraints.list),
	.target_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(network_tput_constraints.qos_lock),
	.notifiers = &network_throughput_notifier,
};
static struct pm_qos_object network_throughput_pm_qos = {
	.constraints = &network_tput_constraints,
	.name = "network_throughput",
};


static BLOCKING_NOTIFIER_HEAD(memory_bandwidth_notifier);
static struct pm_qos_constraints memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(memory_bw_constraints.list),
	.target_value = PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(memory_bw_constraints.qos_lock),
	.notifiers = &memory_bandwidth_notifier,
};
static struct pm_qos_object memory_bandwidth_pm_qos = {
	.constraints = &memory_bw_constraints,
	.name = "memory_bandwidth",
};

#if defined(CONFIG_MACH_MT6771)
static BLOCKING_NOTIFIER_HEAD(disp_freq_notifier);
static struct pm_qos_constraints disp_freq_constraints = {
	.req_list = LIST_HEAD_INIT(disp_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(disp_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(disp_freq_constraints.qos_lock),
	.notifiers = &disp_freq_notifier,
};
static struct pm_qos_object disp_freq_pm_qos = {
	.constraints = &disp_freq_constraints,
	.name = "disp_freq",
};

static BLOCKING_NOTIFIER_HEAD(mdp_freq_notifier);
static struct pm_qos_constraints mdp_freq_constraints = {
	.req_list = LIST_HEAD_INIT(mdp_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(mdp_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(mdp_freq_constraints.qos_lock),
	.notifiers = &mdp_freq_notifier,
};
static struct pm_qos_object mdp_freq_pm_qos = {
	.constraints = &mdp_freq_constraints,
	.name = "mdp_freq",
};

static BLOCKING_NOTIFIER_HEAD(vdec_freq_notifier);
static struct pm_qos_constraints vdec_freq_constraints = {
	.req_list = LIST_HEAD_INIT(vdec_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(vdec_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(vdec_freq_constraints.qos_lock),
	.notifiers = &vdec_freq_notifier,
};
static struct pm_qos_object vdec_freq_pm_qos = {
	.constraints = &vdec_freq_constraints,
	.name = "vdec_freq",
};

static BLOCKING_NOTIFIER_HEAD(venc_freq_notifier);
static struct pm_qos_constraints venc_freq_constraints = {
	.req_list = LIST_HEAD_INIT(venc_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(venc_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(venc_freq_constraints.qos_lock),
	.notifiers = &venc_freq_notifier,
};
static struct pm_qos_object venc_freq_pm_qos = {
	.constraints = &venc_freq_constraints,
	.name = "venc_freq",
};

static BLOCKING_NOTIFIER_HEAD(img_freq_notifier);
static struct pm_qos_constraints img_freq_constraints = {
	.req_list = LIST_HEAD_INIT(img_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(img_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(img_freq_constraints.qos_lock),
	.notifiers = &img_freq_notifier,
};
static struct pm_qos_object img_freq_pm_qos = {
	.constraints = &img_freq_constraints,
	.name = "img_freq",
};

static BLOCKING_NOTIFIER_HEAD(cam_freq_notifier);
static struct pm_qos_constraints cam_freq_constraints = {
	.req_list = LIST_HEAD_INIT(cam_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(cam_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(cam_freq_constraints.qos_lock),
	.notifiers = &cam_freq_notifier,
};
static struct pm_qos_object cam_freq_pm_qos = {
	.constraints = &cam_freq_constraints,
	.name = "cam_freq",
};

static BLOCKING_NOTIFIER_HEAD(dpe_freq_notifier);
static struct pm_qos_constraints dpe_freq_constraints = {
	.req_list = LIST_HEAD_INIT(dpe_freq_constraints.req_list),
	.list = PLIST_HEAD_INIT(dpe_freq_constraints.list),
	.target_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_FREQ_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(dpe_freq_constraints.qos_lock),
	.notifiers = &dpe_freq_notifier,
};
static struct pm_qos_object dpe_freq_pm_qos = {
	.constraints = &dpe_freq_constraints,
	.name = "dpe_freq",
};

static BLOCKING_NOTIFIER_HEAD(cpu_memory_bandwidth_notifier);
static struct pm_qos_constraints cpu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(cpu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(cpu_memory_bw_constraints.list),
	.target_value = PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_CPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(cpu_memory_bw_constraints.qos_lock),
	.notifiers = &cpu_memory_bandwidth_notifier,
};
static struct pm_qos_object cpu_memory_bandwidth_pm_qos = {
	.constraints = &cpu_memory_bw_constraints,
	.name = "cpu_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(gpu_memory_bandwidth_notifier);
static struct pm_qos_constraints gpu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(gpu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(gpu_memory_bw_constraints.list),
	.target_value = PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_GPU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(gpu_memory_bw_constraints.qos_lock),
	.notifiers = &gpu_memory_bandwidth_notifier,
};
static struct pm_qos_object gpu_memory_bandwidth_pm_qos = {
	.constraints = &gpu_memory_bw_constraints,
	.name = "gpu_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(mm_memory_bandwidth_notifier);
static struct pm_qos_constraints mm_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(mm_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm_memory_bw_constraints.list),
	.target_value = PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm_memory_bw_constraints.qos_lock),
	.notifiers = &mm_memory_bandwidth_notifier,
};
static struct pm_qos_object mm_memory_bandwidth_pm_qos = {
	.constraints = &mm_memory_bw_constraints,
	.name = "mm_memory_bandwidth",
};


static BLOCKING_NOTIFIER_HEAD(md_peri_memory_bandwidth_notifier);
static struct pm_qos_constraints md_peri_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(md_peri_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(md_peri_memory_bw_constraints.list),
	.target_value = PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MD_PERI_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(md_peri_memory_bw_constraints.qos_lock),
	.notifiers = &md_peri_memory_bandwidth_notifier,
};
static struct pm_qos_object md_peri_memory_bandwidth_pm_qos = {
	.constraints = &md_peri_memory_bw_constraints,
	.name = "md_peri_memory_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(other_memory_bandwidth_notifier);
static struct pm_qos_constraints other_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(other_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(other_memory_bw_constraints.list),
	.target_value = PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_OTHER_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(other_memory_bw_constraints.qos_lock),
	.notifiers = &other_memory_bandwidth_notifier,
};
static struct pm_qos_object other_memory_bandwidth_pm_qos = {
	.constraints = &other_memory_bw_constraints,
	.name = "other_memory_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(mm0_bandwidth_limiter_notifier);
static struct pm_qos_constraints mm0_bw_limiter_constraints = {
	.req_list = LIST_HEAD_INIT(mm0_bw_limiter_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm0_bw_limiter_constraints.list),
	.target_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm0_bw_limiter_constraints.qos_lock),
	.notifiers = &mm0_bandwidth_limiter_notifier,
};
static struct pm_qos_object mm0_bandwidth_limiter_pm_qos = {
	.constraints = &mm0_bw_limiter_constraints,
	.name = "mm0_bandwidth_limiter",
};

static BLOCKING_NOTIFIER_HEAD(mm1_bandwidth_limiter_notifier);
static struct pm_qos_constraints mm1_bw_limiter_constraints = {
	.req_list = LIST_HEAD_INIT(mm1_bw_limiter_constraints.req_list),
	.list = PLIST_HEAD_INIT(mm1_bw_limiter_constraints.list),
	.target_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.default_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(mm1_bw_limiter_constraints.qos_lock),
	.notifiers = &mm1_bandwidth_limiter_notifier,
};
static struct pm_qos_object mm1_bandwidth_limiter_pm_qos = {
	.constraints = &mm1_bw_limiter_constraints,
	.name = "mm1_bandwidth_limiter",
};

static BLOCKING_NOTIFIER_HEAD(emi_opp_notifier);
static struct pm_qos_constraints emi_opp_constraints = {
	.req_list = LIST_HEAD_INIT(emi_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(emi_opp_constraints.list),
	.target_value = PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_EMI_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(emi_opp_constraints.qos_lock),
	.notifiers = &emi_opp_notifier,
};
static struct pm_qos_object emi_opp_pm_qos = {
	.constraints = &emi_opp_constraints,
	.name = "emi_opp",
};

static BLOCKING_NOTIFIER_HEAD(ddr_opp_notifier);
static struct pm_qos_constraints ddr_opp_constraints = {
	.req_list = LIST_HEAD_INIT(ddr_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(ddr_opp_constraints.list),
	.target_value = PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(ddr_opp_constraints.qos_lock),
	.notifiers = &ddr_opp_notifier,
};
static struct pm_qos_object ddr_opp_pm_qos = {
	.constraints = &ddr_opp_constraints,
	.name = "ddr_opp",
};

static BLOCKING_NOTIFIER_HEAD(vcore_opp_notifier);
static struct pm_qos_constraints vcore_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_opp_constraints.list),
	.target_value = PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(vcore_opp_constraints.qos_lock),
	.notifiers = &vcore_opp_notifier,
};
static struct pm_qos_object vcore_opp_pm_qos = {
	.constraints = &vcore_opp_constraints,
	.name = "vcore_opp",
};

static BLOCKING_NOTIFIER_HEAD(vcore_dvfs_fixed_opp_notifier);
static struct pm_qos_constraints vcore_dvfs_fixed_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_dvfs_fixed_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_dvfs_fixed_opp_constraints.list),
	.target_value = PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_VCORE_DVFS_FIXED_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock =
		__MUTEX_INITIALIZER(vcore_dvfs_fixed_opp_constraints.qos_lock),
	.notifiers = &vcore_dvfs_fixed_opp_notifier,
};
static struct pm_qos_object vcore_dvfs_fixed_opp_pm_qos = {
	.constraints = &vcore_dvfs_fixed_opp_constraints,
	.name = "vcore_dvfs_fixed_opp",
};

static BLOCKING_NOTIFIER_HEAD(scp_vcore_req_notifier);
static struct pm_qos_constraints scp_vcore_req_constraints = {
	.req_list = LIST_HEAD_INIT(scp_vcore_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(scp_vcore_req_constraints.list),
	.target_value = PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.default_value = PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock = __MUTEX_INITIALIZER(scp_vcore_req_constraints.qos_lock),
	.notifiers = &scp_vcore_req_notifier,
};
static struct pm_qos_object scp_vcore_req_pm_qos = {
	.constraints = &scp_vcore_req_constraints,
	.name = "scp_vcore_req",
};

static BLOCKING_NOTIFIER_HEAD(power_model_ddr_req_notifier);
static struct pm_qos_constraints power_model_ddr_req_constraints = {
	.req_list = LIST_HEAD_INIT(power_model_ddr_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(power_model_ddr_req_constraints.list),
	.target_value = PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.default_value = PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock =
		__MUTEX_INITIALIZER(power_model_ddr_req_constraints.qos_lock),
	.notifiers = &power_model_ddr_req_notifier,
};
static struct pm_qos_object power_model_ddr_req_pm_qos = {
	.constraints = &power_model_ddr_req_constraints,
	.name = "power_model_ddr_req",
};

static BLOCKING_NOTIFIER_HEAD(power_model_vcore_req_notifier);
static struct pm_qos_constraints power_model_vcore_req_constraints = {
	.req_list = LIST_HEAD_INIT(power_model_vcore_req_constraints.req_list),
	.list = PLIST_HEAD_INIT(power_model_vcore_req_constraints.list),
	.target_value = PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.default_value = PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.qos_lock =
		__MUTEX_INITIALIZER(power_model_vcore_req_constraints.qos_lock),
	.notifiers = &power_model_vcore_req_notifier,
};
static struct pm_qos_object power_model_vcore_req_pm_qos = {
	.constraints = &power_model_vcore_req_constraints,
	.name = "power_model_vcore_req",
};

static BLOCKING_NOTIFIER_HEAD(vcore_dvfs_force_opp_notifier);
static struct pm_qos_constraints vcore_dvfs_force_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vcore_dvfs_force_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vcore_dvfs_force_opp_constraints.list),
	.target_value = PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock =
		__MUTEX_INITIALIZER(vcore_dvfs_force_opp_constraints.qos_lock),
	.notifiers = &vcore_dvfs_force_opp_notifier,
};
static struct pm_qos_object vcore_dvfs_force_opp_pm_qos = {
	.constraints = &vcore_dvfs_force_opp_constraints,
	.name = "vcore_dvfs_force_opp",
};

static BLOCKING_NOTIFIER_HEAD(isp_hrt_bandwidth_notifier);
static struct pm_qos_constraints isp_hrt_bw_constraints = {
	.req_list = LIST_HEAD_INIT(isp_hrt_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(isp_hrt_bw_constraints.list),
	.target_value = PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(isp_hrt_bw_constraints.qos_lock),
	.notifiers = &isp_hrt_bandwidth_notifier,
};
static struct pm_qos_object isp_hrt_bandwidth_pm_qos = {
	.constraints = &isp_hrt_bw_constraints,
	.name = "isp_hrt_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(apu_memory_bandwidth_notifier);
static struct pm_qos_constraints apu_memory_bw_constraints = {
	.req_list = LIST_HEAD_INIT(apu_memory_bw_constraints.req_list),
	.list = PLIST_HEAD_INIT(apu_memory_bw_constraints.list),
	.target_value = PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.qos_lock = __MUTEX_INITIALIZER(apu_memory_bw_constraints.qos_lock),
	.notifiers = &apu_memory_bandwidth_notifier,
};
static struct pm_qos_object apu_memory_bandwidth_pm_qos = {
	.constraints = &apu_memory_bw_constraints,
	.name = "apu_memory_bandwidth",
};
static BLOCKING_NOTIFIER_HEAD(vvpu_opp_notifier);
static struct pm_qos_constraints vvpu_opp_constraints = {
	.req_list = LIST_HEAD_INIT(vvpu_opp_constraints.req_list),
	.list = PLIST_HEAD_INIT(vvpu_opp_constraints.list),
	.target_value = PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.default_value = PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_VVPU_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.qos_lock = __MUTEX_INITIALIZER(vvpu_opp_constraints.qos_lock),
	.notifiers = &vvpu_opp_notifier,
};
static struct pm_qos_object vvpu_opp_pm_qos = {
	.constraints = &vvpu_opp_constraints,
	.name = "vvpu_opp",
};
#endif
static struct pm_qos_object *pm_qos_array[] = {
	&null_pm_qos,
	&cpu_dma_pm_qos,
	&network_lat_pm_qos,
	&network_throughput_pm_qos,
	&memory_bandwidth_pm_qos,
#if defined(CONFIG_MACH_MT6771)
	&cpu_memory_bandwidth_pm_qos,
	&gpu_memory_bandwidth_pm_qos,
	&mm_memory_bandwidth_pm_qos,
	&md_peri_memory_bandwidth_pm_qos,
	&other_memory_bandwidth_pm_qos,
	&mm0_bandwidth_limiter_pm_qos,
	&mm1_bandwidth_limiter_pm_qos,

	&ddr_opp_pm_qos,
	&emi_opp_pm_qos,
	&vcore_opp_pm_qos,
	&vcore_dvfs_fixed_opp_pm_qos,
	&scp_vcore_req_pm_qos,
	&power_model_ddr_req_pm_qos,
	&power_model_vcore_req_pm_qos,
	&vcore_dvfs_force_opp_pm_qos,

	&disp_freq_pm_qos,
	&mdp_freq_pm_qos,
	&vdec_freq_pm_qos,
	&venc_freq_pm_qos,
	&img_freq_pm_qos,
	&cam_freq_pm_qos,
	&dpe_freq_pm_qos,
	&isp_hrt_bandwidth_pm_qos,
	&apu_memory_bandwidth_pm_qos,
	&vvpu_opp_pm_qos,
#endif
};

static ssize_t pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos);
static ssize_t pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos);
static int pm_qos_power_open(struct inode *inode, struct file *filp);
static int pm_qos_power_release(struct inode *inode, struct file *filp);

static const struct file_operations pm_qos_power_fops = {
	.write = pm_qos_power_write,
	.read = pm_qos_power_read,
	.open = pm_qos_power_open,
	.release = pm_qos_power_release,
	.llseek = noop_llseek,
};

/* unlocked internal variant */
static inline int pm_qos_get_value(struct pm_qos_constraints *c)
{
	struct plist_node *node;
	int total_value = 0;

	if (plist_head_empty(&c->list))
		return c->no_constraint_value;

	switch (c->type) {
	case PM_QOS_MIN:
		return plist_first(&c->list)->prio;

	case PM_QOS_MAX:
		return plist_last(&c->list)->prio;

	case PM_QOS_SUM:
		plist_for_each(node, &c->list)
			total_value += node->prio;

		return total_value;

	default:
		/* runtime check for not using enum */
		BUG();
		return PM_QOS_DEFAULT_VALUE;
	}
}

s32 pm_qos_read_value(struct pm_qos_constraints *c)
{
	return c->target_value;
}

static inline void pm_qos_set_value(struct pm_qos_constraints *c, s32 value)
{
	c->target_value = value;
}

void pm_qos_trace_dbg_show_request(int pm_qos_class)
{
	struct pm_qos_constraints *c;
	struct pm_qos_request *req;
	unsigned long flags;
	struct list_head *l;

	if (pm_qos_class < PM_QOS_RESERVED
		|| pm_qos_class >= PM_QOS_NUM_CLASSES)
		return;

	c = pm_qos_array[pm_qos_class]->constraints;

	spin_lock_irqsave(&pm_qos_lock, flags);
	/* dump owner information*/
	list_for_each(l, &c->req_list) {
		req = list_entry(l, struct pm_qos_request, list_node);
		trace_pm_qos_update_request(req->pm_qos_class,
			req->node.prio);
	}
	spin_unlock_irqrestore(&pm_qos_lock, flags);
}
void pm_qos_trace_dbg_dump(int pm_qos_class)
{
	struct pm_qos_constraints *c;
	struct pm_qos_request *req;
	unsigned long flags;
	struct list_head *l;

	if (pm_qos_class < PM_QOS_RESERVED
		|| pm_qos_class >= PM_QOS_NUM_CLASSES)
		return;

	c = pm_qos_array[pm_qos_class]->constraints;

	spin_lock_irqsave(&pm_qos_lock, flags);
	/* dump owner information*/
	list_for_each(l, &c->req_list) {
		req = list_entry(l, struct pm_qos_request, list_node);
		if ((req->node).prio != c->default_value)
			pr_info("[PMQOS] = %d, %s, %d\n",
				req->pm_qos_class,
				req->owner,
				req->node.prio);
	}
	spin_unlock_irqrestore(&pm_qos_lock, flags);
}
static inline int pm_qos_get_value(struct pm_qos_constraints *c);
static int pm_qos_dbg_show_requests(struct seq_file *s, void *unused)
{
	struct pm_qos_object *qos = (struct pm_qos_object *)s->private;
	struct pm_qos_constraints *c;
	struct pm_qos_request *req;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;
	struct list_head *l;

	if (IS_ERR_OR_NULL(qos)) {
		pr_err("%s: bad qos param!\n", __func__);
		return -EINVAL;
	}
	c = qos->constraints;
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: Bad constraints on qos?\n", __func__);
		return -EINVAL;
	}

	/* Lock to ensure we have a snapshot */
	spin_lock_irqsave(&pm_qos_lock, flags);
	if (plist_head_empty(&c->list)) {
		seq_puts(s, "Empty!\n");
		goto out;
	}

	switch (c->type) {
	case PM_QOS_MIN:
		type = "Minimum";
		break;
	case PM_QOS_MAX:
		type = "Maximum";
		break;
	case PM_QOS_SUM:
		type = "Sum";
		break;
	default:
		type = "Unknown";
	}

	plist_for_each_entry(req, &c->list, node) {
		char *state = "Default";

		if ((req->node).prio != c->default_value) {
			active_reqs++;
			state = "Active";
		}
		tot_reqs++;
		seq_printf(s, "%d: %d: %s\n", tot_reqs,
			   (req->node).prio, state);
	}

	seq_printf(s, "Type=%s, Value=%d, Requests: active=%d / total=%d\n",
		   type, pm_qos_get_value(c), active_reqs, tot_reqs);

	list_for_each(l, &c->req_list) {
		req = list_entry(l, struct pm_qos_request, list_node);

		seq_printf(s, "%s: %d\n", req->owner, req->node.prio);
	}
out:
	spin_unlock_irqrestore(&pm_qos_lock, flags);
	return 0;
}

static int pm_qos_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pm_qos_dbg_show_requests,
			   inode->i_private);
}

static const struct file_operations pm_qos_debug_fops = {
	.open           = pm_qos_dbg_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static inline void pm_qos_set_value_for_cpus(struct pm_qos_constraints *c,
					     bool dev_req)
{
	struct pm_qos_request *req = NULL;
	int cpu;
	s32 qos_val[NR_CPUS] = { [0 ... (NR_CPUS - 1)] = c->default_value };

	/*
	 * pm_qos_set_value_for_cpus expects all c->list elements to be of type
	 * pm_qos_request, however requests from device will contain elements
	 * of type dev_pm_qos_request.
	 * pm_qos_constraints.target_per_cpu can be accessed only for
	 * constraints associated with one of the pm_qos_class and present in
	 * pm_qos_array. Device requests are not associated with any of
	 * pm_qos_class, therefore their target_per_cpu cannot be accessed. We
	 * can safely skip updating target_per_cpu for device requests.
	 */
	if (dev_req)
		return;

	plist_for_each_entry(req, &c->list, node) {
		for_each_cpu(cpu, &req->cpus_affine) {
			switch (c->type) {
			case PM_QOS_MIN:
				if (qos_val[cpu] > req->node.prio)
					qos_val[cpu] = req->node.prio;
				break;
			case PM_QOS_MAX:
				if (req->node.prio > qos_val[cpu])
					qos_val[cpu] = req->node.prio;
				break;
			default:
				break;
			}
		}
	}

	for_each_possible_cpu(cpu)
		c->target_per_cpu[cpu] = qos_val[cpu];
}

/**
 * pm_qos_update_target - manages the constraints list and calls the notifiers
 *  if needed
 * @c: constraints data struct
 * @node: request to add to the list, to update or to remove
 * @action: action to take on the constraints list
 * @value: value of the request to add or update
 *
 * This function returns 1 if the aggregated constraint value has changed, 0
 *  otherwise.
 */
int pm_qos_update_target(struct pm_qos_constraints *c, struct plist_node *node,
			 enum pm_qos_req_action action, int value, bool dev_req)
{
	unsigned long flags;
	int prev_value, curr_value, new_value;
	int ret;

	mutex_lock(&c->qos_lock);
	spin_lock_irqsave(&pm_qos_lock, flags);
	prev_value = pm_qos_get_value(c);
	if (value == PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;
	else
		new_value = value;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		plist_del(node, &c->list);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		plist_del(node, &c->list);
		/* fall through */
	case PM_QOS_ADD_REQ:
		plist_node_init(node, new_value);
		plist_add(node, &c->list);
		break;
	default:
		/* no action */
		;
	}
	curr_value = pm_qos_get_value(c);
	pm_qos_set_value(c, curr_value);
#if !defined(CONFIG_MACH_MT6771)
	pm_qos_set_value_for_cpus(c, dev_req);
#endif
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	trace_pm_qos_update_target(action, prev_value, curr_value);
	if (prev_value != curr_value) {
		ret = 1;
		if (c->notifiers)
			blocking_notifier_call_chain(c->notifiers,
						     (unsigned long)curr_value,
						     NULL);
	} else {
		ret = 0;
	}

	mutex_unlock(&c->qos_lock);
	return ret;
}

/**
 * pm_qos_flags_remove_req - Remove device PM QoS flags request.
 * @pqf: Device PM QoS flags set to remove the request from.
 * @req: Request to remove from the set.
 */
static void pm_qos_flags_remove_req(struct pm_qos_flags *pqf,
				    struct pm_qos_flags_request *req)
{
	s32 val = 0;

	list_del(&req->node);
	list_for_each_entry(req, &pqf->list, node)
		val |= req->flags;

	pqf->effective_flags = val;
}

/**
 * pm_qos_update_flags - Update a set of PM QoS flags.
 * @pqf: Set of flags to update.
 * @req: Request to add to the set, to modify, or to remove from the set.
 * @action: Action to take on the set.
 * @val: Value of the request to add or modify.
 *
 * Update the given set of PM QoS flags and call notifiers if the aggregate
 * value has changed.  Returns 1 if the aggregate constraint value has changed,
 * 0 otherwise.
 */
bool pm_qos_update_flags(struct pm_qos_flags *pqf,
			 struct pm_qos_flags_request *req,
			 enum pm_qos_req_action action, s32 val)
{
	unsigned long irqflags;
	s32 prev_value, curr_value;

	spin_lock_irqsave(&pm_qos_lock, irqflags);

	prev_value = list_empty(&pqf->list) ? 0 : pqf->effective_flags;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		pm_qos_flags_remove_req(pqf, req);
		break;
	case PM_QOS_UPDATE_REQ:
		pm_qos_flags_remove_req(pqf, req);
		/* fall through */
	case PM_QOS_ADD_REQ:
		req->flags = val;
		INIT_LIST_HEAD(&req->node);
		list_add_tail(&req->node, &pqf->list);
		pqf->effective_flags |= val;
		break;
	default:
		/* no action */
		;
	}

	curr_value = list_empty(&pqf->list) ? 0 : pqf->effective_flags;

	spin_unlock_irqrestore(&pm_qos_lock, irqflags);

	trace_pm_qos_update_flags(action, prev_value, curr_value);
	return prev_value != curr_value;
}

/**
 * pm_qos_request - returns current system wide qos expectation
 * @pm_qos_class: identification of which qos value is requested
 *
 * This function returns the current target value.
 */
int pm_qos_request(int pm_qos_class)
{
	return pm_qos_read_value(pm_qos_array[pm_qos_class]->constraints);
}
EXPORT_SYMBOL_GPL(pm_qos_request);

int pm_qos_request_active(struct pm_qos_request *req)
{
	return req->pm_qos_class != 0;
}
EXPORT_SYMBOL_GPL(pm_qos_request_active);

static void __pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value)
{
	trace_pm_qos_update_request(req->pm_qos_class, new_value);

	if (new_value != req->node.prio)
		pm_qos_update_target(
			pm_qos_array[req->pm_qos_class]->constraints,
			&req->node, PM_QOS_UPDATE_REQ, new_value, false);
}

/**
 * pm_qos_work_fn - the timeout handler of pm_qos_update_request_timeout
 * @work: work struct for the delayed work (timeout)
 *
 * This cancels the timeout request by falling back to the default at timeout.
 */
static void pm_qos_work_fn(struct work_struct *work)
{
	struct pm_qos_request *req = container_of(to_delayed_work(work),
						  struct pm_qos_request,
						  work);

	__pm_qos_update_request(req, PM_QOS_DEFAULT_VALUE);
}

#ifdef CONFIG_SMP
static void pm_qos_irq_release(struct kref *ref)
{
	unsigned long flags;
	struct irq_affinity_notify *notify = container_of(ref,
					struct irq_affinity_notify, kref);
	struct pm_qos_request *req = container_of(notify,
					struct pm_qos_request, irq_notify);
	struct pm_qos_constraints *c =
				pm_qos_array[req->pm_qos_class]->constraints;

	spin_lock_irqsave(&pm_qos_lock, flags);
	cpumask_setall(&req->cpus_affine);
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	pm_qos_update_target(c, &req->node, PM_QOS_UPDATE_REQ,
			c->default_value, false);
}

static void pm_qos_irq_notify(struct irq_affinity_notify *notify,
		const cpumask_t *mask)
{
	unsigned long flags;
	struct pm_qos_request *req = container_of(notify,
					struct pm_qos_request, irq_notify);
	struct pm_qos_constraints *c =
				pm_qos_array[req->pm_qos_class]->constraints;

	spin_lock_irqsave(&pm_qos_lock, flags);
	cpumask_copy(&req->cpus_affine, mask);
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	pm_qos_update_target(c, &req->node, PM_QOS_UPDATE_REQ, req->node.prio,
			false);
}
#endif

/**
 * pm_qos_add_request - inserts new qos request into the list
 * @req: pointer to a preallocated handle
 * @pm_qos_class: identifies which list of qos request to use
 * @value: defines the qos request
 *
 * This function inserts a new entry in the pm_qos_class list of requested qos
 * performance characteristics.  It recomputes the aggregate QoS expectations
 * for the pm_qos_class of parameters and initializes the pm_qos_request
 * handle.  Caller needs to save this handle for later use in updates and
 * removal.
 */

void pm_qos_add_request(struct pm_qos_request *req,
			int pm_qos_class, s32 value)
{
	char owner[20];
	if (!req) /*guard against callers passing in null */
		return;

	if (pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_add_request() called for already added request\n");
		return;
	}
	strncpy(req->owner, owner, sizeof(req->owner) - 1);
	req->pm_qos_class = pm_qos_class;
	INIT_DELAYED_WORK(&req->work, pm_qos_work_fn);
	trace_pm_qos_add_request(pm_qos_class, value);
	pm_qos_update_target(pm_qos_array[pm_qos_class]->constraints,
			     &req->node, PM_QOS_ADD_REQ, value, false);

#ifdef CONFIG_SMP
	if (req->type == PM_QOS_REQ_AFFINE_IRQ &&
			irq_can_set_affinity(req->irq)) {
		int ret = 0;

		ret = irq_set_affinity_notifier(req->irq,
					&req->irq_notify);
		if (ret) {
			WARN(1, "IRQ affinity notify set failed\n");
			req->type = PM_QOS_REQ_ALL_CORES;
			cpumask_setall(&req->cpus_affine);
			pm_qos_update_target(
				pm_qos_array[pm_qos_class]->constraints,
				&req->node, PM_QOS_UPDATE_REQ, value, false);
		}
	}
#endif
}
EXPORT_SYMBOL_GPL(pm_qos_add_request);

/**
 * pm_qos_update_request - modifies an existing qos request
 * @req : handle to list element holding a pm_qos request to use
 * @value: defines the qos request
 *
 * Updates an existing qos request for the pm_qos_class of parameters along
 * with updating the target pm_qos_class value.
 *
 * Attempts are made to make this code callable on hot code paths.
 */
void pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (!pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_update_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);
	__pm_qos_update_request(req, new_value);
}
EXPORT_SYMBOL_GPL(pm_qos_update_request);

/**
 * pm_qos_update_request_timeout - modifies an existing qos request temporarily.
 * @req : handle to list element holding a pm_qos request to use
 * @new_value: defines the temporal qos request
 * @timeout_us: the effective duration of this qos request in usecs.
 *
 * After timeout_us, this qos request is cancelled automatically.
 */
void pm_qos_update_request_timeout(struct pm_qos_request *req, s32 new_value,
				   unsigned long timeout_us)
{
	if (!req)
		return;
	if (WARN(!pm_qos_request_active(req),
		 "%s called for unknown object.", __func__))
		return;

	cancel_delayed_work_sync(&req->work);

	trace_pm_qos_update_request_timeout(req->pm_qos_class,
					    new_value, timeout_us);
	if (new_value != req->node.prio)
		pm_qos_update_target(
			pm_qos_array[req->pm_qos_class]->constraints,
			&req->node, PM_QOS_UPDATE_REQ, new_value, false);

	schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
}

/**
 * pm_qos_remove_request - modifies an existing qos request
 * @req: handle to request list element
 *
 * Will remove pm qos request from the list of constraints and
 * recompute the current target value for the pm_qos_class.  Call this
 * on slow code paths.
 */
void pm_qos_remove_request(struct pm_qos_request *req)
{
	if (!req) /*guard against callers passing in null */
		return;
		/* silent return to keep pcm code cleaner */

	if (!pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_remove_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);

	trace_pm_qos_remove_request(req->pm_qos_class, PM_QOS_DEFAULT_VALUE);
	pm_qos_update_target(pm_qos_array[req->pm_qos_class]->constraints,
			     &req->node, PM_QOS_REMOVE_REQ,
			     PM_QOS_DEFAULT_VALUE, false);
	memset(req, 0, sizeof(*req));
}
EXPORT_SYMBOL_GPL(pm_qos_remove_request);

/**
 * pm_qos_add_notifier - sets notification entry for changes to target value
 * @pm_qos_class: identifies which qos target changes should be notified.
 * @notifier: notifier block managed by caller.
 *
 * will register the notifier into a notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_register(
			pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_qos_add_notifier);

/**
 * pm_qos_remove_notifier - deletes notification entry from chain.
 * @pm_qos_class: identifies which qos target changes are notified.
 * @notifier: notifier block to be removed.
 *
 * will remove the notifier from the notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_unregister(
			pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_qos_remove_notifier);

/* User space interface to PM QoS classes via misc devices */
static int register_pm_qos_misc(struct pm_qos_object *qos, struct dentry *d)
{
	qos->pm_qos_power_miscdev.minor = MISC_DYNAMIC_MINOR;
	qos->pm_qos_power_miscdev.name = qos->name;
	qos->pm_qos_power_miscdev.fops = &pm_qos_power_fops;

	if (d) {
		(void)debugfs_create_file(qos->name, S_IRUGO, d,
					  (void *)qos, &pm_qos_debug_fops);
	}

	return misc_register(&qos->pm_qos_power_miscdev);
}

static int find_pm_qos_object_by_minor(int minor)
{
	int pm_qos_class;

	for (pm_qos_class = PM_QOS_CPU_DMA_LATENCY;
		pm_qos_class < PM_QOS_NUM_CLASSES; pm_qos_class++) {
		if (minor ==
			pm_qos_array[pm_qos_class]->pm_qos_power_miscdev.minor)
			return pm_qos_class;
	}
	return -1;
}

static int pm_qos_power_open(struct inode *inode, struct file *filp)
{
	long pm_qos_class;

	pm_qos_class = find_pm_qos_object_by_minor(iminor(inode));
	if (pm_qos_class >= PM_QOS_CPU_DMA_LATENCY) {
		struct pm_qos_request *req = kzalloc(sizeof(*req), GFP_KERNEL);
		if (!req)
			return -ENOMEM;

		pm_qos_add_request(req, pm_qos_class, PM_QOS_DEFAULT_VALUE);
		filp->private_data = req;

		return 0;
	}
	return -EPERM;
}

static int pm_qos_power_release(struct inode *inode, struct file *filp)
{
	struct pm_qos_request *req;

	req = filp->private_data;
	pm_qos_remove_request(req);
	kfree(req);

	return 0;
}


static ssize_t pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	unsigned long flags;
	struct pm_qos_request *req = filp->private_data;

	if (!req)
		return -EINVAL;
	if (!pm_qos_request_active(req))
		return -EINVAL;

	spin_lock_irqsave(&pm_qos_lock, flags);
	value = pm_qos_get_value(pm_qos_array[req->pm_qos_class]->constraints);
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static ssize_t pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct pm_qos_request *req;

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	req = filp->private_data;
	pm_qos_update_request(req, value);

	return count;
}


static int __init pm_qos_power_init(void)
{
	int ret = 0;
	int i;
	struct dentry *d;

#if !defined(CONFIG_MACH_MT6771)
	BUILD_BUG_ON(ARRAY_SIZE(pm_qos_array) != PM_QOS_NUM_CLASSES);
#endif

	d = debugfs_create_dir("pm_qos", NULL);
	if (IS_ERR_OR_NULL(d))
		d = NULL;

	for (i = PM_QOS_CPU_DMA_LATENCY; i < PM_QOS_NUM_CLASSES; i++) {
		ret = register_pm_qos_misc(pm_qos_array[i], d);
		if (ret < 0) {
			pr_err("%s: %s setup failed\n",
			       __func__, pm_qos_array[i]->name);
			return ret;
		}
	}

	return ret;
}

late_initcall(pm_qos_power_init);
