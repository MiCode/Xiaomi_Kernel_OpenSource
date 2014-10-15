#ifndef _MSM_KGSL_H
#define _MSM_KGSL_H

#include <uapi/linux/msm_kgsl.h>

/* Clock flags to show which clocks should be controled by a given platform */
#define KGSL_CLK_SRC	0x00000001
#define KGSL_CLK_CORE	0x00000002
#define KGSL_CLK_IFACE	0x00000004
#define KGSL_CLK_MEM	0x00000008
#define KGSL_CLK_MEM_IFACE 0x00000010
#define KGSL_CLK_AXI	0x00000020
#define KGSL_CLK_ALT_MEM_IFACE 0x00000040
#define KGSL_CLK_RBBMTIMER	0x00000080
#define KGSL_CLK_GFX_GTCU   0x00000100
#define KGSL_CLK_GFX_GTBU   0x00000200

#define KGSL_MAX_PWRLEVELS 10

#define KGSL_3D0_REG_MEMORY	"kgsl_3d0_reg_memory"
#define KGSL_3D0_SHADER_MEMORY	"kgsl_3d0_shader_memory"
#define KGSL_3D0_IRQ		"kgsl_3d0_irq"

/**
 * struct kgsl_pwrlevel - Struct holding different pwrlevel info obtained from
 * from dtsi file
 * @gpu_freq:		GPU frequency vote in Hz
 * @bus_freq:		Bus bandwidth vote index
 * @bus_min:		Min bus index @gpu_freq
 * @bus_max:		Max bus index @gpu_freq
 */
struct kgsl_pwrlevel {
	unsigned int gpu_freq;
	unsigned int bus_freq;
	unsigned int bus_min;
	unsigned int bus_max;
};

/**
 * struct kgsl_device_platform_data - Struct holding all the device info
 * obtained from the dtsi file
 * @pwrlevel:		Array of struct holding pwrlevel information
 * @init_level:		Pwrlevel device is initialized with
 * @num_levels:		Number of pwrlevels for the specific device
 * @idle_timeout:	Timeout for GPU to turn its resources off
 * @strtstp_sleepwake:  Flag to decide b/w SLEEP and SLUMBER
 * @bus_control:	Flag if independent bus voting is supported
 * @clk_map:		Clocks map per platform
 * @bus_scale_table:	Bus table with different b/w votes
 * @iommu_data:		Struct holding iommu context data
 * @iommu_count:	Number of IOMMU units for the GPU
 * @csdev:		Pointer to the coresight device for this device
 * @coresight_pdata:	Coresight configuration for specific device
 * @chipid:		Chip ID for the device's GPU
 * @pm_qos_active_latency:	GPU PM QoS latency request for active state
 * @pm_qos_wakeup_latency:	GPU PM QoS latency request during wakeup
 */
struct kgsl_device_platform_data {
	struct kgsl_pwrlevel pwrlevel[KGSL_MAX_PWRLEVELS];
	int init_level;
	int num_levels;
	unsigned int idle_timeout;
	bool strtstp_sleepwake;
	bool bus_control;
	unsigned int clk_map;
	unsigned int step_mul;
	struct msm_bus_scale_pdata *bus_scale_table;
	struct kgsl_device_iommu_data *iommu_data;
	int iommu_count;
	struct coresight_device *csdev;
	struct coresight_platform_data *coresight_pdata;
	unsigned int chipid;
	unsigned int pm_qos_active_latency;
	unsigned int pm_qos_wakeup_latency;
};

#ifdef CONFIG_MSM_KGSL_DRM
int kgsl_gem_obj_addr(int drm_fd, int handle, unsigned long *start,
			unsigned long *len);
#else
#define kgsl_gem_obj_addr(...) 0
#endif
#endif /* _MSM_KGSL_H */
