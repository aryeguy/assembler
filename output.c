#include <stdio.h>
#include <string.h>

#include "output.h"
#include "types.h"
#include "consts.h"

extern int code_index;
extern int data_index;
extern char data_section[DATA_SECTION_MAX_LENGTH];
extern char code_section[CODE_SECTION_MAX_LENGTH];
extern full_instruction_t full_instructions[CODE_SECTION_MAX_LENGTH];
extern int full_instruction_index;

void print_base4(FILE *fp, int number, int padding)
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

int assemble_instruction(full_instruction_t full_instruction)
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
	assembled_instruction += full_instruction.dest_operand.type << DEST_ADDRESS_MODE_OFFSET;

	if (full_instruction.src_operand.type == DIRECT_REGISTER_ADDRESS \
		       	|| (full_instruction.src_operand.type == INDEX_ADDRESS \
			&& full_instruction.src_operand.index_type == REGISTER))
	{
		assembled_instruction += full_instruction.src_operand.value.reg << SRC_REGISTER_OFFSET;
	}
	assembled_instruction += full_instruction.src_operand.type << SRC_ADDRESS_MODE_OFFSET;

	assembled_instruction += full_instruction.instruction->opcode << OPCODE_OFFSET;
	assembled_instruction += type_field << TYPE_OFFSET;

	return assembled_instruction;
}

void output_data(FILE *fp)
{
	int i;

	for (i = 0; i < data_index; i++)
	{
		print_base4(fp, START_OFFSET + code_index + i, 4);
		fprintf(fp, "\t");
		print_base4(fp, data_section[i], 10);
		fprintf(fp, "\n");
	}
}

void output_code(FILE *fp)
{
	int i;

	for (i = 0; i < full_instruction_index; i++)
	{
		print_base4(fp, START_OFFSET + i, 4);
		fprintf(fp, "\t");
		print_base4(fp, assemble_instruction(full_instructions[i]), 10);
		fprintf(fp, "\ta\n");
	}
}

void output(const char *source_filename)
{
	char obj_filename[MAX_FILENAME_LENGTH];
	FILE *obj_fp;

	strncpy(obj_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(obj_filename, ".obj", MAX_FILENAME_LENGTH);

	obj_fp = fopen(obj_filename, "wt");
	if (NULL == obj_fp) {
		perror("couldn't open obj file"); 
		return;
	}

	print_base4(obj_fp, code_index, 0);
	fprintf(obj_fp, "\t");
	print_base4(obj_fp, data_index, 0);
	fprintf(obj_fp, "\n");

	output_code(obj_fp);
	output_data(obj_fp);
}
