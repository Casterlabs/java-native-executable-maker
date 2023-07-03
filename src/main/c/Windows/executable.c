#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include "./tiny-json.h"

#define sizeofstr(s) (strlen(s) * sizeof(char))
#define MAX_STRING_BUFFER 2048

#define DEBUG 1

int RunCommand(char *to_execute)
{
    // Start up info.
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    // si.dwFlags |= STARTF_USESTDHANDLES;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the window

    // The JOB.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
    ZeroMemory(&ji, sizeof(ji));
    ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    HANDLE job = CreateJobObject(NULL, NULL);
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji, sizeof(ji));

    // Process pointer
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process.
    if (!CreateProcess(
            NULL,
            to_execute,
            NULL,
            NULL,
            FALSE,
            CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
            NULL,
            NULL,
            &si,
            &pi))
    {
        printf("[JNEM] CreateProcess failed (%d).\n", GetLastError());
        return -1;
    }

    AssignProcessToJobObject(job, pi.hProcess); // Needs CREATE_BREAKAWAY_FROM_JOB
    ResumeThread(pi.hThread);

    // Wait until child process exits and cleanup.
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(job);

    // Get the exit code.
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    if (DEBUG)
        printf("[JNEM] Exit code: %ld.\n", exit_code);

    return exit_code;
}

int main(int argc, char **argv)
{
    char this_directory[MAX_STRING_BUFFER];
    // char *this_directory = calloc(MAX_STRING_BUFFER, sizeof(char));
    GetModuleFileName(NULL, this_directory, MAX_STRING_BUFFER);
    PathRemoveFileSpec(this_directory);

    if (DEBUG)
        printf("[JNEM] Directory of .exe: %s\n", this_directory);

    char programjson_contents[MAX_STRING_BUFFER];

    // Read the contents of "./program.json" into the above buffer.
    {
        char programjson_file_path[MAX_STRING_BUFFER];
        PathCombine(programjson_file_path, this_directory, "program.json");

        HANDLE programjson_fhandle = CreateFile(
            programjson_file_path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (programjson_fhandle == INVALID_HANDLE_VALUE)
        {
            printf("[JNEM] Could not open program.json (%d)\n", GetLastError());
            return 1;
        }

        DWORD programjson_fsize = GetFileSize(programjson_fhandle, NULL);
        if (programjson_fsize == INVALID_FILE_SIZE)
        {
            printf("[JNEM] Could not open program.json (%d)\n", GetLastError());
            CloseHandle(programjson_fhandle);
            return 1;
        }

        DWORD bytesRead;
        BOOL result = ReadFile(
            programjson_fhandle,
            programjson_contents,
            MAX_STRING_BUFFER - 1, // Leave room for null-terminator
            &bytesRead,
            NULL);
        if (!result)
        {
            // Handle file read error
            printf("[JNEM] Could not open program.json (%d)\n", GetLastError());
            CloseHandle(programjson_fhandle);
            return 1;
        }

        CloseHandle(programjson_fhandle);

        programjson_contents[bytesRead] = '\0'; // Final null terminator.
    }

    if (DEBUG)
        printf("[JNEM] program.json contents: %s\n", programjson_contents);

    json_t mem[32];
    json_t const *program_json = json_create(programjson_contents, mem, sizeof mem / sizeof *mem);
    if (!program_json)
    {
        printf("[JNEM] Could not parse json.\n");
        return 1;
    }

    char java_command_args[MAX_STRING_BUFFER];
    ZeroMemory(&java_command_args, sizeof(java_command_args));

    // Parse the classpath array.
    {
        json_t const *classpath = json_getProperty(program_json, "classPath");
        if (!classpath || JSON_ARRAY != json_getType(classpath))
        {
            printf("[JNEM] Could not parse json ('classPath' missing or is not array).\n");
            return 1;
        }

        strcat(java_command_args, "-classpath \"");

        json_t const *file;
        for (file = json_getChild(classpath); file != 0; file = json_getSibling(file))
        {
            if (JSON_TEXT == json_getType(file))
            {
                strcat(java_command_args, json_getValue(file));
                strcat(java_command_args, ";"); // Windows specific!
            }
            else
            {
                printf("[JNEM] Could not parse json ('classPath' has a non-text property).\n");
            }
        }

        strcat(java_command_args, "\"");
    }

    // Parse the system properties array.
    {
        json_t const *system_properties = json_getProperty(program_json, "systemProperties");
        if (!system_properties || JSON_ARRAY != json_getType(system_properties))
        {
            printf("[JNEM] Could not parse json ('systemProperties' missing or is not array).\n");
            return 1;
        }

        json_t const *property;
        for (property = json_getChild(system_properties); property != 0; property = json_getSibling(property))
        {
            if (JSON_TEXT == json_getType(property))
            {
                strcat(java_command_args, " ");
                strcat(java_command_args, json_getValue(property));
            }
            else
            {
                printf("[JNEM] Could not parse json ('systemProperties' has a non-text property).\n");
            }
        }
    }

    // Inject our own system properties.
    strcat(java_command_args, " -Djnem.basedir=\"");
    strcat(java_command_args, this_directory);
    strcat(java_command_args, "\"");

    // Add the main class.
    {

        json_t const *main_class = json_getProperty(program_json, "mainClass");
        if (!main_class || JSON_TEXT != json_getType(main_class))
        {
            printf("[JNEM] Could not parse json ('mainClass' missing or is not text).\n");
            return 1;
        }

        strcat(java_command_args, " ");
        strcat(java_command_args, json_getValue(main_class));
    }

    char full_command[MAX_STRING_BUFFER];
    PathCombine(full_command, this_directory, "runtime");
    PathCombine(full_command, full_command, "bin");
    PathCombine(full_command, full_command, "java.exe");
    strcat(full_command, " ");
    strcat(full_command, java_command_args);

    // Append the command line arguments (if present)
    char *raw_programjson_args = GetCommandLine();
    char *first_space = strchr(raw_programjson_args, ' ');

    if (first_space != NULL)
    {
        raw_programjson_args = first_space + 1;

        strcat(full_command, raw_programjson_args);

        if (DEBUG)
            printf("[JNEM] Raw args: %s\n", raw_programjson_args);
    }

    if (DEBUG)
        printf("[JNEM] Full command: %s\n", full_command);

    int exit_code = RunCommand(full_command);
    return exit_code;
}
