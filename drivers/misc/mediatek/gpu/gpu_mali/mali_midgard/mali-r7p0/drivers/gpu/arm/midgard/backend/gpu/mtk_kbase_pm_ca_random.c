/**
 * @file mtk_kbase_pm_ca_random.c
 * A power policy implementing fixed core availability
 */

#include <mali_kbase.h>
#include <mali_kbase_pm.h>

static void random_init(struct kbase_device *kbdev)
{
	kbdev->pm.backend.ca_in_transition = false;
}

static void random_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

static u64 random_get_core_mask(struct kbase_device *kbdev)
{
	const u64 masks[] = { 0xf, 0x7, 0x3, 0x1, 0x7, 0xf, 0x3, 0xf, 0x1, 0x3, 0x7, 0x1 };
	const u32 size = ARRAY_SIZE(masks);
	static u32 idx = 0;

	return masks[(idx++ % size)];
}

static void random_update_core_status(struct kbase_device *kbdev, u64 cores_ready, u64 cores_transitioning)
{
	CSTD_UNUSED(kbdev);
	CSTD_UNUSED(cores_ready);
	CSTD_UNUSED(cores_transitioning);
}

/** The @ref struct kbase_pm_policy structure for the fixed power policy.
 *
 * This is the static structure that defines the fixed power policy's callback and name.
 */
const struct kbase_pm_ca_policy mtk_kbase_pm_ca_random_policy_ops = {
	"random",			/* name */
	random_init,			/* init */
	random_term,			/* term */
	random_get_core_mask,		/* get_core_mask */
	random_update_core_status,	/* update_core_status */
	0u,				/* flags */
	KBASE_PM_CA_POLICY_ID_RANDOM,	/* id */
};

KBASE_EXPORT_TEST_API(mtk_kbase_pm_ca_random_policy_ops)
