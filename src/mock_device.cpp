#include <iostream>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "telemetry_defs.h"

#define DEST_IP "127.0.0.1"
#define DEST_PORT 5000

std::atomic<bool> running(true);

// Helper to get time in seconds
double get_time() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration<double>(now - start).count();
}

#ifdef _WIN32
void imu_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void imu_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    IMUData packet;
    memcpy(packet.header, "IMU", 4);
    memset(&packet, 0, sizeof(packet)); // Zero-initialize entire packet
    memcpy(packet.header, "IMU", 4); // Restore header

    std::cout << "[IMU] Thread started (1000Hz with burst sending)..." << std::endl;

    using namespace std::chrono;
    const double TARGET_RATE = 1000.0; // 1000Hz (realistic IMU rate)
    const int SLEEP_MS = 10;

    double lastBurstTime = get_time();

    while (running) {
        auto burstStart = steady_clock::now();
        double currentTime = get_time();

        double timeSinceLastBurst = currentTime - lastBurstTime;
        int packetsToSend = (int)(timeSinceLastBurst * TARGET_RATE);

        // At 1kHz with 10ms sleep, we expect ~10 packets per burst
        if (packetsToSend > 20) packetsToSend = 20;  // Cap at 20
        if (packetsToSend < 1) packetsToSend = 1;

        for (int i = 0; i < packetsToSend; i++) {
            double t = lastBurstTime + (i + 1) * (timeSinceLastBurst / packetsToSend);
            packet.time = t;

            // Accelerometer data
            packet.accelX = (float)sin(2.0 * 3.14159 * 5 * t) + (float)sin(2.0 * 3.14159 * 25 * t);
            packet.accelY = (float)cos(t);
            packet.accelZ = (float)(sin(t * 0.5) * 2.0f) + 9.81f; // Include gravity

            // Gyroscope data
            packet.gyroX = (float)(0.1 * sin(t * 0.3));
            packet.gyroY = (float)(0.05 * cos(t * 0.5));
            packet.gyroZ = (float)(0.02 * sin(t * 0.7));

            // Magnetometer data
            packet.magX = 25.0f + 2.0f * (float)sin(t * 0.1);
            packet.magY = -15.0f + 1.5f * (float)cos(t * 0.15);
            packet.magZ = 40.0f + 3.0f * (float)sin(t * 0.08);

            // Temperature
            packet.temperature = 35.0f + 5.0f * (float)sin(t * 0.05);

            // Covariance matrices (simplified - diagonal values only)
            for (int j = 0; j < 9; j++) {
                packet.accelCov[j] = (j % 4 == 0) ? 0.01f : 0.0f; // Diagonal
                packet.gyroCov[j] = (j % 4 == 0) ? 0.001f : 0.0f;
                packet.magCov[j] = (j % 4 == 0) ? 0.5f : 0.0f;
            }

            // Calibration status
            packet.accelCalibStatus = 3; // Fully calibrated
            packet.gyroCalibStatus = 3;
            packet.magCalibStatus = 2; // Partially calibrated

            sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        lastBurstTime = currentTime;

        auto burstEnd = steady_clock::now();
        auto burstDuration = duration_cast<milliseconds>(burstEnd - burstStart);
        auto sleepTime = milliseconds(SLEEP_MS) - burstDuration;

        if (sleepTime.count() > 0) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

#ifdef _WIN32
void gps_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void gps_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    GPSData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "GPS", 4);

    std::cout << "[GPS] Thread started (10Hz)..." << std::endl;

    double base_lat = 40.7128;
    double base_lon = -74.0060;

    while (running) {
        double t = get_time();
        packet.time = t;

        // Move in a circle
        packet.latitude = base_lat + (0.001 * sin(t * 0.2));
        packet.longitude = base_lon + (0.001 * cos(t * 0.2));
        packet.altitude = 100.0f + (float)sin(t) * 5.0f;
        packet.speed = 25.0f;
        packet.heading = (float)fmod(t * 10.0, 360.0); // Rotating heading
        packet.verticalSpeed = 0.5f * (float)cos(t * 0.3);

        // Position covariance (simplified diagonal)
        for (int i = 0; i < 9; i++) {
            packet.posCov[i] = (i % 4 == 0) ? 2.5f : 0.0f;
            packet.velCov[i] = (i % 4 == 0) ? 0.1f : 0.0f;
        }

        // GPS quality metrics
        packet.numSatellites = 12;
        packet.fixType = 3; // RTK fix
        packet.hdop = 0.8f + 0.2f * (float)sin(t * 0.1);
        packet.vdop = 1.2f + 0.3f * (float)cos(t * 0.12);

        // Populate satellite info for 12 satellites
        for (int i = 0; i < 12; i++) {
            packet.satellites[i].prn = (uint8_t)(i + 1);
            packet.satellites[i].snr = (uint8_t)(35 + 10 * sin(t * 0.1 + i));
            packet.satellites[i].elevation = (uint8_t)(30 + 20 * sin(t * 0.05 + i * 0.5));
            packet.satellites[i].azimuth = (uint8_t)((i * 30) % 180);
        }

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void battery_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void battery_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    BatteryData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "BAT", 4);
    std::cout << "[Battery] Thread started (1Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.voltage = 12.0f + 0.5f * (float)sin(t * 0.1);
        packet.current = 5.0f + 2.0f * (float)cos(t * 0.15);
        packet.temperature = 25.0f + 5.0f * (float)sin(t * 0.05);
        packet.percentage = (uint8_t)(100 - (int)(t / 10.0) % 100);
        packet.health = (uint8_t)(100 - (int)(t / 100.0) % 10);
        packet.cycleCount = (uint16_t)(t / 5.0);

        // Populate 16 cells with varying voltages and temperatures
        float baseVoltage = packet.voltage / 16.0f;
        for (int i = 0; i < 16; i++) {
            packet.cells[i].voltage = baseVoltage + 0.05f * (float)sin(t * 0.1 + i * 0.1);
            packet.cells[i].temperature = packet.temperature + (i % 4) * 2.0f;
            packet.cells[i].health = (uint8_t)(95 + (i % 5));
            packet.cells[i].balancing = (uint8_t)((i % 3) == 0 ? 1 : 0);
            packet.cells[i].resistance = (uint16_t)(50 + i * 2);
        }

        // Battery pack status
        packet.maxCellVoltage = baseVoltage + 0.1f;
        packet.minCellVoltage = baseVoltage - 0.05f;
        packet.avgCellVoltage = baseVoltage;
        packet.maxCellTemp = packet.temperature + 6.0f;
        packet.minCellTemp = packet.temperature;
        packet.avgCellTemp = packet.temperature + 3.0f;
        packet.powerOut = packet.voltage * packet.current;
        packet.energyConsumed = (float)(t * 0.5);
        packet.energyRemaining = 100.0f - packet.energyConsumed;
        packet.timeToEmpty = (uint32_t)(3600 * (packet.energyRemaining / packet.powerOut));
        packet.timeToFull = 0;
        packet.chargeState = 0; // Discharging
        packet.faultFlags = 0;

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void lidar_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void lidar_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    LIDARData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "LID", 4);
    std::cout << "[LIDAR] Thread started (20Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.range = 10.0f + 5.0f * (float)sin(t);
        packet.intensity = 50.0f + 30.0f * (float)cos(t * 2);
        packet.angleX = (int16_t)(1000 * sin(t * 0.3));
        packet.angleY = (int16_t)(500 * cos(t * 0.3));
        packet.quality = (uint8_t)(80 + 15 * sin(t));
        packet.flags = (uint8_t)(t) % 4;

        // Populate 32 tracks with simulated objects
        packet.numTracks = 8 + (uint8_t)(sin(t * 0.5) * 4); // Vary between 4-12 tracks
        for (int i = 0; i < 32; i++) {
            if (i < packet.numTracks) {
                packet.tracks[i].range = 5.0f + i * 2.0f + (float)sin(t + i) * 2.0f;
                packet.tracks[i].azimuth = (float)(i * 0.3 + t * 0.1);
                packet.tracks[i].elevation = (float)(sin(t * 0.2 + i * 0.1) * 0.5);
                packet.tracks[i].velocity = 10.0f * (float)sin(t * 0.1 + i);
                packet.tracks[i].rcs = 5.0f + (float)(i % 10);
                packet.tracks[i].trackID = (uint8_t)(i + 1);
                packet.tracks[i].confidence = (uint8_t)(80 + (i % 20));
                packet.tracks[i].age = (uint16_t)(t * 10 + i * 5);
            } else {
                // Zero out unused tracks
                memset(&packet.tracks[i], 0, sizeof(packet.tracks[i]));
            }
        }

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void radar_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void radar_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    RADARData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "RAD", 4);
    std::cout << "[RADAR] Thread started (10Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.range = 50.0f + 20.0f * (float)sin(t * 0.2);
        packet.velocity = 15.0f * (float)cos(t * 0.1);
        packet.azimuth = 45.0f + 30.0f * (float)sin(t * 0.15);
        packet.elevation = 10.0f * (float)cos(t * 0.1);
        packet.signalStrength = (int16_t)(-50 - 10 * sin(t));
        packet.targetCount = (uint8_t)(3 + ((int)t % 5));
        packet.trackID = (uint8_t)((int)t % 256);

        // Populate 24 radar tracks
        for (int i = 0; i < 24; i++) {
            if (i < packet.targetCount) {
                packet.tracks[i].range = 30.0f + i * 5.0f + (float)sin(t + i * 0.5) * 10.0f;
                packet.tracks[i].azimuth = -60.0f + i * 5.0f;
                packet.tracks[i].elevation = (float)sin(t * 0.1 + i) * 15.0f;
                packet.tracks[i].velocity = 20.0f * (float)cos(t * 0.1 + i);
                packet.tracks[i].rangeRate = 0.5f * (float)sin(t + i);
                packet.tracks[i].rcs = 15.0f + (float)(i % 8);
                packet.tracks[i].snr = 25.0f + 5.0f * (float)sin(t + i);
                packet.tracks[i].trackID = (uint8_t)(i + 1);
                packet.tracks[i].trackStatus = (uint8_t)((i % 3) + 1); // Cycling through statuses
                packet.tracks[i].classification = (uint8_t)((i % 3) + 1); // Vehicle, pedestrian, etc
                packet.tracks[i].confidence = (uint8_t)(70 + (i % 30));
                packet.tracks[i].age = (uint16_t)(t * 10 + i * 10);
                packet.tracks[i].hits = (uint16_t)(50 + i * 5);
            } else {
                memset(&packet.tracks[i], 0, sizeof(packet.tracks[i]));
            }
        }

        // Radar status
        packet.ambientNoise = -70.0f + 5.0f * (float)sin(t * 0.05);
        packet.temperature = 45.0f + 10.0f * (float)sin(t * 0.03);
        packet.mode = (uint8_t)((int)(t / 10) % 3); // Cycle through modes
        packet.interference = (uint8_t)(10 + 5 * sin(t * 0.2));

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void state_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void state_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    StateData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "STA", 4);
    std::cout << "[State] Thread started (2Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.systemMode = (uint8_t)((int)t % 5);
        packet.armed = (uint8_t)((int)t % 10 > 5 ? 1 : 0);
        packet.statusFlags = (uint16_t)(0x0100 | ((int)t % 16));
        packet.errorCode = (int32_t)((int)t % 7 == 0 ? -42 : 0);
        packet.uptime = (uint32_t)t;
        packet.cpuUsage = 50.0f + 20.0f * (float)sin(t * 0.3);
        packet.memoryUsage = 60.0f + 15.0f * (float)cos(t * 0.2);

        // Extended system state
        packet.diskUsage = 45.0f + 10.0f * (float)sin(t * 0.1);
        packet.networkTxRate = 1500.0f + 500.0f * (float)sin(t * 0.4);
        packet.networkRxRate = 2000.0f + 800.0f * (float)cos(t * 0.35);
        packet.gpuUsage = 30.0f + 15.0f * (float)sin(t * 0.25);
        packet.gpuMemoryUsage = 40.0f + 20.0f * (float)cos(t * 0.3);
        packet.gpuTemperature = 65.0f + 10.0f * (float)sin(t * 0.05);
        packet.cpuTemperature = 55.0f + 8.0f * (float)sin(t * 0.06);
        packet.boardTemperature = 45.0f + 5.0f * (float)sin(t * 0.04);

        // Subsystem health (16 subsystems)
        for (int i = 0; i < 16; i++) {
            packet.subsystems[i].status = (uint8_t)(2 + (i % 3)); // Mostly ready
            packet.subsystems[i].health = (uint8_t)(85 + (i % 15));
            packet.subsystems[i].errorCode = (uint16_t)(i % 3 == 0 ? 100 + i : 0);
            packet.subsystems[i].cpuUsage = 10.0f + i * 2.0f + 5.0f * (float)sin(t + i);
            packet.subsystems[i].memUsage = 50.0f + i * 10.0f;
        }

        // Error history (last 8 errors)
        packet.errorHistoryCount = 3; // 3 recent errors
        for (int i = 0; i < 8; i++) {
            if (i < packet.errorHistoryCount) {
                packet.errorHistory[i].errorCode = 1000 + i * 10;
                packet.errorHistory[i].timestamp = (uint32_t)(t - (8 - i) * 10);
                packet.errorHistory[i].severity = (uint8_t)(i % 4);
                packet.errorHistory[i].subsystem = (uint8_t)(i % 8);
            }
        }

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void debug_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void debug_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    DebugData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "DBG", 4);
    std::cout << "[Debug] Thread started (5Hz)..." << std::endl;

    int64_t counter = 0;
    while (running) {
        double t = get_time();
        packet.time = t;
        packet.counter = counter++;
        packet.eventID = (uint32_t)(counter % 1000);
        packet.priority = (int8_t)(-50 + (counter % 100));
        packet.subsystem = (uint8_t)(counter % 8);
        packet.value1 = (int16_t)(1000 * sin(t));
        packet.value2 = (int16_t)(500 * cos(t * 2));
        packet.metric = (float)(100.0 * sin(t * 0.5));

        // Extended debug data - populate metrics and values arrays
        for (int i = 0; i < 32; i++) {
            packet.metrics[i] = (float)(i * 10.0 + 50.0 * sin(t * 0.1 + i * 0.1));
            packet.values[i] = (int32_t)(i * 100 + 500 * cos(t * 0.2 + i * 0.15));
        }

        // Performance counters (8 counters)
        const char* counterNames[] = {"Frames", "Packets", "Errors", "Warnings", "CPU_Cycles", "Memory_Ops", "Disk_IO", "Net_TX"};
        for (int i = 0; i < 8; i++) {
            strncpy(packet.counters[i].name, counterNames[i], 15);
            packet.counters[i].name[15] = '\0';
            packet.counters[i].count = (uint64_t)(counter * (i + 1) * 10);
            packet.counters[i].rate = 100.0 * (i + 1) + 50.0 * sin(t * 0.1);
            packet.counters[i].average = 500.0 * (i + 1);
        }

        // Stack trace (simulated with 10 frames)
        packet.stackDepth = 10;
        for (int i = 0; i < 16; i++) {
            if (i < packet.stackDepth) {
                packet.stackTrace[i].address = 0x00400000 + i * 0x1000;
                packet.stackTrace[i].offset = (uint32_t)(i * 100);
                packet.stackTrace[i].line = (uint32_t)(50 + i * 10);
            }
        }

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

#ifdef _WIN32
void motor_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void motor_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    MotorData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "MTR", 4);
    std::cout << "[Motor] Thread started (10Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.rpm = (int16_t)(3000 + 500 * sin(t * 0.5));
        packet.torque = 50.0f + 10.0f * (float)cos(t * 0.3);
        packet.power = 1500.0f + 300.0f * (float)sin(t * 0.4);
        packet.temperature = (int8_t)(60 + 15 * sin(t * 0.1));
        packet.throttle = (uint8_t)(50 + 30 * sin(t * 0.2));
        packet.faults = (uint16_t)((int)t % 13 == 0 ? 0x0001 : 0x0000);
        packet.totalRotations = (uint32_t)(t * 50);

        // Extended motor diagnostics
        packet.voltage = 48.0f + 2.0f * (float)sin(t * 0.15);
        packet.current = packet.power / packet.voltage;
        packet.backEMF = 40.0f + 5.0f * (float)cos(t * 0.25);
        packet.efficiency = 85.0f + 10.0f * (float)sin(t * 0.1);
        packet.dutyCycle = (float)packet.throttle * 0.8f;
        packet.motorTemp = (float)packet.temperature + 5.0f;
        packet.controllerTemp = (float)packet.temperature - 10.0f;
        packet.targetRPM = 3500.0f;
        packet.rpmError = packet.targetRPM - (float)packet.rpm;

        // Phase currents (3-phase motor)
        float basePhase = t * 2.0 * 3.14159 * 60; // 60 Hz electrical
        packet.phaseA_current = packet.current * (float)sin(basePhase) / 3.0f;
        packet.phaseB_current = packet.current * (float)sin(basePhase + 2.094) / 3.0f; // 120 deg
        packet.phaseC_current = packet.current * (float)sin(basePhase + 4.189) / 3.0f; // 240 deg

        // Phase voltages
        packet.phaseA_voltage = packet.voltage * (float)sin(basePhase) / 3.0f;
        packet.phaseB_voltage = packet.voltage * (float)sin(basePhase + 2.094) / 3.0f;
        packet.phaseC_voltage = packet.voltage * (float)sin(basePhase + 4.189) / 3.0f;

        // PID control signals
        packet.pidP = packet.rpmError * 0.01f;
        packet.pidI = (float)(t * 0.001);
        packet.pidD = (float)(-packet.rpmError * 0.0001);
        packet.pidOutput = packet.pidP + packet.pidI + packet.pidD;

        // Health metrics
        packet.vibration = 0.5f + 0.2f * (float)sin(t * 5.0);
        packet.acousticNoise = 70.0f + 5.0f * (float)sin(t * 0.3);
        packet.runTime = (uint32_t)t;
        packet.startCount = (uint32_t)(t / 100);
        packet.warningFlags = (uint16_t)((int)t % 20 == 0 ? 0x0002 : 0x0000);

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }
#else
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
#endif

    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);

    std::cout << "Starting Mock Device. Sending to " << DEST_IP << ":" << DEST_PORT << std::endl;
    std::cout << "Press Ctrl+C to stop (or kill the process)." << std::endl;
    std::cout << "\nTransmitting 8 packet types with expanded structures:" << std::endl;
    std::cout << "  IMU (1000Hz, " << sizeof(IMUData) << " bytes)" << std::endl;
    std::cout << "  GPS (10Hz, " << sizeof(GPSData) << " bytes)" << std::endl;
    std::cout << "  Battery (1Hz, " << sizeof(BatteryData) << " bytes)" << std::endl;
    std::cout << "  LIDAR (20Hz, " << sizeof(LIDARData) << " bytes)" << std::endl;
    std::cout << "  RADAR (10Hz, " << sizeof(RADARData) << " bytes)" << std::endl;
    std::cout << "  State (2Hz, " << sizeof(StateData) << " bytes)" << std::endl;
    std::cout << "  Debug (5Hz, " << sizeof(DebugData) << " bytes)" << std::endl;
    std::cout << "  Motor (10Hz, " << sizeof(MotorData) << " bytes)" << std::endl;

    // Calculate data rate
    double bytesPerSecond =
        sizeof(IMUData) * 1000.0 +      // IMU at 1000Hz
        sizeof(GPSData) * 50.0 +        // GPS at 10Hz
        sizeof(BatteryData) * 50.0 +     // Battery at 1Hz
        sizeof(LIDARData) * 50.0 +      // LIDAR at 20Hz
        sizeof(RADARData) * 50.0 +      // RADAR at 10Hz
        sizeof(StateData) * 50.0 +       // State at 2Hz
        sizeof(DebugData) * 50.0 +       // Debug at 5Hz
        sizeof(MotorData) * 50.0;       // Motor at 10Hz

    double mbitsPerSecond = (bytesPerSecond * 8.0) / (1000.0 * 1000.0);
    double mbytesPerSecond = bytesPerSecond / (1024.0 * 1024.0);

    std::cout << "\nCalculated data rate:" << std::endl;
    std::cout << "  " << mbitsPerSecond << " Mb/s (" << mbytesPerSecond << " MB/s)" << std::endl;

    // Start threads
    std::thread imuThread(imu_thread_func, sockfd, dest_addr);
    std::thread gpsThread(gps_thread_func, sockfd, dest_addr);
    std::thread batteryThread(battery_thread_func, sockfd, dest_addr);
    std::thread lidarThread(lidar_thread_func, sockfd, dest_addr);
    std::thread radarThread(radar_thread_func, sockfd, dest_addr);
    std::thread stateThread(state_thread_func, sockfd, dest_addr);
    std::thread debugThread(debug_thread_func, sockfd, dest_addr);
    std::thread motorThread(motor_thread_func, sockfd, dest_addr);

    imuThread.join();
    gpsThread.join();
    batteryThread.join();
    lidarThread.join();
    radarThread.join();
    stateThread.join();
    debugThread.join();
    motorThread.join();

#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}