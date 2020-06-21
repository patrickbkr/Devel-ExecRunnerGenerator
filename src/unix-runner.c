#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "whereami.h"
#include "gen/exec_size.h"

const int marker_size = 33;
const char* marker = "EXEC_RUNNER_WRAPPER_CONFIG_MARKER";

long find_config(FILE *file_handle) {
    if (EXEC_LEN == 0)
        fprintf(stderr, "EXEC_RUNNER_WRAPPER: This executable needs a sane size set to work.\n");
    return EXEC_LEN;
}

long read_arg(FILE *file_handle, char *arg, long arg_size) {
    // Escape symbol is the backslash. We assume UTF-8.
    long work_pos;
    long size;
    char seen_quote;
    char seen_escape;
    size_t buf_size;
    char *buf;
    int i;

    work_pos = ftell(file_handle);
    seen_quote = 0;
    seen_escape = 0;
    size = 0;
    buf_size = 50;
    buf = malloc( buf_size * sizeof(char) );

    while (1) {
        size_t read_size = fread(buf, sizeof(char), buf_size, file_handle);
        i = 0;

        // Skip initial quote.
        if (!seen_quote) {
            if (buf[0] == '"') {
                seen_quote = 1;
                i++;
                work_pos++;
            }
            else {
                fprintf(stderr, "EXEC_RUNNER_WRAPPER: Invalid config found. No starting \" in config argument. Aborting.\n");
                free(buf);
                return -1;
            }
        }

        for (; i < read_size; i++) {

            work_pos++;

            if (buf[i] & 128 || seen_escape) {
                // 1xxxxxxx => UTF-8 high char stuff.
                seen_escape = 0;
                if (size < arg_size)
                    arg[size] = buf[i];
                size++;
            }
            else if (buf[i] == '\\') {
                seen_escape = 1;
            }
            else if (buf[i] == '"') {
                free(buf);

                if (size < arg_size)
                    arg[size] = '\0';
                size++;

                fseek(file_handle, work_pos, SEEK_SET);

                return size;
            }
            else {
                if (size < arg_size)
                    arg[size] = buf[i];
                size++;
            }
        }
    }
}

/*
 * Returns a malloced array of argument strings.
 * The number of arguments will be written to `count`.
 * On error a NULL pointer is returned.
 */
char **read_config_args(FILE *file_handle, int *count) {
    int arg_count;
    int malloc_count;
    size_t read_size;
    char read_buf[marker_size + 1];
    char **args;

    malloc_count = 10;
    args = (char**) malloc(malloc_count * sizeof(char*));
    arg_count = 0;

    read_size = fread(read_buf, sizeof(char), marker_size + 1, file_handle);
    if (read_size != marker_size + 1) {
        fprintf(stderr, "EXEC_RUNNER_WRAPPER: Config not found. Aborting.\n");
        return NULL;
    }
    if (memcmp(marker, read_buf, marker_size) != 0) {
        fprintf(stderr, "EXEC_RUNNER_WRAPPER: Marker not found. Aborting.\n");
        return NULL;
    }
    if (read_buf[marker_size] != '[') {
        fprintf(stderr, "EXEC_RUNNER_WRAPPER: Invalid config. Doesn't start with '['. Aborting.\n");
        return NULL;
    }

    while (1) {
        long arg_size;
        long offset = ftell(file_handle);

        arg_size = read_arg(file_handle, NULL, 0);
        if (arg_size < 1)
            return NULL;

        if (arg_count >= malloc_count) {
            malloc_count *= 2;
            args = realloc(args, malloc_count * sizeof(char*));
        }

        args[arg_count] = malloc(arg_size * sizeof(char*));
        fseek(file_handle, offset, SEEK_SET);
        arg_size = read_arg(file_handle, args[arg_count], arg_size);

        arg_count++;

        read_size = fread(read_buf, sizeof(char), 1, file_handle);

        if (read_size != 1) {
            fprintf(stderr, "EXEC_RUNNER_WRAPPER: Invalid config. Separator or end marker missing. Aborting.\n");
            return NULL;
        }

        if (read_buf[0] == ',') {
            // All good, expecting another argument so just continue.
        }
        else if (read_buf[0] == ']') {
            break;
        }
        else {
            fprintf(stderr, "EXEC_RUNNER_WRAPPER: Invalid config. Unknown separator found. Aborting.\n");
            return NULL;
        }
    }

    *count = arg_count;
    return args;
}


int main(int argc, char *argv[])
{
    int config_argc;
    int exec_argc;
    int my_path_len;
    int my_dir_name_len;
    char **config_argv;
    char **exec_argv;
    char *my_path;
    int c;
    FILE *exec_handle;
    long config_offset;

    // Retrieve own path
    my_path_len = wai_getExecutablePath(NULL, 0, NULL);
    my_path = malloc( my_path_len * sizeof(char*) );
    wai_getExecutablePath(my_path, my_path_len, &my_dir_name_len);

    // Read config.
    exec_handle = fopen(my_path, "r");
    config_offset = find_config(exec_handle);
    if (config_offset == 0) {
        fclose(exec_handle);
        return -12;
    }

    fseek(exec_handle, config_offset, SEEK_SET);
    config_argv = read_config_args(exec_handle, &config_argc);
    if (config_argv == NULL) {
        fclose(exec_handle);
        return -12;
    }

    fclose(exec_handle);

    // Trim to directory name
    my_path[my_dir_name_len] = '\0';

    // config args + program args (without program name) + NULL
    exec_argc = config_argc + (argc - 1) + 1;
    exec_argv = malloc( exec_argc * sizeof(void*) );

    // Copy config args.
    for (c = 0; c < config_argc; c++) {
        exec_argv[c] = config_argv[c];
    }

    // Copy passed args (withoug program name).
    for (c = 0; c < argc - 1; c++) {
        exec_argv[config_argc + c] = argv[c + 1];
    }

    exec_argv[exec_argc - 1] = NULL;

    execvp(exec_argv[0], exec_argv);

    // execvp doesn't return on successful exec.
    fprintf(stderr, "EXEC_RUNNER_WRAPPER: Failed to execute %s. Error code: %i\n", exec_argv[0], errno);
    return EXIT_FAILURE;
}

