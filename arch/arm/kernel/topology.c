/*
 * arch/arm/kernel/topology.c
 *
 * Copyright (C) 2011 Linaro Limited.
 * Written by: Vincent Guittot
 *
 * based on arch/sh/kernel/topology.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

/*
 * cpu power scale management
 */

/*
 * cpu power table
 * This per cpu data structure describes the relative capacity of each core.
 * On a heteregenous system, cores don't have the same computation capacity
 * and we reflect that difference in the cpu_power field so the scheduler can
 * take this difference into account during load balance. A per cpu structure
 * is preferred because each CPU updates its own cpu_power field during the
 * load balance except for idle cores. One idle core is selected to run the
 * rebalance_domains for all idle cores and the cpu_power can be updated
 * during this sequence.
 */

/* when CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY is in use, a new measure of
 * compute capacity is available. This is limited to a maximum of 1024 and
 * scaled between 0 and 1023 according to frequency.
 * Cores with different base CPU powers are scaled in line with this.
 * CPU capacity for each core represents a comparable ratio to maximum
 * achievable core compute capacity for a core in this system.
 *
 * e.g.1 If all cores in the system have a base CPU power of 1024 according to
 * efficiency calculations and are DVFS scalable between 500MHz and 1GHz, the
 * cores currently at 1GHz will have CPU power of 1024 whilst the cores
 * currently at 500MHz will have CPU power of 512.
 *
 * e.g.2
 * If core 0 has a base CPU power of 2048 and runs at 500MHz & 1GHz whilst
 * core 1 has a base CPU power of 1024 and runs at 100MHz and 200MHz, then
 * the following possibilities are available:
 *
 * cpu power\| 1GHz:100Mhz | 1GHz : 200MHz | 500MHz:100MHz | 500MHz:200MHz |
 * ----------|-------------|---------------|---------------|---------------|
 *    core 0 |    1024     |     1024      |     512       |     512       |
 *    core 1 |     256     |      512      |     256       |     512       |
 *
 * This information may be useful to the scheduler when load balancing,
 * so that the compute capacity of the core a task ran on can be baked into
 * task load histories.
 */
static DEFINE_PER_CPU(unsigned long, cpu_scale);
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
static DEFINE_PER_CPU(unsigned long, base_cpu_capacity);
static DEFINE_PER_CPU(unsigned long, invariant_cpu_capacity);
static DEFINE_PER_CPU(unsigned long, prescaled_cpu_capacity);
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

static int frequency_invariant_power_enabled = 1;

/* >0=1, <=0=0 */
void arch_set_invariant_power_enabled(int val)
{
	if(val>0)
		frequency_invariant_power_enabled = 1;
	else
		frequency_invariant_power_enabled = 0;
}

int arch_get_invariant_power_enabled(void)
{
	return frequency_invariant_power_enabled;
}

unsigned long arch_scale_freq_power(struct sched_domain *sd, int cpu)
{
	return per_cpu(cpu_scale, cpu);
}

#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
unsigned long arch_get_cpu_capacity(int cpu)
{
	return per_cpu(invariant_cpu_capacity, cpu);
}
unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(base_cpu_capacity, cpu);
}
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

static void set_power_scale(unsigned int cpu, unsigned long power)
{
	per_cpu(cpu_scale, cpu) = power;
}

#ifdef CONFIG_OF
struct cpu_efficiency {
	const char *compatible;
	unsigned long efficiency;
};

/*
 * Table of relative efficiency of each processors
 * The efficiency value must fit in 20bit and the final
 * cpu_scale value must be in the range
 *   0 < cpu_scale < 3*SCHED_POWER_SCALE/2
 * in order to return at most 1 when DIV_ROUND_CLOSEST
 * is used to compute the capacity of a CPU.
 * Processors that are not defined in the table,
 * use the default SCHED_POWER_SCALE value for cpu_scale.
 */
struct cpu_efficiency table_efficiency[] = {
	{"arm,cortex-a15", 3891},
	{"arm,cortex-a17", 3276},
	{"arm,cortex-a12", 3276},
	{"arm,cortex-a53", 2520},
	{"arm,cortex-a7",  2048},
	{NULL, },
};

struct cpu_capacity {
	unsigned long hwid;
	unsigned long capacity;
};

struct cpu_capacity *cpu_capacity;

unsigned long middle_capacity = 1;
/*
 * Iterate all CPUs' descriptor in DT and compute the efficiency
 * (as per table_efficiency). Also calculate a middle efficiency
 * as close as possible to  (max{eff_i} - min{eff_i}) / 2
 * This is later used to scale the cpu_power field such that an
 * 'average' CPU is of middle power. Also see the comments near
 * table_efficiency[] and update_cpu_power().
 */
static void __init parse_dt_topology(void)
{
	struct cpu_efficiency *cpu_eff;
	struct device_node *cn = NULL;
	unsigned long min_capacity = (unsigned long)(-1);
	unsigned long max_capacity = 0;
	unsigned long capacity = 0;
	int alloc_size, cpu = 0;

	alloc_size = nr_cpu_ids * sizeof(struct cpu_capacity);
	cpu_capacity = kzalloc(alloc_size, GFP_NOWAIT);

	while ((cn = of_find_node_by_type(cn, "cpu"))) {
		const u32 *rate, *reg;
		int len;

		if (cpu >= num_possible_cpus())
			break;

		for (cpu_eff = table_efficiency; cpu_eff->compatible; cpu_eff++)
			if (of_device_is_compatible(cn, cpu_eff->compatible))
				break;

		if (cpu_eff->compatible == NULL)
			continue;

		rate = of_get_property(cn, "clock-frequency", &len);
		if (!rate || len != 4) {
			pr_err("%s missing clock-frequency property\n",
				   cn->full_name);
			continue;
		}

		reg = of_get_property(cn, "reg", &len);
		if (!reg || len != 4) {
			pr_err("%s missing reg property\n", cn->full_name);
			continue;
		}

		capacity = ((be32_to_cpup(rate)) >> 20) * cpu_eff->efficiency;

		/* Save min capacity of the system */
		if (capacity < min_capacity)
			min_capacity = capacity;

		/* Save max capacity of the system */
		if (capacity > max_capacity)
			max_capacity = capacity;

		cpu_capacity[cpu].capacity = capacity;
		cpu_capacity[cpu++].hwid = be32_to_cpup(reg);
	}

	if (cpu < num_possible_cpus())
		cpu_capacity[cpu].hwid = (unsigned long)(-1);

	/* If min and max capacities are equals, we bypass the update of the
	 * cpu_scale because all CPUs have the same capacity. Otherwise, we
	 * compute a middle_capacity factor that will ensure that the capacity
	 * of an 'average' CPU of the system will be as close as possible to
	 * SCHED_POWER_SCALE, which is the default value, but with the
	 * constraint explained near table_efficiency[].
	 */
	if (min_capacity == max_capacity)
		cpu_capacity[0].hwid = (unsigned long)(-1);
	else if (4*max_capacity < (3*(max_capacity + min_capacity)))
		middle_capacity = (min_capacity + max_capacity)
			>> (SCHED_POWER_SHIFT+1);
	else
		middle_capacity = ((max_capacity / 3)
						   >> (SCHED_POWER_SHIFT-1)) + 1;

}

/*
 * Look for a customed capacity of a CPU in the cpu_capacity table during the
 * boot. The update of all CPUs is in O(n^2) for heteregeneous system but the
 * function returns directly for SMP system.
 */
void update_cpu_power(unsigned int cpu, unsigned long hwid)
{
	unsigned int idx = 0;

	/* look for the cpu's hwid in the cpu capacity table */
	for (idx = 0; idx < num_possible_cpus(); idx++) {
		if (cpu_capacity[idx].hwid == hwid)
			break;

		if (cpu_capacity[idx].hwid == -1)
			return;
	}

	if (idx == num_possible_cpus())
		return;

	set_power_scale(cpu, cpu_capacity[idx].capacity / middle_capacity);

	printk(KERN_INFO "CPU%u: update cpu_power %lu\n",
		   cpu, arch_scale_freq_power(NULL, cpu));
}

#else
static inline void parse_dt_topology(void) {}
static inline void update_cpu_power(unsigned int cpuid, unsigned int mpidr) {}
#endif

/*
 * cpu topology table
 */
struct cputopo_arm cpu_topology[NR_CPUS];
EXPORT_SYMBOL_GPL(cpu_topology);

#if defined (CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK) || defined (CONFIG_HMP_PACK_SMALL_TASK)
int arch_sd_share_power_line(void)
{
	return 0*SD_SHARE_POWERLINE;
}
#endif /* CONFIG_MTK_SCHED_CMP_PACK_SMALL_TASK || CONFIG_HMP_PACK_SMALL_TASK  */

const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return &cpu_topology[cpu].core_sibling;
}

void update_siblings_masks(unsigned int cpuid)
{
	struct cputopo_arm *cpu_topo, *cpuid_topo = &cpu_topology[cpuid];
	int cpu;

	/* update core and thread sibling masks */
	for_each_possible_cpu(cpu) {
		cpu_topo = &cpu_topology[cpu];

		if (cpuid_topo->socket_id != cpu_topo->socket_id)
			continue;

		cpumask_set_cpu(cpuid, &cpu_topo->core_sibling);
		if (cpu != cpuid)
			cpumask_set_cpu(cpu, &cpuid_topo->core_sibling);

		if (cpuid_topo->core_id != cpu_topo->core_id)
			continue;

		cpumask_set_cpu(cpuid, &cpu_topo->thread_sibling);
		if (cpu != cpuid)
			cpumask_set_cpu(cpu, &cpuid_topo->thread_sibling);
	}
	smp_wmb();
}

#ifdef CONFIG_MTK_CPU_TOPOLOGY

enum {
	ARCH_UNKNOWN = 0,
	ARCH_SINGLE_CLUSTER,
	ARCH_MULTI_CLUSTER,
	ARCH_BIG_LITTLE,
};

struct cpu_cluster {
	int cluster_id;
	cpumask_t siblings;
	void *next;
};

struct cpu_compatible {
	const char *name;
	const unsigned int cpuidr;
	struct cpu_cluster *cluster;
	int clscnt;
};

struct cpu_arch_info {
	struct cpu_compatible *compat_big;
	struct cpu_compatible *compat_ltt;
	bool arch_ready;
	int arch_type;
	int nr_clusters;
};

/* NOTE: absolute decending ordered by cpu capacity */
struct cpu_compatible cpu_compat_table[] = {
	{ "arm,cortex-a15", ARM_CPU_PART_CORTEX_A15, NULL, 0 },
	{ "arm,cortex-a17", ARM_CPU_PART_CORTEX_A17, NULL, 0 },
	{ "arm,cortex-a12", ARM_CPU_PART_CORTEX_A12, NULL, 0 },
	{ "arm,cortex-a53", ARM_CPU_PART_CORTEX_A53, NULL, 0 },
	{ "arm,cortex-a9",  ARM_CPU_PART_CORTEX_A9, NULL, 0 },
	{ "arm,cortex-a7",  ARM_CPU_PART_CORTEX_A7, NULL, 0 },
	{ NULL, 0, NULL, 0 }
};

static struct cpu_compatible* compat_cputopo[NR_CPUS];

static struct cpu_arch_info default_cpu_arch = {
	NULL,
	NULL,
	0,
	ARCH_UNKNOWN,
	0,
};
static struct cpu_arch_info *glb_cpu_arch = &default_cpu_arch;

static int __arch_type(void)
{
	int i, num_compat = 0;

	if (!glb_cpu_arch->arch_ready)
		return ARCH_UNKNOWN;

	// return the cached setting if query more than once.
	if (glb_cpu_arch->arch_type != ARCH_UNKNOWN)
		return glb_cpu_arch->arch_type;

	for (i = 0; i < ARRAY_SIZE(cpu_compat_table); i++) {
		struct cpu_compatible *mc = &cpu_compat_table[i];
		if (mc->clscnt != 0)
			num_compat++;
	}

	if (num_compat > 1)
		glb_cpu_arch->arch_type = ARCH_BIG_LITTLE;
	else if (glb_cpu_arch->nr_clusters > 1)
		glb_cpu_arch->arch_type = ARCH_MULTI_CLUSTER;
	else if (num_compat == 1 && glb_cpu_arch->nr_clusters == 1)
		glb_cpu_arch->arch_type = ARCH_SINGLE_CLUSTER;

	return glb_cpu_arch->arch_type;
}

static DEFINE_SPINLOCK(__cpu_cluster_lock);
static void __setup_cpu_cluster(const unsigned int cpu,
								struct cpu_compatible * const cpt,
								const u32 mpidr)
{
	struct cpu_cluster *prev_cls, *cls;
	u32 cls_id = -1;

	if (mpidr & MPIDR_MT_BITMASK)
		cls_id = MPIDR_AFFINITY_LEVEL(mpidr, 2);
	else
		cls_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	spin_lock(&__cpu_cluster_lock);

	cls = cpt->cluster;
	prev_cls = cls;
	while (cls) {
		if (cls->cluster_id == cls_id)
			break;
		prev_cls = cls;
		cls = (struct cpu_cluster *)cls->next;
	}

	if (!cls) {
		cls = kzalloc(sizeof(struct cpu_cluster), GFP_ATOMIC);
		BUG_ON(!cls);
		cls->cluster_id = cls_id;
		cpt->clscnt++;
		glb_cpu_arch->nr_clusters++;
		/* link it */
		if (!cpt->cluster)
			cpt->cluster = cls;
		else
			prev_cls->next = cls;
	}
	BUG_ON(cls->cluster_id != cls_id);

	cpumask_set_cpu(cpu, &cls->siblings);
	smp_wmb();

	spin_unlock(&__cpu_cluster_lock);
}

static void setup_cputopo(const unsigned int cpu,
						  struct cpu_compatible * const cpt,
						  const u32 mpidr)

{
	if (compat_cputopo[cpu])
		return;

	compat_cputopo[cpu] = cpt;

	if (!glb_cpu_arch->compat_big || glb_cpu_arch->compat_big > cpt)
		glb_cpu_arch->compat_big = cpt;

	if (!glb_cpu_arch->compat_ltt || glb_cpu_arch->compat_ltt < cpt)
		glb_cpu_arch->compat_ltt = cpt;

	__setup_cpu_cluster(cpu, cpt, mpidr);
}

static void setup_cputopo_def(const unsigned int cpu)
{
	struct cpu_compatible *idx = NULL;
	unsigned int cpuidr = 0, mpidr;

	BUG_ON(cpu != smp_processor_id());
	cpuidr = read_cpuid_part_number();
	mpidr = read_cpuid_mpidr();
	for (idx = cpu_compat_table; idx->name; idx++) {
		if (idx->cpuidr == cpuidr)
			break;
	}
	BUG_ON(!idx || !idx->name);
	setup_cputopo(cpu, idx, mpidr);
}

static void reset_cputopo(void)
{
	struct cpu_compatible *idx;

	memset(glb_cpu_arch, 0, sizeof(struct cpu_arch_info));
	glb_cpu_arch->arch_type = ARCH_UNKNOWN;

	memset(&compat_cputopo, 0, sizeof(compat_cputopo));

	spin_lock(&__cpu_cluster_lock);
	for (idx = cpu_compat_table; idx->name; idx++) {
		struct cpu_cluster *curr, *next;

		if (idx->clscnt == 0)
			continue;
		BUG_ON(!idx->cluster);

		curr = idx->cluster;
		next = (struct cpu_cluster *)curr->next;
		kfree(curr);

		while (next) {
			curr = next;
			next = (struct cpu_cluster *)curr->next;
			kfree(curr);
		}
		idx->cluster = NULL;
		idx->clscnt = 0;
	}
	spin_unlock(&__cpu_cluster_lock);
}

/* verify cpu topology correctness by device tree.
 * This function is called when current CPU is cpuid!
 */
static void verify_cputopo(const unsigned int cpuid, const u32 mpidr)
{
	struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];
	struct cpu_compatible *cpt;
	struct cpu_cluster *cls;

	if (!glb_cpu_arch->arch_ready) {
		int i;

		setup_cputopo_def(cpuid);
		for (i = 0; i < nr_cpu_ids; i++)
			if (!compat_cputopo[i])
				break;
		if (i == nr_cpu_ids)
			glb_cpu_arch->arch_ready = true;

		return;
	}

	cpt = compat_cputopo[cpuid];
	BUG_ON(!cpt);
	cls = cpt->cluster;
	while (cls) {
		if (cpu_isset(cpuid, cls->siblings))
			break;
		cls = cls->next;
	}
	BUG_ON(!cls);
	WARN(cls->cluster_id != cpuid_topo->socket_id,
		 "[%s] cpu id: %d, cluster id (%d) != socket id (%d)\n",
		 __func__, cpuid, cls->cluster_id, cpuid_topo->socket_id);
}

/*
 * return 1 while every cpu is recognizible
 */
void arch_build_cpu_topology_domain(void)
{
	struct device_node *cn = NULL;
	unsigned int cpu = 0;
	u32 mpidr;

	memset(&compat_cputopo, 0, sizeof(compat_cputopo));
	// default by device tree parsing
	while ((cn = of_find_node_by_type(cn, "cpu"))) {
		struct cpu_compatible *idx;
		const u32 *reg;
		int len;

		if (unlikely(cpu >= nr_cpu_ids)) {
			pr_err("[CPUTOPO][%s] device tree cpu%d is over possible's\n",
			       __func__, cpu);
			break;
		}

		for (idx = cpu_compat_table; idx->name; idx++)
			if (of_device_is_compatible(cn, idx->name))
				break;

		if (!idx || !idx->name) {
			int cplen;
			const char *cp;
			cp = (char *) of_get_property(cn, "compatible", &cplen);
			pr_err("[CPUTOPO][%s] device tree cpu%d (%s) is not compatible!!\n",
			       __func__, cpu, cp);
			break;
		}

		reg = of_get_property(cn, "reg", &len);
		if (!reg || len != 4) {
			pr_err("[CPUTOPO][%s] missing reg property\n", cn->full_name);
			break;
		}
		mpidr = be32_to_cpup(reg);
		setup_cputopo(cpu, idx, mpidr);
		cpu++;
	}
	glb_cpu_arch->arch_ready = (cpu == nr_cpu_ids);

	if (!glb_cpu_arch->arch_ready) {
		pr_warn("[CPUTOPO][%s] build cpu topology failed, to be handled by mpidr/cpuidr regs!\n", __func__);
		reset_cputopo();
		setup_cputopo_def(smp_processor_id());
	}
}

int arch_cpu_is_big(unsigned int cpu)
{
	int type;

	if (unlikely(cpu >= nr_cpu_ids))
		BUG();

	type = __arch_type();
	switch(type) {
	case ARCH_BIG_LITTLE:
		return (compat_cputopo[cpu] == glb_cpu_arch->compat_big);
	default:
		/* treat as little */
		return 0;
	}
}

int arch_cpu_is_little(unsigned int cpu)
{
	int type;

	if (unlikely(cpu >= nr_cpu_ids))
		BUG();

	type = __arch_type();
	switch(type) {
	case ARCH_BIG_LITTLE:
		return (compat_cputopo[cpu] == glb_cpu_arch->compat_ltt);
	default:
		/* treat as little */
		return 1;
	}
}

int arch_is_multi_cluster(void)
{
	return (__arch_type() == ARCH_MULTI_CLUSTER || __arch_type() == ARCH_BIG_LITTLE);
}

int arch_is_big_little(void)
{
	return (__arch_type() == ARCH_BIG_LITTLE);
}

int arch_get_nr_clusters(void)
{
	return glb_cpu_arch->nr_clusters;
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cputopo_arm *arm_cputopo = &cpu_topology[cpu];
	struct cpu_compatible *cpt;
	struct cpu_cluster *cls;

	BUG_ON(cpu >= nr_cpu_ids);
	if (!glb_cpu_arch->arch_ready) {
		WARN_ONCE(!glb_cpu_arch->arch_ready, "[CPUTOPO][%s] cpu(%d), socket_id(%d) topology is not ready!\n",
				  __func__, cpu, arm_cputopo->socket_id);
		if (unlikely(arm_cputopo->socket_id < 0))
			return 0;
		return arm_cputopo->socket_id;
	}

	cpt = compat_cputopo[cpu];
	BUG_ON(!cpt);
	cls = cpt->cluster;
	while (cls) {
		if (cpu_isset(cpu, cls->siblings))
			break;
		cls = cls->next;
	}
	BUG_ON(!cls);
	WARN_ONCE(cls->cluster_id != arm_cputopo->socket_id, "[CPUTOPO][%s] cpu(%d): cluster_id(%d) != socket_id(%d) !\n",
			  __func__, cpu, cls->cluster_id, arm_cputopo->socket_id);

	return cls->cluster_id;
}

static struct cpu_cluster *__get_cluster_slowpath(int cluster_id)
{
	int i = 0;
	struct cpu_compatible *cpt;
	struct cpu_cluster *cls;

	for (i = 0; i < nr_cpu_ids; i++) {
		cpt = compat_cputopo[i];
		BUG_ON(!cpt);
		cls = cpt->cluster;
		while (cls) {
			if (cls->cluster_id == cluster_id)
				return cls;
			cls = cls->next;
		}
	}
	return NULL;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	struct cpu_cluster *cls = NULL;

	cpumask_clear(cpus);

	if (likely(glb_cpu_arch->compat_ltt)) {
		cls = glb_cpu_arch->compat_ltt->cluster;
		while (cls) {
			if (cls->cluster_id == cluster_id)
				goto found;
			cls = cls->next;
		}
	}
	if (likely(glb_cpu_arch->compat_big)) {
		cls = glb_cpu_arch->compat_big->cluster;
		while (cls) {
			if (cls->cluster_id == cluster_id)
				goto found;
			cls = cls->next;
		}
	}

	cls = __get_cluster_slowpath(cluster_id);
	BUG_ON(!cls); // debug only.. remove later...
	if (!cls)
		return;

found:
	cpumask_copy(cpus, &cls->siblings);
}

/*
 * arch_get_big_little_cpus - get big/LITTLE cores in cpumask
 * @big: the cpumask pointer of big cores
 * @little: the cpumask pointer of little cores
 *
 * Treat it as little cores, if it's not big.LITTLE architecture
 */
void arch_get_big_little_cpus(struct cpumask *big, struct cpumask *little)
{
	int type;
	struct cpu_cluster *cls = NULL;
	struct cpumask tmpmask;
	unsigned int cpu;

	if (unlikely(!glb_cpu_arch->arch_ready))
		BUG();

	type = __arch_type();
	spin_lock(&__cpu_cluster_lock);
	switch(type) {
	case ARCH_BIG_LITTLE:
		if (likely(1 == glb_cpu_arch->compat_big->clscnt)) {
			cls = glb_cpu_arch->compat_big->cluster;
			cpumask_copy(big, &cls->siblings);
		} else {
			cls = glb_cpu_arch->compat_big->cluster;
			while (cls) {
				cpumask_or(&tmpmask, big, &cls->siblings);
				cpumask_copy(big, &tmpmask);
				cls = cls->next;
			}
		}
		if (likely(1 == glb_cpu_arch->compat_ltt->clscnt)) {
			cls = glb_cpu_arch->compat_ltt->cluster;
			cpumask_copy(little, &cls->siblings);
		} else {
			cls = glb_cpu_arch->compat_ltt->cluster;
			while (cls) {
				cpumask_or(&tmpmask, little, &cls->siblings);
				cpumask_copy(little, &tmpmask);
				cls = cls->next;
			}
		}
		break;
	default:
		/* treat as little */
		cpumask_clear(big);
		cpumask_clear(little);
		for_each_possible_cpu(cpu)
			cpumask_set_cpu(cpu, little);
	}
	spin_unlock(&__cpu_cluster_lock);
}
#else /* !CONFIG_MTK_CPU_TOPOLOGY */
int arch_cpu_is_big(unsigned int cpu) { return 0; }
int arch_cpu_is_little(unsigned int cpu) { return 1; }
int arch_is_big_little(void) { return 0; }

int arch_get_nr_clusters(void)
{
	int max_id = 0;
	unsigned int cpu;

	// assume socket id is monotonic increasing without gap.
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *arm_cputopo = &cpu_topology[cpu];
		if (arm_cputopo->socket_id > max_id)
			max_id = arm_cputopo->socket_id;
	}
	return max_id+1;
}

int arch_is_multi_cluster(void)
{
	return (arch_get_nr_clusters() > 1 ? 1 : 0);
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cputopo_arm *arm_cputopo = &cpu_topology[cpu];
	return arm_cputopo->socket_id < 0 ? 0 : arm_cputopo->socket_id;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu, found_id = -1;

	for_each_possible_cpu(cpu) {
		struct cputopo_arm *arm_cputopo = &cpu_topology[cpu];
		if (arm_cputopo->socket_id == cluster_id) {
			found_id = cluster_id;
			break;
		}
	}
	if (-1 == found_id || cluster_to_logical_mask(found_id, cpus)) {
		cpumask_clear(cpus);
		for_each_possible_cpu(cpu)
			cpumask_set_cpu(cpu, cpus);
	}
}
void arch_get_big_little_cpus(struct cpumask *big, struct cpumask *little)
{
    unsigned int cpu;
    cpumask_clear(big);
    cpumask_clear(little);
    for_each_possible_cpu(cpu)
        cpumask_set_cpu(cpu, little);
}
#endif /* CONFIG_MTK_CPU_TOPOLOGY */

/*
 * store_cpu_topology is called at boot when only one cpu is running
 * and with the mutex cpu_hotplug.lock locked, when several cpus have booted,
 * which prevents simultaneous write access to cpu_topology array
 */
void store_cpu_topology(unsigned int cpuid)
{
	struct cputopo_arm *cpuid_topo = &cpu_topology[cpuid];
	unsigned int mpidr;

	/* If the cpu topology has been already set, just return */
	if (cpuid_topo->core_id != -1)
		return;

	mpidr = read_cpuid_mpidr();

	/* create cpu topology mapping */
	if ((mpidr & MPIDR_SMP_BITMASK) == MPIDR_SMP_VALUE) {
		/*
		 * This is a multiprocessor system
		 * multiprocessor format & multiprocessor mode field are set
		 */

		if (mpidr & MPIDR_MT_BITMASK) {
			/* core performance interdependency */
			cpuid_topo->thread_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
			cpuid_topo->core_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);
			cpuid_topo->socket_id = MPIDR_AFFINITY_LEVEL(mpidr, 2);
		} else {
			/* largely independent cores */
			cpuid_topo->thread_id = -1;
			cpuid_topo->core_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
			cpuid_topo->socket_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);
		}
	} else {
		/*
		 * This is an uniprocessor system
		 * we are in multiprocessor format but uniprocessor system
		 * or in the old uniprocessor format
		 */
		cpuid_topo->thread_id = -1;
		cpuid_topo->core_id = 0;
		cpuid_topo->socket_id = -1;
	}

#ifdef CONFIG_MTK_CPU_TOPOLOGY
	verify_cputopo(cpuid, (u32)mpidr);
#endif

	update_siblings_masks(cpuid);

	update_cpu_power(cpuid, mpidr & MPIDR_HWID_BITMASK);

	printk(KERN_INFO "CPU%u: thread %d, cpu %d, socket %d, mpidr %x\n",
		   cpuid, cpu_topology[cpuid].thread_id,
		   cpu_topology[cpuid].core_id,
		   cpu_topology[cpuid].socket_id, mpidr);
}

/*
 * cluster_to_logical_mask - return cpu logical mask of CPUs in a cluster
 * @socket_id:		cluster HW identifier
 * @cluster_mask:	the cpumask location to be initialized, modified by the
 *			function only if return value == 0
 *
 * Return:
 *
 * 0 on success
 * -EINVAL if cluster_mask is NULL or there is no record matching socket_id
 */
int cluster_to_logical_mask(unsigned int socket_id, cpumask_t *cluster_mask)
{
	int cpu;

	if (!cluster_mask)
		return -EINVAL;

	for_each_online_cpu(cpu)
		if (socket_id == topology_physical_package_id(cpu)) {
			cpumask_copy(cluster_mask, topology_core_cpumask(cpu));
			return 0;
		}

	return -EINVAL;
}

#ifdef CONFIG_SCHED_HMP
static const char * const little_cores[] = {
	"arm,cortex-a53",
	"arm,cortex-a7",
	NULL,
};

static bool is_little_cpu(struct device_node *cn)
{
	const char * const *lc;
	for (lc = little_cores; *lc; lc++)
		if (of_device_is_compatible(cn, *lc)) {
			return true;
		}
	return false;
}

void __init arch_get_fast_and_slow_cpus(struct cpumask *fast,
										struct cpumask *slow)
{
	struct device_node *cn = NULL;
	int cpu;

	cpumask_clear(fast);
	cpumask_clear(slow);
	
	/*
	 * Use the config options if they are given. This helps testing
	 * HMP scheduling on systems without a big.LITTLE architecture.
	 */
	if (strlen(CONFIG_HMP_FAST_CPU_MASK) && strlen(CONFIG_HMP_SLOW_CPU_MASK)) {
		if (cpulist_parse(CONFIG_HMP_FAST_CPU_MASK, fast))
			WARN(1, "Failed to parse HMP fast cpu mask!\n");
		if (cpulist_parse(CONFIG_HMP_SLOW_CPU_MASK, slow))
			WARN(1, "Failed to parse HMP slow cpu mask!\n");
		return;
	}

	/*
	 * Else, parse device tree for little cores.
	 */
	while ((cn = of_find_node_by_type(cn, "cpu"))) {

		const u32 *mpidr;
		int len;

		mpidr = of_get_property(cn, "reg", &len);
		if (!mpidr || len != 4) {
			pr_err("* %s missing reg property\n", cn->full_name);
			continue;
		}

		cpu = get_logical_index(be32_to_cpup(mpidr));
		if (cpu == -EINVAL) {
			pr_err("couldn't get logical index for mpidr %x\n",
				   be32_to_cpup(mpidr));
			break;
		}

		if (is_little_cpu(cn))
			cpumask_set_cpu(cpu, slow);
		else
			cpumask_set_cpu(cpu, fast);
	}

	if (!cpumask_empty(fast) && !cpumask_empty(slow))
		return;

	/*
	 * We didn't find both big and little cores so let's call all cores
	 * fast as this will keep the system running, with all cores being
	 * treated equal.
	 */
	cpumask_setall(fast);
	cpumask_clear(slow);
}

struct cpumask hmp_fast_cpu_mask;
struct cpumask hmp_slow_cpu_mask;

void __init arch_get_hmp_domains(struct list_head *hmp_domains_list)
{
	struct hmp_domain *domain;

	arch_get_fast_and_slow_cpus(&hmp_fast_cpu_mask, &hmp_slow_cpu_mask);

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	if(!cpumask_empty(&hmp_slow_cpu_mask)) {
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		cpumask_copy(&domain->possible_cpus, &hmp_slow_cpu_mask);
		cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
		list_add(&domain->hmp_domains, hmp_domains_list);
	}
	domain = (struct hmp_domain *)
		kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
	cpumask_copy(&domain->possible_cpus, &hmp_fast_cpu_mask);
	cpumask_and(&domain->cpus, cpu_online_mask, &domain->possible_cpus);
	list_add(&domain->hmp_domains, hmp_domains_list);
}
#endif /* CONFIG_SCHED_HMP */

/*
 * init_cpu_topology is called at boot when only one cpu is running
 * which prevent simultaneous write access to cpu_topology array
 */
void __init init_cpu_topology(void)
{
	unsigned int cpu;

	/* init core mask and power*/
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &(cpu_topology[cpu]);

		cpu_topo->thread_id = -1;
		cpu_topo->core_id =  -1;
		cpu_topo->socket_id = -1;
		cpumask_clear(&cpu_topo->core_sibling);
		cpumask_clear(&cpu_topo->thread_sibling);

		set_power_scale(cpu, SCHED_POWER_SCALE);
	}
	smp_wmb();

	parse_dt_topology();
}


#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#include <linux/cpufreq.h>
#define ARCH_SCALE_INVA_CPU_CAP_PERCLS 1

struct cpufreq_extents {
	u32 max;
	u32 flags;
	u32 const_max;
	u32 throttling;
};
/* Flag set when the governor in use only allows one frequency.
 * Disables scaling.
 */
#define CPUPOWER_FREQINVAR_SINGLEFREQ 0x01
static struct cpufreq_extents freq_scale[CONFIG_NR_CPUS];

static unsigned long get_max_cpu_power(void)
{
	unsigned long max_cpu_power = 0;
	int cpu;
	for_each_online_cpu(cpu){
		if( per_cpu(cpu_scale, cpu) > max_cpu_power)
			max_cpu_power = per_cpu(cpu_scale, cpu);
	}
	return max_cpu_power;
}

int arch_get_cpu_throttling(int cpu)
{
	return freq_scale[cpu].throttling;
}

/* Called when the CPU Frequency is changed.
 * Once for each CPU.
 */
static int cpufreq_callback(struct notifier_block *nb,
							unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	struct cpufreq_extents *extents;
	unsigned int curr_freq;
#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	int i = 0;
#endif

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	/* if dynamic load scale is disabled, set the load scale to 1.0 */
	if (!frequency_invariant_power_enabled) {
		per_cpu(invariant_cpu_capacity, cpu) = per_cpu(base_cpu_capacity, cpu);
		return NOTIFY_OK;
	}

	extents = &freq_scale[cpu];
	if (extents->max < extents->const_max) {
		extents->throttling = 1;
	} else {
		extents->throttling = 0;
	}
	/* If our governor was recognised as a single-freq governor,
	 * use curr = max to be sure multiplier is 1.0
	 */
	if (extents->flags & CPUPOWER_FREQINVAR_SINGLEFREQ)
		curr_freq = extents->max >> CPUPOWER_FREQSCALE_SHIFT;
	else
		curr_freq = freq->new >> CPUPOWER_FREQSCALE_SHIFT;

#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	for_each_cpu(i, topology_core_cpumask(cpu)) {
			per_cpu(invariant_cpu_capacity, i) = DIV_ROUND_UP(
				(curr_freq * per_cpu(prescaled_cpu_capacity, i)), CPUPOWER_FREQSCALE_DEFAULT);
	}
#else
	per_cpu(invariant_cpu_capacity, cpu) = DIV_ROUND_UP(
		(curr_freq * per_cpu(prescaled_cpu_capacity, cpu)), CPUPOWER_FREQSCALE_DEFAULT);
#endif
	return NOTIFY_OK;
}

/* Called when the CPUFreq governor is changed.
 * Only called for the CPUs which are actually changed by the
 * userspace.
 */
static int cpufreq_policy_callback(struct notifier_block *nb,
								   unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpufreq_extents *extents;
	int cpu, singleFreq = 0, cpu_capacity;
	static const char performance_governor[] = "performance";
	static const char powersave_governor[] = "powersave";
	unsigned long max_cpu_power;
#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
	int i = 0;
#endif

	if (event == CPUFREQ_START)
		return 0;

	if (event != CPUFREQ_INCOMPATIBLE)
		return 0;

	/* CPUFreq governors do not accurately report the range of
	 * CPU Frequencies they will choose from.
	 * We recognise performance and powersave governors as
	 * single-frequency only.
	 */
	if (!strncmp(policy->governor->name, performance_governor,
				 strlen(performance_governor)) ||
		!strncmp(policy->governor->name, powersave_governor,
				 strlen(powersave_governor)))
		singleFreq = 1;

	max_cpu_power = get_max_cpu_power();
	/* Make sure that all CPUs impacted by this policy are
	 * updated since we will only get a notification when the
	 * user explicitly changes the policy on a CPU.
	 */
	for_each_cpu(cpu, policy->cpus) {
		/* scale cpu_power to max(1024) */
		cpu_capacity = (per_cpu(cpu_scale, cpu) << CPUPOWER_FREQSCALE_SHIFT)
			/ max_cpu_power;
		extents = &freq_scale[cpu];
		extents->max = policy->max >> CPUPOWER_FREQSCALE_SHIFT;
		extents->const_max = policy->cpuinfo.max_freq >> CPUPOWER_FREQSCALE_SHIFT;
		if (!frequency_invariant_power_enabled) {
			/* when disabled, invariant_cpu_scale = cpu_scale */
			per_cpu(base_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
			per_cpu(invariant_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
			/* unused when disabled */
			per_cpu(prescaled_cpu_capacity, cpu) = CPUPOWER_FREQSCALE_DEFAULT;
		} else {
			if (singleFreq)
				extents->flags |= CPUPOWER_FREQINVAR_SINGLEFREQ;
			else
				extents->flags &= ~CPUPOWER_FREQINVAR_SINGLEFREQ;
			per_cpu(base_cpu_capacity, cpu) = cpu_capacity;
#ifdef CONFIG_SCHED_HMP_ENHANCEMENT
			per_cpu(prescaled_cpu_capacity, cpu) =
				((cpu_capacity << CPUPOWER_FREQSCALE_SHIFT) / extents->const_max);
#else
			per_cpu(prescaled_cpu_capacity, cpu) =
				((cpu_capacity << CPUPOWER_FREQSCALE_SHIFT) / extents->max);
#endif

#ifdef ARCH_SCALE_INVA_CPU_CAP_PERCLS
			for_each_cpu(i, topology_core_cpumask(cpu)) {
					per_cpu(invariant_cpu_capacity, i) = DIV_ROUND_UP(
						((policy->cur>>CPUPOWER_FREQSCALE_SHIFT) *
						 per_cpu(prescaled_cpu_capacity, i)), CPUPOWER_FREQSCALE_DEFAULT);
			}
#else
			per_cpu(invariant_cpu_capacity, cpu) = DIV_ROUND_UP(
				((policy->cur>>CPUPOWER_FREQSCALE_SHIFT) *
				 per_cpu(prescaled_cpu_capacity, cpu)), CPUPOWER_FREQSCALE_DEFAULT);
#endif
		}
	}
	return 0;
}

static struct notifier_block cpufreq_notifier = {
	.notifier_call  = cpufreq_callback,
};
static struct notifier_block cpufreq_policy_notifier = {
	.notifier_call  = cpufreq_policy_callback,
};

static int __init register_topology_cpufreq_notifier(void)
{
	int ret;

	/* init safe defaults since there are no policies at registration */
	for (ret = 0; ret < CONFIG_NR_CPUS; ret++) {
		/* safe defaults */
		freq_scale[ret].max = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(base_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(invariant_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
		per_cpu(prescaled_cpu_capacity, ret) = CPUPOWER_FREQSCALE_DEFAULT;
	}

	pr_info("topology: registering cpufreq notifiers for scale-invariant CPU Power\n");
	ret = cpufreq_register_notifier(&cpufreq_policy_notifier,
									CPUFREQ_POLICY_NOTIFIER);

	if (ret != -EINVAL)
		ret = cpufreq_register_notifier(&cpufreq_notifier,
										CPUFREQ_TRANSITION_NOTIFIER);

	return ret;
}

core_initcall(register_topology_cpufreq_notifier);
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */
