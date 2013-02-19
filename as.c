#include <stdio.h> /* for fopen, fclose, fgets */
#include <stdlib.h> /* for EXIT_SUCCESS */
#include <string.h> /* for strncpy and strncat */
#include <ctype.h> /* for isspace */

#include "consts.h"
#include "types.h"
#include "table.h"
#include "parse.h"
#include "output.h"

/**************************************
 * NAME: process_assembly_file 
 * PARAMS: source_filename - the source filename 
 *         without the .as extention
 *************************************/
static int process_assembly_file(const char * source_filename)
{
	/* filename with .as extention */
	char actual_source_filename[MAX_FILENAME_LENGTH];

	/* actual_source_filename <- source_filename + ".as" */
	strncpy(actual_source_filename, source_filename, MAX_FILENAME_LENGTH);
	strncat(actual_source_filename, ".as", MAX_FILENAME_LENGTH - strlen(source_filename));

	/* try to parse file, if succeedes create output files */
	return parse_file(actual_source_filename) || output(source_filename);
}

/***************************************
 * NAME: main
 **************************************/
int main(int argc, const char *argv[])
{
	int i;
	int rc = 0;

	for (i = 1; i < argc; i++) {
		/* using bitwise or to collect any error */
		rc |= process_assembly_file(argv[i]);
	}

	/* return exit code compatible with stdlib */
	exit(rc ? EXIT_SUCCESS : EXIT_FAILURE);
}
