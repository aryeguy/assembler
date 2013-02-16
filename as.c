#include <stdio.h> /* for fopen, fclose, fgets */
#include <stdlib.h> /* for EXIT_SUCCESS */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isspace */

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"
#include "output.h"

void process_assembly_file(const char * source_filename);

void process_assembly_file(const char * source_filename)
{
	char actual_source_filename[MAX_FILENAME_LENGTH];

	/* actual_source_filename <- source_filename + ".as" */
	strncpy(actual_source_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(actual_source_filename, ".as", MAX_FILENAME_LENGTH - strlen(source_filename));

	if (parse_file(actual_source_filename)) {
		return;
	}

	/* best effort, no error handling on purpose */
	output(source_filename);
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
