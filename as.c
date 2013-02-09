#include <stdio.h> /* for fopen, fclose, fgets */
#include <stdlib.h> /* for EXIT_SUCCESS */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isspace */

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"

void process_assembly_file(const char * source_filename);
extern char *input_filename;
extern unsigned int input_linenumber;

void process_assembly_file(const char * source_filename)
{
	char actual_source_filename[MAX_FILENAME_LENGTH];
	FILE *fp;
	char line[MAX_LINE_LENGTH];

	/* actual_source_filename <- source_filename + ".as" */
	strncpy(actual_source_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(actual_source_filename, ".as", MAX_FILENAME_LENGTH);

	fp = fopen(actual_source_filename, "rt");
	input_filename = actual_source_filename;
	if (NULL == fp) {
		perror("couldn't open assembly file"); 
		return;
	}

	for (input_linenumber = 1;
	     fgets(line, MAX_LINE_LENGTH, fp);
	     input_linenumber++) {
		parse_line(line);
	}

	fclose(fp);

}

int main(int argc, const char *argv[])
{
	int i;

	for (i = 1; i < argc; i++) {
		process_assembly_file(argv[i]);
	}

#ifdef DEBUG
	print_labels();
#endif

	exit(EXIT_SUCCESS);
}
