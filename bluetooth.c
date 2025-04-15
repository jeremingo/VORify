#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

static int server_socket = -1;
static int client_socket = -1;

void make_device_discoverable() {
    int dev_id = hci_get_route(NULL);
    int hci_sock = hci_open_dev(dev_id);
    if (dev_id < 0 || hci_sock < 0) {
        perror("Failed to open HCI device");
        exit(EXIT_FAILURE);
    }

    if (hci_write_scan_enable(hci_sock, SCAN_PAGE | SCAN_INQUIRY) < 0) {
        perror("Failed to make device discoverable");
        close(hci_sock);
        exit(EXIT_FAILURE);
    }

    close(hci_sock);
    printf("Bluetooth device is now discoverable.\n");
}

void bluetooth_init(const char *device_name) {
    struct sockaddr_rc loc_addr = { 0 };

    // Set the Bluetooth device name
    int dev_id = hci_get_route(NULL);
    int hci_sock = hci_open_dev(dev_id);
    if (dev_id < 0 || hci_sock < 0) {
        perror("Failed to open HCI device");
        exit(EXIT_FAILURE);
    }

    if (hci_write_local_name(hci_sock, device_name, 0) < 0) {
        perror("Failed to set Bluetooth device name");
        close(hci_sock);
        exit(EXIT_FAILURE);
    }
    close(hci_sock);
    printf("Bluetooth device name set to '%s'\n", device_name);

    make_device_discoverable();

    // Create a Bluetooth socket
    server_socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_socket < 0) {
        perror("Failed to create Bluetooth socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the local Bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t)1;

    if (bind(server_socket, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("Failed to bind Bluetooth socket");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_socket, 1) < 0) {
        perror("Failed to listen on Bluetooth socket");
        exit(EXIT_FAILURE);
    }

    printf("Bluetooth server initialized. Waiting for connection...\n");

    // Accept a connection from a client
    struct sockaddr_rc rem_addr = { 0 };
    socklen_t opt = sizeof(rem_addr);
    client_socket = accept(server_socket, (struct sockaddr *)&rem_addr, &opt);
    if (client_socket < 0) {
        perror("Failed to accept Bluetooth connection");
        exit(EXIT_FAILURE);
    }

    char client_address[18] = { 0 };
    ba2str(&rem_addr.rc_bdaddr, client_address);
    printf("Accepted connection from %s\n", client_address);
}

void bluetooth_send(const char *data) {
    if (client_socket < 0) {
        fprintf(stderr, "No client connected. Cannot send data.\n");
        return;
    }

    // Send data to the connected client
    if (write(client_socket, data, strlen(data)) < 0) {
        perror("Failed to send data over Bluetooth");
    }
}

void bluetooth_stop() {
    if (client_socket >= 0) {
        close(client_socket);
        client_socket = -1;
    }
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    printf("Bluetooth server stopped.\n");
}