#ifndef SECMOD_H
#define SECMOD_H

#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>

struct sec_ops {
    int (*sec_get_rid)(unsigned int *rid);
};

struct sec_mod {
    dev_t                 id;
    int                   init;
    spinlock_t            lock;
    const struct sec_ops *ops;
};

#endif /* end of SECMOD_H */
