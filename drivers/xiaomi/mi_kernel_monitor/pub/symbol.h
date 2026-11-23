

#ifndef __PUB_SYMBOL_H
#define __PUB_SYMBOL_H

extern unsigned long (*xm_kallsyms_lookup_name)(const char *name);
extern int xiaomi_get_symbol_count(char *symbol);
extern int xiaomi_init_symbol(void);

#endif /* __PUB_SYMBOL_H */

