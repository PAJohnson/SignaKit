#include <iostream>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../include/telemetry_defs.h"

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

void imu_thread_func(int sockfd, sockaddr_in dest_addr) {
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

        sendto(sockfd, &packet, sizeof(packet), 0, 
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // ~100Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void gps_thread_func(int sockfd, sockaddr_in dest_addr) {
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

        sendto(sockfd, &packet, sizeof(packet), 0, 
               (struct sockaddr*)&dest_addr, sizeof(dest_addr));

        // ~10Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    // Setup UDP Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &dest_addr.sin_addr);

    std::cout << "Starting Mock Device. Sending to " << DEST_IP << ":" << DEST_PORT << std::endl;
    std::cout << "Press Ctrl+C to stop (or kill the process)." << std::endl;

    // Start threads
    std::thread imuThread(imu_thread_func, sockfd, dest_addr);
    std::thread gpsThread(gps_thread_func, sockfd, dest_addr);

    imuThread.join();
    gpsThread.join();

    close(sockfd);
    return 0;
}