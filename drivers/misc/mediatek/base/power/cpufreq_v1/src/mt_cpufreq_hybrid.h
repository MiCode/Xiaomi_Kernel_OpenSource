#ifndef _MT_CPUFREQ_HYBRID_
#define _MT_CPUFREQ_HYBRID_

/* #ifdef CONFIG_MTK_TINYSYS_SPM2_SUPPORT
#define CONFIG_HYBRID_CPU_DVFS 1
#endif */

#define SRAM_BASE_ADDR 0x00100000

struct cpu_dvfs_log {
	unsigned int time_stamp_log:32;
	unsigned int time_stamp_2_log:32;

	unsigned int vproc_ll_log:7;
	unsigned int opp_ll_log:4;
	unsigned int wfi_ll_log:15;
	unsigned int vsram_ll_log:6;

	unsigned int vproc_l_log:7;
	unsigned int opp_l_log:4;
	unsigned int wfi_l_log:15;
	unsigned int vsram_l_log:6;

	unsigned int vproc_b_log:7;
	unsigned int opp_b_log:4;
	unsigned int wfi_b_log:15;
	unsigned int vsram_b_log:6;

	unsigned int vproc_cci_log:7;
	unsigned int opp_cci_log:4;
	unsigned int wfi_cci_log:15;
	unsigned int vsram_cci_log:6;
};

enum pause_src {
	PAUSE_INIT,
	PAUSE_I2CDRV,
	PAUSE_IDLE,
	PAUSE_SUSPEND,
	PAUSE_HWGOV,

	NUM_PAUSE_SRC
};

/* Parameter Enum */
enum cpu_dvfs_ipi_type {
	IPI_DVFS_INIT,
	IPI_SET_DVFS,
	IPI_SET_MIN_MAX,
	IPI_SET_CLUSTER_ON_OFF,
	IPI_SET_VOLT,
	IPI_GET_VOLT,
	IPI_GET_FREQ,
	IPI_PAUSE_DVFS,

	NR_DVFS_IPI,
};

typedef struct cdvfs_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[3];
		} set_fv;
	} u;
} cdvfs_data_t;

int cpuhvfs_module_init(void);
int cpuhvfs_set_mix_max(int cluster_id, int base, int limit);
int cpuhvfs_set_cluster_on_off(int cluster_id, int state);
int cpuhvfs_get_freq(int pll_id);
int cpuhvfs_set_freq(int cluster_id, unsigned int freq);
int cpuhvfs_set_volt(int cluster_id, unsigned int volt);
int cpuhvfs_get_volt(int buck_id);
int dvfs_to_spm2_command(u32 cmd, struct cdvfs_data *cdvfs_d);

#ifdef CONFIG_HYBRID_CPU_DVFS
extern int cpuhvfs_pause_dvfsp_running(enum pause_src src);
extern void cpuhvfs_unpause_dvfsp_to_run(enum pause_src src);
#else
static inline int cpuhvfs_pause_dvfsp_running(enum pause_src src)	{ return 0; }
static inline void cpuhvfs_unpause_dvfsp_to_run(enum pause_src src)	{}
#endif

#endif
