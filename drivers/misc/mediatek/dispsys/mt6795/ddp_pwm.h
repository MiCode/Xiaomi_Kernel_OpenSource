#ifndef __DDP_PWM_H__
#define __DDP_PWM_H__


typedef enum {
	DISP_PWM0 = 0x1,
	DISP_PWM1 = 0x2,
	DISP_PWM_ALL = (DISP_PWM0 | DISP_PWM1)
} disp_pwm_id_t;


void disp_pwm_set_main(disp_pwm_id_t main);
disp_pwm_id_t disp_pwm_get_main(void);

int disp_pwm_is_enabled(disp_pwm_id_t id);

int disp_pwm_set_backlight(disp_pwm_id_t id, int level_1024);
int disp_pwm_set_backlight_cmdq(disp_pwm_id_t id, int level_1024, void *cmdq);

int disp_pwm_set_max_backlight(disp_pwm_id_t id, unsigned int level_1024);
int disp_pwm_get_max_backlight(disp_pwm_id_t id);

/* For backward compatible */
int disp_bls_set_max_backlight(unsigned int level_1024);
int disp_bls_set_backlight(int level_1024);

void disp_pwm_test(const char *cmd, char *debug_output);

#endif
