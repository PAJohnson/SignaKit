#pragma once
#include <cstdint>

struct __attribute__((packed)) IMUData
{
    char header[4] = {'I', 'M', 'U', '\0'};
    double time;
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
};

struct __attribute__((packed)) GPSData
{
    char header[4] = {'G', 'P', 'S', '\0'};
    double time;
    double latitude;
    double longitude;
    float  altitude;
    float  speed;
};