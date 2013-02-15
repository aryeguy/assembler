#include <stdio.h>
#include <string.h>

#include "output.h"
#include "types.h"
#include "consts.h"
#include "table.h"

extern int code_index;
extern int data_index;
extern char data_section[DATA_SECTION_MAX_LENGTH];
extern char code_section[CODE_SECTION_MAX_LENGTH];
extern full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
extern int full_instruction_index;

FILE *output_file;
FILE *entries_output_file;
FILE *externals_output_file;
const char *original_filenme;
int output_code_index;

static void output_base4(FILE *fp, int number, int padding)
{
   char base_digits[4] = {'0', '1', '2', '3'};

   int converted_number[64];
   int index=0;

   /* convert to the indicated base */
   while (number != 0)
   {
	 converted_number[index] = number % 4;
	 number = number / 4;
	 ++index;
   }

   padding -= index;
   while (padding-- > 0) {
	   converted_number[index++] = 0;
   }

   /* now print the result in reverse order */
   --index;  /* back up to last entry in the array */
   for(  ; index>=0; index--) /* go backward through array */
   {
	 fprintf(fp, "%c", base_digits[converted_number[index]]);
   }
}

static void output_entry_label(label_t *label)
{
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
		fprintf(entries_output_file, "%s\t", label->name);
		output_base4(entries_output_file, label->address + START_OFFSET + (label->section == DATA ? code_index : 0), 4);
		fprintf(entries_output_file, "\n");
	}
}

static void output_code_line(int data, linker_data_t linker_data)
{
	output_base4(output_file, output_code_index++, 4);
	fprintf(output_file, "\t");
	output_base4(output_file, data, 10);
	fprintf(output_file, "\t");
	switch (linker_data) {
		case ABSOLUTE_LINKAGE:
			fprintf(output_file, "a");
			break;
		case RELOCATBLE_LINKAGE:
			fprintf(output_file, "r");
			break;
		case EXTERNAL_LINKAGE:
			fprintf(output_file, "e");
			break;
	}
	fprintf(output_file, "\n");
}

static void output_external_label_use(label_t *label)
{
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
	fprintf(externals_output_file, "%s\t", label->name);
	output_base4(externals_output_file, output_code_index, 4);
	fprintf(externals_output_file, "\n");
}

static void output_operand_label(label_name_t name)
{
	label_t *label = lookup_label(name);
	if (label->type == EXTERNAL) {
		output_external_label_use(label);
		output_code_line(0, EXTERNAL_LINKAGE);
	} else {
		switch (label->section) {
			case CODE:
				output_code_line(label->address + START_OFFSET, RELOCATBLE_LINKAGE);
				break;
			case DATA:
				output_code_line(label->address + code_index + START_OFFSET, RELOCATBLE_LINKAGE);
				break;
		}
	}
}

static void output_operand(operand_t *operand)
{
	switch (operand->type) {
		case NO_ADDRESS:
			break;
		case IMMEDIATE_ADDRESS:
			output_code_line(operand->value.immediate, ABSOLUTE_LINKAGE);
			break;
		case DIRECT_ADDRESS:
			output_operand_label(operand->value.label);
			break;
		case INDEX_ADDRESS:
			output_code_line(lookup_label(operand->value.label)->address, RELOCATBLE_LINKAGE);
			switch (operand->index_type) {
				case IMMEDIATE:
					output_code_line(operand->index.immediate, ABSOLUTE_LINKAGE);
					break;
				case REGISTER:
					break;
				case LABEL:
					output_operand_label(operand->index.label);
					break;
			}
			break;
		case DIRECT_REGISTER_ADDRESS:
			break;
	}
}

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
		default:
			return 0;
	}
}

static void output_instruction(full_instruction_t full_instruction)
{
	int assembled_instruction = 0;
	int type_field;
	int comb_field;

	switch(full_instruction.comb) {
		case FULL_COMB:
			type_field = 0;
			comb_field = 0;
			break;
		case LEFT_SOURCE_LEFT_DEST_COMB:
			type_field = 1;
			comb_field = 0;
			break;
		case LEFT_SOURCE_RIGHT_DEST_COMB:
			type_field = 1;
			comb_field = 1;
			break;
		case RIGHT_SOURCE_LEFT_DEST_COMB:
			type_field = 1;
			comb_field = 2;
			break;
		case RIGHT_SOURCE_RIGHT_DEST_COMB:
			type_field = 1;
			comb_field = 3;
			break;
	}
	assembled_instruction += comb_field << COMB_OFFSET;

	if (full_instruction.dest_operand.type == DIRECT_REGISTER_ADDRESS \
		       	|| (full_instruction.dest_operand.type == INDEX_ADDRESS \
			&& full_instruction.dest_operand.index_type == REGISTER))
	{
		assembled_instruction += full_instruction.dest_operand.value.reg << DEST_REGISTER_OFFSET;
	}
	assembled_instruction += encode_address_mode(full_instruction.dest_operand.type) << DEST_ADDRESS_MODE_OFFSET;

	if (full_instruction.src_operand.type == DIRECT_REGISTER_ADDRESS \
		       	|| (full_instruction.src_operand.type == INDEX_ADDRESS \
			&& full_instruction.src_operand.index_type == REGISTER))
	{
		assembled_instruction += full_instruction.src_operand.value.reg << SRC_REGISTER_OFFSET;
	}
	assembled_instruction += encode_address_mode(full_instruction.src_operand.type) << SRC_ADDRESS_MODE_OFFSET;

	assembled_instruction += full_instruction.instruction->opcode << OPCODE_OFFSET;
	assembled_instruction += type_field << TYPE_OFFSET;

	output_code_line(assembled_instruction, ABSOLUTE_LINKAGE);
}

static void output_data(void)
{
	int i;

	for (i = 0; i < data_index; i++)
	{
		output_base4(output_file, START_OFFSET + code_index + i, 4);
		fprintf(output_file, "\t");
		output_base4(output_file, data_section[i], 10);
		fprintf(output_file, "\n");
	}
}

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
			case 0:
				break;
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

void output(const char *source_filename)
{
	char obj_filename[MAX_FILENAME_LENGTH];

	output_code_index = START_OFFSET;
	original_filenme = source_filename;
	entries_output_file = NULL;
	externals_output_file = NULL;

	strncpy(obj_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(obj_filename, ".obj", MAX_FILENAME_LENGTH);

	output_file = fopen(obj_filename, "wt");
	if (NULL == output_file) {
		perror("couldn't open obj file"); 
		return;
	}

	output_base4(output_file, code_index, 0);
	fprintf(output_file, "\t");
	output_base4(output_file, data_index, 0);
	fprintf(output_file, "\n");

	loop_labels(output_entry_label);
	output_code();
	output_data();

	fclose(output_file);

	if (entries_output_file) {
		fclose(entries_output_file);
	}
	if (externals_output_file) {
		fclose(externals_output_file);
	}
}
