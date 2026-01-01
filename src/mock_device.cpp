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
    // Ensure header is set (in case of compiler quirks)
    memcpy(packet.header, "IMU", 4); 

    std::cout << "[IMU] Thread started (100Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        
        packet.time = t;
        // Generate interesting waveform data
        packet.accelX = (float)sin(t);
        packet.accelY = (float)cos(t);
        packet.accelZ = (float)(sin(t * 0.5) * 2.0f);
        
        packet.gyroX = (float)((double)rand() / RAND_MAX); // Noise
        packet.gyroY = 0.0f;
        packet.gyroZ = 0.0f;

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // ~100Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

#ifdef _WIN32
void gps_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void gps_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    GPSData packet;
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

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // ~10Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#ifdef _WIN32
void battery_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void battery_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    BatteryData packet;
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

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

#ifdef _WIN32
void lidar_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void lidar_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    LIDARData packet;
    memcpy(packet.header, "LID", 4);
    std::cout << "[LIDAR] Thread started (20Hz)..." << std::endl;

    while (running) {
        double t = get_time();
        packet.time = t;
        packet.range = 10.0f + 5.0f * (float)sin(t);
        packet.intensity = 50.0f + 30.0f * (float)cos(t * 2);
        packet.angleX = (int16_t)(1000 * sin(t * 0.3));  // 0.01 degree units
        packet.angleY = (int16_t)(500 * cos(t * 0.3));
        packet.quality = (uint8_t)(80 + 15 * sin(t));
        packet.flags = (uint8_t)(t) % 4;

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

#ifdef _WIN32
void radar_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void radar_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    RADARData packet;
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
        packet.targetCount = (uint8_t)(1 + ((int)t % 5));
        packet.trackID = (uint8_t)((int)t % 256);

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#ifdef _WIN32
void state_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void state_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    StateData packet;
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

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

#ifdef _WIN32
void debug_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void debug_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    DebugData packet;
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

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

#ifdef _WIN32
void motor_thread_func(SOCKET sockfd, sockaddr_in dest_addr) {
#else
void motor_thread_func(int sockfd, sockaddr_in dest_addr) {
#endif
    MotorData packet;
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

        sendto(sockfd, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    std::cout << "\nTransmitting 8 packet types:" << std::endl;
    std::cout << "  IMU (100Hz), GPS (10Hz), Battery (1Hz), LIDAR (20Hz)," << std::endl;
    std::cout << "  RADAR (10Hz), State (2Hz), Debug (5Hz), Motor (10Hz)" << std::endl;

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