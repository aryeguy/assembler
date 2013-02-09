#ifdef DEBUG
#include <stdio.h>
#endif

#include <string.h>

#include "table.h"
#include "consts.h"
#include "types.h"
#include "as.h"

static label_t labels[MAX_LABELS];
static int free_label_index = 0;

int install_label(label_name_t name, label_t **label)
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
	memmove((*label)->name, name, sizeof(label_name_t));

	return 0;
}

label_t* lookup_label(label_name_t name)
{
	int i = 0;
	
	for (i=0; i<free_label_index; i++) {
		if (memcmp(labels[i].name, name, sizeof(name)) == 0) {
			return &labels[i];
		}
	}
	
	return NULL;
}

#ifdef DEBUG
void print_labels(void)
{
	int i;
	label_t *label;

	printf("labels:\n");

	for (i = 0; i < free_label_index; i++) {
		label = &labels[i];

		printf("#%d ", i);
		printf("name: %s ", label->name);
		printf("type: ");
		switch (label->type) {
			case REGULAR:
				printf("REGULAR ");
				break;
			case ENTRY:
				printf("ENTRY ");
				break;
			case EXTERNAL:
				printf("EXTERNAL ");
				break;
		}

		printf("section: ");
		switch (label->section) {
			case CODE:
				printf("CODE ");
				break;
			case DATA:
				printf("DATA ");
				break;
		}

		if (label->has_address) {
			printf("address: %d", label->address);
		}

		printf("\n");
	}
}
#endif
