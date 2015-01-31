#ifndef __I915_ADF_WRAPPER_H__
#define __I915_ADF_WRAPPER_H__

#include <drm/i915_drm.h>
#include "i915_drv.h"

/* i915_adf_wrapper.c */
#ifdef CONFIG_ADF_INTEL
void i915_adf_wrapper_init(struct drm_i915_private *dev_priv);
void i915_adf_wrapper_teardown(void);
void vlv_adf_sideband_rw(struct drm_i915_private *dev_priv, u32 devfn,
			u32 port, u32 opcode, u32 reg, u32 *val);
extern struct intel_adf_context *adf_ctx;
extern void intel_adf_hpd_init(struct intel_adf_context *ctx);
#endif

#endif /* __I915_ADF_WRAPPER_H__ */
