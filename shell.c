// shell.c
// Jacob Gray
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define DELIM " \t\r\n" // Delimiters for tokenizing the input
#define MAX_ENV_VARS 100

char *env_vars[MAX_ENV_VARS][2];
int env_var_count = 0; // Tracks the current number of environment variables

// Function to get the value of an environment variable, used in replace_variables
char *get_env_value(const char *key)
{
    for (int i = 0; i < env_var_count; ++i)
    {
        if (strcmp(env_vars[i][0], key) == 0)
        {
            return env_vars[i][1];
        }
    }
    return NULL; // No value found
}

// Function to replace variables in command, used in parse_command
void replace_variables(char **args)
{
    // Iterate through all arguments, replacing variables as needed
    for (int i = 0; args[i] != NULL; ++i)
    {
        char *dollar_pos = strchr(args[i], '$');

        if (dollar_pos != NULL)
        {
            char temp[MAX_INPUT_SIZE];

            while (dollar_pos != NULL)
            {
                *dollar_pos = '\0';                         // stop the string at the dollar sign
                char *end = strpbrk(dollar_pos + 1, DELIM); // Find the end of the variable name
                char key[128];

                if (end)
                {
                    strncpy(key, dollar_pos + 1, end - dollar_pos - 1);
                    key[end - dollar_pos - 1] = '\0';
                }
                else
                {
                    strcpy(key, dollar_pos + 1);
                }
                char *value = get_env_value(key);
                if (value)
                {
                    sprintf(temp, "%s%s%s", args[i], value, end ? end : "");
                    strcpy(args[i], temp);
                }
                dollar_pos = strchr(args[i], '$');
            }
        }
    }
}

// Function for non-built-in commands
void execute_command(char **args, int background, int input_fd, int output_fd)
{
    pid_t pid = fork();
    if (pid == 0) // Child process
    { 
        if (input_fd != STDIN_FILENO)
        {
            dup2(input_fd, STDIN_FILENO); // Redirect input
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO)
        {
            dup2(output_fd, STDOUT_FILENO); // Redirect output
            close(output_fd);
        }
        execvp(args[0], args); // Replace the child process with the command
        perror("Command not found");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Fork failed");
    }
    else // Parent process
    { 
        if (!background)
        {
            waitpid(pid, NULL, 0);
        }
    }
}

// Command parsing function
void parse_command(char *input)
{
    char *args[MAX_ARGS];               // All args in the command
    char *token = strtok(input, DELIM); // Tokenizes, basically makes substrings
    int argc = 0;
    while (token != NULL && argc < MAX_ARGS - 1)
    {
        args[argc++] = token;
        token = strtok(NULL, DELIM);
    }
    args[argc] = NULL; // Null-terminate the argument array
    if (argc == 0)
        return;
    replace_variables(args);

    // Command List
    if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0)
    {
        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (argc < 2)
        {
            fprintf(stderr, "cd: missing argument\n");
        }
        else
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
            }
        }
    }
    else if (strcmp(args[0], "pwd") == 0)
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
        }
        else
        {
            perror("pwd");
        }
    }
    else if (strcmp(args[0], "set") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "set: missing argument\n");
        }
        else
        {
            // strdup allocates memory and copies the string
            env_vars[env_var_count][0] = strdup(args[1]); // Store the variable name
            env_vars[env_var_count][1] = strdup(args[2]); // Store the variable value
            env_var_count++;
        }
    }
    else if (strcmp(args[0], "unset") == 0)
    {
        if (argc < 2)
        {
            fprintf(stderr, "unset: missing argument\n");
        }
        else
        {
            for (int i = 0; i < env_var_count; ++i)
            {
                if (strcmp(env_vars[i][0], args[1]) == 0)
                {
                    free(env_vars[i][0]);
                    free(env_vars[i][1]);
                    env_vars[i][0] = env_vars[env_var_count - 1][0];
                    env_vars[i][1] = env_vars[env_var_count - 1][1];
                    env_var_count--;
                    break;
                }
            }
        }
    }
    else // External commands and redirection
    {
        int background = 0; // Background flag
        int input_fd = STDIN_FILENO;
        int output_fd = STDOUT_FILENO;

        for (int i = 0; args[i] != NULL; ++i)
        {
            if (strcmp(args[i], "&") == 0)
            {
                background = 1;
                args[i] = NULL; // Remove '&' from the arguments
            }
            else if (strcmp(args[i], "<") == 0)
            {
                input_fd = open(args[i + 1], O_RDONLY); // read
                args[i] = NULL;                         // Remove '<' from the arguments
                args[i + 1] = NULL;                     // Remove the file name from the arguments
            }
            else if (strcmp(args[i], ">") == 0)
            {
                output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // write
                args[i] = NULL;                                                    // Remove '>' from the arguments
                args[i + 1] = NULL;                                                // Remove the file name from the arguments
            }
        }
        execute_command(args, background, input_fd, output_fd);
    }
}

int main()
{
    char input[MAX_INPUT_SIZE];

    // Main shell loop
    while (1)
    {
        printf("xsh# ");
        fflush(stdout);
        // Read input, store it in 'input' buffer
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
        {
            break; // Exit if there is no input
        }
        input[strcspn(input, "\n")] = '\0'; // Removes the newline character from the input
        parse_command(input);
    }
    return 0;
}
