#ifndef __EINT_DRV_H
#define __EINT_DRV_H
#include <mach/eint.h>

struct mt_eint_driver
{
    struct platform_driver driver;
    int (*eint_max_channel)(void);
    void (*enable)(unsigned int eint_num);
    void (*disable)(unsigned int eint_num);
    unsigned int (*is_disable)(unsigned int eint_num);
    unsigned int (*get_sens)(unsigned int eint_num);
    unsigned int (*set_sens)(unsigned int eint_num, unsigned int sens);
    unsigned int (*get_polarity)(unsigned int eint_num);
    void (*set_polarity)(unsigned int eint_num, unsigned int pol);
    unsigned int (*get_debounce_cnt)(unsigned int eint_num);
    void (*set_debounce_cnt)(unsigned int eint_num, unsigned int ms);
    int (*is_debounce_en)(unsigned int eint_num);
    void (*enable_debounce)(unsigned int eint_num);
    void (*disable_debounce)(unsigned int eint_num);
    unsigned int (*get_count)(unsigned int eint_num);
};

struct mt_eint_driver *get_mt_eint_drv(void);

extern int eint_drv_get_max_channel(void);
extern unsigned int eint_drv_get_count(unsigned int eint_num);

#endif
