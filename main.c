#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t bool;
#define true 1
#define false 0

#define COL_RED "\e[31m"
#define COL_RESET "\e[39m"

static const int READ_BUFFER_SIZE = 4096;

static void print_usage(const char *arg0, FILE *file)
{
	fprintf(file, "Usage: %s file1 file2 ...\n", arg0);
}

static void print_byte(int byte, bool diff)
{
	const char *str = diff ? COL_RED : COL_RESET;
	char c[2];
	if (byte < 0 || byte > 0xFF) {
		c[0] = c[1] = '-';
	} else {
		static const char hex[17] = "0123456789ABCDEF";
		c[0] = hex[byte >> 4];
		c[1] = hex[byte & 0xF];
	}
	printf("%s%c%c", str, c[0], c[1]);
}

struct file_t
{
	uint64_t pos;     // Current position in file
	uint64_t size;    // The size of the file
	FILE *descriptor; // The file descriptor
	int buffer_size;
	int buffer_index;
	uint8_t *buffer;
};

int main(int argc, char **argv)
{
	/* Check for 'help' arguments */

	if (argc <= 1) {
		print_usage(argv[0], stderr);
		return 1;
	}

	if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
		print_usage(argv[0], stdout);
		return 0;
	}

	/* Open the input files */

	const int files_count = argc - 1;
	struct file_t files[files_count];

	for (int f = 0; f < files_count; ++f) {
		struct file_t file;
		file.descriptor = fopen(argv[f + 1], "rb");
		if (file.descriptor == NULL) {
			perror("Failed to open file"); // TODO: error message
			// TODO: cleanup
			return 2;
		}
		file.pos = 0;
		fseek(file.descriptor, 0, SEEK_END);
		file.size = ftell(file.descriptor);
		fseek(file.descriptor, 0, SEEK_SET);
		file.buffer_size = 0;
		file.buffer_index = 0;
		file.buffer = (uint8_t *)malloc(READ_BUFFER_SIZE);

		files[f] = file;
	}

	/* Read 16 bytes at a time from file buffer */
	for (;;) {
		// 16 bytes from each file
		// values of -1 on EOF
		int bytes[16][files_count];

		// Read 16 bytes from each buffer
		for (int i = 0; i < 16; ++i) {
			for (int f = 0; f < files_count; ++f) {
				// If the buffer is 'empty', then read from the actual
				// file and fill in the buffer
				if (files[f].buffer_index == files[f].buffer_size) {
					if (files[f].pos == files[f].size) {
						bytes[i][f] = -1;
						continue;
					}
					// TODO: error check
					int b = fread(files[f].buffer, 1, READ_BUFFER_SIZE, files[f].descriptor);
					files[f].pos += b;
					files[f].buffer_size = b;
					files[f].buffer_index = 0;
				}
				bytes[i][f] = files[f].buffer[files[f].buffer_index++];
			}
		}

		// Check if all files are fully read
		bool eof = true;
		for (int f = 0; f < files_count; ++f) {
			if (files[f].buffer_index != files[f].buffer_size) {
				eof = false;
				break;
			}
		}
		if (eof)
			break;

		// diff[i] will be true is the ith byte is different
		// for at least two files
		bool diff[16] = {false};
		for (int i = 0; i < 16; ++i) {
			for (int f = 0; f < files_count - 1; ++f) {
				if (bytes[i][f] != bytes[i][f + 1]) {
					diff[i] = true;
					break;
				}
			}
		}

		// Print the line of bytes
		for (int f = 0; f < files_count; ++f) {
			for (int i = 0; i < 16; ++i) {
				print_byte(bytes[i][f], diff[i]);

				const char *str;
				if (i == 15)
					if (f == files_count - 1)
						str = "\n";
					else
						str = "    ";
				else if (i == 7)
					str = "  ";
				else
					str = " ";
				printf(str);
			}
		}
	}

	// Close all open files
	for (int f = 0; f < files_count; ++f) {
		fclose(files[f].descriptor);
		free(files[f].buffer);
	}

	return 0;
}
