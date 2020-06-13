#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int config_argc;
    int exec_argc;
    char **config_argv;
    char **exec_argv;
    int c;

    config_argc = 1;
    config_argv = malloc( config_argc * sizeof(void*) );
    config_argv[0] = "raku";

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
    fprintf(stderr, "ERROR: Failed to execute %s. Error code: %i\n", exec_argv[0], errno);
    return EXIT_FAILURE;
}
