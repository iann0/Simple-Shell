#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

void handle_sigint(int sig) {
    printf("\n");
}

void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char *user = getenv("USER");
    printf("\033[1;32m%s@myshell\033[0m:\033[1;34m%s\033[0m$ ", user ? user : "user", cwd);
    fflush(stdout);
}

int main() {
    signal(SIGINT, handle_sigint);
    char cmd[MAX_CMD_LEN];

    while (1) {
        print_prompt();

        if (fgets(cmd, MAX_CMD_LEN, stdin) == NULL) break;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strlen(cmd) == 0) continue;

        // Save to history
        history[history_count % HISTORY_SIZE] = strdup(cmd);
        history_count++;

        // Handle pipe
        if (strchr(cmd, '|') != NULL) {
            char *pipe_cmds[2];
            pipe_cmds[0] = strtok(cmd, "|");
            pipe_cmds[1] = strtok(NULL, "|");

            if (!pipe_cmds[1]) {
                fprintf(stderr, "Invalid pipe command\n");
                continue;
            }

            while (*pipe_cmds[0] == ' ') pipe_cmds[0]++;
            while (*pipe_cmds[1] == ' ') pipe_cmds[1]++;

            int fd[2];
            pipe(fd);

            pid_t pid1 = fork();
            if (pid1 == 0) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);

                char *args1[MAX_ARGS];
                int i = 0;
                char *tok = strtok(pipe_cmds[0], " ");
                while (tok) {
                    args1[i++] = tok;
                    tok = strtok(NULL, " ");
                }
                args1[i] = NULL;

                execvp(args1[0], args1);
                perror("execvp failed");
                exit(1);
            }

            pid_t pid2 = fork();
            if (pid2 == 0) {
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                close(fd[0]);

                char *args2[MAX_ARGS];
                int i = 0;
                char *tok = strtok(pipe_cmds[1], " ");
                while (tok) {
                    args2[i++] = tok;
                    tok = strtok(NULL, " ");
                }
                args2[i] = NULL;

                execvp(args2[0], args2);
                perror("execvp failed");
                exit(1);
            }

            close(fd[0]);
            close(fd[1]);
            wait(NULL);
            wait(NULL);
            continue;
        }

        // Tokenize input
        char *args[MAX_ARGS];
        int i = 0;
        char *token = strtok(cmd, " ");
        while (token && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        if (args[0] == NULL) continue;

        // Built-in: exit
        if (strcmp(args[0], "exit") == 0) break;

        // Built-in: cd
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                fprintf(stderr, "cd: expected argument\n");
            } else if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
            continue;
        }

        // Built-in: history
        if (strcmp(args[0], "history") == 0) {
            for (int h = 0; h < history_count && h < HISTORY_SIZE; h++) {
                printf("%d: %s\n", h + 1, history[h]);
            }
            continue;
        }

        // Background process check
        int background = 0;
        if (i > 0 && strcmp(args[i - 1], "&") == 0) {
            background = 1;
            args[i - 1] = NULL;
        }

        // I/O Redirection
        int redirect_out = 0, redirect_in = 0;
        char *outfile = NULL, *infile = NULL;

        for (int j = 0; j < i; j++) {
            if (strcmp(args[j], ">") == 0 && args[j + 1]) {
                redirect_out = 1;
                outfile = args[j + 1];
                args[j] = NULL;
                break;
            } else if (strcmp(args[j], "<") == 0 && args[j + 1]) {
                redirect_in = 1;
                infile = args[j + 1];
                args[j] = NULL;
                break;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (redirect_out && outfile) {
                freopen(outfile, "w", stdout);
            }
            if (redirect_in && infile) {
                freopen(infile, "r", stdin);
            }
            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        } else if (pid > 0) {
            if (!background) {
                wait(NULL);
            } else {
                printf("[background pid %d]\n", pid);
            }
        } else {
            perror("fork failed");
        }
    }

    return 0;
}
