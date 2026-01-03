#pragma once

#include "DataSink.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include <mutex>

class UDPDataSink : public DataSink
{
private:
    std::string ip;
    int port;
    SOCKET sockfd;
    char buffer[1024];

    // Optional logging
    std::ofstream* logFile;
    std::mutex* logFileMutex;

    // Optional packet callback (for Lua scripts)
    std::function<void(const std::string&)> packetCallback;

public:
    UDPDataSink(std::map<std::string, Signal>& registry,
                std::vector<PacketDefinition>& packetDefs,
                const std::string& ipAddress,
                int portNumber,
                std::ofstream* logFilePtr = nullptr,
                std::mutex* logMutex = nullptr)
        : DataSink(registry, packetDefs)
        , ip(ipAddress)
        , port(portNumber)
        , sockfd(INVALID_SOCKET)
        , logFile(logFilePtr)
        , logFileMutex(logMutex)
        , packetCallback(nullptr)
    {
    }

    // Set callback to be invoked when a packet is parsed
    void setPacketCallback(std::function<void(const std::string&)> callback) {
        packetCallback = callback;
    }

    ~UDPDataSink() override {
        close();
    }

    bool open() override {
        // Create socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == INVALID_SOCKET) {
            printf("UDPDataSink: Failed to create socket\n");
            return false;
        }

        // Set socket to non-blocking mode
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);

        // Setup address
        sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);

        // Convert IP address
        if (ip == "localhost" || ip == "127.0.0.1") {
            servaddr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr);
        }

        // Bind socket
        if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            printf("UDPDataSink: Failed to bind socket to %s:%d\n", ip.c_str(), port);
            closesocket(sockfd);
            sockfd = INVALID_SOCKET;
            return false;
        }

        printf("UDPDataSink: Connected to %s:%d\n", ip.c_str(), port);
        return true;
    }

    bool step() override {
        if (!isOpen()) {
            return false;
        }

        // Attempt to receive a packet
        int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);

        if (len == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // No data available
                return false;
            } else {
                // Real error occurred
                printf("UDPDataSink: Socket error: %d\n", error);
                return false;
            }
        }

        if (len <= 0) {
            // Connection closed
            return false;
        }

        // Log raw packet to file if logging is enabled
        if (logFile && logFileMutex) {
            std::lock_guard<std::mutex> lock(*logFileMutex);
            if (logFile->is_open()) {
                logFile->write(buffer, len);
            }
        }

        // Find matching packet by header string
        for (const PacketDefinition& pkt : packets) {
            if (strncmp(buffer, pkt.headerString.c_str(),
                        pkt.headerString.length()) == 0) {

                // Matched packet! Process all signals in this packet
                for (const auto &sig : pkt.signals) {
                    double t = ReadValue(buffer, sig.timeType, sig.timeOffset);
                    double v = ReadValue(buffer, sig.type, sig.offset);
                    signalRegistry[sig.key].AddPoint(t, v);
                }

                // Invoke Lua callback if registered
                if (packetCallback) {
                    packetCallback(pkt.id);
                }

                // Break after finding the matching packet type
                break;
            }
        }

        return true;
    }

    void close() override {
        if (sockfd != INVALID_SOCKET) {
            closesocket(sockfd);
            sockfd = INVALID_SOCKET;
            printf("UDPDataSink: Disconnected\n");
        }
    }

    bool isOpen() const override {
        return sockfd != INVALID_SOCKET;
    }

    // Update connection parameters (must be closed first)
    void setConnectionParams(const std::string& ipAddress, int portNumber) {
        ip = ipAddress;
        port = portNumber;
    }
};