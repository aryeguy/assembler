#include <stdio.h>
#include <string.h>

#include "table.h"
#include "consts.h"
#include "types.h"
#include "as.h"

/* all labels declared or defined 
 * in a source file */
static label_t labels[MAX_LABELS];
static int free_label_index = 0;

void init_labels(void)
{
	free_label_index = 0;
}

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

void loop_labels(void (*fun)(label_t *))
{
	int i;

	for (i = 0; i < free_label_index; i++)
	{
		fun(&labels[i]);
	}
}

int install_label(char *name, label_t **label)
{
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
