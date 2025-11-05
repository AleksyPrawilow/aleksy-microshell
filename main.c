#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define COMMAND_BUFFER_SIZE 1024
#define TOKEN_DELIMITERS  " \t\r\n\a"
#define MAX_TOKENS          64

struct tokens {
    char ** items;
    int     size;
};

enum EXIT_CODES {
    EXIT_SUCCESS_CODE          = 0,
    EXIT_SIGINT_CODE           = 1,
    EXIT_ALLOCATION_ERROR_CODE = 2,
    EXIT_MISC_FAILURE_CODE     = 3
};

void          handle_exit   (int error_code);
void          signal_handler(int signum);
void          shell_loop();
void          print_user_directory_prefix();
char *        read_input();
struct tokens parse_input(const char *input);
int           execute_command(struct tokens args);
int           shell_help     (struct tokens args);
int           shell_exit     (struct tokens args);
int           shell_cd       (struct tokens args);
int           shell_echo     (struct tokens args);

char * available_commands[]                  = {"cd", "exit", "help", "echo"};
int ( * command_functions[]) (struct tokens) = {&shell_cd, &shell_exit, &shell_help, &shell_echo};

int main() {
    signal(SIGINT, signal_handler);
    shell_loop();
    return EXIT_SUCCESS;
}

void handle_exit(int error_code) {
    switch (error_code)
    {
    case EXIT_SUCCESS_CODE:
        printf("\nmicroshell: Exiting microshell. Goodbye!\n");
        exit(EXIT_SUCCESS);
        break;
    case EXIT_SIGINT_CODE:
        printf("\nmicroshell: Exiting microshell due to SIGINT (Ctrl+C).\n");
        exit(EXIT_SUCCESS);
        break;
    case EXIT_ALLOCATION_ERROR_CODE:
        fprintf(stderr, "\nmicroshell: Exiting microshell due to allocation error.\n");
        exit(EXIT_FAILURE);
        break;
    case EXIT_MISC_FAILURE_CODE:
        fprintf(stderr, "\nmicroshell: Exiting microshell due to an error.\n");
        exit(EXIT_FAILURE);
        break;
    default:
        printf("\nmicroshell: Exiting microshell...\n");
        exit(EXIT_SUCCESS);
        break;
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        handle_exit(EXIT_SIGINT_CODE);
    }
}

void shell_loop() {
    char * input;
    struct tokens args;
    int run_status = 0;
    do {
        print_user_directory_prefix();
        input = read_input();
        args = parse_input(input);
        run_status = execute_command(args);
        free(input);
        free(args.items);
    } while (run_status == 0);
}

void print_user_directory_prefix() {
    char * username = getenv("USER");
    char directory_path[1024];
    if (getcwd(directory_path, sizeof(directory_path)) == NULL) {
        perror("microshell: getcwd() error");
        directory_path[0] = '\0';
    }
    printf("%s@%s > ", username ? username : "Unknown user", directory_path);
}

char * read_input() {
    size_t buffer_size = COMMAND_BUFFER_SIZE;
    char * buffer = malloc(COMMAND_BUFFER_SIZE * sizeof(char));
    if (!buffer) {
        fprintf(stderr, "microshell: Allocation error\n");
        handle_exit(EXIT_ALLOCATION_ERROR_CODE);
    }
    int num_characters = getline(&buffer, &buffer_size, stdin);
    if (num_characters == -1) {
        free(buffer);
        printf("microshell: An error has occured. Exiting\n");
        handle_exit(EXIT_MISC_FAILURE_CODE);
    }
    return buffer;
}

struct tokens parse_input(const char * input) {
    struct tokens result;
    int token_position = 0;
    char ** tokens = malloc(MAX_TOKENS * sizeof(char * ));
    char * token;
    if (!tokens) {
        fprintf(stderr, "microshell: Allocation error\n");
        handle_exit(EXIT_ALLOCATION_ERROR_CODE);
    }
    token = strtok((char *)input, TOKEN_DELIMITERS);
    while (token != NULL) {
        tokens[token_position] = token;
        token_position++;
        if (token_position >= MAX_TOKENS) {
            fprintf(stderr, "microshell: Too many tokens\n");
            handle_exit(EXIT_MISC_FAILURE_CODE);
        }
        token = strtok(NULL, TOKEN_DELIMITERS);
    }
    tokens[token_position] = NULL;
    result.items = tokens;
    result.size = token_position;
    return result;
}

int execute_command(struct tokens args) {
    if (args.size == 0) {
        return 0;
    }
    for (int i = 0; i < sizeof(available_commands) / sizeof(available_commands[0]); i++) {
        if (strcmp(args.items[0], available_commands[i]) == 0) {
            return ((int (*)(struct tokens))command_functions[i])(args);
        }
    }
    printf("microshell: Command not found: %s\n", args.items[0]);
    return 0;
}

int shell_help(struct tokens args) {
    printf("Microshell Help:\n");
    printf("Available commands:\n");
    for (int i = 0; i < sizeof(available_commands) / sizeof(available_commands[0]); i++) {
        printf(" - %s\n", available_commands[i]);
    }
    return 0;
}

int shell_exit(struct tokens args) {
    handle_exit(EXIT_SUCCESS_CODE);
    return 0;
}

int shell_cd(struct tokens args) {
    if (args.size != 2) {
        fprintf(stderr, "microshell: Expected only 1 argument to \"cd\"\n");
        return 0;
    }
    if (chdir(args.items[1]) != 0) {
        perror("microshell: Failed to change directory");
    }
    return 0;
}

int shell_echo(struct tokens args) {
    for (int i = 1; i < args.size; i++) {
        if (args.items[i][0] == '$') {
            const char * pos = strchr(args.items[i] + 1, '\\');
            if (pos == NULL) {
                const char * env_var = getenv(args.items[i] + 1);
                if (env_var) {
                    printf("%s", env_var);
                } else {
                    printf("{environment variable \"%s\" not found} ", args.items[i] + 1);
                }
            } else {
                char real_env_var[256];
                size_t len = pos - (args.items[i] + 1);
                strncpy(real_env_var, args.items[i] + 1, len);
                real_env_var[len] = '\0';
                const char * env_var = getenv(real_env_var);
                if (env_var) {
                    printf("%s%s ", env_var, pos + 1);
                } else {
                    printf("{environment variable \"%s\" not found}%s ", real_env_var, pos + 1);
                }
            }
            continue;
        }
        printf("%s ", args.items[i]);
    }
    printf("\n");
    return 0;
}