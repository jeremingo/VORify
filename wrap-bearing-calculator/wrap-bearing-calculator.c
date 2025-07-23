#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_LINE_LEN 256

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable> [args...]\n", argv[0]);
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout and stderr to write end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Execute the command
        execvp(argv[1], &argv[1]);

        // If execvp fails
        perror("execvp failed");
        exit(1);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    FILE *fp = fdopen(pipefd[0], "r");
    if (!fp) {
        perror("fdopen failed");
        kill(pid, SIGKILL);
        close(pipefd[0]);
        return 1;
    }

    char line[MAX_LINE_LEN];
    double value;
    bool assigned = false;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char extra[MAX_LINE_LEN];

        // Match lines with exactly one double and nothing else
        if (sscanf(line, " %lf %s", &value, extra) == 1) {
          assigned = true;
              break;
        }
    }

    // Kill the child process
    kill(pid, SIGTERM); // or SIGKILL for immediate stop

    // Wait for child to exit (prevent zombie)
    waitpid(pid, NULL, 0);

    fclose(fp);

    if (!assigned) {
        fprintf(stderr, "No bearing found\n");
        return 1;
    }

    printf("%f\n", value);

    return 0;
}

