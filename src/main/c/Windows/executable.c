#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#define sizeofstr(s) (strlen(s) * sizeof(char))
#define MAX_STRING_BUFFER 2048

#define DEBUG 0

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

    char command_contents[MAX_STRING_BUFFER];

    // Read the contents of "./command.txt" into the above buffer.
    {
        char command_file_path[MAX_STRING_BUFFER];
        PathCombine(command_file_path, this_directory, "command.txt");

        HANDLE command_fhandle = CreateFile(
            command_file_path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (command_fhandle == INVALID_HANDLE_VALUE)
        {
            printf("[JNEM] Could not open command.txt (%d)\n", GetLastError());
            return 1;
        }

        DWORD command_fsize = GetFileSize(command_fhandle, NULL);
        if (command_fsize == INVALID_FILE_SIZE)
        {
            printf("[JNEM] Could not open command.txt (%d)\n", GetLastError());
            CloseHandle(command_fhandle);
            return 1;
        }

        DWORD bytesRead;
        BOOL result = ReadFile(
            command_fhandle,
            command_contents,
            MAX_STRING_BUFFER - 1, // Leave room for null-terminator
            &bytesRead,
            NULL);
        if (!result)
        {
            // Handle file read error
            printf("[JNEM] Could not open command.txt (%d)\n", GetLastError());
            CloseHandle(command_fhandle);
            return 1;
        }

        CloseHandle(command_fhandle);

        command_contents[bytesRead] = '\0'; // Final null terminator.
    }

    if (DEBUG)
        printf("[JNEM] Command content: %s\n", command_contents);

    char full_command[MAX_STRING_BUFFER];
    PathCombine(full_command, this_directory, "runtime");
    PathCombine(full_command, full_command, "bin");
    PathCombine(full_command, full_command, "java.exe");
    strcat(full_command, " ");
    strcat(full_command, command_contents);

    // Append the command line arguments (if present)
    char *raw_command_args = GetCommandLine();
    char *first_space = strchr(raw_command_args, ' ');

    if (first_space != NULL)
    {
        raw_command_args = first_space + 1;

        strcat(full_command, raw_command_args);

        if (DEBUG)
            printf("[JNEM] Raw args: %s\n", raw_command_args);
    }

    if (DEBUG)
        printf("[JNEM] Full command: %s\n", full_command);

    int exit_code = RunCommand(full_command);
    return exit_code;
}
