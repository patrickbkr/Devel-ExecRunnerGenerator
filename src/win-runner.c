#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <WinDef.h>

// Loosely according to https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
int argvQuote(wchar_t *in, wchar_t *out) {
    int ipos;
    int opos;
    int bs_count;
    int c;

    ipos = 0;
    opos = 0;

    if (!wcschr(in, L' ') && !wcschr(in, L'\\"') && !wcschr(in, L'\\t') && !wcschr(in, L'\\n') && !wcschr(in, L'\\v')) {
        if (out) wcscpy(out, in);
        return wcslen(in) + 1;
    }

    if (out) out[opos] = L'\\"';
    opos++;

    while (in[ipos] != 0) {
        bs_count = 0;
        while (in[ipos] != 0 && in[ipos] == L'\\\\') {
            ipos++;
            bs_count++;
        }

        if (in[ipos] == 0) {
            for (c = 0; c < (bs_count * 2); c++) {
                if (out) out[opos] = L'\\\\';
                opos++;
            }
            break;
        }
        else if (in[ipos] == L'\\"') {
            for (c = 0; c < (bs_count * 2 + 1); c++) {
                if (out) out[opos] = L'\\\\';
                opos++;
            }
            if (out) out[opos] = in[ipos];
            opos++;
        }
        else {
            for (c = 0; c < bs_count; c++) {
                if (out) out[opos] = L'\\\\';
                opos++;
            }
            if (out) out[opos] = in[ipos];
            opos++;
        }

        ipos++;
    }

    if (out) out[opos] = L'\\"';
    opos++;
    if (out) out[opos] = 0;
    opos++;

    return opos;
}


int wmain(int argc, wchar_t *argv[]) {
    int config_argc;
    int exec_argc;
    int c;
    wchar_t **config_argv;
    wchar_t **exec_argv;
    size_t arg_size;
    wchar_t *program;
    size_t cmd_line_size;
    wchar_t *cmd_line;
    DWORD exit_code;
    BOOL success;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    config_argc = 1;
    config_argv = malloc( config_argc * sizeof(void*) );
    config_argv[0] = L"raku";

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

    // Encode program name
    arg_size = argvQuote(exec_argv[0], NULL);
    program = malloc(arg_size * sizeof(wchar_t));
    argvQuote(exec_argv[0], program);

    // Size of the final command line string.
    // cmd_line_size = size of each argument + one space each - the last space + the trailing \0
    cmd_line_size = 0;
    cmd_line = NULL;

    for (c = 0; c < exec_argc - 1; c++) {
        // argvQuote leaves space for a trailing \0
        arg_size = argvQuote(exec_argv[c], NULL);
        cmd_line = realloc(cmd_line, (cmd_line_size + arg_size) * sizeof(wchar_t));
        argvQuote(exec_argv[c], cmd_line + cmd_line_size);
        cmd_line_size += arg_size;
        cmd_line[cmd_line_size - 1] = L' ';
    }
    cmd_line[cmd_line_size - 1] = 0;

    // Execute the command and wait for it to finish.
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    ZeroMemory(&pi, sizeof(pi));

    success = CreateProcessW(
        program,  // ApplicationName
        cmd_line, // CommandLine
        NULL,     // ProcessAttributes
        NULL,     // ThreadAttributes
        FALSE,    // InheritHandles
        0,        // CreationFlags
        NULL,     // Environment
        NULL,     // CurrentDirectory
        &si,      // StartupInfo
        &pi);     // ProcessInformation

    if (!success) {
        fprintf(stderr, "ERROR: Failed to execute %s. Error code: %ld\n", program, GetLastError());
        return EXIT_FAILURE;
    }

    // I guess processes only signal when they are done. So no need to check the return value.
    WaitForSingleObject(pi.hProcess, INFINITE);
    success = GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!success) {
        fprintf(stderr, "Couldn't retrieve exit code. Error code: %ld\n", GetLastError());
        return EXIT_FAILURE;
    }

    return exit_code;
}
