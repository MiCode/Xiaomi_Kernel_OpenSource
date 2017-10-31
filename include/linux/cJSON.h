/*
 * Copyright (c) 2009 Dave Gamble
 * Copyright (C) 2017 XiaoMi, Inc.
 */
#ifndef _LINUX_CJSON_H
#define _LINUX_CJSON_H

/* cJSON Types: */
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)

#define cJSON_IsReference 256

/* The cJSON structure: */
typedef struct cJSON {
	struct cJSON *next, *prev;	/* next/prev allow you to walk
					   array/object chains. Alternatively,
				use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *child;	/* An array or object item will have a child
				   pointer pointing to a chain of the items
				   in the array/object. */

	int type;		/* The type of the item, as above. */

	char *valuestring;	/* The item's string, if type==cJSON_String */
	int valueint;		/* The item's number, if type==cJSON_Number */

	char *string;		/* The item's name string, if this item is
				   the child of, or is in the list of
				   subitems of an object. */
} cJSON;

char *cJSON_Print(cJSON *item);
/* Delete a cJSON entity and all subentities. */
void cJSON_Delete(cJSON *c);

/* Get item "string" from object. Case insensitive. */
cJSON *cJSON_GetObjectItem(cJSON *object, const char *string);

/* Get item "string" from object. Case insensitive. */
int cJSON_HasObjectItem(cJSON *object, const char *string);

/* These calls create a cJSON item of the appropriate type. */
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateObject(void);

/* For analysing failed parses. This returns a pointer to the parse error.
 * You'll probably need to look a few chars back to make sense of it.
 * Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
const char *cJSON_GetErrorPtr(void);

/* Append item to the specified array/object. */
void cJSON_AddItemToObject(cJSON *object, const char *string,
			   cJSON *item);
/* Remove/Detatch items from Arrays/Objects. */
cJSON *cJSON_DetachItemFromObject(cJSON *object,
				  const char *string);
void cJSON_DeleteItemFromObject(cJSON *object, const char *string);

/* Update array items. */
void cJSON_ReplaceItemInObject(cJSON *object, const char *string,
			       cJSON *newitem);

/* Macros for creating things quickly. */
#define cJSON_AddStringToObject(object, name, s) \
	cJSON_AddItemToObject(object, name, cJSON_CreateString(s))

#endif
