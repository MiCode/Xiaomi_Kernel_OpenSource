#ifndef _MSM_DRM_PP_H_
#define _MSM_DRM_PP_H_

#include <drm/drm.h>

/**
 * struct drm_msm_pcc_coeff - PCC coefficient structure for each color
 *                            component.
 * @c: constant coefficient.
 * @r: red coefficient.
 * @g: green coefficient.
 * @b: blue coefficient.
 * @rg: red green coefficient.
 * @gb: green blue coefficient.
 * @rb: red blue coefficient.
 * @rgb: red blue green coefficient.
 */

struct drm_msm_pcc_coeff {
	__u32 c;
	__u32 r;
	__u32 g;
	__u32 b;
	__u32 rg;
	__u32 gb;
	__u32 rb;
	__u32 rgb;
};

/**
 * struct drm_msm_pcc - pcc feature structure
 * flags: for enable/disable, read/write or customize operations
 * r: red coefficients.
 * g: green coefficients.
 * b: blue coefficients.
 */

struct drm_msm_pcc {
	__u64 flags;
	struct drm_msm_pcc_coeff r;
	struct drm_msm_pcc_coeff g;
	struct drm_msm_pcc_coeff b;
};

#endif /* _MSM_DRM_PP_H_ */
