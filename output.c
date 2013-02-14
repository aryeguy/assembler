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
	assembled_instruction += full_instruction.comb << COMB_OFFSET;
	assembled_instruction += full_instruction.first_op.value.reg << SRC_REGISTER_OFFSET;
	assembled_instruction += full_instruction.first_op.type << SRC_ADDRESS_MODE_OFFSET;
	assembled_instruction += full_instruction.second_op.value.reg << DEST_REGISTER_OFFSET;
	assembled_instruction += full_instruction.second_op.type << DEST_ADDRESS_MODE_OFFSET;
	assembled_instruction += full_instruction.instruction->code << DEST_ADDRESS_MODE_OFFSET;
	assembled_instruction += full_instruction.instruction->code << DEST_ADDRESS_MODE_OFFSET;
	return assembled_instruction;
}

void output_data(FILE *fp)
{
	int i;

	for (i = 0; i < data_index; i++)
	{
		print_base4(fp, START_OFFSET + code_index + i, 8);
		fprintf(fp, "\t");
		print_base4(fp, data_section[i], 8);
		fprintf(fp, "\n");
	}
}

void output_code(FILE *fp)
{
	int i;

	for (i = 0; i < full_instruction_index; i++)
	{
		print_base4(fp, START_OFFSET + i, 8);
		fprintf(fp, "\t");
		print_base4(fp, assemble_instruction(full_instructions[i]), 8);
		fprintf(fp, "\n");
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
