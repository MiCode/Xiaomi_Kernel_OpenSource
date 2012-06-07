#ifndef __LINUX_PWM_H
#define __LINUX_PWM_H

struct pwm_device;

/* Add __weak functions to support PWM */

/*
 * pwm_request - request a PWM device
 */
struct pwm_device __weak *pwm_request(int pwm_id, const char *label);

/*
 * pwm_free - free a PWM device
 */
void __weak pwm_free(struct pwm_device *pwm);

/*
 * pwm_config - change a PWM device configuration
 */
int __weak pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns);

/*
 * pwm_enable - start a PWM output toggling
 */
int __weak pwm_enable(struct pwm_device *pwm);

/*
 * pwm_disable - stop a PWM output toggling
 */
void __weak pwm_disable(struct pwm_device *pwm);

#endif /* __LINUX_PWM_H */
