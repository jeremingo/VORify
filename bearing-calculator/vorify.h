#define FSINT 50000

extern int freq;

typedef struct {
    char name[100];       // Name of the VOR station
    double frequency;     // Frequency in Hz
    double latitude;      // Latitude in decimal degrees
    double longitude;     // Longitude in decimal degrees
    double elevation;     // Elevation in meters
} VORStation;