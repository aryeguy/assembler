#ifndef TYPES_H
#define TYPES_H

#include "consts.h"

typedef char label_name_t[MAX_LABEL_LENGTH];
typedef unsigned int address_t;

typedef struct {
	char lines[MAX_LINES][MAX_LINE_LENGTH];
	int number_of_lines;
} source_code_t;

typedef enum {
	CODE,
	DATA
} label_section_t;

typedef enum {
	ENTRY,
	EXTERNAL,
	REGULAR
} label_type_t;

typedef enum {
	FULL_COMB,
	LEFT_SOURCE_LEFT_DEST_COMB,
	RIGHT_SOURCE_LEFT_DEST_COMB,
	LEFT_SOURCE_RIGHT_DEST_COMB,
	RIGHT_SOURCE_RIGHT_DEST_COMB
} instruction_comb_t;

typedef struct {
	label_section_t section;
	label_type_t type;
	label_name_t name;
	address_t address;
	int has_address;
} label_t;

typedef enum {
	NO_ADDRESS,
	IMMEDIATE_ADDRESS,
	DIRECT_ADDRESS,
	INDEX_ADDRESS,
	DIRECT_REGISTER_ADDRESS
} address_mode_t;

typedef struct {
	address_mode_t type;
	union {
		long immediate;
		int reg;
		label_name_t label;
	} value;
	union {
		long immediate;
		int reg;
		label_name_t label;
	} index;
} operand_t;

typedef struct {
		char name[INSTRUCTION_NAME_LENGTH];
		address_mode_t src_address_modes;
		address_mode_t dest_address_modes;
		unsigned int num_opernads;
		int code;
} instruction_t;

typedef struct {
	instruction_t *instruction;
	instruction_comb_t comb;
	operand_t first_op, second_op;
} full_instruction_t;

typedef enum {
	FIRST_PASS,
	SECOND_PASS
} pass_t;

#endif /* end of include guard: TYPES_H */
