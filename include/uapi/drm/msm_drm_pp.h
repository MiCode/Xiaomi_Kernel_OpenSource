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
 * flags: for customizing operations
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

/* struct drm_msm_pa_vlut - picture adjustment vLUT structure
 * flags: for customizing vlut operation
 * val: vLUT values
 */
#define PA_VLUT_SIZE 256
struct drm_msm_pa_vlut {
	__u64 flags;
	__u32 val[PA_VLUT_SIZE];
};

/* struct drm_msm_memcol - Memory color feature strucuture.
 *                         Skin, sky, foliage features are supported.
 * @prot_flags: Bit mask for enabling protection feature.
 * @color_adjust_p0: Adjustment curve.
 * @color_adjust_p1: Adjustment curve.
 * @color_adjust_p2: Adjustment curve.
 * @blend_gain: Blend gain weightage from othe PA features.
 * @sat_hold: Saturation hold value.
 * @val_hold: Value hold info.
 * @hue_region: Hue qualifier.
 * @sat_region: Saturation qualifier.
 * @val_region: Value qualifier.
 */
#define DRM_MSM_MEMCOL
struct drm_msm_memcol {
	__u64 prot_flags;
	__u32 color_adjust_p0;
	__u32 color_adjust_p1;
	__u32 color_adjust_p2;
	__u32 blend_gain;
	__u32 sat_hold;
	__u32 val_hold;
	__u32 hue_region;
	__u32 sat_region;
	__u32 val_region;
};

#endif /* _MSM_DRM_PP_H_ */
