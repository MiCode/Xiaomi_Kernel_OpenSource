#ifndef __MT_LASTPC_H__
#define __MT_LASTPC_H__

/* public APIs for those want to dump lastpc to buf, the len of buf is len */
int lastpc_dump(char *buf, int len);

//for backward-compatibilty
int mt_reg_dump(char *buf);

#endif //end of __MT_LASTPC_H__
