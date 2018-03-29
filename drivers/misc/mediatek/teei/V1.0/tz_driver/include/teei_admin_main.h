#ifndef __TZ_TEEI_ADMIN_MAIN_H__
#define __TZ_TEEI_ADMIN_MAIN_H__

extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
extern int mt_eint_set_deint(int eint_num, int irq_num);
extern int mt_eint_clr_deint(int eint_num);
extern void neu_disable_touch_irq(void);
extern void neu_enable_touch_irq(void);
extern void *tz_malloc_shared_mem(size_t size, int flags);
extern void tz_free_shared_mem(void *addr, size_t size);

#endif /* __TZ_TEEI_ADMIN_MAIN_H__ */
