#ifndef TABLE_H
#define TABLE_H
#include "types.h"

int install_label(label_name_t name, label_t **label);
label_t* lookup_label(label_name_t name);
int validate_labels(void);
void init_labels(void);
void loop_labels(void (*fun)(label_t *));

#ifdef DEBUG
void print_labels(void);
#endif

#endif /* end of include guard: TABLE_H */
