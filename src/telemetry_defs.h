#pragma once
#include <cstdint>

struct __attribute__((packed)) IMUData
{
    char header[4] = {'I', 'M', 'U', '\0'};
    double time;
    // Accelerometer data (m/s^2)
    float accelX;
    float accelY;
    float accelZ;
    // Gyroscope data (rad/s)
    float gyroX;
    float gyroY;
    float gyroZ;
    // Magnetometer data (µT)
    float magX;
    float magY;
    float magZ;
    // Sensor temperature (°C)
    float temperature;
    // Covariance matrix for accelerometer (9 elements, symmetric)
    float accelCov[9];
    // Covariance matrix for gyroscope (9 elements, symmetric)
    float gyroCov[9];
    // Covariance matrix for magnetometer (9 elements, symmetric)
    float magCov[9];
    // Calibration status flags
    uint8_t accelCalibStatus;
    uint8_t gyroCalibStatus;
    uint8_t magCalibStatus;
    uint8_t padding;
};

struct __attribute__((packed)) GPSData
{
    char header[4] = {'G', 'P', 'S', '\0'};
    double time;
    double latitude;   // degrees
    double longitude;  // degrees
    float altitude;    // meters MSL
    float speed;       // m/s
    float heading;     // degrees (0-360)
    float verticalSpeed; // m/s
    // Position covariance (3x3 = 9 elements)
    float posCov[9];
    // Velocity covariance (3x3 = 9 elements)
    float velCov[9];
    // GPS quality metrics
    uint8_t numSatellites;
    uint8_t fixType;    // 0=no fix, 1=2D, 2=3D, 3=RTK
    float hdop;         // Horizontal dilution of precision
    float vdop;         // Vertical dilution of precision
    // Satellite info for up to 12 satellites
    struct {
        uint8_t prn;    // Satellite PRN number
        uint8_t snr;    // Signal-to-noise ratio (dB)
        uint8_t elevation; // degrees
        uint8_t azimuth;   // degrees / 2 (0-180)
    } satellites[12];
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
    // Cell-level diagnostics (up to 16 cells)
    struct {
        float voltage;    // Individual cell voltage (V)
        float temperature; // Cell temperature (°C)
        uint8_t health;   // Cell health (0-100%)
        uint8_t balancing; // Balancing status (0=off, 1=on)
        uint16_t resistance; // Internal resistance (milliohms)
    } cells[16];
    // Battery pack status
    float maxCellVoltage;
    float minCellVoltage;
    float avgCellVoltage;
    float maxCellTemp;
    float minCellTemp;
    float avgCellTemp;
    float powerOut;       // Instantaneous power output (W)
    float energyConsumed; // Total energy consumed (Wh)
    float energyRemaining; // Estimated energy remaining (Wh)
    uint32_t timeToEmpty; // Estimated time to empty (seconds)
    uint32_t timeToFull;  // Estimated time to full charge (seconds)
    uint8_t chargeState;  // 0=discharging, 1=charging, 2=idle
    uint8_t faultFlags;   // Battery fault flags
    uint16_t padding;
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
    // Track data for up to 32 detected objects
    struct {
        float range;       // meters
        float azimuth;     // radians
        float elevation;   // radians
        float velocity;    // m/s (doppler if available)
        float rcs;         // Radar cross-section estimate
        uint8_t trackID;   // Tracking ID
        uint8_t confidence; // Track confidence 0-100
        uint16_t age;      // Track age in frames
    } tracks[32];
    uint8_t numTracks;
    uint8_t padding[3];
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
    // Track data for up to 24 detected targets
    struct {
        float range;       // meters
        float azimuth;     // degrees
        float elevation;   // degrees
        float velocity;    // m/s (doppler)
        float rangeRate;   // m/s^2 (acceleration)
        float rcs;         // Radar cross-section (dBsm)
        float snr;         // Signal-to-noise ratio (dB)
        uint8_t trackID;   // Tracking ID
        uint8_t trackStatus; // Track status (0=tentative, 1=confirmed, 2=coasting)
        uint8_t classification; // Target class (0=unknown, 1=vehicle, 2=pedestrian, etc)
        uint8_t confidence; // Classification confidence (0-100%)
        uint16_t age;      // Track age in frames
        uint16_t hits;     // Number of detections
    } tracks[24];
    // Radar status
    float ambientNoise;   // Ambient noise level (dBm)
    float temperature;    // Radar sensor temperature (°C)
    uint8_t mode;         // Operating mode (0=short range, 1=medium, 2=long)
    uint8_t interference; // Interference detected (0-100%)
    uint16_t padding;
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
    // Extended system state
    float diskUsage;      // Disk usage (0-100%)
    float networkTxRate;  // Network transmit rate (KB/s)
    float networkRxRate;  // Network receive rate (KB/s)
    float gpuUsage;       // GPU usage (0-100%)
    float gpuMemoryUsage; // GPU memory usage (0-100%)
    float gpuTemperature; // GPU temperature (°C)
    float cpuTemperature; // CPU temperature (°C)
    float boardTemperature; // Board temperature (°C)
    // Subsystem health (up to 16 subsystems)
    struct {
        uint8_t status;   // 0=offline, 1=initializing, 2=ready, 3=error, 4=degraded
        uint8_t health;   // Health score (0-100%)
        uint16_t errorCode; // Subsystem-specific error code
        float cpuUsage;   // Subsystem CPU usage (0-100%)
        float memUsage;   // Subsystem memory usage (MB)
        uint16_t padding;
    } subsystems[16];
    // Error history (last 8 errors)
    struct {
        uint32_t errorCode;
        uint32_t timestamp; // Seconds since boot
        uint8_t severity;   // 0=info, 1=warning, 2=error, 3=critical
        uint8_t subsystem;  // Subsystem ID
        uint16_t padding;
    } errorHistory[8];
    uint8_t errorHistoryCount; // Number of valid entries in errorHistory
    uint8_t padding[3];
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
    // Extended debug data
    float metrics[32];    // Array of debug metrics
    int32_t values[32];   // Array of debug values
    // Performance counters
    struct {
        char name[16];    // Counter name (null-terminated)
        uint64_t count;   // Counter value
        double rate;      // Rate (per second)
        double average;   // Average value
    } counters[8];
    // Stack trace (up to 16 frames)
    struct {
        uint64_t address; // Instruction pointer
        uint32_t offset;  // Offset from function start
        uint32_t line;    // Source line number
    } stackTrace[16];
    uint8_t stackDepth;   // Number of valid stack frames
    uint8_t padding[7];
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
    // Extended motor diagnostics
    float voltage;        // Motor voltage (V)
    float current;        // Motor current (A)
    float backEMF;        // Back EMF (V)
    float efficiency;     // Efficiency (0-100%)
    float dutyCycle;      // PWM duty cycle (0-100%)
    float motorTemp;      // Motor winding temperature (°C)
    float controllerTemp; // Motor controller temperature (°C)
    float targetRPM;      // Target RPM setpoint
    float rpmError;       // RPM error (target - actual)
    // Phase currents (for 3-phase motors)
    float phaseA_current;
    float phaseB_current;
    float phaseC_current;
    // Phase voltages
    float phaseA_voltage;
    float phaseB_voltage;
    float phaseC_voltage;
    // Control signals
    float pidP;           // PID proportional term
    float pidI;           // PID integral term
    float pidD;           // PID derivative term
    float pidOutput;      // PID output
    // Health metrics
    float vibration;      // Vibration level (g RMS)
    float acousticNoise;  // Acoustic noise (dB)
    uint32_t runTime;     // Total run time (seconds)
    uint32_t startCount;  // Number of starts
    uint16_t warningFlags; // Warning flags
    uint16_t padding;
};