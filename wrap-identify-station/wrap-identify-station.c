#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>

#define MAX_LINE_LEN 256
#define TIMEOUT_SECONDS 15

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

        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execvp(argv[1], &argv[1]);

        perror("execvp failed");
        exit(1);
    }

    // Parent process
    close(pipefd[1]);

    FILE *fp = fdopen(pipefd[0], "r");
    if (!fp) {
        perror("fdopen failed");
        kill(pid, SIGKILL);
        close(pipefd[0]);
        return 1;
    }

    int didNotMatchCount = 0;
    char line[MAX_LINE_LEN];
    time_t start = time(NULL);

    while (1) {
        fd_set readfds;
        struct timeval timeout;

        FD_ZERO(&readfds);
        FD_SET(fileno(fp), &readfds);

        timeout.tv_sec = 1;  // Poll every 1 second
        timeout.tv_usec = 0;

        int ready = select(fileno(fp) + 1, &readfds, NULL, NULL, &timeout);

        if (ready > 0 && FD_ISSET(fileno(fp), &readfds)) {
            if (fgets(line, sizeof(line), fp) == NULL) {
                break; // EOF or error
            }

            if (strstr(line, "Station ID matched")) {
                kill(pid, SIGTERM);
                waitpid(pid, NULL, 0);
                fclose(fp);
                return 0; // Success
            }

            if (strstr(line, "Station ID did not match")) {
                didNotMatchCount++;
                if (didNotMatchCount >= 2) {
                    kill(pid, SIGTERM);
                    waitpid(pid, NULL, 0);
                    fclose(fp);
                    return 1; // Error
                }
            }
        }

        // Check for timeout
        if (difftime(time(NULL), start) > TIMEOUT_SECONDS) {
            fprintf(stderr, "Timeout reached.\n");
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            fclose(fp);
            return 1; // Error due to timeout
        }
    }

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    fclose(fp);

    return 1; // No conditions met
}

