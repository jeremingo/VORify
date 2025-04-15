#ifndef BLUETOOTH_H
#define BLUETOOTH_H

// Initializes the Bluetooth server
void bluetooth_init(const char *device_name);

// Sends GPS data over Bluetooth
void bluetooth_send(const char *data);

// Stops the Bluetooth server
void bluetooth_stop();

#endif // BLUETOOTH_H