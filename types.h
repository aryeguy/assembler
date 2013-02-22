#ifndef TYPES_H
#define TYPES_H

#include "consts.h"

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
	ABSOLUTE_LINKAGE,
	RELOCATBLE_LINKAGE,
	EXTERNAL_LINKAGE
} linker_data_t;

typedef struct {
	label_section_t section;
	label_type_t type;
	char name[MAX_LABEL_LENGTH];
	int address;
	int has_address;
} label_t;

typedef enum {
	NO_ADDRESS = 1,
	IMMEDIATE_ADDRESS = 2,
	DIRECT_ADDRESS = 4,
	INDEX_ADDRESS = 8,
	DIRECT_REGISTER_ADDRESS = 16
} address_mode_t;

typedef struct {
	address_mode_t type;
	union {
		long immediate;
		int reg;
		char label[MAX_LABEL_LENGTH];
	} value;
	enum {
		IMMEDIATE,
		REGISTER,
		LABEL
	} index_type;
	union {
		long immediate;
		int reg;
		char label[MAX_LABEL_LENGTH];
	} index;
} operand_t;

typedef struct {
		char name[INSTRUCTION_NAME_LENGTH];
		address_mode_t src_address_modes;
		address_mode_t dest_address_modes;
		unsigned int num_opernads;
		int opcode;
} instruction_t;

typedef struct {
	instruction_t *instruction;
	int type;
	int comb;
	operand_t src_operand;
	operand_t dest_operand;
} full_instruction_t;

#endif /* end of include guard: TYPES_H */
