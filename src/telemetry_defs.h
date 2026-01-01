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

struct __attribute__((packed)) BatteryData
{
    char header[4] = {'B', 'A', 'T', '\0'};
    double time;
    float voltage;        // Battery voltage (V)
    float current;        // Battery current (A)
    float temperature;    // Temperature (°C)
    uint8_t percentage;   // State of charge (0-100%)
    uint8_t health;       // Battery health (0-100%)
    uint16_t cycleCount;  // Number of charge cycles
};

struct __attribute__((packed)) LIDARData
{
    char header[4] = {'L', 'I', 'D', '\0'};
    double time;
    float range;          // Distance measurement (m)
    float intensity;      // Return signal intensity
    int16_t angleX;       // Horizontal angle (0.01 degree units)
    int16_t angleY;       // Vertical angle (0.01 degree units)
    uint8_t quality;      // Measurement quality (0-100)
    uint8_t flags;        // Status flags
};

struct __attribute__((packed)) RADARData
{
    char header[4] = {'R', 'A', 'D', '\0'};
    double time;
    float range;          // Target distance (m)
    float velocity;       // Relative velocity (m/s)
    float azimuth;        // Azimuth angle (degrees)
    float elevation;      // Elevation angle (degrees)
    int16_t signalStrength; // Signal strength (dBm)
    uint8_t targetCount;  // Number of detected targets
    uint8_t trackID;      // Target tracking ID
};

struct __attribute__((packed)) StateData
{
    char header[4] = {'S', 'T', 'A', '\0'};
    double time;
    uint8_t systemMode;   // Operating mode (0-255)
    uint8_t armed;        // Armed state (0=disarmed, 1=armed)
    uint16_t statusFlags; // System status flags
    int32_t errorCode;    // Last error code (0 = no error)
    uint32_t uptime;      // System uptime (seconds)
    float cpuUsage;       // CPU usage (0-100%)
    float memoryUsage;    // Memory usage (0-100%)
};

struct __attribute__((packed)) DebugData
{
    char header[4] = {'D', 'B', 'G', '\0'};
    double time;
    int64_t counter;      // Debug counter
    uint32_t eventID;     // Event identifier
    int8_t priority;      // Priority level (-128 to 127)
    uint8_t subsystem;    // Subsystem ID
    int16_t value1;       // Generic debug value 1
    int16_t value2;       // Generic debug value 2
    float metric;         // Debug metric
};

struct __attribute__((packed)) MotorData
{
    char header[4] = {'M', 'T', 'R', '\0'};
    double time;
    int16_t rpm;          // Motor speed (RPM)
    float torque;         // Motor torque (Nm)
    float power;          // Motor power (W)
    int8_t temperature;   // Motor temperature (°C)
    uint8_t throttle;     // Throttle position (0-100%)
    uint16_t faults;      // Fault flags
    uint32_t totalRotations; // Total rotation count
};