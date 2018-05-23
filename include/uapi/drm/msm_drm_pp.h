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

#define PA_HSIC_HUE_ENABLE (1 << 0)
#define PA_HSIC_SAT_ENABLE (1 << 1)
#define PA_HSIC_VAL_ENABLE (1 << 2)
#define PA_HSIC_CONT_ENABLE (1 << 3)
#define PA_HSIC_LEFT_DISPLAY_ONLY (1 << 4)
#define PA_HSIC_RIGHT_DISPLAY_ONLY (1 << 5)
/**
 * struct drm_msm_pa_hsic - pa hsic feature structure
 * @flags: flags for the feature customization, values can be:
 *         - PA_HSIC_HUE_ENABLE: Enable hue adjustment
 *         - PA_HSIC_SAT_ENABLE: Enable saturation adjustment
 *         - PA_HSIC_VAL_ENABLE: Enable value adjustment
 *         - PA_HSIC_CONT_ENABLE: Enable contrast adjustment
 *
 * @hue: hue setting
 * @saturation: saturation setting
 * @value: value setting
 * @contrast: contrast setting
 */
#define DRM_MSM_PA_HSIC
struct drm_msm_pa_hsic {
	__u64 flags;
	__u32 hue;
	__u32 saturation;
	__u32 value;
	__u32 contrast;
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

#define GAMUT_3D_MODE_17 1
#define GAMUT_3D_MODE_5 2
#define GAMUT_3D_MODE_13 3

#define GAMUT_3D_MODE17_TBL_SZ 1229
#define GAMUT_3D_MODE5_TBL_SZ 32
#define GAMUT_3D_MODE13_TBL_SZ 550
#define GAMUT_3D_SCALE_OFF_SZ 16
#define GAMUT_3D_SCALEB_OFF_SZ 12
#define GAMUT_3D_TBL_NUM 4
#define GAMUT_3D_SCALE_OFF_TBL_NUM 3
#define GAMUT_3D_MAP_EN (1 << 0)

/**
 * struct drm_msm_3d_col - 3d gamut color component structure
 * @c0: Holds c0 value
 * @c2_c1: Holds c2/c1 values
 */
struct drm_msm_3d_col {
	__u32 c2_c1;
	__u32 c0;
};
/**
 * struct drm_msm_3d_gamut - 3d gamut feature structure
 * @flags: flags for the feature values are:
 *         0 - no map
 *         GAMUT_3D_MAP_EN - enable map
 * @mode: lut mode can take following values:
 *        - GAMUT_3D_MODE_17
 *        - GAMUT_3D_MODE_5
 *        - GAMUT_3D_MODE_13
 * @scale_off: Scale offset table
 * @col: Color component tables
 */
struct drm_msm_3d_gamut {
	__u64 flags;
	__u32 mode;
	__u32 scale_off[GAMUT_3D_SCALE_OFF_TBL_NUM][GAMUT_3D_SCALE_OFF_SZ];
	struct drm_msm_3d_col col[GAMUT_3D_TBL_NUM][GAMUT_3D_MODE17_TBL_SZ];
};

#define PGC_TBL_LEN 512
#define PGC_8B_ROUND (1 << 0)
/**
 * struct drm_msm_pgc_lut - pgc lut feature structure
 * @flags: flags for the featue values can be:
 *         - PGC_8B_ROUND
 * @c0: color0 component lut
 * @c1: color1 component lut
 * @c2: color2 component lut
 */
struct drm_msm_pgc_lut {
	__u64 flags;
	__u32 c0[PGC_TBL_LEN];
	__u32 c1[PGC_TBL_LEN];
	__u32 c2[PGC_TBL_LEN];
};


#define IGC_TBL_LEN 256
#define IGC_DITHER_ENABLE (1 << 0)
/**
 * struct drm_msm_igc_lut - igc lut feature structure
 * @flags: flags for the feature customization, values can be:
 *             - IGC_DITHER_ENABLE: Enable dither functionality
 * @c0: color0 component lut
 * @c1: color1 component lut
 * @c2: color2 component lut
 * @strength: dither strength, considered valid when IGC_DITHER_ENABLE
 *            is set in flags. Strength value based on source bit width.
 */
struct drm_msm_igc_lut {
	__u64 flags;
	__u32 c0[IGC_TBL_LEN];
	__u32 c1[IGC_TBL_LEN];
	__u32 c2[IGC_TBL_LEN];
	__u32 strength;
};

#define HIST_V_SIZE 256
/**
 * struct drm_msm_hist - histogram feature structure
 * @flags: for customizing operations
 * @data: histogram data
 */
struct drm_msm_hist {
	__u64 flags;
	__u32 data[HIST_V_SIZE];
};

#endif /* _MSM_DRM_PP_H_ */
