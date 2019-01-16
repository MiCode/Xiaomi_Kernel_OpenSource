#ifndef __CONN_MD_DBG_H_
#define __CONN_MD_DBG_H_

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#define ARRAY_SIZE(x) sizeof(x)/sizeof(x[0])
#endif

typedef int (*CONN_MD_DEV_DBG_FUNC) (int par1, int par2, int par3);

extern int conn_md_dbg_init(void);

extern int conn_md_test(void);

#endif/*__CONN_MD_DBG_H_*/
