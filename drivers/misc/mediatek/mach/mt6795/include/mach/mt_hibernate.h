#ifndef __MT_HIBERNATE_H__
#define __MT_HIBErNATE_H__

#define POWERMODE_HIBERNATE 0xadfeefda

int notrace swsusp_arch_save_image(unsigned long unused);

#endif /* __MT_HIBERNATE_H__ */
