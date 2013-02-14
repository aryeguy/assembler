#include <stdio.h> /* for fopen, fclose, fgets */
#include <stdlib.h> /* for EXIT_SUCCESS */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isspace */
#include <limits.h>
#include <assert.h>

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"

#define FLAG(x) (1 << x)

char *input_filename = NULL;
unsigned int input_linenumber = 0;
char *input_line = NULL;
char *input_line_start = NULL;
char data_section[DATA_SECTION_MAX_LENGTH];
char code_section[CODE_SECTION_MAX_LENGTH];
full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
int full_instruction_index = 0;

label_name_t label_definition;
label_name_t label_declaration;
unsigned int data_index = 0;
unsigned int code_index = 0;
int label_defined = 0;
pass_t pass;
instruction_t instructions[] = {
	{"mov", FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		2,
		0},
	{"cmp", FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		2,
		1},
	{"add", FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS),
		2,
		2},
	{"sub", FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		2,
		3},
	{"lea", FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS),
		2,
		4},
	{"not", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		5},
	{"clr", FLAG(NO_ADDRESS),
	       	FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		6},
	{"inc", FLAG(NO_ADDRESS),
	       	FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		7},
	{"dec", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		8},
	{"jmp", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		9},
	{"bne", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		10},
	{"red", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		11},
	{"prn", FLAG(NO_ADDRESS), 
		FLAG(IMMEDIATE_ADDRESS) | FLAG(DIRECT_ADDRESS) | FLAG(INDEX_ADDRESS) | FLAG(DIRECT_REGISTER_ADDRESS), 
		1,
		12},
	{"jsr", FLAG(NO_ADDRESS), 
		FLAG(DIRECT_ADDRESS), 
		1,
		13},
	{"rts", FLAG(NO_ADDRESS), 
		FLAG(NO_ADDRESS), 
		0,
		14},
	{"stp", FLAG(NO_ADDRESS), 
		FLAG(NO_ADDRESS), 
		0,
		15},
};

void parse_debug(char *gripe)
{
	fprintf(stderr, "%s:%u:%u: debug: %s\n",
			input_filename,
			input_linenumber,
			input_line - input_line_start,
		       	gripe);
}

void parse_error(char *gripe)
{
	fprintf(stderr, "%s:%u:%u: error: %s\n",
			input_filename,
			input_linenumber,
			input_line - input_line_start,
		       	gripe);
}

int isblank(char c)
{
	return c == ' ' || c == '\t';
}

void parse_whitespace(void)
{
	while (isblank(*input_line++));
	input_line--;
}

int parse_whitespace_must(void)
{
	if (!isblank(*input_line)) {
		parse_error("expected whitespace");
		return 1;
	}

	parse_whitespace();
	return 0;
}

int is_comment(void) 
{
	return input_line[0] == ';';
}

int is_empty(void)
{
	char *p;

	for (p = input_line; *p != '\n'; p++) {
		if (!isblank(*p)) {
			return 0;
		}
	}

	return 1;
}

int parse_line(char *line)
{
	input_line = input_line_start = line;
	if (is_comment() || is_empty()) {
		return 0;
	}
	else {
		return parse_action_line();
	}
}

int parse_label(label_name_t name)
{
	char *label_begin = input_line;

	if (!isalpha(*input_line)) {
		parse_error("label must start with alphabetic character");
		return 1;
	}

	while (isalnum(*input_line++));
	input_line--;

	if (input_line - label_begin > MAX_LABEL_LENGTH) {
		parse_error("label too long");
		return 1;
	}

	strncpy(name, label_begin, input_line - label_begin);
	name[input_line - label_begin] = '\0';

	return 0;
}

int parse_label_definition(void)
{
	if (strchr(input_line, ':')) {
		label_defined = 1;
		if (parse_label(label_definition)) {
			return 1;
		}
		
		if (*input_line++ != ':') {
			parse_error("expected :");
			return 1;
		}
	}

	return 0;
}

int parse_label_declaration(void)
{
	return parse_label(label_declaration);
}

int parse_number(long *x)
{
	char *p;

	*x = strtol(input_line, &p, 10);
	if (*x == LONG_MIN || *x == LONG_MAX) {
		parse_error("long overflow or underflow");
		return 1;
	}

	input_line = p;

	return 0;
}

int parse_data_number(void)
{
	long int x;

	if (parse_number(&x)) {
		return 1;
	}

	data_section[data_index++] = x;
	return 0;
}

int install_label_defintion(label_section_t section)
{
	label_t *l;

	l = lookup_label(label_definition);
	if (pass == SECOND_PASS) {
		return 0;
	}
	if (l != NULL) {
		if (l->type == EXTERNAL) {
			parse_error("label declared as external cannot be defined");
			return 1;
		}
		else if (l->has_address) {
			parse_error("label already defined");
			return 1;
		}
	}
	else {
		if (install_label(label_definition, &l)) {
			return 1;
		}

		l->type = REGULAR;
		strncpy(l->name, label_definition, strlen(label_definition));
	}
	
	l->section = section;
	switch (section) {
		case CODE:
			l->address = code_index;
			break;
		case DATA:
			l->address = data_index;
			break;
	}
	l->has_address = 1;


	return 0;
}

int install_label_declaration(label_type_t type) 
{
	label_t *l;

	if (pass == SECOND_PASS) {
		return 0;
	}

	l = lookup_label(label_declaration);
	if (l != NULL) {
		if (l->type != REGULAR) {
			parse_error("label already declared");
			return 1;
		}
	}
	else {
		if (install_label(label_declaration, &l)) {
			return 1;
		}

		l->has_address = 0;
		strncpy(l->name, label_declaration, strlen(label_declaration));
	}

	l->type = type;

	return 0;
}

int parse_data_directive(void)
{
	if (label_defined) {
		if (install_label_defintion(DATA)) {
			return 1;
		}
	}
	
	if (parse_data_number()) {
		return 1;
	}

	while (strchr(input_line, ',')) {
		parse_whitespace();
		if (parse_data_number()) {
			return 1;
		}
		parse_whitespace();
	}

	return 0;
}

int parse_string_directive(void)
{
	if (*input_line++ != '"') {
		parse_error("expected \"");
		return 1;
	}

	if (label_defined) {
		if (install_label_defintion(DATA)) {
			return 1;
		}
	}

	while (*input_line && *input_line != '"')
	{
		data_section[data_index] = *input_line;
		data_index++;
		input_line++;
	}

	if (*input_line++ != '"') {
		parse_error("expected \"");
		return 1;
	}

	data_section[data_index++] = '\0';

	return 0;
}

int parse_entry_directive(void)
{
	if (parse_label_declaration()) {
		return 1;
	}

	return install_label_declaration(ENTRY);
}

int parse_extern_directive(void)
{
	if (parse_label_declaration()) {
		return 1;
	}

	return install_label_declaration(EXTERNAL);
}

int parse_end_of_line(void)
{
	parse_whitespace();
	if (*input_line && *input_line != '\n') {
		parse_error("garbage in end of line");
		return 1;
	}

	return 0;
}

int parse_directive(void)
{
	struct {
		char name[MAX_DIRECTIVE_NAME_LENGTH];
		int (*function)(void);
	} directives[] = {{"data", parse_data_directive},
			{"string", parse_string_directive},
			{"entry", parse_entry_directive},
			{"extern", parse_extern_directive},
			{"\0", NULL}};
	int i;

	for (i = 0; sizeof(directives)/sizeof(directives[0]); i++) {
		if (!*directives[i].name) {
			parse_error("no such directive");
			return 1;
		}
		else if (strncmp(input_line, directives[i].name, strlen(directives[i].name)) == 0) {
			input_line += strlen(directives[i].name);
			if (parse_whitespace_must()) {
				return 1;
			}
			if (directives[i].function()) {
				return 1;
			}

			return 0;
		}
	}
}

int parse_instruction_comb(instruction_comb_t *combp)
{
	int source_operand_right;
	int dest_operand_right;

	if (*input_line++ != '/') {
		parse_error("expected /");
		return 1;
	}
	if (*input_line == '0') {
		input_line++;
		*combp = FULL_COMB;
		return 0;
	} else if (*input_line == '1') {
		input_line++;

		if (*input_line++ != '/') {
			parse_error("expected /");
			return 1;
		}
		if (*input_line == '0') {
			input_line++;
			source_operand_right = 0;
		} else if (*input_line == '1') {
			input_line++;
			source_operand_right = 0;
		} else {
			parse_error("expected 0 or 1");
			return 1;
		}

		if (*input_line++ != '/') {
			parse_error("expected /");
			return 1;
		}
		if (*input_line == '0') {
			input_line++;
			dest_operand_right = 0;
		} else if (*input_line == '1') {
			input_line++;
			dest_operand_right = 1;
		} else {
			parse_error("expected 0 or 1");
			return 1;
		}
	} else {
		parse_error("expected 0 or 1");
		return 1;
	}

	/* TODO change output */
	if (source_operand_right && dest_operand_right) {
		*combp = RIGHT_SOURCE_RIGHT_DEST_COMB;
	}
	else if (source_operand_right) {
		*combp = RIGHT_SOURCE_LEFT_DEST_COMB;
	}
	else if (dest_operand_right) {
		*combp = LEFT_SOURCE_RIGHT_DEST_COMB;
	}
	else {
		*combp = LEFT_SOURCE_LEFT_DEST_COMB;
	}
	return 0;
}

int is_register_operand(void)
{
	return *input_line == 'r' && input_line[1] >= '0' && input_line[1] <= '7';
}

int is_immediate_operand(void)
{
	return *input_line == '#';
}

int is_direct_register_operand(void)
{
	return is_register_operand();
}

int parse_register(int *reg)
{
	assert(*input_line == 'r');
	/* skip r */
	input_line++;

	*reg = *input_line++ - '0';
	if (*reg < 0 || *reg > 7) {
		parse_error("invalid register");
		return 1;
	}
	return 0;
}

int parse_instrcution_operand_immediate(long *immediate)
{
	assert(*input_line == '#');
	/* skip # */
	input_line++;

	if (parse_number(immediate)) {
		return 1;
	}
	return 0;
}

int parse_instrcution_operand_register_direct(int *reg)
{
	return parse_register(reg);
}

int parse_instrcution_operand(operand_t *operand, int available_address_modes)
{
	if (is_immediate_operand()) {
		code_index++;
		operand->type = IMMEDIATE_ADDRESS;
		if (parse_instrcution_operand_immediate(&(operand->value.immediate))) {
			return 1;
		}
	} else if (is_direct_register_operand()) {
		operand->type = DIRECT_REGISTER_ADDRESS;
		if (parse_instrcution_operand_register_direct(&(operand->value.reg))) {
			return 1;
		}
	}
	else {
		code_index++;
		if (parse_label(operand->value.label)) {
			return 1;
		}
		parse_whitespace();
		if (*input_line == '{') {
			operand->type = INDEX_ADDRESS;
			input_line++;
			if (is_register_operand()) {
				if (parse_register(&(operand->index.reg))) {
					return 1;
				}
			} else if (isdigit(*input_line)) {
				code_index++;
				if (parse_number(&(operand->index.immediate))) {
					return 1;
				}
			} else {
				code_index++;
				if (parse_label(operand->index.label)) {
					return 1;
				}
			}
			if (*input_line++ != '}')
			{
				parse_error("expected }");
				return 1;
			}
			return 0;
		} else {
			operand->type = DIRECT_ADDRESS;
		}

	}

	if (!(FLAG(operand->type) & available_address_modes)) {
		parse_error("address mode noy allowed for this instruction");
		return 1;
	}
	return 0;
}

int parse_instruction_name(instruction_t **instruction)
{
	int i;

	for (i = 0; i < sizeof(instructions)/sizeof(instructions[0]); i++) {
		if (strncmp(input_line, instructions[i].name, INSTRUCTION_NAME_LENGTH) == 0) {
			break;
		}
	}

	/* instruction not found */
	if (i == sizeof(instructions)/sizeof(instructions[0])) {
		parse_error("invalid instruction");
		return 1;
	}

	*instruction = &instructions[i];
	input_line += INSTRUCTION_NAME_LENGTH;
	return 0;
}

int parse_instruction(void) 
{
	full_instruction_t *full_instruction = &full_instructions[full_instruction_index++];
	if (label_defined) {
		if (install_label_defintion(CODE)) {
			return 1;
		}
	}

	if (parse_instruction_name(&(full_instruction->instruction))) {
		return 1;
	}

	if (parse_instruction_comb(&(full_instruction->comb))) {
		return 1;
	}

	code_index++;

	switch (full_instruction->instruction->num_opernads) {
		case 2:
			if (parse_whitespace_must()) {
				return 1;
			}
			if (parse_instrcution_operand(&(full_instruction->first_op), full_instruction->instruction->src_address_modes)) {
				return 1;
			}
			parse_whitespace();
			if (*input_line++ != ',') {
				parse_error("expected ','");
				return 1;
			}
			parse_whitespace();
			if (parse_instrcution_operand(&(full_instruction->second_op), full_instruction->instruction->dest_address_modes)) {
				return 1;
			}
			parse_whitespace();
			break;
		case 1: 
			if (parse_whitespace_must()) {
				return 1;
			}
			if (parse_instrcution_operand(&(full_instruction->second_op), full_instruction->instruction->dest_address_modes)) {
				return 1;
			}
			parse_whitespace();
			break;
		case 0:
			break;
	}

	return 0;
}

int parse_action_line(void)
{
	label_defined = 0;

	if (parse_label_definition()) {
		return 1;
	}

	if (label_defined) {
		parse_whitespace_must();
	}
	else {
		parse_whitespace();
	}

	if (*input_line == '.') {
		input_line++;
		if (parse_directive()) {
			return 1;
		}
	}

	else {
		if (parse_instruction()) {
			return 1;
		}
	}

	return parse_end_of_line();	
}
