/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <mach/ocmem_priv.h>
#include <mach/rpm-smd.h>

static unsigned num_regions;
static unsigned num_macros;
static unsigned num_ports;
static unsigned num_banks;

static unsigned long macro_size;
static unsigned long region_size;

static bool rpm_power_control;

struct ocmem_hw_macro {
	atomic_t m_on[OCMEM_CLIENT_MAX];
	atomic_t m_retain[OCMEM_CLIENT_MAX];
	unsigned m_state;
};

struct ocmem_hw_region {
	unsigned psgsc_ctrl;
	bool interleaved;
	unsigned int mode;
	unsigned int num_macros;
	struct ocmem_hw_macro *macro;
	struct msm_rpm_request *rpm_req;
	unsigned r_state;
};

static struct ocmem_hw_region *region_ctrl;
static struct mutex region_ctrl_lock;
static void *ocmem_base;

#define OCMEM_V1_REGIONS 3
#define OCMEM_V1_MACROS 8

#define OC_HW_VERS (0x0)
#define OC_HW_PROFILE (0x4)
#define OC_GEN_STATUS (0xC)
#define OC_PSGSC_STATUS (0x38)
#define OC_PSGSC_CTL (0x3C)
#define OC_REGION_CTL (0x1000)

#define NUM_PORTS_MASK (0xF << 0)
#define NUM_PORTS_SHIFT (0)

#define NUM_MACROS_MASK (0xF << 8)
#define NUM_MACROS_SHIFT (8)

#define INTERLEAVING_MASK (0x1 << 17)
#define INTERLEAVING_SHIFT (17)

/* Power states of each memory macro */
#define PASSTHROUGH (0x0)
#define CORE_ON (0x2)
#define PERI_ON (0x1)
#define CLK_OFF (0x4)
#define MACRO_ON (CORE_ON|PERI_ON)
#define MACRO_SLEEP_RETENTION (CLK_OFF|CORE_ON)
#define MACRO_SLEEP_RETENTION_PERI_ON (CLK_OFF|MACRO_ON)
#define MACRO_OFF (CLK_OFF)

#define M_PSCGC_CTL_n(x) (0x7 << (x * 4))

#define PSCGC_CTL_IDX(x) ((x) * 0x4)
#define PSCGC_CTL_n(x) (OC_PSGSC_CTL + (PSCGC_CTL_IDX(x)))

/* Power states of each ocmem region */
#define REGION_NORMAL_PASSTHROUGH 0x00000000
#define REGION_FORCE_PERI_ON 0x00001111
#define REGION_FORCE_CORE_ON 0x00002222
#define REGION_FORCE_ALL_ON 0x00003333
#define REGION_SLEEP_NO_RETENTION 0x00004444
#define REGION_SLEEP_PERI_OFF 0x00006666
#define REGION_SLEEP_PERI_ON 0x00007777

#define REGION_DEFAULT_OFF REGION_SLEEP_NO_RETENTION
#define REGION_DEFAULT_ON REGION_FORCE_ALL_ON
#define REGION_DEFAULT_RETENTION REGION_SLEEP_PERI_OFF

enum rpm_macro_state {
	rpm_macro_off = 0x0,
	rpm_macro_retain,
	rpm_macro_on,
};

static int rpm_write(unsigned long val, unsigned id);

static inline unsigned hw_macro_state(unsigned region_state)
{
	unsigned macro_state;

	switch (region_state) {
	case REGION_DEFAULT_ON:
		macro_state = MACRO_ON;
		break;
	case REGION_DEFAULT_OFF:
		macro_state = MACRO_OFF;
		break;
	case REGION_DEFAULT_RETENTION:
		macro_state = MACRO_SLEEP_RETENTION;
		break;
	default:
		macro_state = MACRO_OFF;
		break;
	}
	return macro_state;
}

static inline unsigned rpm_macro_state(unsigned hw_macro_state)
{
	unsigned macro_state;

	switch (hw_macro_state) {
	case MACRO_ON:
		macro_state = rpm_macro_on;
		break;
	case MACRO_OFF:
		macro_state = rpm_macro_off;
		break;
	case MACRO_SLEEP_RETENTION:
		macro_state = rpm_macro_retain;
		break;
	default:
		macro_state = rpm_macro_off;
		break;
	}
	return macro_state;
}

/* Generic wrapper that sets the region state either
   by a direct write or through appropriate RPM call
*/
/* Must be called with region mutex held */
static int commit_region_state(unsigned region_num)
{
	int rc = -1;
	unsigned new_state;

	if (region_num >= num_regions)
		return -EINVAL;

	new_state = region_ctrl[region_num].r_state;
	pr_debug("ocmem: commit region (%d) new state %x\n", region_num,
								new_state);
	if (rpm_power_control)
		rc = rpm_write(new_state, region_num);
	else
		rc = ocmem_write(new_state,
					ocmem_base + PSCGC_CTL_n(region_num));
	return 0;
}

/* Returns the current state of a OCMEM region */
/* Must be called with region mutex held */
static int read_region_state(unsigned region_num)
{
	int state;

	pr_debug("rpm_get_region_state: #: %d\n", region_num);

	if (region_num >= num_regions)
		return -EINVAL;

	if (rpm_power_control)
		state = region_ctrl[region_num].r_state;
	else
		state = ocmem_read(ocmem_base + PSCGC_CTL_n(region_num));

	pr_debug("ocmem: region (%d) state %x\n", region_num, state);

	return state;
}

/* Returns the current state of a OCMEM macro that belongs to a region */
static int read_macro_state(unsigned region_num, unsigned macro_num)
{
	int state;

	if (macro_num >= num_banks)
		return -EINVAL;

	state = read_region_state(region_num);

	if (state < 0)
		return -EINVAL;

	state &= M_PSCGC_CTL_n(macro_num);
	state = state >> (macro_num * 4);

	pr_debug("rpm_get_macro_state: macro (%d) region (%d) state %x\n",
			macro_num, region_num, state);

	return state;
}

static int apply_macro_vote(int id, unsigned region_num,
				unsigned macro_num, int new_state)
{
	struct ocmem_hw_macro *m = NULL;
	struct ocmem_hw_region *region = NULL;

	if (region_num >= num_regions)
		return -EINVAL;

	if (macro_num >= num_banks)
		return -EINVAL;

	region = &region_ctrl[region_num];

	m = &region->macro[macro_num];

	pr_debug("m (%d): curr state %x votes (on: %d retain %d) new state %x\n",
			macro_num, m->m_state,
			atomic_read(&m->m_on[id]),
			atomic_read(&m->m_retain[id]),
			new_state);

	switch (m->m_state) {
	case MACRO_OFF:
		if (new_state == MACRO_ON)
			atomic_inc(&m->m_on[id]);
		break;
	case MACRO_ON:
		if (new_state == MACRO_OFF) {
			atomic_dec(&m->m_on[id]);
		} else if (new_state == MACRO_SLEEP_RETENTION) {
			atomic_inc(&m->m_retain[id]);
			atomic_dec(&m->m_on[id]);
		}
		break;
	case MACRO_SLEEP_RETENTION:
		if (new_state == MACRO_OFF) {
			atomic_dec(&m->m_retain[id]);
		} else if (new_state == MACRO_ON) {
			atomic_inc(&m->m_on[id]);
			atomic_dec(&m->m_retain[id]);
		}
		break;
	}

	pr_debug("macro (%d) region (%d) votes for %d (on: %d retain %d)\n",
				region_num, macro_num, id,
				atomic_read(&m->m_on[id]),
				atomic_read(&m->m_retain[id]));
	return 0;
}

static int aggregate_macro_state(unsigned region_num, unsigned macro_num)
{
	struct ocmem_hw_macro *m = NULL;
	struct ocmem_hw_region *region = NULL;
	int i = 0;
	/* The default is for the macro to be OFF */
	unsigned m_state = MACRO_OFF;

	if (region_num >= num_regions)
		return -EINVAL;

	if (macro_num >= num_banks)
		return -EINVAL;

	region = &region_ctrl[region_num];
	m = &region->macro[macro_num];

	for (i = 0; i < OCMEM_CLIENT_MAX; i++) {
		if (atomic_read(&m->m_on[i]) > 0) {
			/* atleast one client voted for ON state */
			m_state = MACRO_ON;
			goto done_aggregation;
		} else if (atomic_read(&m->m_retain[i]) > 0) {
			m_state = MACRO_SLEEP_RETENTION;
			/* continue and examine votes of other clients */
		}
	}
done_aggregation:
	m->m_state = m_state;
	pr_debug("macro (%d) region (%d) aggregated state %x", macro_num,
						region_num, m->m_state);
	return 0;
}

static int aggregate_region_state(unsigned region_num)
{
	struct ocmem_hw_region *region = NULL;
	unsigned r_state;
	unsigned i = 0;

	if (region_num >= num_regions)
		return -EINVAL;

	region = &region_ctrl[region_num];
	r_state = REGION_DEFAULT_OFF;

	/* In wide mode all macros must have the same state */
	if (region->mode == WIDE_MODE) {
		for (i = 0; i < region->num_macros; i++) {
			if (region->macro[i].m_state == MACRO_ON) {
				r_state = REGION_DEFAULT_ON;
				break;
			} else if (region->macro[i].m_state ==
						MACRO_SLEEP_RETENTION) {
				r_state = REGION_DEFAULT_RETENTION;
			}
		}
	} else {
	/* In narrow mode each macro is allowed to be in a different state */
	/* The region mode is simply the collection of all macro states */
		for (i = 0; i < region->num_macros; i++) {
			r_state &= ~M_PSCGC_CTL_n(i);
			r_state |= region->macro[i].m_state << (i * 4);
		}
	}

	pr_debug("region (%d) curr state (%x) aggregated state (%x)\n",
			region_num, region->r_state, r_state);
	region->r_state = r_state;
	return 0;
}

static int rpm_write(unsigned long val, unsigned id)
{
	int i = 0;
	int ret = 0;
	struct ocmem_hw_region *region;

	region = &region_ctrl[id];

	for (i = 0; i < region->num_macros; i++) {
		unsigned macro_state;
		unsigned rpm_state;

		macro_state = read_macro_state(id, i);
		rpm_state = rpm_macro_state(macro_state);

		if (val == REGION_DEFAULT_ON) {
			pr_debug("macro (%d) region (%d) -> active\n",
				i, id);
			rpm_state = rpm_macro_on;
		}

		if (val == REGION_DEFAULT_OFF) {
			pr_debug("macro (%d) region (%d) -> off\n",
				i, id);
			rpm_state = rpm_macro_off;
		}

		ret = msm_rpm_add_kvp_data(region->rpm_req, i,
						(u8 *) &rpm_state, 4);

		if (ret < 0) {
			pr_err("ocmem: Error adding key %d val %d on rsc %d\n",
					i, rpm_state, id);
			return -EINVAL;
		}
	}

	ret = msm_rpm_send_request(region->rpm_req);

	if (ret < 0) {
		pr_err("ocmem: Error sending RPM request\n");
		return -EINVAL;
	}

	pr_debug("Transmit request to rpm for region %d\n", id);
	return 0;
}

#if defined(CONFIG_MSM_OCMEM_POWER_DISABLE)
static int ocmem_core_set_default_state(void)
{
	int rc = 0;

	/* The OCMEM core clock and branch clocks are always turned ON */
	rc = ocmem_enable_core_clock();
	if (rc < 0)
		return rc;

	rc = ocmem_enable_iface_clock();
	if (rc < 0)
		return rc;

	return 0;
}

/* Initializes a region to be turned ON in wide mode */
static int ocmem_region_set_default_state(unsigned int r_num)
{
	unsigned m_num = 0;

	mutex_lock(&region_ctrl_lock);

	for (m_num = 0; m_num < num_banks; m_num++) {
		apply_macro_vote(0, r_num, m_num, MACRO_ON);
		aggregate_macro_state(r_num, m_num);
	}

	aggregate_region_state(r_num);
	commit_region_state(r_num);

	mutex_unlock(&region_ctrl_lock);
	return 0;
}

#else
static int ocmem_region_set_default_state(unsigned int region_num)
{
	return 0;
}

static int ocmem_core_set_default_state(void)
{
	return 0;
}
#endif

#if defined(CONFIG_MSM_OCMEM_POWER_DEBUG)

static int read_hw_region_state(unsigned region_num)
{
	int state;

	pr_debug("rpm_get_region_state: #: %d\n", region_num);

	if (region_num >= num_regions)
		return -EINVAL;

	state = ocmem_read(ocmem_base + PSCGC_CTL_n(region_num));

	pr_debug("ocmem: region (%d) state %x\n", region_num, state);

	return state;
}

int ocmem_region_toggle(unsigned int r_num)
{
	unsigned reboot_state = ~0x0;
	unsigned m_num = 0;

	mutex_lock(&region_ctrl_lock);
	/* Turn on each macro at boot for quick hw sanity check */
	reboot_state = read_hw_region_state(r_num);

	if (reboot_state != REGION_DEFAULT_OFF) {
		pr_err("Region %d not in power off state (%x)\n",
				r_num, reboot_state);
		goto toggle_fail;
	}

	for (m_num = 0; m_num < num_banks; m_num++) {
		apply_macro_vote(0, r_num, m_num, MACRO_ON);
		aggregate_macro_state(r_num, m_num);
	}

	aggregate_region_state(r_num);
	commit_region_state(r_num);

	reboot_state = read_hw_region_state(r_num);

	if (reboot_state != REGION_DEFAULT_ON) {
		pr_err("Failed to power on Region %d(state:%x)\n",
				r_num, reboot_state);
		goto toggle_fail;
	}

	/* Turn off all memory macros again */

	for (m_num = 0; m_num < num_banks; m_num++) {
		apply_macro_vote(0, r_num, m_num, MACRO_OFF);
		aggregate_macro_state(r_num, m_num);
	}

	aggregate_region_state(r_num);
	commit_region_state(r_num);

	reboot_state = read_hw_region_state(r_num);

	if (reboot_state != REGION_DEFAULT_OFF) {
		pr_err("Failed to power off Region %d(state:%x)\n",
				r_num, reboot_state);
		goto toggle_fail;
	}
	mutex_unlock(&region_ctrl_lock);
	return 0;

toggle_fail:
	mutex_unlock(&region_ctrl_lock);
	return -EINVAL;
}

int memory_is_off(unsigned int num)
{
	if (read_hw_region_state(num) == REGION_DEFAULT_OFF)
		return 1;
	else
		return 0;
}

#else
int ocmem_region_toggle(unsigned int region_num)
{
	return 0;
}

int memory_is_off(unsigned int num)
{
	return 0;
}
#endif /* CONFIG_MSM_OCMEM_POWER_DEBUG */

/* Memory Macro Power Transition Sequences
 * Normal to Sleep With Retention:
	REGION_DEFAULT_ON -> REGION_DEFAULT_RETENTION
 * Sleep With Retention to Normal:
	REGION_DEFAULT_RETENTION -> REGION_FORCE_CORE_ON -> REGION_DEFAULT_ON
 * Normal to OFF:
	REGION_DEFAULT_ON -> REGION_DEFAULT_OFF
 * OFF to Normal:
	REGION_DEFAULT_OFF -> REGION_DEFAULT_ON
**/

#if defined(CONFIG_MSM_OCMEM_POWER_DISABLE)
/* If power management is disabled leave the macro states as is */
static int switch_power_state(int id, unsigned long offset, unsigned long len,
			unsigned new_state)
{
	return 0;
}

#else
static int switch_power_state(int id, unsigned long offset, unsigned long len,
			unsigned new_state)
{
	unsigned region_start = num_regions;
	unsigned region_end = num_regions;
	unsigned curr_state = 0x0;
	int i = 0;
	int j = 0;
	unsigned start_m = num_banks;
	unsigned end_m = num_banks;
	unsigned long region_offset = 0;
	int rc = 0;

	if (offset < 0)
		return -EINVAL;

	if (len < macro_size)
		return -EINVAL;


	pr_debug("ocmem: power_transition to %x for client %d\n", new_state,
							id);

	region_start = offset / region_size;
	region_end = (offset + len - 1) / region_size;

	pr_debug("ocmem: region start %u end %u\n", region_start, region_end);

	if (region_start >= num_regions ||
		(region_end >= num_regions))
			return -EINVAL;

	rc = ocmem_enable_core_clock();

	if (rc < 0) {
		pr_err("ocmem: Power transistion request for client %s (id: %d) failed\n",
				get_name(id), id);
		return rc;
	}

	mutex_lock(&region_ctrl_lock);

	for (i = region_start; i <= region_end; i++) {

		curr_state = read_region_state(i);

		switch (curr_state) {
		case REGION_DEFAULT_OFF:
			if (new_state != REGION_DEFAULT_ON)
				goto invalid_transition;
			break;
		case REGION_DEFAULT_RETENTION:
			if (new_state != REGION_DEFAULT_ON)
				goto invalid_transition;
			break;
		default:
			break;
		}

		if (len >= region_size) {
			pr_debug("switch: entire region (%d)\n", i);
			start_m = 0;
			end_m = num_banks;
		} else {
			region_offset = offset - (i * region_size);
			start_m = region_offset / macro_size;
			end_m = (region_offset + len - 1) / macro_size;
			pr_debug("switch: macro (%u to %u)\n", start_m, end_m);
		}

		for (j = start_m; j <= end_m; j++) {
			pr_debug("vote: macro (%d) region (%d)\n", j, i);
			apply_macro_vote(id, i, j,
				hw_macro_state(new_state));
			aggregate_macro_state(i, j);
		}
		aggregate_region_state(i);
		commit_region_state(i);
		len -= region_size;

		/* If we voted ON/retain the banks must never be OFF */
		if (new_state != REGION_DEFAULT_OFF) {
			if (memory_is_off(i)) {
				pr_err("ocmem: Accessing memory during sleep\n");
				WARN_ON(1);
			}
		}

	}
	mutex_unlock(&region_ctrl_lock);
	ocmem_disable_core_clock();
	return 0;
invalid_transition:
	mutex_unlock(&region_ctrl_lock);
	ocmem_disable_core_clock();
	pr_err("ocmem_core: Invalid state transition detected for %d\n", id);
	pr_err("ocmem_core: Offset %lx Len %lx curr_state %x new_state %x\n",
			offset, len, curr_state, new_state);
	WARN_ON(1);
	return -EINVAL;
}
#endif

/* Interfaces invoked from the scheduler */
int ocmem_memory_off(int id, unsigned long offset, unsigned long len)
{
	return switch_power_state(id, offset, len, REGION_DEFAULT_OFF);
}

int ocmem_memory_on(int id, unsigned long offset, unsigned long len)
{
	return switch_power_state(id, offset, len, REGION_DEFAULT_ON);
}

int ocmem_memory_retain(int id, unsigned long offset, unsigned long len)
{
	return switch_power_state(id, offset, len, REGION_DEFAULT_RETENTION);
}

int ocmem_core_init(struct platform_device *pdev)
{
	struct device   *dev = &pdev->dev;
	struct ocmem_plat_data *pdata = NULL;
	unsigned hw_ver;
	bool interleaved;
	unsigned i, j, k;
	unsigned rsc_type = 0;
	int rc = 0;

	pdata = platform_get_drvdata(pdev);
	ocmem_base = pdata->reg_base;

	rc = ocmem_enable_core_clock();

	if (rc < 0)
		return rc;

	hw_ver = ocmem_read(ocmem_base + OC_HW_PROFILE);

	if (pdata->nr_regions != OCMEM_V1_REGIONS) {
		pr_err("Invalid number of regions (%d)\n", pdata->nr_regions);
		goto hw_not_supported;
	}

	num_macros = (hw_ver & NUM_MACROS_MASK) >> NUM_MACROS_SHIFT;
	num_ports = (hw_ver & NUM_PORTS_MASK) >> NUM_PORTS_SHIFT;

	if (num_macros != OCMEM_V1_MACROS) {
		pr_err("Invalid number of macros (%d)\n", pdata->nr_macros);
		goto hw_not_supported;
	}

	interleaved = (hw_ver & INTERLEAVING_MASK) >> INTERLEAVING_SHIFT;

	if (interleaved == false) {
		pr_err("Interleaving is disabled\n");
		goto hw_not_supported;
	}

	num_regions = pdata->nr_regions;

	pdata->interleaved = true;
	pdata->nr_macros = num_macros;
	pdata->nr_ports = num_ports;
	macro_size = SZ_64K;
	region_size = macro_size * num_ports;
	num_banks = num_ports / 2;
	rsc_type = pdata->rpm_rsc_type;

	pr_debug("ocmem_core: ports %d regions %d macros %d interleaved %d\n",
				num_ports, num_regions, num_macros,
				interleaved);

	region_ctrl = devm_kzalloc(dev, sizeof(struct ocmem_hw_region)
					 * num_regions, GFP_KERNEL);

	if (!region_ctrl) {
		goto err_no_mem;
	}

	mutex_init(&region_ctrl_lock);

	for (i = 0 ; i < num_regions; i++) {
		struct ocmem_hw_region *region = &region_ctrl[i];
		struct msm_rpm_request *req = NULL;
		region->interleaved = interleaved;
		region->mode = MODE_DEFAULT;
		region->r_state = REGION_DEFAULT_OFF;
		region->num_macros = num_banks;

		region->macro = devm_kzalloc(dev,
					sizeof(struct ocmem_hw_macro) *
						num_banks, GFP_KERNEL);
		if (!region->macro) {
			goto err_no_mem;
		}

		for (j = 0; j < num_banks; j++) {
			struct ocmem_hw_macro *m = &region->macro[j];
			m->m_state = MACRO_OFF;
			for (k = 0; k < OCMEM_CLIENT_MAX; k++) {
				atomic_set(&m->m_on[k], 0);
				atomic_set(&m->m_retain[k], 0);
			}
		}

		if (pdata->rpm_pwr_ctrl) {
			rpm_power_control = true;
			req = msm_rpm_create_request(MSM_RPM_CTX_ACTIVE_SET,
					rsc_type, i, num_banks);

			if (!req) {
				pr_err("Unable to create RPM request\n");
				goto region_init_error;
			}

			pr_debug("rpm request type %x (rsc: %d) with %d elements\n",
						rsc_type, i, num_banks);

			region->rpm_req = req;
		}

		if (ocmem_region_toggle(i)) {
			pr_err("Failed to verify region %d\n", i);
			goto region_init_error;
		}

		if (ocmem_region_set_default_state(i)) {
			pr_err("Failed to initialize region %d\n", i);
			goto region_init_error;
		}
	}

	rc = ocmem_core_set_default_state();

	if (rc < 0)
		return rc;

	ocmem_disable_core_clock();
	return 0;

err_no_mem:
	pr_err("ocmem: Unable to allocate memory\n");
region_init_error:
hw_not_supported:
	pr_err("Unsupported OCMEM h/w configuration %x\n", hw_ver);
	ocmem_disable_core_clock();
	return -EINVAL;
}
