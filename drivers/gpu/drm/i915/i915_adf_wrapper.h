#ifndef __I915_ADF_WRAPPER_H__
#define __I915_ADF_WRAPPER_H__

#include <drm/i915_drm.h>
#include "i915_drv.h"

/* i915_adf_wrapper.c */
#ifdef CONFIG_ADF_INTEL
void i915_adf_wrapper_init(struct drm_i915_private *dev_priv);
void i915_adf_wrapper_teardown(void);
#endif

#endif /* __I915_ADF_WRAPPER_H__ */
