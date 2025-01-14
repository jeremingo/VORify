#include <stdio.h>
#include <stdlib.h>
#include <rtl-sdr.h>

#define DEFAULT_FREQUENCY 978000000  // 978 MHz
#define DEFAULT_SAMPLE_RATE 2048000  // 2.048 MS/s
#define BUFFER_LENGTH (16 * 16384)   // Buffer length

void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx) {
    // Process the buffer data here
    printf("Received %u bytes\n", len);
}

int main() {
    int device_index = 0;  // Use the first detected device
    rtlsdr_dev_t *dev = NULL;
    int r;

    // Open the RTL-SDR device
    r = rtlsdr_open(&dev, device_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open RTL-SDR device\n");
        return EXIT_FAILURE;
    }

    // Set the center frequency
    r = rtlsdr_set_center_freq(dev, DEFAULT_FREQUENCY);
    if (r < 0) {
        fprintf(stderr, "Failed to set center frequency\n");
        rtlsdr_close(dev);
        return EXIT_FAILURE;
    }

    // Set the sample rate
    r = rtlsdr_set_sample_rate(dev, DEFAULT_SAMPLE_RATE);
    if (r < 0) {
        fprintf(stderr, "Failed to set sample rate\n");
        rtlsdr_close(dev);
        return EXIT_FAILURE;
    }

    // Reset the buffer
    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "Failed to reset buffer\n");
        rtlsdr_close(dev);
        return EXIT_FAILURE;
    }

    // Read samples asynchronously
    r = rtlsdr_read_async(dev, rtlsdr_callback, NULL, 0, BUFFER_LENGTH);
    if (r < 0) {
        fprintf(stderr, "Failed to read samples asynchronously\n");
        rtlsdr_close(dev);
        return EXIT_FAILURE;
    }

    // Close the RTL-SDR device
    rtlsdr_close(dev);

    return EXIT_SUCCESS;
}