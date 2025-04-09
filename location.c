#include <stdio.h>
#include <math.h>
#include <vorify.h>
#define M_PI 3.14159265358979323846

double deg_to_rad(double degrees) {
    return degrees * M_PI / 180.0;
}

double rad_to_deg(double radians) {
    return radians * 180.0 / M_PI;
}

void calculate_location(VORStation stations[3], double bearings[3], double *latitude, double *longitude, double *elevation) {
    double lat1 = deg_to_rad(stations[0].latitude);
    double lon1 = deg_to_rad(stations[0].longitude);
    double lat2 = deg_to_rad(stations[1].latitude);
    double lon2 = deg_to_rad(stations[1].longitude);
    double lat3 = deg_to_rad(stations[2].latitude);
    double lon3 = deg_to_rad(stations[2].longitude);

    double bearing1 = deg_to_rad(bearings[0]);
    double bearing2 = deg_to_rad(bearings[1]);
    double bearing3 = deg_to_rad(bearings[2]);

    double x1 = cos(lat1) * cos(lon1);
    double y1 = cos(lat1) * sin(lon1);
    double z1 = sin(lat1);

    double x2 = cos(lat2) * cos(lon2);
    double y2 = cos(lat2) * sin(lon2);
    double z2 = sin(lat2);

    double x3 = cos(lat3) * cos(lon3);
    double y3 = cos(lat3) * sin(lon3);
    double z3 = sin(lat3);

    double x = (x1 + x2 + x3) / 3.0;
    double y = (y1 + y2 + y3) / 3.0;
    double z = (z1 + z2 + z3) / 3.0;

    *latitude = rad_to_deg(atan2(z, sqrt(x * x + y * y)));
    *longitude = rad_to_deg(atan2(y, x));

    *elevation = (stations[0].elevation + stations[1].elevation + stations[2].elevation) / 3.0;
}
