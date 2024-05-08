#ifndef _IO_LIMIT_H_
#define _IO_LIMIT_H_

#define NATIVE_ADJ -1000
#define FOREGROUND_APP_ADJ 0
#define PERCEPTIBLE_APP_ADJ 250
#define SERVICE_ADJ 500
#define PREVIOUS_APP_ADJ 700
#define SERVICE_B_ADJ 800

int rbtree_show(char *buf);
int rbtree_store(const char *buf, size_t len);
bool init_limit_node_cachep(void);
void del_limit_node(pid_t pid);
bool check_pid_in_limit_list(pid_t pid);
void free_limit_node_cachep(void);

#endif