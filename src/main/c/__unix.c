// This file is used only on *nix.
// See __windows.c for Windows.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./tiny-json.h"

#define MAX_STRING_BUFFER 8192

#define DEBUG 1

char *escape_argument(const char *arg)
{
    size_t len = strlen(arg);
    size_t escaped_len = 2;

    // Count the number of characters that need escaping
    for (size_t idx = 0; idx < len; idx++)
    {
        if (arg[idx] == '\\' || arg[idx] == '\"')
            escaped_len += 2;
        else
            escaped_len += 1;
    }

    // Allocate memory for the escaped argument
    char *escaped_arg = (char *)malloc(escaped_len + 1);
    escaped_arg[escaped_len + 1] = 0;

    // Put quotes around it.
    escaped_arg[0] = '"';
    escaped_arg[escaped_len - 1] = '"';

    // Copy the argument, escaping special characters
    size_t escaped_index = 1;
    for (size_t arg_index = 0; arg_index < len; arg_index++)
    {
        if (arg[arg_index] == '\\' || arg[arg_index] == '\"')
        {
            escaped_arg[escaped_index++] = '\\';
            escaped_arg[escaped_index++] = arg[arg_index];
        }
        else
        {
            escaped_arg[escaped_index++] = arg[arg_index];
        }
    }

    return escaped_arg;
}

void concatenate_args(char *concatenated_args, int argc, char *argv[])
{
    concatenated_args[0] = '\0';

    for (int i = 1; i < argc; i++)
    {
        char *escaped_arg = escape_argument(argv[i]);
        strcat(concatenated_args, escaped_arg);
        free(escaped_arg);
        if (i < argc - 1)
        {
            strcat(concatenated_args, " ");
        }
    }
}

int main(int argc, char *argv[])
{
    char this_directory[MAX_STRING_BUFFER];
    realpath(argv[0], this_directory);

    {
        // Yucky hack, basically sets the last '/' to NULL so that string parsers think it's the end of the string.
        // Theorerically this should never be NULLPTR, but I guess the first segfault will find out.
        char *this_directory_base = strrchr(this_directory, '/');
        *this_directory_base = 0;
    }

    if (DEBUG)
        printf("[JNEM] Directory of exec: %s\n", this_directory);

    // Read the contents of "./program.json" into a buffer.
    char programjson_contents[MAX_STRING_BUFFER];
    {
        char programjson_file_path[MAX_STRING_BUFFER];
        programjson_file_path[0] = 0; // Shhhh.
        strcat(programjson_file_path, this_directory);
        strcat(programjson_file_path, "/program.json");

        FILE *fp = fopen(programjson_file_path, "r");
        if (fp != NULL)
        {
            size_t newLen = fread(programjson_contents, sizeof(char), MAX_STRING_BUFFER, fp);
            if (ferror(fp) != 0)
            {
                printf("[JNEM] Could not open program.json\n");
                fclose(fp);
                return 1;
            }
            else
            {
                programjson_contents[newLen++] = '\0'; /* Just to be safe. */
            }

            fclose(fp);
        }
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
    java_command_args[0] = 0;

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
    full_command[0] = 0;

    strcat(full_command, this_directory);
    strcat(full_command, "/runtime");
    strcat(full_command, "/bin");
    strcat(full_command, "/java");
    strcat(full_command, " ");
    strcat(full_command, java_command_args);

    char raw_program_args[MAX_STRING_BUFFER];
    raw_program_args[0] = 0;
    concatenate_args(raw_program_args, argc, argv);

    if (DEBUG)
        printf("[JNEM] Raw args: %s\n", raw_program_args);

    strcat(full_command, " ");
    strcat(full_command, raw_program_args);

    if (DEBUG)
        printf("[JNEM] Full command: %s\n", full_command);

    int exit_code = system(full_command);
    return exit_code;
}