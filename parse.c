#include <stdlib.h> /* for strtol */
#include <stdio.h> /* for fopen, fclose, fgets */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isalpha and isalnum */
#include <limits.h> /* for LONG_MIN and LONG_MAX */
#include <assert.h> /* for assert */

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"

full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
int full_instruction_index = 0;
char data_section[DATA_SECTION_MAX_LENGTH];
unsigned int data_index = 0;
unsigned int code_index = 0;

static int label_defined = 0;
static enum {
	FIRST_PASS,
	SECOND_PASS
} pass;
static label_name_t label_definition;
static label_name_t label_declaration;
static char *input_line = NULL;
static char *input_line_start = NULL;
static char *input_filename = NULL;
static unsigned int input_linenumber = 0;
static instruction_t instructions[] = {
	{"mov", IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		2,
		00},
	{"cmp", IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		2,
		01},
	{"add", IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS,
		2,
		02},
	{"sub", IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		2,
		03},
	{"lea", DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS,
		2,
		06},
	{"not", NO_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		04},
	{"clr", NO_ADDRESS,
	       	DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		05},
	{"inc", NO_ADDRESS,
	       	DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		07},
	{"dec", NO_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		010},
	{"jmp", NO_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		011},
	{"bne", NO_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		012},
	{"red", NO_ADDRESS, 
		DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		013},
	{"prn", NO_ADDRESS, 
		IMMEDIATE_ADDRESS | DIRECT_ADDRESS | INDEX_ADDRESS | DIRECT_REGISTER_ADDRESS, 
		1,
		014},
	{"jsr", NO_ADDRESS, 
		DIRECT_ADDRESS, 
		1,
		015},
	{"rts", NO_ADDRESS, 
		NO_ADDRESS, 
		0,
		016},
	{"stp", NO_ADDRESS, 
		NO_ADDRESS, 
		0,
		017},
};

/************************************************
 * NAME: parse_error
 * PARAMS: gripe - the error message 
 * DESCRIPTION: output an error message based
 *              on current parsed file, current
 *              parsed line and current parsed
 *              location on line 
 ***********************************************/
void parse_error(char *gripe)
{
	fprintf(stderr, "%s:%u:%u: error: %s\n",
			input_filename,
			input_linenumber,
			input_line - input_line_start,
		       	gripe);
}

/************************************************
 * NAME: isblank
 * PARAMS: c - checked character 
 * RETURN VALUE: 1 if blank
 * DESCRIPTION: check if a charcater is a blank 
 *              character 
 ***********************************************/
static int isblank(char c)
{
	return c == ' ' || c == '\t';
}

/************************************************
 * NAME: parse_whitespace
 * DESCRIPTION: promote input_line to a nonblank
 *              character
 ***********************************************/
static void parse_whitespace(void)
{
	while (isblank(*input_line++));
	input_line--;
}

/************************************************
 * NAME: parse_whitespace_must
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION: parse all whitespace and report
 * 		if there is no whitespace at all
 ************************************************/
static int parse_whitespace_must(void)
{
	if (!isblank(*input_line)) {
		parse_error("expected whitespace");
		return 1;
	}

	parse_whitespace();
	return 0;
}

/************************************************
 * NAME: is_comment
 * RETURN VALUE: 1 if the current parsed line is
 * 		 a comment, 0 otherwise
 * DESCRIPTION: check if the current parsed line 
 * 		is a comment by checking the first
 * 		character 
 ************************************************/
static int is_comment(void)
{
	return input_line[0] == ';';
}

/************************************************
 * NAME: is_empty
 * RETURN VALUE: 1 if the current parsed line is
 * 		 empty , 0 otherwise
 * DESCRIPTION: an empty line a a line that 
 * 		contains only blank characters 
 ************************************************/
static int is_empty(void)
{
	char *p;

	for (p = input_line; *p != '\n' && *p != '\0'; p++) {
		if (!isblank(*p)) {
			return 0;
		}
	}

	return 1;
}

static int parse_label(label_name_t name)
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

static int parse_label_use(label_name_t name)
{
	if (parse_label(name)) {
		return 1;
	}
	if (pass == SECOND_PASS) {
		if (!lookup_label(name)) {
			parse_error("label that isn't defined is used");
			return 1;
		}
	}

	return 0;
}

static int parse_label_definition(void)
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

static int parse_label_declaration(void)
{
	return parse_label(label_declaration);
}

static int parse_number(long *x)
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

static int parse_data_number(void)
{
	long int x;

	if (parse_number(&x)) {
		return 1;
	}

	data_section[data_index++] = x;
	return 0;
}

static int install_label_defintion(label_section_t section)
{
	label_t *l;

	l = lookup_label(label_definition);
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

static int install_label_declaration(label_type_t type)
{
	label_t *l;

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

static int parse_data_directive(void)
{
	if (label_defined && install_label_defintion(DATA)) {
		return 1;
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

static int parse_string_directive(void)
{
	if (*input_line++ != '"') {
		parse_error("expected \"");
		return 1;
	}

	if (label_defined && install_label_defintion(DATA)) {
		return 1;
	}

	while (*input_line && *input_line != '"')
	{
		data_section[data_index++] = *input_line;
		input_line++;
	}

	if (*input_line++ != '"') {
		parse_error("expected \"");
		return 1;
	}

	data_section[data_index++] = '\0';

	return 0;
}

static int parse_entry_directive(void)
{
	if (parse_label_declaration()) {
		return 1;
	}

	return install_label_declaration(ENTRY);
}

static int parse_extern_directive(void)
{
	if (parse_label_declaration()) {
		return 1;
	}

	return install_label_declaration(EXTERNAL);
}

static int parse_end_of_line(void)
{
	parse_whitespace();
	if (*input_line && *input_line != '\n') {
		parse_error("garbage in end of line");
		return 1;
	}

	return 0;
}

static int parse_directive(void)
{
	struct {
		char name[MAX_DIRECTIVE_NAME_LENGTH];
		int (*function)(void);
	} directives[] = {
		{"data", parse_data_directive},
		{"string", parse_string_directive},
		{"entry", parse_entry_directive},
		{"extern", parse_extern_directive},
			};
	int i;

	for (i = 0; sizeof(directives)/sizeof(directives[0]); i++) {
	 	if (strncmp(input_line, directives[i].name, strlen(directives[i].name)) == 0) {
			input_line += strlen(directives[i].name);
			if (parse_whitespace_must()) {
				return 1;
			}
			return directives[i].function();
		}
	}
	
	parse_error("no such directive");
	return 1;
}

static int parse_instruction_comb(instruction_comb_t *combp)
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

static int is_register_operand(void)
{
	return *input_line == 'r' && input_line[1] >= '0' && input_line[1] <= '7';
}

static int is_immediate_operand(void)
{
	return *input_line == '#';
}

static int is_direct_register_operand(void)
{
	return is_register_operand();
}

static int parse_register(int *reg)
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

static int parse_instruction_operand_immediate(long *immediate)
{
	assert(*input_line == '#');
	/* skip # */
	input_line++;

	if (parse_number(immediate)) {
		return 1;
	}
	return 0;
}

static int parse_instruction_operand_register_direct(int *reg)
{
	return parse_register(reg);
}

static int parse_instruction_operand(operand_t *operand, int available_address_modes)
{
	if (is_immediate_operand()) {
		code_index++;
		operand->type = IMMEDIATE_ADDRESS;
		if (parse_instruction_operand_immediate(&(operand->value.immediate))) {
			return 1;
		}
	} else if (is_direct_register_operand()) {
		operand->type = DIRECT_REGISTER_ADDRESS;
		if (parse_instruction_operand_register_direct(&(operand->value.reg))) {
			return 1;
		}
	}
	else {
		code_index++;
		if (parse_label_use(operand->value.label)) {
			return 1;
		}
		parse_whitespace();
		if (*input_line == '{') {
			operand->type = INDEX_ADDRESS;
			input_line++;
			if (is_register_operand()) {
				operand->index_type = REGISTER;
				if (parse_register(&(operand->index.reg))) {
					return 1;
				}
			} else if (isdigit(*input_line) || *input_line == '-' || *input_line == '+') {
				operand->index_type = IMMEDIATE;
				code_index++;
				if (parse_number(&(operand->index.immediate))) {
					return 1;
				}
			} else if (!parse_label_use(operand->index.label)) {
				operand->index_type = LABEL;
				code_index++;
			} else {
				parse_error("invalid index addersing");
				return 1;
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

	if (!(operand->type & available_address_modes)) {
		parse_error("address mode not allowed for this instruction");
		return 1;
	}
	return 0;
}

static int parse_instruction_name(instruction_t **instruction)
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

static int parse_instruction(void)
{
	full_instruction_t *full_instruction = &full_instructions[full_instruction_index++];
	if (label_defined && pass == FIRST_PASS) {
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
			if (parse_instruction_operand(&(full_instruction->src_operand), full_instruction->instruction->src_address_modes)) {
				return 1;
			}
			parse_whitespace();
			if (*input_line++ != ',') {
				parse_error("expected ','");
				return 1;
			}
			parse_whitespace();
			if (parse_instruction_operand(&(full_instruction->dest_operand), full_instruction->instruction->dest_address_modes)) {
				return 1;
			}
			break;
		case 1: 
			if (parse_whitespace_must()) {
				return 1;
			}
			if (parse_instruction_operand(&(full_instruction->dest_operand), full_instruction->instruction->dest_address_modes)) {
				return 1;
			}
			break;
		case 0:
			break;
	}

	return 0;
}

static int parse_action_line(void)
{
	if (parse_label_definition()) {
		return 1;
	}

	if (label_defined) {
		if (parse_whitespace_must()) {
			return 1;
		}
	} else {
		parse_whitespace();
	}

	if (*input_line == '.') {
		if (pass == SECOND_PASS) {
			return 0;
		}
		input_line++;
		if (parse_directive()) {
			return 1;
		}
	} else {
		if (parse_instruction()) {
			return 1;
		}
	}

	return parse_end_of_line();	
}

static int parse_line(char *line)
{
	label_defined = 0;
	input_line = input_line_start = line;
	if (is_comment() || is_empty()) {
		return 0;
	}
	else {
		return parse_action_line();
	}
}

/* exported functions */

int parse_file(char *filename)
{
	FILE *fp;
	char line[MAX_LINE_LENGTH];
	int failed = 0;

	init_labels();

	fp = fopen(filename, "rt");

	if (NULL == fp) {
		perror("couldn't open assembly file"); 
		return 1;
	}

	/* for error reporting */
	input_filename = filename;

	/* initialized global variables for first pass */
	pass = FIRST_PASS;
	full_instruction_index = 0;
	code_index = 0;
	data_index = 0;

	/* first pass (expecting failure) */
	for (input_linenumber = 1;
	     fgets(line, MAX_LINE_LENGTH, fp);
	     input_linenumber++) {
		/* TODO replace with |= ? */
		if (parse_line(line)) {
			failed = 1;
		}
	}

	/* first pass failed so close the file
	 * and return (no need to do a second pass) */
	if (failed)
	{
		fclose(fp);
		return 1;
	}

	/* rewind to the beginning of the file
	 * note that rewind is not used on purpose */
	if (fseek(fp, 0, SEEK_SET)) {
		perror("input file rewind failed");
		fclose(fp);
	}

	/* initialized global variables for second pass 
	 * data index is not initialized on purpose */
	pass = SECOND_PASS;
	full_instruction_index = 0;
	code_index = 0;

	/* second pass */
	for (input_linenumber = 1;
	     fgets(line, MAX_LINE_LENGTH, fp);
	     input_linenumber++) {
		if (parse_line(line))
		{
			failed = 1;
		}
	}

	fclose(fp);
	
	return failed;
}

