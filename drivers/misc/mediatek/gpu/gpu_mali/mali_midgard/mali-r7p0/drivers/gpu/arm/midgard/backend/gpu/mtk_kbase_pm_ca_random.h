/**
 * @file mtk_kbase_pm_ca_random.h
 * A power policy implementing fixed core availability
 */

#ifndef MTK_KBASE_PM_CA_RANDOM_H
#define MTK_KBASE_PM_CA_RANDOM_H

/**
 * Private structure for policy instance data.
 *
 * This contains data that is private to the particular power policy that is active.
 */
typedef struct mtk_kbasep_pm_ca_policy_random {
	/** No state needed - just have a dummy variable here */
	int dummy;
} mtk_kbasep_pm_ca_policy_random;

#endif /* MTK_KBASE_PM_CA_RANDOM_H */

