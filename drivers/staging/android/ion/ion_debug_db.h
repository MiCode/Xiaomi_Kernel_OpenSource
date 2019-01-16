#ifndef ION_DEBUG_DB_H
#define ION_DEBUG_DB_H

/* NOTE: This is only included in ion.c */

/*
 * ION Debug DB Organization
 */

/* DB Checking Leakage enums */
enum dbcl_types
{
	DBCL_CLIENT,
	DBCL_BUFFER,
	DBCL_MMAP,
	DBCL_FD,
	_TOTAL_DBCL,
};

const char *dbcl_child_name[_TOTAL_DBCL]
	= { "client",
	    "buffer",
	    "mmap",
	    "fd",
	};

/* DB Child for Checking Leakage */
struct dbcl_child {
	void *raw_key;
	struct dentry *root;
	struct dentry *type[_TOTAL_DBCL];
	atomic_t refcount;
	struct list_head entry;
};

/* Checking Leakage DB */
struct debug_dbcl {
	struct list_head child;		/* Pids */
};

/* DB Ion Statistics enums */
enum dbis_types
{
	/* member */
	DBIS_CLIENTS = 0,
	DBIS_BUFFERS,
	DBIS_MMAPS,
	DBIS_FDS,
	DBIS_PIDS,
	_TOTAL_DBIS,

	/* attr */
	DBIS_FILE,
	DBIS_DIR,
};

/* DB Child for Ion Statistics */
struct dbis_child {
	enum dbis_types attr;
	char *name;
};

struct dbis_child dbis_child_attr[] 
		= { 
		    {DBIS_FILE, "clients"},
		    {DBIS_FILE, "buffers"},
		    {DBIS_FILE, "mmaps"},
		    {DBIS_FILE, "fds"},
		    {DBIS_FILE, "pids"},
		    {DBIS_DIR,  "history_record"},
		    {DBIS_FILE, "history_clients"},
		    {DBIS_FILE, "history_buffers"},
		    {DBIS_FILE, "history_mmaps"},
		    {DBIS_FILE, "history_fds"},
		    {DBIS_FILE, "history_pids"},
		};

/* Ion Statistics DB */
struct debug_dbis {
	struct dentry *child[_TOTAL_DBIS+1];
	/* This is for history */
	struct dentry *history_record[_TOTAL_DBIS];	/* buffers, mmaps, fds */
};

/* ION Debug DB Root */
struct ion_debug_db {
	struct dentry *checking_leakage;
	struct debug_dbcl dbcl;
	struct dentry *ion_statistics;
	struct debug_dbis dbis;
};

/*
 * ION Debug DB - DebugFS
 */
static struct ion_debug_db debug_db_root;

/* These are used in ion_debug_dbis_show "case DBIS_PIDS:" */
struct dbis_client_entry {
	void *client;
	struct dbis_client_entry *next;
};
struct dbis_process_entry {
	pid_t pid;
	struct dbis_client_entry *clients;
	struct dbis_process_entry *next;
};
/*
 *     proclist          /---> 1st_pe           /---> 2nd_pe           /---> 3rd_pe
 *     .pid = -1,        |     .pid = pid1,     |     .pid = pid2,     |
 *     .clients = NULL,  |     .clients = ...,  |     .clients = ...,  |      ... ...
 *     .next = ----------/     .next = ---------/     .next = ---------/
 *
 *     (pid1 < pid2 < pid3 ...)
 *
 *     clients               /---> 2nd_ce                /---> 3rd_ce
 *     .client = (client1),  |     .client = (client2),  |     
 *     .next = --------------/     .next = --------------/      ... ...
 */
static inline void dbis_insert_proc_clients(struct dbis_process_entry *plist, void *client, pid_t pid)
{
	struct dbis_client_entry *client_entry = NULL;
	struct dbis_process_entry *process_entry = NULL, *pe_pos = NULL;
	struct dbis_process_entry **next_pe;
	struct dbis_client_entry **next_ce;

	/* Allocate client_entry & initialize it */
	client_entry = kmalloc(sizeof(struct dbis_client_entry), GFP_KERNEL);
	if (!client_entry)
		return;
	client_entry->client = client;
	client_entry->next = NULL;

	/* Insert it in the process_entry clients list */
	//next_pe = &plist->next;
	next_pe = &plist;
	while (*next_pe) {
		/* The same process */
		if ((*next_pe)->pid == pid) {
			next_ce = &(*next_pe)->clients;
			while (*next_ce) {
				next_ce = &(*next_ce)->next;
			}
			*next_ce = client_entry;
//			(*next_ce)->next = client_entry;
			return;
		}
		/* Record the pos */
		if ((*next_pe)->pid < pid) {
			pe_pos = *next_pe;
		} else {
			break;
		}
		next_pe = &(*next_pe)->next;
	}
	
	/* No found process */
	process_entry = kmalloc(sizeof(struct dbis_process_entry), GFP_KERNEL);
	if (!process_entry) {
		kfree(client_entry);
		return;
	}
	process_entry->pid = pid;
	process_entry->clients = client_entry;
	process_entry->next = NULL;

	/* Chain the new pe to proclist */
#if 0
	if (!pe_pos) {
		*next_pe = process_entry;
	} else {
#endif
		process_entry->next = pe_pos->next;
		pe_pos->next = process_entry;
//	}
}
static inline void destroy_proclist(struct dbis_process_entry *plist)
{
	struct dbis_process_entry *next_pe, *pe_free;
	struct dbis_client_entry *next_ce, *ce_free;

	/* Go through pe list & free them */
	next_pe = plist->next;
	while(next_pe) {
		pe_free = next_pe;
		next_pe = next_pe->next;
		/* Go through ce list & free them */
		next_ce = pe_free->clients;
		while (next_ce) {
			ce_free = next_ce;
			next_ce = next_ce->next;
			/* free ce */
			kfree(ce_free);
		}
		/* free pe */
		kfree(pe_free);
	}
}

#endif // ION_DEBUG_DB_H
