#ifndef HALLS_H
#define HALLS_H

typedef void (*halls_status_notify)(int halls_status); extern void halls_register_halls_status_notify(halls_status_notify halls_cb);

#endif /* HALLS_H */
