#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bluetooth.h"

void generate_mock_gps_data(char *buffer, size_t buffer_size) {
    // Mock GPS coordinates
    double latitude = 37.7749;
    double longitude = -122.4194;
    double elevation = 15.0;

    snprintf(buffer, buffer_size, "GPS: Lat=%.6f, Lon=%.6f, Elev=%.2f\n",
             latitude, longitude, elevation);
}

int main() {
    char gps_data[256];

    // Initialize the Bluetooth server
    bluetooth_init("MockGPS");

    // Continuously send mock GPS data
    while (1) {
        generate_mock_gps_data(gps_data, sizeof(gps_data));
        printf("Sending: %s", gps_data);
        bluetooth_send(gps_data);
        sleep(1);  // Send data every second
    }

    // Stop the Bluetooth server (this will never be reached in this example)
    bluetooth_stop();

    return 0;
}
