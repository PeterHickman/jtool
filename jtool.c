/*
 * jtool - JPEG editing tool
 *
 * $Id: jtool.c,v 1.10 2005/10/26 21:10:02 peterhickman Exp $
 * $Revision: 1.10 $
 * $Author: peterhickman $
 * $Date: 2005/10/26 21:10:02 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * TODO
 *
 * Write at least some sort of test suite
 *
 * Add an --add-comment option
 */

/****************************************************************************
 * Flags used to indicate which command line options were selected          *
 ****************************************************************************/

static bool     show_sig = false;
static bool     show_tail = false;
static bool     show_comment = false;
static bool     show_filename = false;
static bool     delete_tail = false;
static bool     delete_comment = false;
static bool     any_argument = false;

/****************************************************************************
 * Keeping an eye on which tags we have already processed                   *
 ****************************************************************************/

static int      previous_tag;

/****************************************************************************
 * The jpg is parsed with a state machine, these are the states             *
 ****************************************************************************/

#define READ_FF 1
#define READ_TAG 2
#define READ_ECS 3
#define READ_APPENDED1 4
#define READ_APPENDED2 5

/****************************************************************************
 * How big a block of data to read when reading sized tags                  *
 ****************************************************************************/

#define EAT_SIZE 1000
#define COPY_SIZE 10000

/****************************************************************************
 * Usage                                                                    *
 ****************************************************************************/

void
usage(void)
{
	puts("jtool - jpg editing tool");
	puts("usage: jtool [options] list of jpg files");
	puts("");
	puts("$Revision: 1.10 $");
	puts("$Date: 2005/10/26 21:10:02 $");
	puts("");
	puts("Options");
	puts("-------");
	puts("   --show-sig       : Show the file tag signature");
	puts("   --show-comment   : Display any comment in the file");
	puts("   --show-tail      : Display any data appended to the file");
	puts("");
	puts("   --show-filename  : Display the filename when processing the file");
	puts("");
	puts("   --delete-comment : Delete the comment from the file");
	puts("   --delete-tail    : Delete any data appended to the file");
	puts("");
	puts("Defaults to --show-filename and --show-sig if no options are given");

	exit(0);
}

/****************************************************************************
 * Check that the file, appears, to be a JPG file                           *
 ****************************************************************************/

bool
check_magic(FILE * fh)
{
	char            data[2];

	fread(data, 1, 2, fh);
	rewind(fh);

	if (data[0] == (char) 0xff && data[1] == (char) 0xd8) {
		return true;
	} else {
		return false;
	}
}

/****************************************************************************
 * Validate the parameters                                                  *
 ****************************************************************************/

int
checkparameters(int argc, char *argv[])
{
	static const char *OPT_SHOW_SIG = "--show-sig";
	static const char *OPT_SHOW_TAIL = "--show-tail";
	static const char *OPT_SHOW_COMMENT = "--show-comment";
	static const char *OPT_SHOW_FILENAME = "--show-filename";
	static const char *OPT_DELETE_TAIL = "--delete-tail";
	static const char *OPT_DELETE_COMMENT = "--delete-comment";
	static const char *OPT_START = "--";

	int             i;
	int             file_index = 0;

	for (i = 1; i < argc; ++i) {
		/*
		 * Check for arguments
		 */

		if (strncmp(argv[i], OPT_START, 2) == 0) {
			if (file_index != 0)
				err(1, "Cannot give arguments after files");

			any_argument = true;

			if (strcmp(argv[i], OPT_SHOW_SIG) == 0) {
				show_sig = true;
			} else if (strcmp(argv[i], OPT_SHOW_TAIL) == 0) {
				show_tail = true;
			} else if (strcmp(argv[i], OPT_SHOW_COMMENT) == 0) {
				show_comment = true;
			} else if (strcmp(argv[i], OPT_SHOW_FILENAME) == 0) {
				show_filename = true;
			} else if (strcmp(argv[i], OPT_DELETE_TAIL) == 0) {
				delete_tail = true;
			} else if (strcmp(argv[i], OPT_DELETE_COMMENT) == 0) {
				delete_comment = true;
			} else {
				err(1, "Invalid argument: %s", argv[i]);
			}
		} else {
			/*
			 * A file to process
			 */

			if (file_index == 0)
				file_index = i;
		}
	}

	return file_index;
}

/****************************************************************************
 * Read the required number of bytes from the file handle                   *
 ****************************************************************************/

int
eat_bytes(FILE * fh, bool write)
{
	int             size;	/* How much data to read */
	int             size_plus;	/* The size of the data after the tag */
	int             c;	/* Helps to calculate the above */
	int             x;	/* How much data we read in one go */
	char            data[EAT_SIZE];

	/* How big is the chunk of data ? */

	size = fgetc(fh);
	if (size == EOF) {
		puts("Ran out of data reading tag size [1]");
		return -1;
	}

	c = fgetc(fh);
	if (size == EOF) {
		puts("Ran out of data reading tag size [2]");
		return -1;
	}

	size_plus = ((size * 256) + c);
	size = size_plus - 2;

	/* Read the data in EAT_SIZE chunks */

	while (size > 0) {
		x = (size > EAT_SIZE) ? EAT_SIZE : size;

		c = fread(data, 1, x, fh);
		if (c != x) {
			puts("Ran out of data eating a buffer");
			return -1;
		}
		if (write)
			fwrite(data, 1, x, stdout);
		size -= x;
	}

	return size_plus;
}

/****************************************************************************
 * Display tags if required                                                 *
 ****************************************************************************/

void
report_tag(const int tag)
{
	if(tag != 216 && show_sig)
		putchar(' ');

	if (tag == -1) {
		if (previous_tag != -1 && show_sig)
			printf("XX");
	} else if (show_sig)
		printf("%02x", tag);

	previous_tag = tag;
}

/****************************************************************************
 * Return true if the given tag has data after it                           *
 ****************************************************************************/

bool
tag_has_size(const int tag)
{
	static const int tags_with_size[] = {
		0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
		0x0d, 0x0e, 0x0f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
		0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd,
		0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
		0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xda, 0xdb, 0xdc, 0xdd,
		0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
		0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
		0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
		0x00
	};

	int             i = 0;

	while (tags_with_size[i] != 0) {
		if (tags_with_size[i] == tag)
			return true;
		++i;
	}

	return false;
}

/****************************************************************************
 * Delete a comment from the file                                           *
 ****************************************************************************/

void
copy_file_contents(const char *infilename, const char *outfilename)
{
	FILE           *infh, *outfh;
	char            data[EAT_SIZE];
	int             amount;

	if (!(infh = fopen(infilename, "rb"))) {
		printf("Failed to open the file: %s\n", infilename);
		return;
	}
	if (!(outfh = fopen(outfilename, "wb"))) {
		printf("Failed to open the file: %s\n", outfilename);
		return;
	}
	amount = -1;
	while (amount != 0) {
		amount = fread(data, 1, COPY_SIZE, infh);
		fwrite(data, 1, amount, outfh);
	}

	if (fclose(infh)) {
		printf("Failed to close the file: %s\n", infilename);
		return;
	}
	if (fclose(outfh)) {
		printf("Failed to close the file: %s\n", outfilename);
		return;
	}
}

void
delete_comment_from_file(const char *filename, long start, long size)
{
	char           *tempfile;	/* Holds the temp file name */
	FILE           *infh;	/* Handle for the source file */
	FILE           *outfh;	/* Handle for the temp file */
	int             bytes_to_read;
	char            data[EAT_SIZE];
	int             x;
	int             c;

	/* Create a temporary file */
	tempfile = tmpnam(NULL);

	/* Open the files */

	if (!(infh = fopen(filename, "rb"))) {
		printf("Failed to open the file: %s\n", filename);
		return;
	}
	if (!(outfh = fopen(tempfile, "wb"))) {
		printf("Failed to open the file: %s\n", tempfile);
		return;
	}
	/*
	 * Read the first (start - 2) bytes to include the tag with the data
	 * being removed
	 */

	bytes_to_read = start - 2;

	while (bytes_to_read > 0) {
		x = (bytes_to_read > EAT_SIZE) ? EAT_SIZE : bytes_to_read;

		c = fread(data, 1, x, infh);
		if (c != x)
			err(1, "Ran out of data eating a buffer");

		c = fwrite(data, 1, x, outfh);
		if (c != x)
			err(1, "Unable to write data to the temp file");

		bytes_to_read -= x;
	}

	/* Skip the next size bytes */

	bytes_to_read = size;
	while (bytes_to_read > 0) {
		x = (bytes_to_read > EAT_SIZE) ? EAT_SIZE : bytes_to_read;

		c = fread(data, 1, x, infh);
		if (c != x)
			err(1, "Ran out of data eating a buffer");
		bytes_to_read -= x;
	}

	/* Read to end of file */

	bytes_to_read = 1;
	while (bytes_to_read > 0) {
		bytes_to_read = fread(data, 1, EAT_SIZE, infh);
		if (bytes_to_read) {
			c = fwrite(data, 1, bytes_to_read, outfh);
			if (c != bytes_to_read)
				err(1, "Unable to write data to the temp file");
		}
	}

	/* Close down and swap the files over */
	if (fclose(infh)) {
		printf("Failed to close the file: %s\n", filename);
		return;
	}
	if (fclose(outfh)) {
		printf("Failed to close the file: %s\n", tempfile);
		return;
	}
	/* Remove the source file */
	if ((c = unlink(filename)) != 0) {
		printf("Unable to unlink %s\n", filename);
		return;
	}
	/* Write the commentless data bacl into the file */
	copy_file_contents(tempfile, filename);
}

/****************************************************************************
 * Process a single file                                                    *
 ****************************************************************************/

void
process(const char *filename)
{
	static const int TAG_LAST = 217;
	static const int TAG_COMMENT = 254;
	static const int TAG_MARK = 255;

	FILE           *fh;
	int             mode;
	int             c;
	long            tail_starts = 0;
	long            comment_starts = 0;
	long            comment_size = 0;

	if (!(fh = fopen(filename, "rb"))) {
		printf("Failed to open the file: %s\n", filename);
		return;
	}
	if (show_filename)
		printf("%s ", filename);

	if (check_magic(fh) == false) {
		puts("Does not appear to be a JPG file");
		return;
	}
	previous_tag = 0;

	mode = READ_FF;

	while ((c = fgetc(fh)) != EOF) {
		switch (mode) {
		case READ_FF:
			if (c == TAG_MARK) {
				mode = READ_TAG;
			} else {
				report_tag(-1);
				mode = READ_ECS;
			}
			break;
		case READ_TAG:
			if (c != 0 && c != TAG_MARK) {
				int             size;

				report_tag(c);

				/* Is this a comment */
				if (c == TAG_COMMENT)
					comment_starts = ftell(fh) - 1;

				/* Eat up the data that goes with the tag */
				if (tag_has_size(c)) {
					if (c == TAG_COMMENT && show_comment)
						size = eat_bytes(fh, true);
					else
						size = eat_bytes(fh, false);

					if(size == -1) 
						return;
				}
				/* Are we past the end of the jpg data? */
				if (c == TAG_LAST) {
					mode = READ_APPENDED1;
				} else
					mode = READ_FF;

				/* Is this a comment */
				if (c == TAG_COMMENT)
					comment_size = size + 2;
			} else {
				mode = READ_ECS;
			}
			break;
		case READ_ECS:
			if (c == TAG_MARK)
				mode = READ_TAG;
			break;
		case READ_APPENDED1:
			tail_starts = ftell(fh) - 1;
			report_tag(-1);
			mode = READ_APPENDED2;
			if (show_tail)
				putchar(c);
			break;
		case READ_APPENDED2:
			if (show_tail)
				putchar(c);
			break;
		}
	}

	if (show_filename || show_sig)
		printf("\n");

	if (fclose(fh)) {
		printf("Failed to close the file: %s\n", filename);
		return;
	}
	/* Chop off the tail if we have to */

	if (delete_tail && tail_starts > 0) {
		if (truncate(filename, tail_starts) != 0) {
			puts("Unable to truncate file");
		}
	}
	/* Do we have a comment to remove */
	if (delete_comment && comment_starts > 0)
		delete_comment_from_file(filename, comment_starts, comment_size);
}

/****************************************************************************
 * Main: Check the parameters and if all is ok process the files            *
 ****************************************************************************/

int
main(int argc, char *argv[])
{
	int             i;
	int             file_index;

	/* Check first for any command line parameters */

	if (argc == 1)
		usage();

	/* Check and tick off the various options we know about */

	file_index = checkparameters(argc, argv);

	/* Are there any files on the command line */

	if (file_index == 0)
		err(1, "No files given to process");

	/* Default the arguments if none are given */

	if (any_argument == false) {
		show_sig = true;
		show_filename = true;
	} else {
		if (show_sig && show_comment)
			err(1, "Do not show the signature when showing the comment");

		if (show_sig && (delete_comment || delete_tail))
			err(1, "Show signature makes no sense when deleting anything");
	}

	/* Process each of the files */

	for (i = file_index; i < argc; ++i)
		process(argv[i]);

	return (0);
}
