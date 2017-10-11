#ifndef _MSM_DRM_PP_H_
#define _MSM_DRM_PP_H_

#include <linux/types.h>
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
 * @flags: for customizing operations
 * @r: red coefficients.
 * @g: green coefficients.
 * @b: blue coefficients.
 * @r_rr: second order coefficients
 * @r_gg: second order coefficients
 * @r_bb: second order coefficients
 * @g_rr: second order coefficients
 * @g_gg: second order coefficients
 * @g_bb: second order coefficients
 * @b_rr: second order coefficients
 * @b_gg: second order coefficients
 * @b_bb: second order coefficients
 */
#define DRM_MSM_PCC3
struct drm_msm_pcc {
	__u64 flags;
	struct drm_msm_pcc_coeff r;
	struct drm_msm_pcc_coeff g;
	struct drm_msm_pcc_coeff b;
	__u32 r_rr;
	__u32 r_gg;
	__u32 r_bb;
	__u32 g_rr;
	__u32 g_gg;
	__u32 g_bb;
	__u32 b_rr;
	__u32 b_gg;
	__u32 b_bb;
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

/* struct drm_msm_memcol - Memory color feature structure.
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

#define AD4_LUT_GRP0_SIZE 33
#define AD4_LUT_GRP1_SIZE 32
/*
 * struct drm_msm_ad4_init - ad4 init structure set by user-space client.
 *                           Init param values can change based on tuning
 *                           hence it is passed by user-space clients.
 */
struct drm_msm_ad4_init {
	__u32 init_param_001[AD4_LUT_GRP0_SIZE];
	__u32 init_param_002[AD4_LUT_GRP0_SIZE];
	__u32 init_param_003[AD4_LUT_GRP0_SIZE];
	__u32 init_param_004[AD4_LUT_GRP0_SIZE];
	__u32 init_param_005[AD4_LUT_GRP1_SIZE];
	__u32 init_param_006[AD4_LUT_GRP1_SIZE];
	__u32 init_param_007[AD4_LUT_GRP0_SIZE];
	__u32 init_param_008[AD4_LUT_GRP0_SIZE];
	__u32 init_param_009;
	__u32 init_param_010;
	__u32 init_param_011;
	__u32 init_param_012;
	__u32 init_param_013;
	__u32 init_param_014;
	__u32 init_param_015;
	__u32 init_param_016;
	__u32 init_param_017;
	__u32 init_param_018;
	__u32 init_param_019;
	__u32 init_param_020;
	__u32 init_param_021;
	__u32 init_param_022;
	__u32 init_param_023;
	__u32 init_param_024;
	__u32 init_param_025;
	__u32 init_param_026;
	__u32 init_param_027;
	__u32 init_param_028;
	__u32 init_param_029;
	__u32 init_param_030;
	__u32 init_param_031;
	__u32 init_param_032;
	__u32 init_param_033;
	__u32 init_param_034;
	__u32 init_param_035;
	__u32 init_param_036;
	__u32 init_param_037;
	__u32 init_param_038;
	__u32 init_param_039;
	__u32 init_param_040;
	__u32 init_param_041;
	__u32 init_param_042;
	__u32 init_param_043;
	__u32 init_param_044;
	__u32 init_param_045;
	__u32 init_param_046;
	__u32 init_param_047;
	__u32 init_param_048;
	__u32 init_param_049;
	__u32 init_param_050;
	__u32 init_param_051;
	__u32 init_param_052;
	__u32 init_param_053;
	__u32 init_param_054;
	__u32 init_param_055;
	__u32 init_param_056;
	__u32 init_param_057;
	__u32 init_param_058;
	__u32 init_param_059;
	__u32 init_param_060;
	__u32 init_param_061;
	__u32 init_param_062;
	__u32 init_param_063;
	__u32 init_param_064;
	__u32 init_param_065;
	__u32 init_param_066;
	__u32 init_param_067;
	__u32 init_param_068;
	__u32 init_param_069;
	__u32 init_param_070;
	__u32 init_param_071;
	__u32 init_param_072;
	__u32 init_param_073;
	__u32 init_param_074;
	__u32 init_param_075;
};

/*
 * struct drm_msm_ad4_cfg - ad4 config structure set by user-space client.
 *                           Config param values can vary based on tuning,
 *                           hence it is passed by user-space clients.
 */
struct drm_msm_ad4_cfg {
	__u32 cfg_param_001;
	__u32 cfg_param_002;
	__u32 cfg_param_003;
	__u32 cfg_param_004;
	__u32 cfg_param_005;
	__u32 cfg_param_006;
	__u32 cfg_param_007;
	__u32 cfg_param_008;
	__u32 cfg_param_009;
	__u32 cfg_param_010;
	__u32 cfg_param_011;
	__u32 cfg_param_012;
	__u32 cfg_param_013;
	__u32 cfg_param_014;
	__u32 cfg_param_015;
	__u32 cfg_param_016;
	__u32 cfg_param_017;
	__u32 cfg_param_018;
	__u32 cfg_param_019;
	__u32 cfg_param_020;
	__u32 cfg_param_021;
	__u32 cfg_param_022;
	__u32 cfg_param_023;
	__u32 cfg_param_024;
	__u32 cfg_param_025;
	__u32 cfg_param_026;
	__u32 cfg_param_027;
	__u32 cfg_param_028;
	__u32 cfg_param_029;
	__u32 cfg_param_030;
	__u32 cfg_param_031;
	__u32 cfg_param_032;
	__u32 cfg_param_033;
	__u32 cfg_param_034;
	__u32 cfg_param_035;
	__u32 cfg_param_036;
	__u32 cfg_param_037;
	__u32 cfg_param_038;
	__u32 cfg_param_039;
	__u32 cfg_param_040;
	__u32 cfg_param_041;
	__u32 cfg_param_042;
	__u32 cfg_param_043;
	__u32 cfg_param_044;
	__u32 cfg_param_045;
	__u32 cfg_param_046;
	__u32 cfg_param_047;
	__u32 cfg_param_048;
	__u32 cfg_param_049;
	__u32 cfg_param_050;
	__u32 cfg_param_051;
	__u32 cfg_param_052;
	__u32 cfg_param_053;
};

#define DITHER_MATRIX_SZ 16

/**
 * struct drm_msm_dither - dither feature structure
 * @flags: for customizing operations
 * @temporal_en: temperal dither enable
 * @c0_bitdepth: c0 component bit depth
 * @c1_bitdepth: c1 component bit depth
 * @c2_bitdepth: c2 component bit depth
 * @c3_bitdepth: c2 component bit depth
 * @matrix: dither strength matrix
 */
struct drm_msm_dither {
	__u64 flags;
	__u32 temporal_en;
	__u32 c0_bitdepth;
	__u32 c1_bitdepth;
	__u32 c2_bitdepth;
	__u32 c3_bitdepth;
	__u32 matrix[DITHER_MATRIX_SZ];
};

#endif /* _MSM_DRM_PP_H_ */
