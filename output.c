#include <stdio.h>
#include <string.h>

#include "output.h"
#include "consts.h"

extern int code_index;
extern int data_index;
extern char data_section[DATA_SECTION_MAX_LENGTH];
extern char code_section[CODE_SECTION_MAX_LENGTH];

void print_base4(FILE *fp, int number)
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

   /* now print the result in reverse order */
   --index;  /* back up to last entry in the array */
   for(  ; index>=0; index--) /* go backward through array */
   {
	 fprintf(fp, "%c", base_digits[converted_number[index]]);
   }
}

void output_data(FILE *fp)
{
	int i;

	for (i = 0; i < data_index; i++)
	{
		print_base4(fp, DATA_START_OFFSET + i);
		fprintf(fp, "\t");
		print_base4(fp, data_section[i]);
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

	print_base4(obj_fp, code_index);
	fprintf(obj_fp, "\t");
	print_base4(obj_fp, data_index);
	fprintf(obj_fp, "\n");

	output_data(obj_fp);
}
