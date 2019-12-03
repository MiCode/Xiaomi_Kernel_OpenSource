/*
 * File loading code for Mini-XML, a small XML file parsing library.
 *
 * Copyright 2003-2017 by Michael R Sweet.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Michael R Sweet and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "COPYING"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at:
 *
 *     https://michaelrsweet.github.io/mxml
 */

/*
 * Include necessary headers...
 */

#ifndef WIN32
#endif /* !WIN32 */
#include "mxml-private.h"
#include <linux/errno.h>

/*
 * Character encoding...
 */

#define ENCODE_UTF8		0	/* UTF-8 */
#define ENCODE_UTF16BE	1	/* UTF-16 Big-Endian */
#define ENCODE_UTF16LE	2	/* UTF-16 Little-Endian */

/*
 * Macro to test for a bad XML character...
 */

#define mxml_bad_char(ch) ((ch) < ' ' && (ch) != '\n' && (ch) != '\r' && (ch) != '\t')

/*
 * Types and structures...
 */

typedef int (*_mxml_getc_cb_t) (void *, int *);
typedef int (*_mxml_putc_cb_t) (int, void *);

typedef struct _mxml_fdbuf_s {
/**** File descriptor buffer ****/
	int fd;					/* File descriptor */
	unsigned char *curr,	/* Current position in buffer */
	*end,					/* End of buffer */
	buffer[8192];			/* Character buffer */
} _mxml_fdbuf_t;

/*
 * Local functions...
 */

static int mxml_add_char(int ch, char **ptr, char **buffer, int *bufsize);

static int mxml_get_entity(mxml_node_t *parent, void *p,
		int *encoding, _mxml_getc_cb_t getc_cb);
static inline int mxml_isspace(int ch)
{
	return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

static mxml_node_t *mxml_load_data(mxml_node_t *top, void *p,
		mxml_load_cb_t cb, _mxml_getc_cb_t getc_cb,
		mxml_sax_cb_t sax_cb, void *sax_data);
static int mxml_parse_element(mxml_node_t *node, void *p,
		int *encoding, _mxml_getc_cb_t getc_cb);
static int mxml_string_getc(void *p, int *encoding);
static int mxml_string_putc(int ch, void *p);
static int mxml_write_name(const char *s, void *p, _mxml_putc_cb_t putc_cb);
static int mxml_write_node(mxml_node_t *node, void *p,
		mxml_save_cb_t cb, int col, _mxml_putc_cb_t putc_cb, _mxml_global_t *global);
static int mxml_write_string(const char *s, void *p, _mxml_putc_cb_t putc_cb);
static int mxml_write_ws(mxml_node_t *node, void *p,
		mxml_save_cb_t cb, int ws, int col, _mxml_putc_cb_t putc_cb);


/*
 * 'mxmlLoadString()' - Load a string into an XML node tree.
 *
 * The nodes in the specified string are added to the specified top node.
 * If no top node is provided, the XML string MUST be well-formed with a
 * single parent node like <?xml> for the entire string. The callback
 * function returns the value type that should be used for child nodes.
 * The constants @code MXML_INTEGER_CALLBACK@, @code MXML_OPAQUE_CALLBACK@,
 * @code MXML_REAL_CALLBACK@, and @code MXML_TEXT_CALLBACK@ are defined for
 * loading child (data) nodes of the specified type.
 */

mxml_node_t *		/* O - First node or @code NULL@ if the string has errors. */
mxmlLoadString(mxml_node_t *top,	/* I - Top node */
			const char *s,			/* I - String to load */
			mxml_load_cb_t cb)
{									/* I - Callback function or constant */
	/*
	 * Read the XML data...
	 */

	return (mxml_load_data
		(top, (void *)&s, cb, mxml_string_getc, MXML_NO_CALLBACK,
		NULL));
}

/*
 * 'mxmlSaveAllocString()' - Save an XML tree to an allocated string.
 *
 * This function returns a pointer to a string containing the textual
 * representation of the XML node tree.  The string should be freed
 * using the free() function when you are done with it.  @code NULL@ is returned
 * if the node would produce an empty string or if the string cannot be
 * allocated.
 *
 * The callback argument specifies a function that returns a whitespace
 * string or NULL before and after each element.  If @code MXML_NO_CALLBACK@
 * is specified, whitespace will only be added before @code MXML_TEXT@ nodes
 * with leading whitespace and before attribute names inside opening
 * element tags.
 */

char *				/* O - Allocated string or @code NULL@ */
mxmlSaveAllocString(mxml_node_t *node,	/* I - Node to write */
		mxml_save_cb_t cb)
{				/* I - Whitespace callback or @code MXML_NO_CALLBACK@ */
	int bytes;		/* Required bytes */
	//char buffer[2000];	/* Temporary buffer */
	char buffer[512];	/* Temporary buffer */
	char *s;		/* Allocated string */

	/*
	 * Write the node to the temporary buffer...
	 */

	bytes = mxmlSaveString(node, buffer, sizeof(buffer), cb);

	if (bytes <= 0)
		return NULL;
	if (bytes < (int)(sizeof(buffer) - 1)) {
		/*
		 * Node fit inside the buffer, so just duplicate that string and
		 * return...
		 */
		return strdup(buffer);
	}
	/*
	 * Allocate a buffer of the required size and save the node to the
	 * new buffer...
	 */
	s = malloc(bytes + 1);
	if (s == NULL)
		return NULL;

	mxmlSaveString(node, s, bytes + 1, cb);
	/* Return the allocated string... */
	return s;
}

/*
 * 'mxmlSaveString()' - Save an XML node tree to a string.
 *
 * This function returns the total number of bytes that would be
 * required for the string but only copies (bufsize - 1) characters
 * into the specified buffer.
 *
 * The callback argument specifies a function that returns a whitespace
 * string or NULL before and after each element. If @code MXML_NO_CALLBACK@
 * is specified, whitespace will only be added before @code MXML_TEXT@ nodes
 * with leading whitespace and before attribute names inside opening
 * element tags.
 */
/* O - Size of string */
int mxmlSaveString(mxml_node_t *node,	/* I - Node to write */
					char *buffer,	/* I - String buffer */
					int bufsize,	/* I - Size of string buffer */
					mxml_save_cb_t cb)
{				/* I - Whitespace callback or @code MXML_NO_CALLBACK@ */
	int col;		/* Final column */
	char *ptr[2];		/* Pointers for putc_cb */
	_mxml_global_t *global = _mxml_global();
	/* Global data */
	/* Write the node... */
	ptr[0] = buffer;
	ptr[1] = buffer + bufsize;
	col = mxml_write_node(node, ptr, cb, 0, mxml_string_putc, global);
	if (col < 0)
		return (-1);

	if (col > 0)
		mxml_string_putc('\n', ptr);

	/* Nul-terminate the buffer... */

	if (ptr[0] >= ptr[1])
		buffer[bufsize - 1] = '\0';
	else
		ptr[0][0] = '\0';
	/* Return the number of characters... */
	return ((int)(ptr[0] - buffer));
}

/*
 * 'mxmlSAXLoadString()' - Load a string into an XML node tree
 *                         using a SAX callback.
 *
 * The nodes in the specified string are added to the specified top node.
 * If no top node is provided, the XML string MUST be well-formed with a
 * single parent node like <?xml> for the entire string. The callback
 * function returns the value type that should be used for child nodes.
 * The constants @code MXML_INTEGER_CALLBACK@, @code MXML_OPAQUE_CALLBACK@,
 * @code MXML_REAL_CALLBACK@, and @code MXML_TEXT_CALLBACK@ are defined for
 * loading child nodes of the specified type.
 *
 * The SAX callback must call @link mxmlRetain@ for any nodes that need to
 * be kept for later use. Otherwise, nodes are deleted when the parent
 * node is closed or after each data, comment, CDATA, or directive node.
 *
 * @since Mini-XML 2.3@
 */

mxml_node_t *			/* O - First node or @code NULL@ if the string has errors. */
mxmlSAXLoadString(mxml_node_t *top,	/* I - Top node */
		const char *s,	/* I - String to load */
		mxml_load_cb_t cb,	/* I - Callback function or constant */
		mxml_sax_cb_t sax_cb,	/* I - SAX callback or @code MXML_NO_CALLBACK@ */
		void *sax_data)
{				/* I - SAX user data */
	/* Read the XML data... */
	return (mxml_load_data
		(top, (void *)&s, cb, mxml_string_getc, sax_cb, sax_data));
}

/*
 * 'mxmlSetCustomHandlers()' - Set the handling functions for custom data.
 *
 * The load function accepts a node pointer and a data string and must
 * return 0 on success and non-zero on error.
 *
 * The save function accepts a node pointer and must return a malloc'd
 * string on success and @code NULL@ on error.
 *
 */

void mxmlSetCustomHandlers(mxml_custom_load_cb_t load,	/* I - Load function */
			mxml_custom_save_cb_t save)
{				/* I - Save function */
	_mxml_global_t *global = _mxml_global();
	/* Global data */
	global->custom_load_cb = load;
	global->custom_save_cb = save;
}

/*
 * 'mxmlSetErrorCallback()' - Set the error message callback.
 */

void mxmlSetErrorCallback(mxml_error_cb_t cb)
{		/* I - Error callback function */
	_mxml_global_t *global = _mxml_global();
	/* Global data */
	global->error_cb = cb;
}

/*
 * 'mxmlSetWrapMargin()' - Set the wrap margin when saving XML data.
 *
 * Wrapping is disabled when "column" is 0.
 *
 * @since Mini-XML 2.3@
 */

void mxmlSetWrapMargin(int column)
{				/* I - Column for wrapping, 0 to disable wrapping */
	_mxml_global_t *global = _mxml_global();
	/* Global data */
	global->wrap = column;
}

/*
 * 'mxml_add_char()' - Add a character to a buffer, expanding as needed.
 */

static int /* O  - 0 on success, -1 on error */ mxml_add_char(int ch,	/* I  - Character to add */
				char **bufptr,	/* IO - Current position in buffer */
				char **buffer,	/* IO - Current buffer */
				int *bufsize)
{				/* IO - Current buffer size */
	char *newbuffer;	/* New buffer value */
	if (*bufptr >= (*buffer + *bufsize - 4)) {
		/* Increase the size of the buffer... */
		if (*bufsize < 1024)
			(*bufsize) *= 2;
		else
			(*bufsize) += 1024;
		newbuffer = realloc(*buffer, *bufsize);
		if (newbuffer == NULL) {
			free(*buffer);
			mxml_error("Unable to expand string buffer to %d bytes!", *bufsize);
			return (-1);
		}
		*bufptr = newbuffer + (*bufptr - *buffer);
		*buffer = newbuffer;
	}

	if (ch < 0x80) {
		/* Single byte ASCII... */
		*(*bufptr)++ = ch;
	} else if (ch < 0x800) {
		/* Two-byte UTF-8... */
		*(*bufptr)++ = 0xc0 | (ch >> 6);
		*(*bufptr)++ = 0x80 | (ch & 0x3f);
	} else if (ch < 0x10000) {
		/* Three-byte UTF-8... */
		*(*bufptr)++ = 0xe0 | (ch >> 12);
		*(*bufptr)++ = 0x80 | ((ch >> 6) & 0x3f);
		*(*bufptr)++ = 0x80 | (ch & 0x3f);
	} else {
		/* Four-byte UTF-8... */
		*(*bufptr)++ = 0xf0 | (ch >> 18);
		*(*bufptr)++ = 0x80 | ((ch >> 12) & 0x3f);
		*(*bufptr)++ = 0x80 | ((ch >> 6) & 0x3f);
		*(*bufptr)++ = 0x80 | (ch & 0x3f);
	}
	return 0;
}

/*
 * 'mxml_get_entity()' - Get the character corresponding to an entity...
 */

static int mxml_get_entity(mxml_node_t *parent,void *p,
			int *encoding, int(*getc_cb)(void *, int *))
{
	int ch;				/* Current character */
	char entity[64],	/* Entity string */
	*entptr;			/* Pointer into entity */

	entptr = entity;
	while ((ch = (*getc_cb) (p, encoding)) != EOF)
		if (ch > 126 || (!isalnum(ch) && ch != '#'))
			break;
		else if (entptr < (entity + sizeof(entity) - 1))
			*entptr++ = ch;
		else {
			mxml_error("Entity name too long under parent <%s>!",
					parent ? parent->value.element.name : "null");
			break;
		}

	*entptr = '\0';
	if (ch != ';') {
		mxml_error
			("Character entity \"%s\" not terminated under parent <%s>!",
			entity, parent ? parent->value.element.name : "null");
		return EOF;
	}

	if (entity[0] == '#') {
		if (entity[1] == 'x')
			ch = (int)strtol(entity + 2, NULL, 16);
		else
			ch = (int)strtol(entity + 1, NULL, 10);
	} else {
		ch = mxmlEntityGetValue(entity);
		if (ch < 0)
			mxml_error("Entity name \"%s;\" not supported under parent <%s>!",
				entity, parent ? parent->value.element.name : "null");
	}
	if (mxml_bad_char(ch)) {
		mxml_error("Bad control character 0x%02x under parent <%s> not allowed by XML standard!",
			ch, parent ? parent->value.element.name : "null");
		return EOF;
	}
	return ch;
}

/*
 * 'mxml_load_data()' - Load data into an XML node tree.
 */

static mxml_node_t *		/* O - First node or NULL if the file could not be read. */
mxml_load_data(mxml_node_t *top,	/* I - Top node */
		void *p,		/* I - Pointer to data */
		mxml_load_cb_t cb,	/* I - Callback function or MXML_NO_CALLBACK */
		_mxml_getc_cb_t getc_cb,	/* I - Read function */
		mxml_sax_cb_t sax_cb,	/* I - SAX callback or MXML_NO_CALLBACK */
		void *sax_data)
{				/* I - SAX user data */
	mxml_node_t *node = NULL,	/* Current node */
	*first = NULL,				/* First node added */
	*parent = NULL;				/* Current parent node */
	int ch,						/* Character from file */
	whitespace;					/* Non-zero if whitespace seen */
	char *buffer = NULL,		/* String buffer */
	*bufptr = NULL;				/* Pointer into buffer */
	int bufsize;				/* Size of buffer */
	mxml_type_t type;			/* Current node type */
	int encoding;				/* Character encoding */
	_mxml_global_t *global = _mxml_global();
	/* Global data */
	static const char *const types[] =	/* Type strings... */
	{
		"MXML_ELEMENT",	/* XML element with attributes */
		"MXML_INTEGER",	/* Integer value */
		"MXML_OPAQUE",	/* Opaque string */
		"MXML_REAL",	/* Real value */
		"MXML_TEXT",	/* Text fragment */
		"MXML_CUSTOM"	/* Custom data */
	};

	/*
	 * Read elements and other nodes from the file...
	 */
	buffer = malloc(64);
	if (buffer == NULL) {
		mxml_error("Unable to allocate string buffer!");
		return NULL;
	}

	bufsize = 64;
	bufptr = buffer;
	parent = top;
	first = NULL;
	whitespace = 0;
	encoding = ENCODE_UTF8;

	if (cb && parent)
		type = (*cb) (parent);
	else if (parent)
		type = MXML_TEXT;
	else
		type = MXML_IGNORE;

	while ((ch = (*getc_cb) (p, &encoding)) != EOF) {
		if ((ch == '<' ||
			(mxml_isspace(ch) && type != MXML_OPAQUE
			&& type != MXML_CUSTOM)) && bufptr > buffer) {
			/* Add a new value node... */

			*bufptr = '\0';
			switch (type) {
			case MXML_INTEGER:
				node =
					mxmlNewInteger(parent,
						(int)strtol(buffer, &bufptr, 0));
				break;

			case MXML_OPAQUE:
				node = mxmlNewOpaque(parent, buffer);
				break;

			case MXML_REAL:
				break;

			case MXML_TEXT:
				node = mxmlNewText(parent, whitespace, buffer);
				break;

			case MXML_CUSTOM:
				if (global->custom_load_cb) {
					/* * Use the callback to fill in the custom data... */
					node = mxmlNewCustom(parent, NULL, NULL);
					if ((*global->custom_load_cb)(node, buffer)) {
						mxml_error("Bad custom value '%s' in parent <%s>!",
							buffer, parent ? parent->value.element.name : "null");
						mxmlDelete(node);
						node = NULL;
					}
					break;
				}

			default:	/* Ignore... */
				node = NULL;
				break;
			}

			if (*bufptr) {
				/* Bad integer/real number value... */

				mxml_error("Bad %s value '%s' in parent <%s>!",
					type ==
					MXML_INTEGER ? "integer" : "real",
					buffer,
					parent ? parent->value.element.
					name : "null");
				break;
			}

			bufptr = buffer;
			whitespace = mxml_isspace(ch) && type == MXML_TEXT;

			if (!node && type != MXML_IGNORE) {
				/* Print error and return... */
				mxml_error
					("Unable to add value node of type %s to parent <%s>!",
					types[type],
					parent ? parent->value.element.
					name : "null");
				goto error;
			}

			if (sax_cb) {
				(*sax_cb) (node, MXML_SAX_DATA, sax_data);

				if (!mxmlRelease(node))
					node = NULL;
			}

			if (!first && node)
				first = node;
		} else if (mxml_isspace(ch) && type == MXML_TEXT)
			whitespace = 1;

		/*
		 * Add lone whitespace node if we have an element and existing
		 * whitespace...
		 */

		if (ch == '<' && whitespace && type == MXML_TEXT) {
			if (parent) {
				node = mxmlNewText(parent, whitespace, "");
				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_DATA, sax_data);
					if (!mxmlRelease(node))
						node = NULL;
				}
				if (!first && node)
					first = node;
			}
			whitespace = 0;
		}

		if (ch == '<') {
			/* Start of open/close tag... */
			bufptr = buffer;
			while ((ch = (*getc_cb) (p, &encoding)) != EOF)
				if (mxml_isspace(ch) || ch == '>'
					|| (ch == '/' && bufptr > buffer))
					break;
				else if (ch == '<') {
					mxml_error("Bare < in element!");
					goto error;
				} else if (ch == '&') {
					ch = mxml_get_entity(parent, p, &encoding, getc_cb);
					if (ch == EOF)
						goto error;
					if (mxml_add_char
						(ch, &bufptr, &buffer, &bufsize))
						goto error;
				} else if (ch < '0' && ch != '!' && ch != '-'
						&& ch != '.' && ch != '/')
					goto error;
				else if (mxml_add_char
					(ch, &bufptr, &buffer, &bufsize))
					goto error;
				else if (((bufptr - buffer) == 1
					&& buffer[0] == '?')
					|| ((bufptr - buffer) == 3
					&& !strncmp(buffer, "!--", 3))
					|| ((bufptr - buffer) == 8
					&& !strncmp(buffer, "![CDATA[",
					8)))
					break;

			*bufptr = '\0';
			if (!strcmp(buffer, "!--")) {
				/* Gather rest of comment... */
				while ((ch = (*getc_cb) (p, &encoding)) != EOF) {
					if (ch == '>' && bufptr > (buffer + 4)
						&& bufptr[-3] != '-'
						&& bufptr[-2] == '-'
						&& bufptr[-1] == '-')
						break;
					else if (mxml_add_char
						(ch, &bufptr, &buffer,
						&bufsize))
						goto error;
				}

				/* Error out if we didn't get the whole comment... */
				if (ch != '>') {
					/* Print error and return... */

					mxml_error
						("Early EOF in comment node!");
					goto error;
				}
				/* Otherwise add this as an element under the curr parent... */
				*bufptr = '\0';
				if (!parent && first) {
					/* There can only be one root element! */
					mxml_error
						("<%s> cannot be a second root node after <%s>",
						buffer, first->value.element.name);
					goto error;
				}
				node = mxmlNewElement(parent, buffer);
				if (node == NULL) {
					/* Just print error for now... */
					mxml_error("Unable to add comment node to parent <%s>!",
						parent ? parent->value.element.
						name : "null");
					break;
				}

				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_COMMENT, sax_data);
					if (!mxmlRelease(node))
						node = NULL;
				}
				if (node && !first)
					first = node;
			} else if (!strcmp(buffer, "![CDATA[")) {
				/* Gather CDATA section... */
				while ((ch = (*getc_cb) (p, &encoding)) != EOF) {
					if (ch == '>'
						&& !strncmp(bufptr - 2, "]]", 2)) {
						/* Drop terminator from CDATA string... */
						bufptr[-2] = '\0';
						break;
					} else
						if (mxml_add_char
						(ch, &bufptr, &buffer,
						 &bufsize))
						goto error;
				}
				/* Error out if we didn't get the whole comment... */
				if (ch != '>') {
					/* Print error and return... */
					mxml_error("Early EOF in CDATA node!");
					goto error;
				}
				/* Otherwise add this as an element under the curr parent... */

				*bufptr = '\0';

				if (!parent && first) {
					/* There can only be one root element! */
					mxml_error("<%s> cannot be a second root node after <%s>",
						buffer, first->value.element.name);
					goto error;
				}
				node = mxmlNewElement(parent, buffer);
				if (node == NULL) {
					/* Print error and return... */
					mxml_error
						("Unable to add CDATA node to parent <%s>!",
						parent ? parent->value.element.
						name : "null");
					goto error;
				}

				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_CDATA, sax_data);
					if (!mxmlRelease(node))
						node = NULL;
				}
				if (node && !first)
					first = node;
			} else if (buffer[0] == '?') {
				/* Gather rest of processing instruction... */
				while ((ch = (*getc_cb) (p, &encoding)) != EOF) {
					if (ch == '>' && bufptr > buffer
						&& bufptr[-1] == '?')
						break;
					else if (mxml_add_char
						(ch, &bufptr, &buffer,
						&bufsize))
						goto error;
				}

				/* Error out if we didn't get the whole processing instruction... */
				if (ch != '>') {
					/* Print error and return... */
					mxml_error
						("Early EOF in processing instruction node!");
					goto error;
				}
				/* Otherwise add this as an element under the current parent... */
				*bufptr = '\0';
				if (!parent && first) {
					/* * There can only be one root element! */
					mxml_error
						("<%s> cannot be a second root node after <%s>",
						buffer, first->value.element.name);
					goto error;
				}
				node = mxmlNewElement(parent, buffer);
				if (node == NULL) {
					/* Print error and return... */
					mxml_error
						("Unable to add processing instruction node to parent <%s>!",
						parent ? parent->value.element.
						name : "null");
					goto error;
				}

				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_DIRECTIVE, sax_data);
					if (!mxmlRelease(node))
						node = NULL;
				}

				if (node) {
					if (!first)
						first = node;
					if (!parent) {
						parent = node;
						if (cb)
							type = (*cb) (parent);
						else
							type = MXML_TEXT;
					}
				}
			} else if (buffer[0] == '!') {
				/* Gather rest of declaration... */
				do {
					if (ch == '>')
						break;
					else {
						if (ch == '&') {
							ch = mxml_get_entity
									(parent, p,
									&encoding,
									getc_cb);
							if (ch == EOF)
								goto error;
						}
						if (mxml_add_char
							(ch, &bufptr, &buffer,
							&bufsize))
							goto error;
					}
				} while ((ch = (*getc_cb) (p, &encoding)) != EOF);
				/* Error out if we didn't get the whole declaration... */
				if (ch != '>') {
					/* Print error and return... */
					mxml_error
						("Early EOF in declaration node!");
					goto error;
				}
				/* Otherwise add this as an element under the current parent... */
				*bufptr = '\0';
				if (!parent && first) {
					/* * There can only be one root element! */
					mxml_error
						("<%s> cannot be a second root node after <%s>",
						buffer, first->value.element.name);
					goto error;
				}
				node = mxmlNewElement(parent, buffer);
				if (node == NULL) {
					/* * Print error and return... */
					mxml_error
						("Unable to add declaration node to parent <%s>!",
						parent ? parent->value.element.
						name : "null");
					goto error;
				}

				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_DIRECTIVE,
						sax_data);
					if (!mxmlRelease(node))
						node = NULL;
				}

				if (node) {
					if (!first)
						first = node;
					if (!parent) {
						parent = node;
						if (cb)
							type = (*cb) (parent);
						else
							type = MXML_TEXT;
					}
				}
			} else if (buffer[0] == '/') {
				/* Handle close tag... */
				if (!parent
					|| strcmp(buffer + 1,
					parent->value.element.name)) {
					/* * Close tag doesn't match tree; print an error for now... */
					mxml_error
						("Mismatched close tag <%s> under parent <%s>!",
						buffer,
						parent ? parent->value.element.
						name : "(null)");
					goto error;
				}

				/* * Keep reading until we see >... */
				while (ch != '>' && ch != EOF)
					ch = (*getc_cb) (p, &encoding);
				node = parent;
				parent = parent->parent;
				if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_ELEMENT_CLOSE,
						sax_data);
					if (!mxmlRelease(node) && first == node)
						first = NULL;
				}
				/* * Ascend into the parent and set the value type as needed... */
				if (cb && parent)
					type = (*cb) (parent);
			} else {
				/* * Handle open tag... */
				if (!parent && first) {
					/* * There can only be one root element! */
					mxml_error
						("<%s> cannot be a second root node after <%s>",
						buffer, first->value.element.name);
					goto error;
				}
				node = mxmlNewElement(parent, buffer);
				if (node == NULL) {
					/* * Just print error for now... */
					mxml_error
						("Unable to add element node to parent <%s>!",
						parent ? parent->value.element.
						name : "null");
					goto error;
				}

				if (mxml_isspace(ch)) {
					ch = mxml_parse_element(node, p, &encoding, getc_cb);
					if (ch == EOF)
						goto error;
				} else if (ch == '/') {
					ch = (*getc_cb) (p, &encoding);
					if (ch != '>') {
						mxml_error
							("Expected > but got '%c' instead for element <%s/>!",
							ch, buffer);
						mxmlDelete(node);
						goto error;
					}
					ch = '/';
				}
				if (sax_cb)
					(*sax_cb) (node, MXML_SAX_ELEMENT_OPEN, sax_data);
				if (!first)
					first = node;
				if (ch == EOF)
					break;
				if (ch != '/') {
					/* * Descend into this node, setting the value type as needed... */
					parent = node;
					if (cb && parent)
						type = (*cb) (parent);
					else
						type = MXML_TEXT;
				} else if (sax_cb) {
					(*sax_cb) (node, MXML_SAX_ELEMENT_CLOSE,
						sax_data);
					if (!mxmlRelease(node) && first == node)
						first = NULL;
				}
			}
			bufptr = buffer;
		} else if (ch == '&') {
			/* * Add character entity to current buffer... */
			ch = mxml_get_entity(parent, p, &encoding, getc_cb);
			if (ch == EOF)
				goto error;
			if (mxml_add_char(ch, &bufptr, &buffer, &bufsize))
				goto error;
		} else if (type == MXML_OPAQUE || type == MXML_CUSTOM
			|| !mxml_isspace(ch)) {
			/* * Add character to current buffer... */
			if (mxml_add_char(ch, &bufptr, &buffer, &bufsize))
				goto error;
		}
	}
	/* * Free the string buffer - we don't need it anymore... */
	free(buffer);
	/* * Find the top element and return it... */
	if (parent) {
		node = parent;
		while (parent != top && parent->parent)
			parent = parent->parent;
		if (node != parent) {
			mxml_error("Missing close tag </%s> under parent <%s>!",
				node->value.element.name,
				node->parent ? node->parent->value.element.
				name : "(null)");
			mxmlDelete(first);
			return NULL;
		}
	}
	if (parent)
		return parent;
	else
		return first;
	/* * Common error return... */

error:
	mxmlDelete(first);
	free(buffer);
	return NULL;
}

/*
 * 'mxml_parse_element()' - Parse an element for any attributes...
 */

static int /* O  - Terminating character */ mxml_parse_element(
								mxml_node_t *node,	/* I  - Element node */
								void *p,	/* I  - Data to read from */
								int *encoding,	/* IO - Encoding */
								_mxml_getc_cb_t
								getc_cb)
{					/* I  - Data callback */
	int ch,			/* Current character in file */
	 quote;			/* Quoting character */
	char *name,		/* Attribute name */
	*value,			/* Attribute value */
	*ptr;			/* Pointer into name/value */
	int namesize,		/* Size of name string */
	 valsize;		/* Size of value string */

	/* Initialize the name and value buffers... */
	name = malloc(64);
	if (name == NULL) {
		mxml_error("Unable to allocate memory for name!");
		return EOF;
	}

	namesize = 64;
	value = malloc(64);
	if (value == NULL) {
		free(name);
		mxml_error("Unable to allocate memory for value!");
		return EOF;
	}
	valsize = 64;
	/* * Loop until we hit a >, /, ?, or EOF... */

	while ((ch = (*getc_cb) (p, encoding)) != EOF) {
	/* * Skip leading whitespace... */
		if (mxml_isspace(ch))
			continue;
		/* * Stop at /, ?, or >... */
		if (ch == '/' || ch == '?') {
			/* * Grab the > character and print an error if it isn't there... */
			quote = (*getc_cb) (p, encoding);
			if (quote != '>') {
				mxml_error
					("Expected '>' after '%c' for element %s, but got '%c'!",
					ch, node->value.element.name, quote);
				goto error;
			}
			break;
		} else if (ch == '<') {
			mxml_error("Bare < in element %s!",
				node->value.element.name);
			goto error;
		} else if (ch == '>')
			break;
		/* * Read the attribute name... */
		name[0] = ch;
		ptr = name + 1;
		if (ch == '\"' || ch == '\'') {
			/* * Name is in quotes, so get a quoted string... */
			quote = ch;
			while ((ch = (*getc_cb) (p, encoding)) != EOF) {
				if (ch == '&') {
					ch = mxml_get_entity(node, p, encoding,
								getc_cb);
					if (ch == EOF)
						goto error;
				}
				if (mxml_add_char(ch, &ptr, &name, &namesize))
					goto error;
				if (ch == quote)
					break;
			}
		} else {
			/* * Grab an normal, non-quoted name... */
			while ((ch = (*getc_cb) (p, encoding)) != EOF)
				if (mxml_isspace(ch) || ch == '=' || ch == '/'
					|| ch == '>' || ch == '?')
					break;
				else {
					if (ch == '&') {
						ch = mxml_get_entity(node, p,
									encoding,
									getc_cb);
						if (ch == EOF)
							goto error;
					}
					if (mxml_add_char
						(ch, &ptr, &name, &namesize))
						goto error;
				}
		}
		*ptr = '\0';
		if (mxmlElementGetAttr(node, name))
			goto error;
		while (ch != EOF && mxml_isspace(ch))
			ch = (*getc_cb) (p, encoding);
		if (ch == '=') {
			/* * Read the attribute value... */
			do {
				ch = (*getc_cb) (p, encoding);
			} while (ch != EOF && mxml_isspace(ch)) ;

			if (ch == EOF) {
				mxml_error
					("Missing value for attribute '%s' in element %s!",
					name, node->value.element.name);
				goto error;
			}
			if (ch == '\'' || ch == '\"') {
				/* * Read quoted value... */
				quote = ch;
				ptr = value;
				while ((ch = (*getc_cb) (p, encoding)) != EOF)
					if (ch == quote)
						break;
					else {
						if (ch == '&') {
							ch = mxml_get_entity
								(node, p, encoding,
								getc_cb);
							if (ch == EOF)
								goto error;
						}
						if (mxml_add_char
							(ch, &ptr, &value,
							&valsize))
							goto error;
					}
				*ptr = '\0';
			} else {
				/* * Read unquoted value... */
				value[0] = ch;
				ptr = value + 1;
				while ((ch = (*getc_cb) (p, encoding)) != EOF)
					if (mxml_isspace(ch) || ch == '='
						|| ch == '/' || ch == '>')
						break;
					else {
						if (ch == '&') {
							ch = mxml_get_entity
								(node, p, encoding,
								getc_cb);
							if (ch == EOF)
								goto error;
						}
						if (mxml_add_char
							(ch, &ptr, &value,
							&valsize))
							goto error;
					}
				*ptr = '\0';
			}
			/* * Set the attribute with the given string value... */
			mxmlElementSetAttr(node, name, value);
		} else {
			mxml_error
				("Missing value for attribute '%s' in element %s!",
				name, node->value.element.name);
			goto error;
		}

		/* * Check the end character... */
		if (ch == '/' || ch == '?') {
			/* * Grab the > character and print an error if it isn't there... */
			quote = (*getc_cb) (p, encoding);
			if (quote != '>') {
				mxml_error
					("Expected '>' after '%c' for element %s, but got '%c'!",
					ch, node->value.element.name, quote);
				ch = EOF;
			}
			break;
		} else if (ch == '>')
			break;
	}
	/* * Free the name and value buffers and return... */
	free(name);
	free(value);
	return ch;
	/* * Common error return point... */
error:
	free(name);
	free(value);
	return EOF;
}

/*
 * 'mxml_string_getc()' - Get a character from a string.
 */

static int /* O  - Character or EOF */ mxml_string_getc(void *p,	/* I  - Pointer to file */
							int *encoding)
{						/* IO - Encoding */
	int ch;				/* Character */
	const char **s;		/* Pointer to string pointer */

	s = (const char **)p;
	ch = (*s)[0] & 255;
	if (ch != 0 || *encoding == ENCODE_UTF16LE) {
		/* * Got character; convert UTF-8 to integer and return... */

		(*s)++;
		switch (*encoding) {
		case ENCODE_UTF8:
			if (!(ch & 0x80)) {
				if (mxml_bad_char(ch)) {
					mxml_error
						("Bad control character 0x%02x not allowed by XML standard!",
						ch);
					return EOF;
				}
				return ch;
			} else if (ch == 0xfe) {
				/* * UTF-16 big-endian BOM? */
				if (((*s)[0] & 255) != 0xff)
					return EOF;
				*encoding = ENCODE_UTF16BE;
				(*s)++;
				return mxml_string_getc(p, encoding);
			} else if (ch == 0xff) {
				/* * UTF-16 little-endian BOM? */
				if (((*s)[0] & 255) != 0xfe)
					return EOF;
				*encoding = ENCODE_UTF16LE;
				(*s)++;
				return mxml_string_getc(p, encoding);
			} else if ((ch & 0xe0) == 0xc0) {
				if (((*s)[0] & 0xc0) != 0x80)
					return EOF;
				ch = ((ch & 0x1f) << 6) | ((*s)[0] & 0x3f);
				(*s)++;
				if (ch < 0x80) {
					mxml_error
						("Invalid UTF-8 sequence for character 0x%04x!",
						ch);
					return EOF;
				}
				return ch;
			} else if ((ch & 0xf0) == 0xe0) {
				if (((*s)[0] & 0xc0) != 0x80 ||
					((*s)[1] & 0xc0) != 0x80)
					return EOF;
				ch = ((((ch & 0x0f) << 6) | ((*s)[0] & 0x3f)) <<
					6) | ((*s)[1] & 0x3f);
				(*s) += 2;
				if (ch < 0x800) {
					mxml_error("Invalid UTF-8 sequence for character 0x%04x!", ch);
					return EOF;
				}
				if (ch == 0xfeff)
					return mxml_string_getc(p, encoding);
				return ch;
			} else if ((ch & 0xf8) == 0xf0) {
			if (((*s)[0] & 0xc0) != 0x80 ||
					((*s)[1] & 0xc0) != 0x80 ||
					((*s)[2] & 0xc0) != 0x80)
					return EOF;
				ch = ((((((ch & 0x07) << 6) | ((*s)[0] & 0x3f))
					<< 6) | ((*s)[1] & 0x3f)) << 6) |
					((*s)[2] & 0x3f);
				(*s) += 3;
				if (ch < 0x10000) {
					mxml_error
						("Invalid UTF-8 sequence for character 0x%04x!",
						ch);
					return EOF;
				}
				return ch;
			} else
				return EOF;

		case ENCODE_UTF16BE:
			ch = (ch << 8) | ((*s)[0] & 255);
			(*s)++;
			if (mxml_bad_char(ch)) {
				mxml_error("Bad control character 0x%02x not allowed by XML standard!", ch);
				return EOF;
			} else if (ch >= 0xd800 && ch <= 0xdbff) {
				int lch;	/* Lower word */
				if (!(*s)[0])
					return EOF;
				lch = (((*s)[0] & 255) << 8) | ((*s)[1] & 255);
				(*s) += 2;
				if (lch < 0xdc00 || lch >= 0xdfff)
					return EOF;
				ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) +
					0x10000;
			}
			return ch;

		case ENCODE_UTF16LE:
			ch = ch | (((*s)[0] & 255) << 8);
			if (!ch) {
				(*s)--;
				return EOF;
			}
			(*s)++;
			if (mxml_bad_char(ch)) {
				mxml_error
					("Bad control character 0x%02x not allowed by XML standard!",
					ch);
				return EOF;
			} else if (ch >= 0xd800 && ch <= 0xdbff) {
				int lch;	/* Lower word */
				if (!(*s)[1])
					return EOF;
				lch = (((*s)[1] & 255) << 8) | ((*s)[0] & 255);
				(*s) += 2;
				if (lch < 0xdc00 || lch >= 0xdfff)
					return EOF;
				ch = (((ch & 0x3ff) << 10) | (lch & 0x3ff)) +
					0x10000;
			}
			return ch;
		}
	}
	return EOF;
}

/*
 * 'mxml_string_putc()' - Write a character to a string.
 */

static int /* O - 0 on success, -1 on failure */ mxml_string_putc(int ch,	/* I - Character to write */
								  void *p)
{				/* I - Pointer to string pointers */
	char **pp;		/* Pointer to string pointers */
	pp = (char **)p;
	if (pp[0] < pp[1])
		pp[0][0] = ch;
	pp[0]++;
	return 0;
}

/*
 * 'mxml_write_name()' - Write a name string.
 */

static int /* O - 0 on success, -1 on failure */ mxml_write_name(const char *s,	/* I - Name to write */
								void *p,	/* I - Write pointer */
								int (*putc_cb)
								(int, void *))
					/* I - Write callback */
{
	char quote;			/* Quote character */
	const char *name;	/* Entity name */

	if (*s == '\"' || *s == '\'') {
		if ((*putc_cb) (*s, p) < 0)
			return (-1);
		quote = *s++;
		while (*s && *s != quote) {
			name = mxmlEntityGetName(*s);
			if (name != NULL) {
				if ((*putc_cb) ('&', p) < 0)
					return (-1);
				while (*name) {
					if ((*putc_cb) (*name, p) < 0)
						return (-1);
					name++;
				}
				if ((*putc_cb) (';', p) < 0)
					return (-1);
			} else if ((*putc_cb) (*s, p) < 0)
				return (-1);
			s++;
		}
		if ((*putc_cb) (quote, p) < 0)
			return (-1);
	} else {
		while (*s) {
			if ((*putc_cb) (*s, p) < 0)
				return (-1);
			s++;
		}
	}
	return 0;
}

/*
 * 'mxml_write_node()' - Save an XML node to a file.
 */

static int /* O - Column or -1 on error */ mxml_write_node(mxml_node_t *node,	/* I - Node to write */
							void *p,	/* I - File to write to */
							mxml_save_cb_t cb,	/* I - Whitespace callback */
							int col,	/* I - Current column */
							_mxml_putc_cb_t putc_cb,	/* I - Output callback */
							_mxml_global_t *
							global)
{				/* I - Global data */
	mxml_node_t *curr,	/* Current node */
	*next;			/* Next node */
	int i,			/* Looping var */
	 width;			/* Width of attr + value */
	mxml_attr_t *attr;	/* Current attribute */
	char s[255];		/* Temporary string */

	/* * Loop through this node and all of its children... */
	for (curr = node; curr; curr = next) {
		switch (curr->type) {
		case MXML_ELEMENT:
			col =
				mxml_write_ws(curr, p, cb, MXML_WS_BEFORE_OPEN, col, putc_cb);
			if ((*putc_cb) ('<', p) < 0)
				return (-1);
			if (curr->value.element.name[0] == '?' ||
				!strncmp(curr->value.element.name, "!--", 3)) {
				/*
				 * Comments and processing instructions do not use character
				 * entities.
				 */
				const char *ptr;	/* Pointer into name */
				for (ptr = curr->value.element.name; *ptr;
					ptr++)
					if ((*putc_cb) (*ptr, p) < 0)
						return (-1);
			} else
				if (!strncmp
				(curr->value.element.name, "![CDATA[", 8)) {
				/*
				 * CDATA elements do not use character entities, but also need the
				 * "]]" terminator added at the end.
				 */
				const char *ptr;	/* Pointer into name */
				for (ptr = curr->value.element.name; *ptr;
					ptr++)
					if ((*putc_cb) (*ptr, p) < 0)
						return (-1);
				if ((*putc_cb) (']', p) < 0)
					return (-1);
				if ((*putc_cb) (']', p) < 0)
					return (-1);
			} else
				if (mxml_write_name
					(curr->value.element.name, p, putc_cb) < 0)
				return (-1);
			col += strlen(curr->value.element.name) + 1;
			for (i = curr->value.element.num_attrs, attr =
				curr->value.element.attrs; i > 0; i--, attr++) {
				width = (int)strlen(attr->name);
				if (attr->value)
					width += strlen(attr->value) + 3;
				{
					if ((*putc_cb) (' ', p) < 0)
						return (-1);
					col++;
				}
				if (mxml_write_name(attr->name, p, putc_cb) < 0)
					return (-1);
				if (attr->value) {
					if ((*putc_cb) ('=', p) < 0)
						return (-1);
					if ((*putc_cb) ('\"', p) < 0)
						return (-1);
					if (mxml_write_string
						(attr->value, p, putc_cb) < 0)
						return (-1);
					if ((*putc_cb) ('\"', p) < 0)
						return (-1);
				}
				col += width;
			}
			if (curr->child) {
				if ((*putc_cb) ('>', p) < 0)
					return (-1);
				else
					col++;
				col =
					mxml_write_ws(curr, p, cb,
						MXML_WS_AFTER_OPEN, col,
						putc_cb);
			} else if (curr->value.element.name[0] == '!'
				|| curr->value.element.name[0] == '?') {
				if ((*putc_cb) ('>', p) < 0)
					return (-1);
				else
					col++;
				col =
					mxml_write_ws(curr, p, cb,
						MXML_WS_AFTER_OPEN, col,
						putc_cb);
			} else {
				if ((*putc_cb) (' ', p) < 0)
					return (-1);
				if ((*putc_cb) ('/', p) < 0)
					return (-1);
				if ((*putc_cb) ('>', p) < 0)
					return (-1);
				col += 3;
				col =
					mxml_write_ws(curr, p, cb,
						MXML_WS_AFTER_OPEN, col,
						putc_cb);
			}
			break;
		case MXML_INTEGER:
			if (curr->prev) {
				if (global->wrap > 0 && col > global->wrap) {
					if ((*putc_cb) ('\n', p) < 0)
						return (-1);
					col = 0;
				} else if ((*putc_cb) (' ', p) < 0)
					return (-1);
				else
					col++;
			}
			snprintf(s, sizeof(s), "%d", curr->value.integer);
			if (mxml_write_string(s, p, putc_cb) < 0)
				return (-1);
			col += strlen(s);
			break;
		case MXML_OPAQUE:
			if (mxml_write_string(curr->value.opaque, p, putc_cb) <
				0)
				return (-1);
			col += strlen(curr->value.opaque);
			break;

		case MXML_REAL:
			if (curr->prev) {
				if (global->wrap > 0 && col > global->wrap) {
					if ((*putc_cb) ('\n', p) < 0)
						return (-1);
					col = 0;
				} else if ((*putc_cb) (' ', p) < 0)
					return (-1);
				else
					col++;
			}
			/*sprintf(s, "%f", curr->value.real);*/
			if (mxml_write_string(s, p, putc_cb) < 0)
				return (-1);

			col += strlen(s);
			break;

		case MXML_TEXT:
			if (curr->value.text.whitespace && col > 0) {
				if (global->wrap > 0 && col > global->wrap) {
					if ((*putc_cb) ('\n', p) < 0)
						return (-1);

					col = 0;
				} else if ((*putc_cb) (' ', p) < 0)
					return (-1);
				else
					col++;
			}

			if (mxml_write_string
				(curr->value.text.string, p, putc_cb) < 0)
				return (-1);

			col += strlen(curr->value.text.string);
			break;

		case MXML_CUSTOM:
			if (global->custom_save_cb) {
				char *data;	/* Custom data string */
				const char *newline;	/* Last newline in string */
				data = (*global->custom_save_cb)(curr);
				if (data == NULL)
					return (-1);
				if (mxml_write_string(data, p, putc_cb) < 0)
					return (-1);
				newline = strrchr(data, '\n');
				if (newline == NULL)
					col += strlen(data);
				else
					col = (int)strlen(newline);

				free(data);
				break;
			}

		default:	/* Should never happen */
			return (-1);
		}

		next = curr->child;
		if (next == NULL) {
			while ((next = curr->next) == NULL) {
				if (curr == node)
					break;

				curr = curr->parent;
				if (curr->value.element.name[0] != '!' &&
					curr->value.element.name[0] != '?') {
					col =
						mxml_write_ws(curr, p, cb,
							MXML_WS_BEFORE_CLOSE,
							col, putc_cb);
					if ((*putc_cb) ('<', p) < 0)
						return (-1);
					if ((*putc_cb) ('/', p) < 0)
						return (-1);
					if (mxml_write_string
						(curr->value.element.name, p,
						putc_cb) < 0)
						return (-1);
					if ((*putc_cb) ('>', p) < 0)
						return (-1);
					col +=
						strlen(curr->value.element.name) +
						3;
					col = mxml_write_ws(curr, p, cb,
							MXML_WS_AFTER_CLOSE,
							col, putc_cb);
				}
			}
		}
	}
	return col;
}

/*
 * 'mxml_write_string()' - Write a string, escaping & and < as needed.
 */

static int /* O - 0 on success, -1 on failure */ mxml_write_string(
									const char *s,	/* I - String to write */
									void *p,	/* I - Write pointer */
									_mxml_putc_cb_t
									putc_cb)
{				/* I - Write callback */
	const char *name;	/* Entity name, if any */

	while (*s) {
		name = mxmlEntityGetName(*s);
		if (name != NULL) {
			if ((*putc_cb) ('&', p) < 0)
				return (-1);
			while (*name) {
				if ((*putc_cb) (*name, p) < 0)
					return (-1);
				name++;
			}
			if ((*putc_cb) (';', p) < 0)
				return (-1);
		} else if ((*putc_cb) (*s, p) < 0)
			return (-1);
		s++;
	}
	return 0;
}

/*
 * 'mxml_write_ws()' - Do whitespace callback...
 */

static int /* O - New column */ mxml_write_ws(mxml_node_t *node,	/* I - Current node */
						void *p,	/* I - Write pointer */
						mxml_save_cb_t cb,	/* I - Callback function */
						int ws,	/* I - Where value */
						int col,	/* I - Current column */
						_mxml_putc_cb_t putc_cb)
{				/* I - Write callback */
	const char *s;		/* Whitespace string */
	s = (*cb) (node, ws);
	if (cb && s != NULL) {
		while (*s) {
			if ((*putc_cb) (*s, p) < 0)
				return (-1);
			else if (*s == '\n')
				col = 0;
			else if (*s == '\t') {
				col += MXML_TAB;
				col = col - (col % MXML_TAB);
			} else
				col++;
			s++;
		}
	}
	return col;
}
