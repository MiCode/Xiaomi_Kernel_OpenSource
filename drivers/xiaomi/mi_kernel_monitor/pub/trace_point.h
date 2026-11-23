

#ifndef __PUB_TRACE_POINT_H
#define __PUB_TRACE_POINT_H

extern int hook_tracepoint(const char *name, void *probe, void *data);
extern int unhook_tracepoint(const char *name, void *probe, void *data);

#endif /* __PUB_TRACE_POINT_H */
