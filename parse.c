#include <stdlib.h> /* for strtol */
#include <stdio.h> /* for fopen, fclose, fgets */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isalpha and isalnum */
#include <limits.h> /* for LONG_MIN and LONG_MAX */

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"

/* gloabl variables that the parsed file is stored in
 * used by the output function in output.c */
full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
int full_instruction_index = 0;
char data_section[DATA_SECTION_MAX_LENGTH];
unsigned int data_index = 0;
unsigned int code_index = 0;

/* global variables used internaly for parsing */
static int label_defined; 
static enum {
	FIRST_PASS,
	SECOND_PASS
} pass; /* parser pass number */
static char label_definition[MAX_LABEL_LENGTH];
static char label_declaration[MAX_LABEL_LENGTH];
static char *input_line = NULL; /* the current parsed line */
static char *input_line_start = NULL; /* the current parsed line start */
static char *input_filename = NULL; /* used for errors */
static unsigned int input_linenumber = 0; /* used for errors */
/* available instructions */
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

/* =============================================
 * =============================================
 * Note: all parsing function start with parse_
 * and have the same basic structure: on error 
 * they return 1 so logical operators where used
 * to simplify parsing this way:
 * || - means that there was no error next parsing
 * function should be called 
 * ==============================================
 * ============================================*/

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
 * NAME: parse_string
 * PARAMS: s - the string to parse 
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION: parse a string 
 ***********************************************/
static int parse_string(char *s)
{
	char gripe[MAX_LINE_LENGTH];
	if (strncmp(input_line, s, strlen(s)) == 0) {
		input_line += strlen(s);
		return 0;
	}

	sprintf(gripe, "expected %s", s);
	parse_error(gripe);
	return 1;
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
 * RETURN VALUE: 0 always (compatible parse 
 * 	 	 function return value)
 * DESCRIPTION: promote input_line to a nonblank
 *              character
 ***********************************************/
static int parse_whitespace(void)
{
	while (isblank(*input_line)) {
		input_line++;
	}
	return 0;
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

	return parse_whitespace();
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

/************************************************
 * NAME: parse_label
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: name - parsed label
 * DESCRIPTION: parse a label and return it in 
 * 		the name output parameter 
 ************************************************/
static int parse_label(char *name)
{
	/* used for getting the size of the length */
	char *label_begin = input_line;
	int label_length;

	if (!isalpha(*input_line)) {
		parse_error("label must start with alphabetic character");
		return 1;
	}

	/* skip all alphanumberic characters */
	while (isalnum(*input_line)) {
		input_line++;
	}

	label_length = input_line - label_begin;
	if (label_length > MAX_LABEL_LENGTH) {
		parse_error("label too long");
		return 1;
	}

	/* copy the parsed string to the out parameter */
	strncpy(name, label_begin, label_length);
	/* put null at the end of the string */
	name[label_length] = '\0';

	return 0;
}

/************************************************
 * NAME: parse_label_use
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: name - parsed label
 * DESCRIPTION: parse a label and check if it's
 * 		installed on the second pass 
 ************************************************/
static int parse_label_use(char *name)
{
	if (parse_label(name)) {
		return 1;
	}

	if (pass == SECOND_PASS && !lookup_label(name)) {
		parse_error("label that isn't defined is used");
		return 1;
	}

	return 0;
}

/************************************************
 * NAME: parse_label_definition
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse a label and return it in 
 * 		the name output parameter 
 ************************************************/
static int parse_label_definition(void)
{
	if (strchr(input_line, ':')) {
		label_defined = 1;
		return parse_label(label_definition) || parse_string(":");
	}

	return 0;
}

/************************************************
 * NAME: parse_label_declaration
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse the label declration 
 *              (global variable) 
 ************************************************/
static int parse_label_declaration(void)
{
	return parse_label(label_declaration);
}

/************************************************
 * NAME: parse_number
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: x - number output parameter
 * DESCRIPTION: parse an integer
 ************************************************/
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

/************************************************
 * NAME: parse_data_number
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse an integer into data section
 ************************************************/
static int parse_data_number(void)
{
	long int x;

	if (parse_number(&x)) {
		return 1;
	}

	data_section[data_index] = x;
	data_index++;
	return 0;
}

/************************************************
 * NAME: install_label_defintion
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: section - the section associated with 
 * 		     the label to install
 * DESCRIPTION: install a label defintion
 ************************************************/
static int install_label_defintion(label_section_t section)
{
	label_t *l;

	/* look if the label is already installed  */
	l = lookup_label(label_definition);
	if (l != NULL) {
		if (l->type == EXTERNAL) {
			parse_error("label declared as external " \
			            "cannot be defined");
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
	l->address = section == CODE ? code_index : data_index;
	l->has_address = 1;

	return 0;
}

/************************************************
 * NAME: install_label_declaration
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: type - the type of the label to be
 * 		  installed
 * DESCRIPTION: install a label declration
 ************************************************/
static int install_label_declaration(label_type_t type)
{
	label_t *l;

	/* check if the label is already installed 
	 * and if so, check if it was declared before */
	l = lookup_label(label_declaration);
	if (l != NULL && l->type != REGULAR) {
		parse_error("label already declared");
		return 1;
	}
	else {
		/* install the new label */
		if (install_label(label_declaration, &l)) {
			return 1;
		}

		l->has_address = 0;
		strncpy(l->name, label_declaration, strlen(label_declaration));
	}

	l->type = type;

	return 0;
}

/************************************************
 * NAME: parse_data_directive
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse a data directive 
 ************************************************/
static int parse_data_directive(void)
{
	/* if a label is defined install it */
	if (label_defined && install_label_defintion(DATA)) {
		return 1;
	}
	
	/* parse at least one number */
	if (parse_data_number()) {
		return 1;
	}

	/* parse the rest of the numbers */
	while (strchr(input_line, ',')) {
		if (parse_whitespace() || \
		    parse_string(",") || \
		    parse_whitespace() || \
		    parse_data_number() || \
		    parse_whitespace()) {
			return 1;
		}
	}

	return 0;
}

/************************************************
 * NAME: parse_string_directive
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse a string directive 
 ************************************************/
static int parse_string_directive(void)
{
	/* a string starts with " */
	if (parse_string("\"")) {
		return 1;
	}

	/* install the label if defined  */
	if (label_defined && install_label_defintion(DATA)) {
		return 1;
	}

	/* parse all chaacters until " */
	while (*input_line && *input_line != '"')
	{
		data_section[data_index] = *input_line;
		data_index++;
		input_line++;
	}

	/* expect a closing " */
	if (parse_string("\"")) {
		return 1;
	}

	/* close the string */
	data_section[data_index] = '\0';
	/* for last byte */
	data_index++;

	return 0;
}

/************************************************
 * NAME: parse_entry_directive
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse an entry directive 
 ************************************************/
static int parse_entry_directive(void)
{
	return parse_label_declaration() || install_label_declaration(ENTRY);
}

/************************************************
 * NAME: parse_extern_directive
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse an extern directive 
 ************************************************/
static int parse_extern_directive(void)
{
	return parse_label_declaration() || install_label_declaration(EXTERNAL);
}

/************************************************
 * NAME: parse_end_of_line
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse NULL or newline at the end 
 * 		at the end of the line 
 ************************************************/
static int parse_end_of_line(void)
{
	parse_whitespace();
	if (*input_line != '\0' && *input_line != '\n') {
		parse_error("garbage in end of line");
		return 1;
	}

	return 0;
}

/************************************************
 * NAME: parse_directive
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse a directive  
 ************************************************/
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

	/* try to find a matching directive and if so run it's parsing function */
	for (i = 0; sizeof(directives)/sizeof(directives[0]); i++) {
	 	if (strncmp(input_line, directives[i].name, strlen(directives[i].name)) == 0) {
			return parse_string(directives[i].name) || parse_whitespace_must() || directives[i].function();
		}
	}
	
	/* if flow reaches here no directive was found */
	parse_error("no such directive");
	return 1;
}

/************************************************
 * NAME: parse_instruction_comb
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: full_instruction - the instrcution 
 *                            fill the comb and
 *                            type value at
 * DESCRIPTION: parse the comb value after the
 * 		instruction   
 ************************************************/
static int parse_instruction_comb(full_instruction_t *full_instruction)
{
	full_instruction->type = 0;
	full_instruction->comb = 0;

	if (parse_string("/")) {
		return 1;
	}
	if (*input_line == '0') {
		return parse_string("0");
	} else if (*input_line == '1') {
		full_instruction->type = 1;
		if (parse_string("1")) {
			return 1;
		}

		if (parse_string("/")) {
			return 1;
		}

		if (*input_line == '0') {
			if (parse_string("0")) {
				return 1;
			}
		} else if (*input_line == '1') {
			if (parse_string("1")) {
				return 1;
			}
			full_instruction->comb += 2;
		} else {
			parse_error("expected 0 or 1");
			return 1;
		}

		if (parse_string("/")) {
			return 1;
		}

		if (*input_line == '0') {
			if (parse_string("0")) {
				return 1;
			}
		} else if (*input_line == '1') {
			if (parse_string("1")) {
				return 1;
			}
			full_instruction->comb += 1;
		} else {
			parse_error("expected 0 or 1");
			return 1;
		}
	} else {
		parse_error("expected 0 or 1");
		return 1;
	}

	return 0;
}

/************************************************
 * NAME: is_register_operand
 * RETURN VALUE: is register operand
 * DESCRIPTION: check if parsing a register operand 
 * ************************************************/
static int is_register_operand(void)
{
	return input_line[0] == 'r' && \
		input_line[1] >= '0' && \
		input_line[1] <= '7';
}

/************************************************
 * NAME: is_immediate_operand
 * RETURN VALUE: is immediate operand
 * DESCRIPTION: check if parsing an immediate operand 
 * ************************************************/
static int is_immediate_operand(void)
{
	return input_line[0] == '#';
}

/************************************************
 * NAME: is_direct_register_operand
 * RETURN VALUE: is a direct register operand
 * DESCRIPTION: check if parsing a direct register 
 * 		operand 
 * ************************************************/
static int is_direct_register_operand(void)
{
	return is_register_operand();
}

/************************************************
 * NAME: parse_register
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: reg - a pointer to the register operand
 * DESCRIPTION: parse a register operand 
 * ************************************************/
static int parse_register(int *reg)
{
	if (parse_string("r")) {
		return 1;
	}

	*reg = *input_line++ - '0';
	if (*reg < 0 || *reg > 7) {
		parse_error("invalid register");
		return 1;
	}
	return 0;
}

/************************************************
 * NAME: parse_instruction_operand_immediate
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: immediate - a pointer to the immediate 
 * 		       operand
 * DESCRIPTION: parse an immediate operand 
 * ************************************************/
static int parse_instruction_operand_immediate(long *immediate)
{
	return parse_string("#") || parse_number(immediate);
}

/************************************************
 * NAME: parse_instruction_operand_register_direct
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: reg - a pointer to the register operand
 * DESCRIPTION: parse a direct register operand 
 * ************************************************/
static int parse_instruction_operand_register_direct(int *reg)
{
	return parse_register(reg);
}

/************************************************
 * NAME: parse_instruction_operand
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: operand - a pointer to the operand to
 * 		     to be parsed
 * 	   available_address_modes - the available 
 * 	                             address modes 
 * DESCRIPTION: parse an operand  
 * ************************************************/
static int parse_instruction_operand(operand_t *operand, int available_address_modes)
{
	/* check for immediate operand */
	if (is_immediate_operand()) {
		code_index++;
		operand->type = IMMEDIATE_ADDRESS;
		if (parse_instruction_operand_immediate(&(operand->value.immediate))) {
			return 1;
		}
	/* check for direct register operand */
	} else if (is_direct_register_operand()) {
		operand->type = DIRECT_REGISTER_ADDRESS;
		if (parse_instruction_operand_register_direct(&(operand->value.reg))) {
			return 1;
		}
	}
	/* check for direct or index operand */
	else {
		/* parse the label */
		code_index++;
		if (parse_label_use(operand->value.label)) {
			return 1;
		}
		parse_whitespace();
		/* check for index operand */
		if (*input_line == '{') {
			operand->type = INDEX_ADDRESS;
			if (parse_string("{")) {
				return 1;
			}
			/* check for index register operand */
			if (is_register_operand()) {
				operand->index_type = REGISTER;
				if (parse_register(&(operand->index.reg))) {
					return 1;
				}
			/* check for index immediate operand */
			} else if (isdigit(*input_line) || *input_line == '-' || *input_line == '+') {
				operand->index_type = IMMEDIATE;
				code_index++;
				if (parse_number(&(operand->index.immediate))) {
					return 1;
				}
			/* index direct operand */
			} else if (!parse_label_use(operand->index.label)) {
				operand->index_type = LABEL;
				code_index++;
			} else {
				parse_error("invalid index addersing");
				return 1;
			}
			if (parse_string("}")) {
				return 1;
			}
		} else {
			operand->type = DIRECT_ADDRESS;
		}

	}

	/* check if the address mode is available */
	if (!(operand->type & available_address_modes)) {
		parse_error("address mode not allowed for this instruction");
		return 1;
	}
	return 0;
}

/************************************************
 * NAME: parse_instruction_name
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: instruction - a pointer to the parsed 
 * 			 instruction  
 * DESCRIPTION: parse the instruction name 
 * ************************************************/
static int parse_instruction_name(instruction_t **instruction)
{
	int i;

	for (i = 0; i < sizeof(instructions)/sizeof(instructions[0]); i++) {
		if (strncmp(input_line, instructions[i].name, INSTRUCTION_NAME_LENGTH) == 0) {
			*instruction = &instructions[i];
			return parse_string((*instruction)->name);
		}
	}

	/* instruction not found */
	parse_error("invalid instruction");
	return 1;

}

/************************************************
 * NAME: parse_instruction
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse an instruction
 *************************************************/
static int parse_instruction(void)
{
	/* get an instruction to hold parseed instruction */
	full_instruction_t *full_instruction = &full_instructions[full_instruction_index];
	full_instruction_index++;


	/* install label if on first pass */
	if (label_defined && pass == FIRST_PASS && install_label_defintion(CODE)) {
		return 1;
	}

	/* parse instruction name */
	if (parse_instruction_name(&(full_instruction->instruction))) {
		return 1;
	}

	/* parse instruction comb */
	if (parse_instruction_comb(full_instruction)) {
		return 1;
	}

	code_index++;
	/* parse instruction operands */
	switch (full_instruction->instruction->num_opernads) {
		case 2:
			return parse_whitespace_must() || \
			       parse_instruction_operand(&(full_instruction->src_operand),
					       		full_instruction->instruction->src_address_modes) || \
			       parse_whitespace() || \
			       parse_string(",") || \
			       parse_whitespace() || \
			       parse_instruction_operand(&(full_instruction->dest_operand), 
							full_instruction->instruction->dest_address_modes);
			break;
		case 1: 
			return parse_whitespace_must() || \
			       parse_instruction_operand(&(full_instruction->dest_operand), 
						        full_instruction->instruction->dest_address_modes);
	}

	return 0;
}

/************************************************
 * NAME: parse_action_line
 * RETURN VALUE: 0 on success, 1 otherwise
 * DESCRIPTION: parse an action line
 * ************************************************/
static int parse_action_line(void)
{
	/* try to parse the label defintion */
	if (parse_label_definition()) {
		return 1;
	}

	/* if a label was defined whitespace must
	 * be after it's defintion, else whitespace
	 * can occurr but not must */
	if (label_defined) {
		if (parse_whitespace_must()) {
			return 1;
		}
	} else {
		parse_whitespace();
	}

	/* check for directive */
	if (*input_line == '.') {
		/* skip directive parsing on second pass */
		if (pass == SECOND_PASS) {
			return 0;
		}
		if (parse_string(".") || parse_directive()) {
			return 1;
		}
	} else {
		/* if it's not a directive it must 
		 * be an instruction */
		if (parse_instruction()) {
			return 1;
		}
	}

	return parse_end_of_line();	
}

/************************************************
 * NAME: parse_line
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: line - the line to parse  
 * DESCRIPTION: run the first and second pass on 
 * 		a given file
 * ************************************************/
static int parse_line(char *line)
{
	/* zero globals corresponding to a single 
	 * line parse */
	label_defined = 0;
	input_line = input_line_start = line;

	/* parse a line only if it's not a
	 * comment and not empty */
	if (!is_comment() && !is_empty()) {
		return parse_action_line();
	}
	return 0;
}

/* exported functions */

/************************************************
 * NAME: parse_file
 * RETURN VALUE: 0 on success, 1 otherwise
 * PARAMS: filename - the filename to parse  
 * DESCRIPTION: run the first and second pass on 
 * 		a given file
 * ************************************************/
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
		failed |= parse_line(line);
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

