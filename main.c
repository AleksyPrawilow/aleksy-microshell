#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#define COMMAND_BUFFER_SIZE 1024
#define TOKEN_DELIMITERS  " \t\r\n\a"
#define MAX_TOKENS          64

#define ANSI_RESET_ALL     "\033[0m"
#define ANSI_COLOR_RED     "\033[0;31m"
#define ANSI_COLOR_GREEN   "\033[0;32m"
#define ANSI_COLOR_YELLOW  "\033[0;33m"
#define ANSI_COLOR_BLUE    "\033[0;34m"
#define ANSI_COLOR_MAGENTA "\033[0;35m"
#define ANSI_COLOR_CYAN    "\033[0;36m"
#define ANSI_COLOR_WHITE   "\033[0;37m"

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

void          set_text_color(const char * color_code);
void          reset_text_color();
void          handle_exit   (int error_code);
void          signal_handler(int signum);
void          shell_loop();
void          print_user_directory_prefix();
char *        read_input();
struct tokens parse_input(const char *input);
int           check_flag(struct tokens args, char flag);
int           execute_command(struct tokens args);
int           shell_help     (struct tokens args);
int           shell_exit     (struct tokens args);
int           shell_cd       (struct tokens args);
int           shell_echo     (struct tokens args);
int           shell_tree     (struct tokens args);
void          shell_recursive_tree(const char prev_dir_name[], const char dir_name[], const char print_prefix[], int current_iteration, int max_iterations);

char * available_commands[]                  = {"cd", "exit", "help", "echo", "tree"};
char * built_in_command_descriptions[] = {
    "Change the current directory. Usage: cd <directory_path>",
    "Exit the microshell.",
    "Display this help message.",
    "Echo the input arguments to the console. Supports environment variables with $VAR_NAME.",
    "Display the current folder tree. You can also give a name of the folder you want to see the contents of as an argument. You can add a flag -r to perform a recursive search."
};
int ( * command_functions[]) (struct tokens) = {&shell_cd, &shell_exit, &shell_help, &shell_echo, &shell_tree};

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGSTOP, signal_handler);
    shell_loop();
    return EXIT_SUCCESS;
}

void set_text_color(const char * color_code) {
    printf("%s", color_code);
}

void reset_text_color() {
    printf("\033[0m");
}

void handle_exit(int error_code) {
    switch (error_code)
    {
    case EXIT_SUCCESS_CODE:
        set_text_color(ANSI_COLOR_GREEN);
        printf("\nmicroshell: Exiting microshell. Goodbye!\n");
        reset_text_color();
        exit(EXIT_SUCCESS);
        break;
    case EXIT_SIGINT_CODE:
        set_text_color(ANSI_COLOR_GREEN);
        printf("\nmicroshell: Exiting microshell due to SIGINT (Ctrl+C).\n");
        reset_text_color();
        exit(EXIT_SUCCESS);
        break;
    case EXIT_ALLOCATION_ERROR_CODE:
        set_text_color(ANSI_COLOR_RED);
        fprintf(stderr, "\nmicroshell: Exiting microshell due to allocation error.\n");
        reset_text_color();
        exit(EXIT_FAILURE);
        break;
    case EXIT_MISC_FAILURE_CODE:
        set_text_color(ANSI_COLOR_RED);
        fprintf(stderr, "\nmicroshell: Exiting microshell due to an error.\n");
        reset_text_color();
        exit(EXIT_FAILURE);
        break;
    default:
        set_text_color(ANSI_COLOR_RED);
        printf("\nmicroshell: Exiting microshell...\n");
        reset_text_color();
        exit(EXIT_SUCCESS);
        break;
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        handle_exit(EXIT_SIGINT_CODE);
    }
    if (signum == SIGSTOP) {
        printf("microshell: Received SIGSTOP signal. Ignoring.\n");
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
    printf("%s@%s Microshell: > ", username ? username : "Unknown user", directory_path);
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

int check_flag(struct tokens args, char flag) {
    int found_flags = 0;
    for (int i = 0; i < args.size; i++) {
        if (args.items[i][0] == '-') {
            found_flags = 1;
            int current_character = 1;
            while (args.items[i][current_character] != '\0') {
                if (args.items[i][current_character] == flag) {
                    return 1;
                }
                current_character++;
            }
        }
    }
    if (found_flags == 0) {
        return 0;
    }
    set_text_color(ANSI_COLOR_RED);
    printf("The flags provided are not compatible with this command.\n");
    reset_text_color();
    return 2;
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
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        execvp(args.items[0], args.items);
        set_text_color(ANSI_COLOR_RED);
        printf("microshell: Failed to execute.\n");
        reset_text_color();
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        set_text_color(ANSI_COLOR_RED);
        printf("microshell: Something went wrong while creating a process.\n");
        reset_text_color();
    }
    return 0;
}

int shell_help(struct tokens args) {
    set_text_color(ANSI_COLOR_CYAN);
    printf("This is a microshell created by Aleksey Pravilov - s498780\n");
    printf("Microshell Help:\n");
    set_text_color(ANSI_COLOR_GREEN);
    printf("Available commands:\n");
    for (int i = 0; i < sizeof(available_commands) / sizeof(available_commands[0]); i++) {
        set_text_color(ANSI_COLOR_YELLOW);
        printf(" - %s\n", available_commands[i]);
        set_text_color(ANSI_COLOR_MAGENTA);
        printf("     %s\n", built_in_command_descriptions[i]);
    }
    reset_text_color();
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

int shell_tree(struct tokens args) {
    if (args.size > 3) {
        set_text_color(ANSI_COLOR_RED);
        printf("Expected no more than 2 arguments, received %d.\n", args.size - 1);
        reset_text_color();
        return 0;
    }
    int recursion = check_flag(args, 'r');
    if (recursion == 2) {
        return 0;
    }
    int max_iterations = check_flag(args, 'r') * 5;
    char path[1024] = ".";
    for (int i = 1; i < args.size; i++) {
        if (args.items[i][0] != '-') {
            snprintf(path, sizeof(path), "%s", args.items[i]);
            break;
        }
    }
    shell_recursive_tree("", path, "", 0, max_iterations);
    return 0;
}

void shell_recursive_tree(const char prev_dir_name[], const char dir_name[], const char print_prefix[], int current_iteration, int max_iterations) {
    if (current_iteration > max_iterations) {
        return;
    }
    char path[1024];
    if (strcmp(prev_dir_name, "") == 0) {
        snprintf(path, sizeof(path), "%s", dir_name);
    } else {
        snprintf(path, sizeof(path), "%s/%s", prev_dir_name, dir_name);
    }
    DIR * dir = opendir(path);
    struct dirent * entry;
    if (dir == NULL) {
        perror("opendir");
        return;
    }
    int total_files = 0;
    int current_file = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_type == DT_DIR || entry->d_type == DT_REG) {
            total_files++;
        }
    }
    dir = opendir(path);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (current_file == total_files - 1) {
            printf("%s└──", print_prefix);
        } else {
            printf("%s├──", print_prefix);
        }
        if (entry->d_type == DT_DIR) {
            char new_prefix[1024];
            if (current_file == total_files - 1) {
                snprintf(new_prefix, sizeof(new_prefix), "%s   ", print_prefix);
            } else {
                snprintf(new_prefix, sizeof(new_prefix), "%s│  ", print_prefix);
            }
            if (entry->d_name[0] == '.') {
                set_text_color(ANSI_COLOR_CYAN);
            } else {
                set_text_color(ANSI_COLOR_MAGENTA);
            }
            printf("%s\n", entry->d_name);
            reset_text_color();
            shell_recursive_tree(path, entry->d_name, new_prefix, current_iteration + 1, max_iterations);
        }
        else if (entry->d_type == DT_REG) {
            if (entry->d_name[0] == '.') {
                set_text_color(ANSI_COLOR_CYAN);
            }
            printf("%s\n", entry->d_name);
            reset_text_color();
        }
        else {
            if (entry->d_name[0] == '.') {
                set_text_color(ANSI_COLOR_CYAN);
            }
            printf("%s\n", entry->d_name);
            reset_text_color();
        }
        current_file += 1;
    }
    closedir(dir);
}