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
#define MAX_DOUBLES 5

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
    double first_value = 0.0;
    double value;
    char extra[MAX_LINE_LEN];
    bool first_assigned = false;
    int count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && count < MAX_DOUBLES) {
        if (sscanf(line, " %lf %s", &value, extra) == 1) {
            if (!first_assigned) {
                first_value = value;
                first_assigned = true;
            } else {
                if (value != first_value) {
                    fprintf(stderr, "Mismatch: got %f but expected %f\n", value, first_value);
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    fclose(fp);
                    return 1;
                }
            }
            count++;
        }
    }

    // Kill child process (in case it’s still running)
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    fclose(fp);

    if (!first_assigned) {
        fprintf(stderr, "No bearing found\n");
        return 1;
    }

    printf("%f\n", first_value);
    return 0;
}