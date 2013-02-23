#include <stdio.h> /* for fopen, fprintf and fclose */
#include <string.h> /* for strcpy and strncat */
#include <math.h> /* for pow */

#include "output.h"
#include "types.h"
#include "consts.h"
#include "table.h"

/* global variables declared in parse.c that are used
 * for output */
extern int code_index;
extern int data_index;
extern char data_section[DATA_SECTION_MAX_LENGTH];
extern full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
extern int full_instruction_index;

/* internal global variables */
static FILE *ob_output_file;
static FILE *entries_output_file;
static FILE *externals_output_file;
static const char *original_filenme;
static int output_code_index;

/************************************************
 * NAME: convert
 * PARAMS: number - the number to convert 
 * RETURN VALUE: converted number
 * DESCRIPTION: convert a number to base 4 with
 * 		only 20 bits 
 ***********************************************/
unsigned int convert(unsigned int number)
{
	unsigned int converted_number = 0;
	int i = 0;

	number &= 0xfffff; /* use only 20 bits of the number */

	while (number) {
		converted_number += pow(10, i)*(number % 4);
		i++;
		number /= 4;
	}
	return converted_number;
}

/************************************************
 * NAME: output_entry_label
 * PARAMS: label - the label to output 
 * DESCRIPTION: output a label to the entry file 
 ***********************************************/
static void output_entry_label(label_t *label)
{
	/* open file for writing if not opened */
	if (NULL == entries_output_file) {
		char filename[MAX_FILENAME_LENGTH];

		strncpy(filename, original_filenme, MAX_FILENAME_LENGTH);
		strncat(filename, ".ent", MAX_FILENAME_LENGTH - strlen(original_filenme));
		entries_output_file = fopen(filename, "wt");
		if (NULL == entries_output_file) {
			perror("couldn't create entry file");
			return;
		}
	}
	if (label->type == ENTRY) {
		fprintf(entries_output_file, 
			"%s\t%010u\n", 
			label->name, 
			convert(label->address + START_OFFSET + (label->section == DATA ? code_index : 0)));
	}
}

/************************************************
 * NAME: output_code_line
 * PARAMS: data - the code to output 
 * 	   linkder_data - the linker data 
 * DESCRIPTION: output a code line to the ob file 
 ***********************************************/
static void output_code_line(int data, linker_data_t linker_data)
{
	fprintf(ob_output_file, 
		"%04u\t%010u\t%c\n", 
		convert(output_code_index++), 
		convert(data), 
		linker_data);
}

/************************************************
 * NAME: output_external_label_use
 * PARAMS: label - the label to output 
 * DESCRIPTION: output a label to the extern file 
 ***********************************************/
static void output_external_label_use(label_t *label)
{
	/* open file for writing if not opened */
	if (NULL == externals_output_file) {
		char filename[MAX_FILENAME_LENGTH];

		strncpy(filename, original_filenme, MAX_FILENAME_LENGTH);
		strncat(filename, ".ext", MAX_FILENAME_LENGTH - strlen(original_filenme));
		externals_output_file = fopen(filename, "wt");
		if (NULL == externals_output_file) {
			perror("couldn't create entry file");
			return;
		}
	}
	fprintf(externals_output_file, "%s\t%04u\n", label->name, convert(output_code_index));
}

/************************************************
 * NAME: output_operand_label
 * PARAMS: name - the label to output 
 * DESCRIPTION: output a label to ob file and
 * 		to extern file if needed
 ***********************************************/
static void output_operand_label(char *name)
{
	label_t *label = lookup_label(name);
	if (label->type == EXTERNAL) {
		/* output the label use the to .ext file */
		output_external_label_use(label);
		/* output a line with a zero data and symbol the linker 
		 * that this line needs external linkage */
		output_code_line(0, EXTERNAL_LINKAGE);
	} else {
		output_code_line(label->address + START_OFFSET + (label->section == DATA ? code_index : 0), 
				RELOCATBLE_LINKAGE);
	}
}

/************************************************
 * NAME: output_operand
 * PARAMS: operand - the operand to output 
 * DESCRIPTION: output an operand to ob file and
 * 		to extern file if needed
 ***********************************************/
static void output_operand(operand_t *operand)
{
	switch (operand->type) {
		case IMMEDIATE_ADDRESS:
			output_code_line(operand->value.immediate, ABSOLUTE_LINKAGE);
			break;
		case DIRECT_ADDRESS:
			output_operand_label(operand->value.label);
			break;
		case INDEX_ADDRESS:
			output_operand_label(operand->value.label);
			switch (operand->index_type) {
				case IMMEDIATE:
					output_code_line(operand->index.immediate, ABSOLUTE_LINKAGE);
					break;
				case LABEL:
					output_operand_label(operand->index.label);
					break;
				case REGISTER:
					break;
			}
			break;
		case NO_ADDRESS:
		case DIRECT_REGISTER_ADDRESS: /* FALLTHROUGH */
			break;
	}
}

/************************************************
 * NAME: encode_address_mode
 * PARAMS: mode - the mode to mode 
 * RETURN VALUE: encoded mode
 * DESCRIPTION: encode an address mode 
 ***********************************************/
static int encode_address_mode(address_mode_t mode)
{
	switch (mode) {
		case NO_ADDRESS:
		case IMMEDIATE_ADDRESS: /* FALLTHROUGH */
			return 0;
		case DIRECT_ADDRESS:
			return 1;
		case INDEX_ADDRESS:
			return 2;
		case DIRECT_REGISTER_ADDRESS:
			return 3;
	}
	return 0;
}

/************************************************
 * NAME: output_instruction
 * PARAMS: full_instruction - the instruction to 
 * 			      output 
 * DESCRIPTION: output an instruction to ob file
 ***********************************************/
static void output_instruction(full_instruction_t full_instruction)
{
	int assembled_instruction = 0;

	assembled_instruction += full_instruction.comb << COMB_OFFSET;

	if (full_instruction.dest_operand.type == DIRECT_REGISTER_ADDRESS)
	{
		assembled_instruction += full_instruction.dest_operand.value.reg << DEST_REGISTER_OFFSET;
	}
	else if (full_instruction.dest_operand.type == INDEX_ADDRESS \
	         && full_instruction.dest_operand.index_type == REGISTER) 
	{
		assembled_instruction += full_instruction.dest_operand.index.reg << DEST_REGISTER_OFFSET;
	}
	assembled_instruction += encode_address_mode(full_instruction.dest_operand.type) << DEST_ADDRESS_MODE_OFFSET;

	if (full_instruction.src_operand.type == DIRECT_REGISTER_ADDRESS)
	{
		assembled_instruction += full_instruction.src_operand.value.reg << SRC_REGISTER_OFFSET;
	}
	else if (full_instruction.src_operand.type == INDEX_ADDRESS \
	         && full_instruction.src_operand.index_type == REGISTER) 
	{
		assembled_instruction += full_instruction.src_operand.index.reg << SRC_REGISTER_OFFSET;
	}
	assembled_instruction += encode_address_mode(full_instruction.src_operand.type) << SRC_ADDRESS_MODE_OFFSET;

	assembled_instruction += full_instruction.instruction->opcode << OPCODE_OFFSET;
	assembled_instruction += full_instruction.type << TYPE_OFFSET;

	output_code_line(assembled_instruction, ABSOLUTE_LINKAGE);
}

/************************************************
 * NAME: output_data
 * DESCRIPTION: output all data to the ob file
 ***********************************************/
static void output_data(void)
{
	int i;

	for (i = 0; i < data_index; i++)
	{
		fprintf(ob_output_file,
			"%04u\t%010u\n",
			convert(START_OFFSET + code_index + i),
		       	convert(data_section[i]));
	}
}

/************************************************
 * NAME: output_code
 * DESCRIPTION: output all code to the ob file
 ***********************************************/
static void output_code(void)
{
	int i;
	full_instruction_t *full_instruction;
	int code_index = START_OFFSET;

	for (i = 0; i < full_instruction_index; i++, code_index++)
	{
		full_instruction = &full_instructions[i];
		output_instruction(*full_instruction);
		switch (full_instruction->instruction->num_opernads) {
			case 1:
				output_operand(&full_instruction->dest_operand);
				break;
			case 2:
				output_operand(&full_instruction->src_operand);
				output_operand(&full_instruction->dest_operand);
				break;
		}
	}
}

/************************************************
 * NAME: output_header
 * DESCRIPTION: output ob file header
 ***********************************************/
static void output_header(void)
{
	fprintf(ob_output_file, 
		"%u\t%u\n",
	       	convert(code_index),
	       	convert(data_index));
}

/************************************************
 * NAME: output
 * PARAMS: source_filename - the filename to 
 * 			     output 
 * RETURN VALUE: 1 on error, 0 on success
 * DESCRIPTION:  assemble instructions and output
 * 		 all parsed data to ob ext and 
 * 		 ent files 
 * ASSUMPTIONS: using the global variables from
 * 		parse.c
 ***********************************************/
int output(const char *source_filename)
{
	char obj_filename[MAX_FILENAME_LENGTH];

	output_code_index = START_OFFSET;
	original_filenme = source_filename;
	entries_output_file = NULL;
	externals_output_file = NULL;

	strncpy(obj_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(obj_filename, ".ob", MAX_FILENAME_LENGTH - strlen(source_filename));

	ob_output_file = fopen(obj_filename, "wt");
	if (NULL == ob_output_file) {
		perror("couldn't open obj file"); 
		return 1;
	}

	output_header();
	output_code();
	output_data();

	fclose(ob_output_file);

	/* output all entry labels by looping on the labels table */
	loop_labels(output_entry_label);

	/* close the files if they were opened */
	if (entries_output_file) {
		fclose(entries_output_file);
		entries_output_file = NULL;
	}

	if (externals_output_file) {
		fclose(externals_output_file);
		externals_output_file = NULL;
	}

	return 0;
}
