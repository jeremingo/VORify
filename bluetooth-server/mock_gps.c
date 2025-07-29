#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include "bluetooth.h"

int main() {
  char gps_data[256];

  // Initialize the Bluetooth server
  bluetooth_init("MockGPS");

  while (1) {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int retval = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

    if (retval == -1) {
      perror("select()");
      break;
    } else if (retval) {
      // New data on stdin
      if (fgets(gps_data, sizeof(gps_data), stdin) != NULL) {
        // Remove trailing newline
        size_t len = strlen(gps_data);
        if (len > 0 && gps_data[len - 1] == '\n') {
          gps_data[len - 1] = '\0';
        }
        printf("Sending from stdin: %s\n", gps_data);
        char with_crlf[260];
        snprintf(with_crlf, sizeof(with_crlf), "%s\r\n", gps_data);
        bluetooth_send(with_crlf);
      }
    }
    // else: do nothing on timeout
  }

  bluetooth_stop();
  return 0;
}

