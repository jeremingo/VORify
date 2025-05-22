#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

static int server_socket = -1;
static int client_socket = -1;

sdp_session_t *register_service(uint8_t rfcomm_channel) {
  uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid;
  sdp_list_t *l2cap_list = 0, *rfcomm_list = 0;
  sdp_list_t *proto_list = 0, *access_proto_list = 0, *root_list = 0;

  sdp_record_t *record = sdp_record_alloc();

  // Set the service class
  sdp_uuid16_create(&svc_uuid, SERIAL_PORT_SVCLASS_ID);
  sdp_set_service_id(record, svc_uuid);

  // Set the service browse group
  sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
  root_list = sdp_list_append(0, &root_uuid);
  sdp_set_browse_groups(record, root_list);

  // L2CAP protocol
  sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
  l2cap_list = sdp_list_append(0, &l2cap_uuid);

  // RFCOMM protocol and channel
  sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
  sdp_data_t *channel_data = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
  rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
  rfcomm_list = sdp_list_append(rfcomm_list, channel_data);

  // Build protocol list
  proto_list = sdp_list_append(0, l2cap_list);
  proto_list = sdp_list_append(proto_list, rfcomm_list);
  access_proto_list = sdp_list_append(0, proto_list);
  sdp_set_access_protos(record, access_proto_list);
  sdp_set_info_attr(record, "My BT Service", "RaspberryPi", "Serial communication service");

  sdp_session_t *session = sdp_connect(BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
  if (!session) {
    perror("sdp_connect failed");
    sdp_record_free(record);
    return NULL;
  }
  sdp_record_register(session, record, 0);

  return session;
}


void make_device_discoverable() {
    int dev_id = hci_get_route(NULL);
    int hci_sock = hci_open_dev(dev_id);
    if (dev_id < 0 || hci_sock < 0) {
        perror("Failed to open HCI device");
        exit(EXIT_FAILURE);
    }

    struct hci_request rq;

    
    struct {
      uint8_t scan_enable;
    } scan_cp;
    uint8_t status;

    memset(&rq, 0, sizeof(rq));
    memset(&scan_cp, 0, sizeof(scan_cp));

    scan_cp.scan_enable  = SCAN_PAGE | SCAN_INQUIRY;

    rq.ogf = OGF_HOST_CTL;
    rq.ocf = OCF_WRITE_SCAN_ENABLE;
    rq.cparam = &scan_cp;
    rq.clen = sizeof(scan_cp);
    rq.rparam = &status;
    rq.rlen = sizeof(status);
    

    if (hci_send_req(hci_sock, &rq, 1000) < 0 || status) {
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

    register_service(loc_addr.rc_channel);

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
