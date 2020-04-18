#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

typedef uint8_t bool;
#define true 1
#define false 0

// ANSI color escape codes
enum
{
	COL_RESET     =  0,
	COL_UNDERLINE =  4,
	COL_FG_RED    = 31,
	COL_FG_GREEN  = 32,
	COL_FG_YELLOW = 33,
	COL_FG_BLUE   = 34,
	COL_FG_PURPLE = 35,
	COL_FG_TEAL   = 36,
	COL_BG_RED    = 41,
	COL_BG_GREEN  = 42,
	COL_BG_YELLOW = 43,
	COL_BG_BLUE   = 44,
	COL_BG_PURPLE = 45,
	COL_BG_TEAL   = 46,
};

static int colors_rainbow_fg[] = {
	COL_FG_RED,
	COL_FG_GREEN,
	COL_FG_YELLOW,
	COL_FG_BLUE,
	COL_FG_PURPLE,
	COL_FG_TEAL,
};

static int colors_rainbow_bg[] = {
	COL_BG_RED,
	COL_BG_GREEN,
	COL_BG_YELLOW,
	COL_BG_BLUE,
	COL_BG_PURPLE,
	COL_BG_TEAL,
};

static void print_usage(const char *arg0, FILE *file)
{
	const char *help =
		"Usage: %s [OPTIONS] file1 file2 ... fileN\n"
		"\n"
		"  --help             Show this message\n"
		"\n"
		"  --no-color         Disables the highlighting of the changed bytes\n"
		"                     The changed bytes will be \e[4munderlined\e[0m instead\n"
		"\n"
		"  --color=COLOR      Highlight the changed bytes with the color COLOR.\n"
		"                     The Possible values are:\n"
		"                       rainbow [DEFAULT] - alternating colors,\n"
		"                       red, green, yellow, blue, purple or teal\n"
		"\n"
		"  --colorfg          Color the text instead of the background\n"
		"\n"
		"  --                 Everything after this argument will be treated as a file\n";
	fprintf(file, help, arg0);
}

static void print_byte(int byte)
{
	char c[2];
	if (byte < 0 || byte > 0xFF) {
		c[0] = c[1] = '-';
	} else {
		static const char hex[17] = "0123456789ABCDEF";
		c[0] = hex[byte >> 4];
		c[1] = hex[byte & 0xF];
	}
	printf("%c%c", c[0], c[1]);
}

struct file_t
{
	const char *name; // The name of the file
	uint64_t pos;     // Current position in file
	uint64_t size;    // The size of the file
	FILE *descriptor; // The file descriptor
	int buffer_size;
	int buffer_index;
	uint8_t *buffer;
};

static const int READ_BUFFER_SIZE = 4096;

// How to 'highlight' the changed bytes
// -1 - Use underlines
//  0 - Rainbow
//  1, 2, 3, 4, 5, 6 - red, green, yellow, blue, purple, teal
static int opt_highlight_col = 0;

// Select whenether the foreground or background
// color will be used to highlight the changed bytes
static bool opt_highlight_fg = false;

static uint64_t largest_file_size = 0;

#define MAX_FILES 32
static struct file_t files[MAX_FILES];
static int files_count = 0;

void parse_arguments(int argc, char **argv)
{
	bool as_files = false;

	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];

		if (!strncmp(a, "--", 2) && !as_files) {
			if (!strcmp(a, "--help")) {
				print_usage(argv[0], stdout);
				exit(0);
			} else if (!strcmp(a, "--no-color")) {
				opt_highlight_col = -1;
			} else if (!strncmp(a, "--color=", 8)) {
				static const char *args[] = {
					"rainbow", "red", "green", "yellow", "blue", "purple", "teal",
				};
				int val = -1;
				for (unsigned int i = 0; i < sizeof(args) / sizeof(const char *); ++i) {
					if (!strcmp(a + 8, args[i])) {
						val = i;
						break;
					}
				}
				if (val == -1) {
					fprintf(stderr, "Invalid argument '%s' to --color\n", a + 8);
					print_usage(argv[0], stderr);
					exit(1);
				}
				opt_highlight_col = val;
			} else if (!strcmp(a, "--colorfg")) {
				opt_highlight_fg = true;
			} else if (!strcmp(a, "--")) {
				as_files = true;
			} else {
				fprintf(stderr, "Invalid option: %s\n", a);
				print_usage(argv[0], stderr);
				exit(1);
			}
		} else {
			if (files_count == MAX_FILES) {
				fprintf(stderr, "Cannot diff more than %d files\n", MAX_FILES);
				exit(1);
			}
			files[files_count++].name = a;
		}
	}
}

static void print_multiple(int n, char c)
{
	if (n <= 0)
		return;
	char s[n + 1];
	for (int i = 0; i < n; ++i)
		s[i] = c;
	s[n] = '\0';
	printf(s);
}

static void print_file_names(int left_margin)
{
	print_multiple(left_margin, ' ');

	for (int f = 0; f < files_count; ++f) {
		const char *nm = files[f].name;
		int l = strlen(nm);
		const int space = 3*16;
		int padding_left = (space - l) / 2;
		int padding_right = space - padding_left - l;
		print_multiple(padding_left, '-');
		if (l <= space)
			printf("%s", nm);
		else
			printf("...%s", nm + l - space + 3);
		print_multiple(padding_right, '-');
		printf("    ");
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	/* Parse command line arguments */
	parse_arguments(argc, argv);
	if (files_count == 0) {
		print_usage(argv[0], stderr);
		return 1;
	}

	/* Open the input files */
	for (int f = 0; f < files_count; ++f) {
		struct file_t file = files[f];
		file.descriptor = fopen(file.name, "rb");
		if (file.descriptor == NULL) {
			fprintf(stderr, "Failed to open file '%s': %s\n", file.name, strerror(errno));
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

		if (largest_file_size < file.size)
			largest_file_size = file.size;
	}

	int address_digits = 0;
	uint64_t s = largest_file_size;
	do {
		s /= 16;
		++address_digits;
	} while (s > 1);

	uint64_t differentBytesCount = 0;

	/* Print the file names */
	print_file_names(address_digits + 2);

	/* Read 16 bytes at a time from file buffer */
	for (uint64_t index;; index += 16) {
		// Check if all files are fully read
		bool eof = true;
		for (int f = 0; f < files_count; ++f) {
			if (files[f].buffer_index != files[f].buffer_size ||
					files[f].pos != files[f].size) {
				eof = false;
				break;
			}
		}
		if (eof)
			break;

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

		// Compare each of the 16 bytes between all files
		// diff[i] will be true is the ith byte is different
		// for at least two files
		bool diff[16] = {false};
		for (int i = 0; i < 16; ++i) {
			for (int f = 0; f < files_count - 1; ++f) {
				if (bytes[i][f] != bytes[i][f + 1]) {
					++differentBytesCount;
					diff[i] = true;
					break;
				}
			}
		}

		// Print the line address
		printf("%0*lX: ", address_digits, index);

		// Print the line of bytes
		printf("\e[%dm", COL_RESET);
		for (int f = 0; f < files_count; ++f) {
			int color_index = 0;
			for (int i = 0; i < 16; ++i) {
				if (diff[i]) {
					int code;
					if (opt_highlight_col == -1) {
						code = COL_UNDERLINE;
					} else if (opt_highlight_col == 0) {
						if (opt_highlight_fg) {
							code = colors_rainbow_fg[color_index++];
							color_index %= sizeof(colors_rainbow_fg) / sizeof(int);
						} else {
							code = colors_rainbow_bg[color_index++];
							color_index %= sizeof(colors_rainbow_bg) / sizeof(int);
						}
					} else {
						if (opt_highlight_fg)
							code = colors_rainbow_fg[opt_highlight_col - 1];
						else
							code = colors_rainbow_bg[opt_highlight_col - 1];
					}
					printf("\e[%dm", code);
				}
				print_byte(bytes[i][f]);
				if (diff[i])
					printf("\e[%dm", COL_RESET);

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

	// Print summary
	printf("\nDifferent bytes: %ld\n", differentBytesCount);

	// Close all open files
	for (int f = 0; f < files_count; ++f) {
		fclose(files[f].descriptor);
		free(files[f].buffer);
	}

	return 0;
}
