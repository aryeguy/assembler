#include <stdio.h> /* for fprintf */
#include <string.h> /* for strncmp */

#include "table.h"
#include "consts.h"
#include "types.h"
#include "as.h"

/* all labels declared or defined 
 * in a source file */
static label_t labels[MAX_LABELS];
static int free_label_index = 0;

/************************************************
 * NAME: init_labels
 * DESCRIPTION: init the labels table 
 ***********************************************/
void init_labels(void)
{
	free_label_index = 0;
}

/************************************************
 * NAME: validate_labels
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION: validate that all entry and 
 * 		regular labels are defined and 
 * 		that all extern labels are not 
 * 		defined
 ***********************************************/
int validate_labels(void)
{
	int i;
	int failed = 0;

	for (i = 0; i < free_label_index; i++)
	{
		label_t *label = &labels[i];
		switch (label->type) {
			case REGULAR: /* FALLTHROUGH */
			case ENTRY: 
				if (!label->has_address) {
					fprintf(stderr, "label isn't defined: %s\n", label->name);
					failed = 1;
				}
				break;
			case EXTERNAL:
				if (label->has_address) {
					fprintf(stderr, "externl label defined: %s\n", label->name);
					failed = 1;
				}
				break;
		}
	}
	return failed;
}

/************************************************
 * NAME: loop_labels
 * PARAMS: fun - the function to invoke
 * DESCRIPTION: inovke a given function on all 
 * 		labels 
 ***********************************************/
void loop_labels(void (*fun)(label_t *))
{
	int i;

	for (i = 0; i < free_label_index; i++)
	{
		fun(&labels[i]);
	}
}

/************************************************
 * NAME: install_label
 * PARAMS: name - the name of the label to be 
 * 		  installed
 * 	   label - a pointer to the installed
 * 	           label
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION: install a given label
 ***********************************************/
int install_label(char *name, label_t **label)
{
	/* check that there is a free space for the label */
	if (free_label_index == MAX_LABELS) {
		parse_error("too much labels defined");
		return 1;
	}

	/* check if label already exist */
	if (lookup_label(name)) {
		parse_error("label already defined");
		return 1;
	}

	*label = &labels[free_label_index++];
	memmove((*label)->name, name, MAX_LABEL_LENGTH);

	return 0;
}

/************************************************
 * NAME: install_label
 * PARAMS: name - the name of the label to be 
 * 		  looked up
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION: lookup for a label with a given
 * 		name
 ***********************************************/
label_t* lookup_label(char *name)
{
	int i;
	
	for (i = 0; i<free_label_index; i++) {
		if (strncmp(labels[i].name, name, sizeof(name)) == 0) {
			return &labels[i];
		}
	}
	
	return NULL;
}
