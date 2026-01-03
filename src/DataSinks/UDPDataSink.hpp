#pragma once

#include "DataSink.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include <mutex>

// Helper to read a value from the buffer based on type
// Supports all stdint.h types plus legacy C types for backwards compatibility
// All values are cast to double for storage in the signal buffers
double ReadValue(const char *buffer, const std::string &type, int offset);

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
    {
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

// Helper to read a value from the buffer based on type
// Supports all stdint.h types plus legacy C types for backwards compatibility
// All values are cast to double for storage in the signal buffers
inline double ReadValue(const char *buffer, const std::string &type, int offset) {
  // Floating point types
  if (type == "double") {
    double val;
    memcpy(&val, buffer + offset, sizeof(double));
    return val;
  } else if (type == "float") {
    float val;
    memcpy(&val, buffer + offset, sizeof(float));
    return (double)val;
  }
  // Signed integer types
  else if (type == "int8_t" || type == "int8") {
    int8_t val;
    memcpy(&val, buffer + offset, sizeof(int8_t));
    return (double)val;
  } else if (type == "int16_t" || type == "int16") {
    int16_t val;
    memcpy(&val, buffer + offset, sizeof(int16_t));
    return (double)val;
  } else if (type == "int32_t" || type == "int32" || type == "int") {
    int32_t val;
    memcpy(&val, buffer + offset, sizeof(int32_t));
    return (double)val;
  } else if (type == "int64_t" || type == "int64") {
    int64_t val;
    memcpy(&val, buffer + offset, sizeof(int64_t));
    return (double)val;
  }
  // Unsigned integer types
  else if (type == "uint8_t" || type == "uint8") {
    uint8_t val;
    memcpy(&val, buffer + offset, sizeof(uint8_t));
    return (double)val;
  } else if (type == "uint16_t" || type == "uint16") {
    uint16_t val;
    memcpy(&val, buffer + offset, sizeof(uint16_t));
    return (double)val;
  } else if (type == "uint32_t" || type == "uint32") {
    uint32_t val;
    memcpy(&val, buffer + offset, sizeof(uint32_t));
    return (double)val;
  } else if (type == "uint64_t" || type == "uint64") {
    uint64_t val;
    memcpy(&val, buffer + offset, sizeof(uint64_t));
    return (double)val;
  }
  // Standard C types (for backwards compatibility)
  else if (type == "char") {
    char val;
    memcpy(&val, buffer + offset, sizeof(char));
    return (double)val;
  } else if (type == "short") {
    short val;
    memcpy(&val, buffer + offset, sizeof(short));
    return (double)val;
  } else if (type == "long") {
    long val;
    memcpy(&val, buffer + offset, sizeof(long));
    return (double)val;
  } else if (type == "unsigned char") {
    unsigned char val;
    memcpy(&val, buffer + offset, sizeof(unsigned char));
    return (double)val;
  } else if (type == "unsigned short") {
    unsigned short val;
    memcpy(&val, buffer + offset, sizeof(unsigned short));
    return (double)val;
  } else if (type == "unsigned int") {
    unsigned int val;
    memcpy(&val, buffer + offset, sizeof(unsigned int));
    return (double)val;
  } else if (type == "unsigned long") {
    unsigned long val;
    memcpy(&val, buffer + offset, sizeof(unsigned long));
    return (double)val;
  }

  // Unknown type - return 0 and warn
  fprintf(stderr, "Warning: Unknown field type '%s'\n", type.c_str());
  return 0.0;
}
