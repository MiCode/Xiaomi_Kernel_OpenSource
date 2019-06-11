/*
 * Index support code for Mini-XML, a small XML file parsing library.
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

#include "config.h"
#include "mxml.h"

/*
 * Sort functions...
 */

static int index_compare(mxml_index_t *ind, mxml_node_t *first,
			mxml_node_t *second);
static int index_find(mxml_index_t *ind, const char *element,
			const char *value, mxml_node_t *node);
static void index_sort(mxml_index_t *ind, int left, int right);

/*
 * 'mxmlIndexDelete()' - Delete an index.
 */

void mxmlIndexDelete(mxml_index_t *ind)
{				/* I - Index to delete */
	if (!ind)
		return;
	if (ind->attr)
		free(ind->attr);
	if (ind->alloc_nodes)
		free(ind->nodes);
	free(ind);
}

/*
 * 'mxmlIndexEnum()' - Return the next node in the index.
 *
 * You should call @link mxmlIndexReset@ prior to using this function to get
 * the first node in the index.  Nodes are returned in the sorted order of the
 * index.
 */

mxml_node_t *			/* O - Next node or @code NULL@ if there is none */
mxmlIndexEnum(mxml_index_t *ind)
{				/* I - Index to enumerate */
	if (!ind)
		return NULL;
	if (ind->cur_node < ind->num_nodes)
		return ind->nodes[ind->cur_node++];
	else
		return NULL;
}

/*
 * 'mxmlIndexFind()' - Find the next matching node.
 *
 * You should call @link mxmlIndexReset@ prior to using this function for
 * the first time with a particular set of "element" and "value"
 * strings. Passing @code NULL@ for both "element" and "value" is equivalent
 * to calling @link mxmlIndexEnum@.
 */

mxml_node_t *			/* O - Node or @code NULL@ if none found */
mxmlIndexFind(mxml_index_t *ind,	/* I - Index to search */
		const char *element,	/* I - Element name to find, if any */
		const char *value)
{				/* I - Attribute value, if any */
	int diff,		/* Difference between names */
	curr,			/* Current entity in search */
	first,			/* First entity in search */
	last;			/* Last entity in search */

	if (!ind || (!ind->attr && value)) {
		return NULL;
	}

	/*
	 * If both element and value are NULL, just enumerate the nodes in the
	 * index...
	 */

	if (!element && !value)
		return mxmlIndexEnum(ind);

	/* * If there are no nodes in the index, return NULL... */

	if (!ind->num_nodes) {
		return NULL;
	}

	/* * If cur_node == 0, then find the first matching node... */
	if (ind->cur_node == 0) {
		/* * Find the first node using a modified binary search algorithm... */
		first = 0;
		last = ind->num_nodes - 1;
		while ((last - first) > 1) {
			curr = (first + last) / 2;
			diff = index_find(ind, element, value,
					ind->nodes[curr]);
			if (diff == 0) {
				/* * Found a match, move back to find the first... */
				while (curr > 0 &&
					!index_find(ind, element, value,
						ind->nodes[curr - 1]))
					curr--;
				/* * Return the first match and save the index to the next... */
				ind->cur_node = curr + 1;
				return ind->nodes[curr];
			} else if (diff < 0)
				last = curr;
			else
				first = curr;
		}
		/* * If we get this far, then we found exactly 0 or 1 matches... */
		for (curr = first; curr <= last; curr++)
			if (!index_find(ind, element, value, ind->nodes[curr])) {
				/* * Found exactly one (or possibly two) match... */
				ind->cur_node = curr + 1;
				return ind->nodes[curr];
			}
		ind->cur_node = ind->num_nodes;
		return NULL;
	} else if (ind->cur_node < ind->num_nodes &&
		!index_find(ind, element, value, ind->nodes[ind->cur_node])) {
		/* * Return the next matching node... */
		return ind->nodes[ind->cur_node++];
	}
	/* * If we get this far, then we have no matches... */
	ind->cur_node = ind->num_nodes;
	return NULL;
}

/*
 * 'mxmlIndexGetCount()' - Get the number of nodes in an index.
 *
 * @since Mini-XML 2.7@
 */

int /* I - Number of nodes in index */ mxmlIndexGetCount(mxml_index_t *ind)
{				/* I - Index of nodes */
	if (!ind)
		return 0;
	/*
	 * Return the number of nodes in the index...
	 */
	return ind->num_nodes;
}

/*
 * 'mxmlIndexNew()' - Create a new index.
 *
 * The index will contain all nodes that contain the named element and/or
 * attribute.  If both "element" and "attr" are @code NULL@, then the index will
 * contain a sorted list of the elements in the node tree.  Nodes are
 * sorted by element name and optionally by attribute value if the "attr"
 * argument is not NULL.
 */

mxml_index_t *			/* O - New index */
mxmlIndexNew(mxml_node_t *node,	/* I - XML node tree */
		const char *element,	/* I - Element to index or @code NULL@ for all */
		const char *attr)
{				/* I - Attribute to index or @code NULL@ for none */
	mxml_index_t *ind;	/* New index */
	mxml_node_t *curr,	/* Current node in index */
	**temp;			/* Temporary node pointer array */

	if (!node)
		return NULL;

	ind = calloc(1, sizeof(mxml_index_t));
	if (ind == NULL) {
		mxml_error("Unable to allocate %d bytes for index - %s",
			sizeof(mxml_index_t), NULL);
		return NULL;
	}

	if (attr)
		ind->attr = strdup(attr);

	if (!element && !attr)
		curr = node;
	else
		curr = mxmlFindElement(node, node, element, attr, NULL, MXML_DESCEND);

	while (curr) {
		if (ind->num_nodes >= ind->alloc_nodes) {
			if (!ind->alloc_nodes)
				temp = malloc(64 * sizeof(mxml_node_t *));
			else
				temp =
					realloc(ind->nodes,
						(ind->alloc_nodes +
						64) * sizeof(mxml_node_t *));
			if (!temp) {
				/* * Unable to allocate memory for the index, so abort... */
				mxml_error
					("Unable to allocate %d bytes for index: %s",
					(ind->alloc_nodes +
					64) * sizeof(mxml_node_t *), NULL);
				mxmlIndexDelete(ind);
				return NULL;
			}
			ind->nodes = temp;
			ind->alloc_nodes += 64;
		}
		ind->nodes[ind->num_nodes++] = curr;
		curr = mxmlFindElement(curr, node, element, attr, NULL, MXML_DESCEND);
	}

	/*
	 * Sort nodes based upon the search criteria...
	 */

	if (ind->num_nodes > 1)
		index_sort(ind, 0, ind->num_nodes - 1);
	return ind;
}

/*
 * 'mxmlIndexReset()' - Reset the enumeration/find pointer in the index and
 * return the first node in the index.
 *
 * This function should be called prior to using @link mxmlIndexEnum@ or
 * @link mxmlIndexFind@ for the first time.
 */

mxml_node_t *			/* O - First node or @code NULL@ if there is none */
mxmlIndexReset(mxml_index_t *ind)
{				/* I - Index to reset */
	if (!ind)
		return NULL;

	/* * Set the index to the first element... */
	ind->cur_node = 0;
	if (ind->num_nodes)
		return ind->nodes[0];
	else
		return NULL;
}

/*
 * 'index_compare()' - Compare two nodes.
 */

static int /* O - Result of comparison */ index_compare(mxml_index_t *ind,	/* I - Index */
							mxml_node_t *first,	/* I - First node */
							mxml_node_t *second)
{				/* I - Second node */
	int diff;		/* Difference */
	diff = strcmp(first->value.element.name,
		second->value.element.name);
	if (diff != 0)
		return diff;
	if (ind->attr) {
		diff = strcmp(mxmlElementGetAttr(first, ind->attr),
				mxmlElementGetAttr(second, ind->attr));
		if (diff != 0)
			return diff;
	}
	return 0;
}

/*
 * 'index_find()' - Compare a node with index values.
 */

static int /* O - Result of comparison */ index_find(mxml_index_t *ind,	/* I - Index */
						const char *element,	/* I - Element name or @code NULL@ */
						const char *value,	/* I - Attribute value or @code NULL@ */
						mxml_node_t *node)
{				/* I - Node */
	int diff;		/* Difference */
	if (element) {
		diff = strcmp(element, node->value.element.name);
		if (diff != 0)
			return diff;
	}
	if (value) {
		diff = strcmp(value, mxmlElementGetAttr(node, ind->attr));
		if (diff != 0)
			return diff;
	}
	return 0;
}

/*
 * 'index_sort()' - Sort the nodes in the index...
 *
 * This function implements the classic quicksort algorithm...
 */

static void index_sort(mxml_index_t *ind,	/* I - Index to sort */
			int left,	/* I - Left node in partition */
			int right)
{				/* I - Right node in partition */
	mxml_node_t *pivot,	/* Pivot node */
	*temp;			/* Swap node */
	int templ,		/* Temporary left node */
	tempr;			/* Temporary right node */

	/* * Loop until we have sorted all the way to the right... */

	do {
		/* * Sort the pivot in the curr partition... */
		pivot = ind->nodes[left];
		for (templ = left, tempr = right; templ < tempr;) {
			/* * Move left while left node <= pivot node... */
			while ((templ < right) &&
				index_compare(ind, ind->nodes[templ],
				pivot) <= 0)
				templ++;
			/* * Move right while right node > pivot node... */
			while ((tempr > left) &&
				index_compare(ind, ind->nodes[tempr], pivot) > 0)
				tempr--;
			/* * Swap nodes if needed... */
			if (templ < tempr) {
				temp = ind->nodes[templ];
				ind->nodes[templ] = ind->nodes[tempr];
				ind->nodes[tempr] = temp;
			}
		}

		/*
		 * When we get here, the right (tempr) node is the new position for the
		 * pivot node...
		 */

		if (index_compare(ind, pivot, ind->nodes[tempr]) > 0) {
			ind->nodes[left] = ind->nodes[tempr];
			ind->nodes[tempr] = pivot;
		}

		/* * Recursively sort the left partition as needed... */
		if (left < (tempr - 1))
			index_sort(ind, left, tempr - 1);
	} while (right > (left = tempr + 1));
}
