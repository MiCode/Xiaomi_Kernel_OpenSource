/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 * Shashank Sharma <shashank.sharma@intel.com>
 * Uma Shankar <uma.shankar@intel.com>
 * Shobhit Kumar <skumar40@intel.com>
 */

#ifndef _I915_CLR_MNGR_H_
#define _I915_CLR_MNGR_H_
#include "drmP.h"
#include "intel_drv.h"

struct cont_brightlut {
	u32 sprite_no;
	u32 val;
};

struct hue_saturationlut {
	u32 sprite_no;
	u32 val;
};

#define CLRMGR_PROP_MAX		10
#define CLRMGR_PROP_NAME_MAX	128

/* CSC correction */
#define CLRMGR_BASE   16
#define CSC_MAX_COEFF_COUNT		6
#define CLR_MGR_PARSE_MAX		128
#define PIPECONF_GAMMA			(1<<24)
#define GAMMA_CORRECT_MAX_COUNT 256
#define CRTC_ID_TOKEN_COUNT	1
#define ENABLE_TOKEN_MAX_COUNT	1
#define GC_MAX_COUNT 3
#define GAMMA_SP_MAX_COUNT		6
#define CB_MAX_COEFF_COUNT      2
#define HS_MAX_COEFF_COUNT      2
#define UPDATE_GAMMA_PRIMARY	(1 << 0)
#define UPDATE_GAMMA_SPRITE1	(1 << 1)
#define UPDATE_GAMMA_SPRITE2	(1 << 2)

/* Gamma correction defines */
#define GAMMA_MAX_VAL			1024
#define SHIFTBY6(val) (val<<6)
#define SHIFTRBY6(val) (val>>6)
#define SHIFTRBY2(val) (val>>2)
#define SHIFTLBY16(val) (val<<16)
#define SHIFTRBY16(val) (val>>16)
#define EVEN(val)	(2*val)
#define ODD(val)	((2*val)+1)
#define PIPEA_GAMMA_MAX_RED	(dev_priv->info.display_mmio_offset + 0x70010)
#define PIPEB_GAMMA_MAX_RED	(dev_priv->info.display_mmio_offset + 0x71010)
#define PIPE_GAMMA_MAX_RED(pipe)    _PIPE(pipe, PIPEA_GAMMA_MAX_RED, \
							PIPEB_GAMMA_MAX_RED)

#define PIPEA_GAMMA_MAX_GREEN	(dev_priv->info.display_mmio_offset + 0x70014)
#define PIPEB_GAMMA_MAX_GREEN	(dev_priv->info.display_mmio_offset + 0x71014)
#define PIPE_GAMMA_MAX_GREEN(pipe)  _PIPE(pipe, PIPEA_GAMMA_MAX_GREEN, \
							PIPEB_GAMMA_MAX_GREEN)

#define PIPEA_GAMMA_MAX_BLUE	(dev_priv->info.display_mmio_offset + 0x70018)
#define PIPEB_GAMMA_MAX_BLUE	(dev_priv->info.display_mmio_offset + 0x71018)
#define PIPE_GAMMA_MAX_BLUE(pipe) _PIPE(pipe, PIPEA_GAMMA_MAX_BLUE, \
							PIPEB_GAMMA_MAX_BLUE)

/* Sprite gamma correction regs */
#define GAMMA_SPA_GAMC0		(dev_priv->info.display_mmio_offset + 0x721F4)
#define GAMMA_SPA_GAMC1		(dev_priv->info.display_mmio_offset + 0x721F0)
#define GAMMA_SPA_GAMC2		(dev_priv->info.display_mmio_offset + 0x721EC)
#define GAMMA_SPA_GAMC3		(dev_priv->info.display_mmio_offset + 0x721E8)
#define GAMMA_SPA_GAMC4		(dev_priv->info.display_mmio_offset + 0x721E4)
#define GAMMA_SPA_GAMC5		(dev_priv->info.display_mmio_offset + 0x721E0)

#define GAMMA_SPB_GAMC0		(dev_priv->info.display_mmio_offset + 0x722F4)
#define GAMMA_SPB_GAMC1		(dev_priv->info.display_mmio_offset + 0x722F0)
#define GAMMA_SPB_GAMC2		(dev_priv->info.display_mmio_offset + 0x722EC)
#define GAMMA_SPB_GAMC3		(dev_priv->info.display_mmio_offset + 0x722E8)
#define GAMMA_SPB_GAMC4		(dev_priv->info.display_mmio_offset + 0x722E4)
#define GAMMA_SPB_GAMC5		(dev_priv->info.display_mmio_offset + 0x722E0)

#define GAMMA_SPC_GAMC0		(dev_priv->info.display_mmio_offset + 0x723F4)
#define GAMMA_SPC_GAMC1		(dev_priv->info.display_mmio_offset + 0x723F0)
#define GAMMA_SPC_GAMC2		(dev_priv->info.display_mmio_offset + 0x723EC)
#define GAMMA_SPC_GAMC3		(dev_priv->info.display_mmio_offset + 0x723E8)
#define GAMMA_SPC_GAMC4		(dev_priv->info.display_mmio_offset + 0x723E4)
#define GAMMA_SPC_GAMC5		(dev_priv->info.display_mmio_offset + 0x723E0)

#define GAMMA_SPD_GAMC0		(dev_priv->info.display_mmio_offset + 0x724F4)
#define GAMMA_SPD_GAMC1		(dev_priv->info.display_mmio_offset + 0x724F0)
#define GAMMA_SPD_GAMC2		(dev_priv->info.display_mmio_offset + 0x724EC)
#define GAMMA_SPD_GAMC3		(dev_priv->info.display_mmio_offset + 0x724E8)
#define GAMMA_SPD_GAMC4		(dev_priv->info.display_mmio_offset + 0x724E4)
#define GAMMA_SPD_GAMC5		(dev_priv->info.display_mmio_offset + 0x724E0)

#define GAMMA_SP1_GAMC0(pipe)	_PIPE(pipe, GAMMA_SPA_GAMC0, GAMMA_SPC_GAMC0)
#define GAMMA_SP2_GAMC0(pipe)	_PIPE(pipe, GAMMA_SPB_GAMC0, GAMMA_SPD_GAMC0)


#define GAMMA_SPA_CNTRL		(dev_priv->info.display_mmio_offset + 0x72180)
#define GAMMA_SPB_CNTRL		(dev_priv->info.display_mmio_offset + 0x72280)
#define GAMMA_SPC_CNTRL		(dev_priv->info.display_mmio_offset + 0x72380)
#define GAMMA_SPD_CNTRL		(dev_priv->info.display_mmio_offset + 0x72480)
#define GAMMA_SP1_CNTRL(pipe)	_PIPE(pipe, GAMMA_SPA_CNTRL, GAMMA_SPC_CNTRL)
#define GAMMA_SP2_CNTRL(pipe)	_PIPE(pipe, GAMMA_SPB_CNTRL, GAMMA_SPD_CNTRL)


#define GAMMA_ENABLE_SPR			(1<<30)
#define GAMMA_SP_MAX_COUNT			6
#define NO_SPRITE_REG				4
#define MAX_PIPES_VLV				2
#define PIPEA_CGM_CTRL      0x67A00
#define PIPEB_CGM_CTRL      0x69A00
#define PIPEC_CGM_CTRL      0x6BA00
#define CGM_DEGAMMA_EN      1
#define CGM_CSC_EN          2
#define CGM_GAMMA_EN        4
#define CGM_CSC_MAX_REGS	5

#define CHV_CGM_CSC_MATRIX_MAX_VALS    9
#define CHV_CGM_GAMMA_MATRIX_MAX_VALS   257
#define CHV_CGM_DEGAMMA_MATRIX_MAX_VALS	65

#define PIPEA_CGM_DEGAMMA_ST 0x66000
#define PIPEA_CGM_GAMMA_ST   0x67000
#define PIPEA_CGM_CSC_ST     0x67900
#define PIPEB_CGM_DEGAMMA_ST 0x68000
#define PIPEB_CGM_GAMMA_ST   0x69000
#define PIPEB_CGM_CSC_ST     0x69900
#define PIPEC_CGM_DEGAMMA_ST 0x6A000
#define PIPEC_CGM_GAMMA_ST   0x6B000
#define PIPEC_CGM_CSC_ST     0x6B900

/* Color manager features */
enum clrmgr_tweaks {
	cgm_csc = 0,
	cgm_gamma,
	cgm_degamma,
	tweak_invalid
};

/* Color manager features */
enum clrmgrfeatures {
	clrmgrcsc = 1,
	clrmgrgamma,
	clrmgrcontrbright,
	clrmgrhuesat,
};

struct gamma_lut_data {
	u16 red;
	u16 green;
	u16 blue;
};

struct lut_info {
	bool enable;
	u32 len;
	void *data;
};

/*
* clrmgr_regd_propery structure
* This structure encapsulates drm_property, and some
* additional values which are required during the runtime
* after registration.
*/
struct clrmgr_regd_prop {
	bool enabled;
	struct drm_property *property;

	/*
	* A void * is first arg, so that the same function ptr can be used
	* for both crtc_property and plane_property
	*/
	bool (*set_property)(void *,
		const struct clrmgr_regd_prop *prop, const struct lut_info *);
};

/*
* clrmgr_propery structure
* This structure encapsulates drm_property with other
* values required during the property registration time.
*/
struct clrmgr_property {
	enum clrmgr_tweaks tweak_id;
	u32 type;
	u32 len;
	u64 min;
	u64 max;
	char name[CLRMGR_PROP_NAME_MAX];
	bool (*set_property)(void *, const struct clrmgr_regd_prop *,
						const struct lut_info *);
};

enum clr_property_type {
	clr_property_pipe = 0,
	clr_property_plane
};

/* Request to register property */
struct clrmgr_reg_request {
	u32 no_of_properties;
	struct clrmgr_property cp[CLRMGR_PROP_MAX];
};

/* Status of color properties on pipe at any time */
struct clrmgr_status {
	u32 no_of_properties;
	struct clrmgr_regd_prop *cp[CLRMGR_PROP_MAX];
};
/* Required for sysfs entry calls */
extern u32 csc_softlut[MAX_PIPES_VLV][CSC_MAX_COEFF_COUNT];
extern u32 gamma_softlut[MAX_PIPES_VLV][GAMMA_CORRECT_MAX_COUNT];
extern u32 gcmax_softlut[MAX_PIPES_VLV][GC_MAX_COUNT];
extern u32 gamma_sprite_softlut[GAMMA_SP_MAX_COUNT];

/* Prototypes */
int parse_clrmgr_input(uint *dest, char *src, int max, int *num_bytes);
int do_intel_enable_csc(struct drm_device *dev, void *data,
				struct drm_crtc *crtc);
void do_intel_disable_csc(struct drm_device *dev, struct drm_crtc *crtc);
int intel_crtc_enable_gamma(struct drm_crtc *crtc, u32 identifier);

bool intel_clrmgr_set_cgm_csc(void *crtc,
				const struct clrmgr_regd_prop *cgm_csc,
				const struct lut_info *info);
bool intel_clrmgr_set_cgm_gamma(void *crtc,
				const struct clrmgr_regd_prop *cgm_gamma,
				const struct lut_info *info);
bool intel_clrmgr_set_cgm_degamma(void *crtc,
				const struct clrmgr_regd_prop *cgm_gamma,
				const struct lut_info *info);
int intel_crtc_disable_gamma(struct drm_crtc *crtc, u32 identifier);
int intel_sprite_cb_adjust(struct drm_i915_private *dev_priv,
		struct cont_brightlut *cb_ptr);
int intel_sprite_hs_adjust(struct drm_i915_private *dev_priv,
		struct hue_saturationlut *hs_ptr);
void intel_save_clr_mgr_status(struct drm_device *dev);
bool intel_restore_clr_mgr_status(struct drm_device *dev);

/*
 * intel_clrmgr_set_property
 * Set the value of a DRM color correction property
 * and program the corresponding registers
 * Inputs:
 *  - intel_crtc *
 *  - color manager registered property * which encapsulates
 *    drm_property and additional data.
 * - value is a pointer to the lut information.
 */
bool intel_clrmgr_set_pipe_property(struct intel_crtc *intel_crtc,
		struct clrmgr_regd_prop *cp, uint64_t value);
bool intel_clrmgr_register_pipe_property(struct intel_crtc *intel_crtc,
		struct clrmgr_reg_request *features);

/*
 * intel_attach_pipe_color_correction:
 * register color correction properties as DRM CRTC properties
 * input:
 * - intel_crtc : CRTC to attach color correcection with
 */
void
intel_attach_pipe_color_correction(struct intel_crtc *intel_crtc);

/*
 * intel_clrmgr_deregister
 * De register color manager properties
 * destroy the DRM property and cleanup
 * input:
 * - struct drm device *dev
 * - status: attached colot status
 */
void intel_clrmgr_deregister(struct drm_device *dev,
	struct clrmgr_status *status);

/*
 * intel_clrmgr_init:
 * allocate memory to save color correction
 * status per pipe
 * input: struct drm_device
 */
struct clrmgr_status *intel_clrmgr_init(struct drm_device *dev);

/*
 * intel_clrmgr_exit
 * Free allocated memory for color status
 * Should be called from CRTC/Plane .destroy function
 * input: color status
 */
void intel_clrmgr_exit(struct drm_device *dev, struct clrmgr_status *status);
#endif
