#include <iostream>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <timeapi.h> // For timeBeginPeriod
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "telemetry_defs.h"

#define DEST_IP "127.0.0.1"
#define DEST_PORT 5000

// =========================================================================
// TARGET RATES (Hz) - Adjust these to change data bandwidth
// =========================================================================
const double RATE_IMU      = 500.0;
const double RATE_GPS      = 50.0;
const double RATE_BATTERY  = 10.0;
const double RATE_LIDAR    = 600.0;
const double RATE_RADAR    = 800.0;
const double RATE_STATE    = 50.0;
const double RATE_DEBUG    = 100.0;
const double RATE_MOTOR    = 400.0;

const int SLEEP_INTERVAL_MS = 10; // Main loop sleep for all threads
// =========================================================================

std::atomic<bool> running(true);

// Helper to get time in seconds
double get_time() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration<double>(now - start).count();
}

#ifdef _WIN32
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

void imu_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    IMUData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "IMU", 4);

    std::cout << "[IMU] Thread started (" << RATE_IMU << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_IMU);
        int toSend = (int)(targetPackets - packetsSent);

        if (toSend > 100) toSend = 100; // Cap burst size

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_IMU);
            packet.time = t;
            packet.accelX = (float)sin(2.0 * M_PI * 5 * t) + (float)sin(2.0 * M_PI * 25 * t);
            packet.accelY = (float)cos(t);
            packet.accelZ = (float)(sin(t * 0.5) * 2.0f) + 9.81f;
            packet.gyroX = (float)(0.1 * sin(t * 0.3));
            packet.gyroY = (float)(0.05 * cos(t * 0.5));
            packet.gyroZ = (float)(0.02 * sin(t * 0.7));
            packet.magX = 25.0f + 2.0f * (float)sin(t * 0.1);
            packet.magY = -15.0f + 1.5f * (float)cos(t * 0.15);
            packet.magZ = 40.0f + 3.0f * (float)sin(t * 0.08);
            packet.temperature = 35.0f + 5.0f * (float)sin(t * 0.05);

            for (int j = 0; j < 9; j++) {
                packet.accelCov[j] = (j % 4 == 0) ? 0.01f : 0.0f;
                packet.gyroCov[j] = (j % 4 == 0) ? 0.001f : 0.0f;
                packet.magCov[j] = (j % 4 == 0) ? 0.5f : 0.0f;
            }
            packet.accelCalibStatus = 3;
            packet.gyroCalibStatus = 3;
            packet.magCalibStatus = 2;

            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }

        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void gps_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    GPSData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "GPS", 4);
    std::cout << "[GPS] Thread started (" << RATE_GPS << " Hz)..." << std::endl;

    double base_lat = 40.7128;
    double base_lon = -74.0060;
    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_GPS);
        int toSend = (int)(targetPackets - packetsSent);

        if (toSend > 50) toSend = 50;

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_GPS);
            packet.time = t;
            packet.latitude = base_lat + (0.001 * sin(t * 0.2));
            packet.longitude = base_lon + (0.001 * cos(t * 0.2));
            packet.altitude = 100.0f + (float)sin(t) * 5.0f;
            packet.speed = 25.0f;
            packet.heading = (float)fmod(t * 10.0, 360.0);
            packet.numSatellites = 12;
            packet.fixType = 3;
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void battery_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    BatteryData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "BAT", 4);
    std::cout << "[Battery] Thread started (" << RATE_BATTERY << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_BATTERY);
        int toSend = (int)(targetPackets - packetsSent);

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_BATTERY);
            packet.time = t;
            packet.voltage = 12.0f + 0.5f * (float)sin(t * 0.1);
            packet.current = 5.0f + 2.0f * (float)cos(t * 0.15);
            packet.percentage = (uint8_t)(100 - (int)(t / 10.0) % 100);
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void lidar_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    LIDARData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "LID", 4);
    std::cout << "[LIDAR] Thread started (" << RATE_LIDAR << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_LIDAR);
        int toSend = (int)(targetPackets - packetsSent);
        if (toSend > 50) toSend = 50;

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_LIDAR);
            packet.time = t;
            packet.numTracks = 8 + (uint8_t)(sin(t * 0.5) * 4);
            packet.range = 10.0f + 5.0f * (float)sin(t);
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void radar_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    RADARData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "RAD", 4);
    std::cout << "[RADAR] Thread started (" << RATE_RADAR << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_RADAR);
        int toSend = (int)(targetPackets - packetsSent);
        if (toSend > 50) toSend = 50;

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_RADAR);
            packet.time = t;
            packet.targetCount = (uint8_t)(3 + ((int)t % 5));
            packet.range = 50.0f + 20.0f * (float)sin(t * 0.2);
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void state_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    StateData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "STA", 4);
    std::cout << "[State] Thread started (" << RATE_STATE << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_STATE);
        int toSend = (int)(targetPackets - packetsSent);

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_STATE);
            packet.time = t;
            packet.cpuUsage = 50.0f + 20.0f * (float)sin(t * 0.3);
            packet.memoryUsage = 60.0f + 15.0f * (float)cos(t * 0.2);
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void debug_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    DebugData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "DBG", 4);
    std::cout << "[Debug] Thread started (" << RATE_DEBUG << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_DEBUG);
        int toSend = (int)(targetPackets - packetsSent);

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_DEBUG);
            packet.time = t;
            packet.metric = (float)(100.0 * sin(t * 0.5));
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

void motor_thread_func(socket_t sockfd, sockaddr_in dest_addr) {
    MotorData packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.header, "MTR", 4);
    std::cout << "[Motor] Thread started (" << RATE_MOTOR << " Hz)..." << std::endl;

    double startTime = get_time();
    long long packetsSent = 0;

    while (running) {
        double elapsed = get_time() - startTime;
        long long targetPackets = (long long)(elapsed * RATE_MOTOR);
        int toSend = (int)(targetPackets - packetsSent);
        if (toSend > 50) toSend = 50;

        for (int i = 0; i < toSend; i++) {
            double t = (packetsSent + i) * (1.0 / RATE_MOTOR);
            packet.time = t;
            packet.rpm = (int16_t)(3000 + 500 * sin(t * 0.5));
            packet.torque = 50.0f + 10.0f * (float)cos(t * 0.3);
            sendto(sockfd, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        packetsSent += toSend;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
    socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    timeBeginPeriod(1);
#else
    socket_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    if (sockfd < 0) return 1;

    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);

    double bytesPerSecond =
        sizeof(IMUData) * RATE_IMU +
        sizeof(GPSData) * RATE_GPS +
        sizeof(BatteryData) * RATE_BATTERY +
        sizeof(LIDARData) * RATE_LIDAR +
        sizeof(RADARData) * RATE_RADAR +
        sizeof(StateData) * RATE_STATE +
        sizeof(DebugData) * RATE_DEBUG +
        sizeof(MotorData) * RATE_MOTOR;

    double mbitsPerSecond = (bytesPerSecond * 8.0) / 1000000.0;
    std::cout << "Starting Mock Device. Target Rate: " << mbitsPerSecond << " Mb/s" << std::endl;

    std::thread imuThread(imu_thread_func, sockfd, dest_addr);
    std::thread gpsThread(gps_thread_func, sockfd, dest_addr);
    std::thread batteryThread(battery_thread_func, sockfd, dest_addr);
    std::thread lidarThread(lidar_thread_func, sockfd, dest_addr);
    std::thread radarThread(radar_thread_func, sockfd, dest_addr);
    std::thread stateThread(state_thread_func, sockfd, dest_addr);
    std::thread debugThread(debug_thread_func, sockfd, dest_addr);
    std::thread motorThread(motor_thread_func, sockfd, dest_addr);

    imuThread.join(); gpsThread.join(); batteryThread.join();
    lidarThread.join(); radarThread.join(); stateThread.join();
    debugThread.join(); motorThread.join();

#ifdef _WIN32
    closesocket(sockfd); WSACleanup();
    timeEndPeriod(1);
#else
    close(sockfd);
#endif
    return 0;
}