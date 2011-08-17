#ifndef __ASM_ARM_ARCH_FLASHLIGHT_H
#define __ASM_ARM_ARCH_FLASHLIGHT_H

#define FLASHLIGHT_NAME "flashlight"

#define FLASHLIGHT_OFF   0
#define FLASHLIGHT_TORCH 1
#define FLASHLIGHT_FLASH 2
#define FLASHLIGHT_NUM   3

struct flashlight_platform_data {
	int (*gpio_init) (void);
	int torch;
	int flash;
	int flash_duration_ms;
};

int flashlight_control(int level);

#endif /*__ASM_ARM_ARCH_FLASHLIGHT_H*/
