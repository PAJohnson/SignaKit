#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <vector>
#include <tuple>
#include <cstdio>

// Tier 5: Lua-accessible Serial Port wrapper using Win32 API (Windows only)
class LuaSerialPort {
private:
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    bool is_open_flag = false;

public:
    LuaSerialPort() = default;

    ~LuaSerialPort() {
        close();
    }

    // Open serial port
    // port_name: COM port name (e.g., "COM1", "COM3", "\\\\.\\COM10" for ports >= 10)
    // baud_rate: Baud rate (e.g., 9600, 115200)
    bool open(const std::string& port_name, int baud_rate = 9600) {
        if (is_open_flag) {
            printf("[LuaSerialPort] Port already open\\n");
            return false;
        }

        // Handle COM port naming for ports >= 10
        std::string portPath = port_name;
        if (port_name.find("\\\\.\\") == std::string::npos && 
            port_name.find("COM") == 0) {
            // Extract port number
            int portNum = 0;
            try {
                portNum = std::stoi(port_name.substr(3));
                if (portNum >= 10) {
                    portPath = "\\\\.\\" + port_name;
                }
            } catch (...) {
                // Keep original name if parsing fails
            }
        }

        // Open the serial port
        hSerial = CreateFileA(
            portPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,      // No sharing
            NULL,   // No security
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL    // No template
        );

        if (hSerial == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            printf("[LuaSerialPort] Failed to open %s: Error %lu\\n", 
                   portPath.c_str(), error);
            return false;
        }

        // Configure the port with default settings
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            printf("[LuaSerialPort] Failed to get comm state\\n");
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        dcbSerialParams.BaudRate = baud_rate;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            printf("[LuaSerialPort] Failed to set comm state\\n");
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        // Set timeouts for non-blocking mode (return immediately)
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            printf("[LuaSerialPort] Failed to set timeouts\\n");
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        is_open_flag = true;
        printf("[LuaSerialPort] Opened %s at %d baud\\n", portPath.c_str(), baud_rate);
        return true;
    }

    // Configure serial port parameters
    bool configure(int baud_rate, const std::string& parity, int stop_bits, int byte_size = 8) {
        if (!is_open_flag) {
            printf("[LuaSerialPort] Port not open\\n");
            return false;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            printf("[LuaSerialPort] Failed to get comm state\\n");
            return false;
        }

        dcbSerialParams.BaudRate = baud_rate;
        dcbSerialParams.ByteSize = byte_size;

        // Parse parity
        if (parity == "none" || parity == "N") {
            dcbSerialParams.Parity = NOPARITY;
        } else if (parity == "odd" || parity == "O") {
            dcbSerialParams.Parity = ODDPARITY;
        } else if (parity == "even" || parity == "E") {
            dcbSerialParams.Parity = EVENPARITY;
        } else if (parity == "mark" || parity == "M") {
            dcbSerialParams.Parity = MARKPARITY;
        } else if (parity == "space" || parity == "S") {
            dcbSerialParams.Parity = SPACEPARITY;
        } else {
            printf("[LuaSerialPort] Invalid parity: %s\\n", parity.c_str());
            return false;
        }

        // Parse stop bits
        if (stop_bits == 1) {
            dcbSerialParams.StopBits = ONESTOPBIT;
        } else if (stop_bits == 2) {
            dcbSerialParams.StopBits = TWOSTOPBITS;
        } else {
            printf("[LuaSerialPort] Invalid stop bits: %d\\n", stop_bits);
            return false;
        }

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            printf("[LuaSerialPort] Failed to set comm state\\n");
            return false;
        }

        printf("[LuaSerialPort] Configured: %d baud, %s parity, %d stop bits, %d byte size\\n",
               baud_rate, parity.c_str(), stop_bits, byte_size);
        return true;
    }

    // Send data
    std::tuple<int, std::string> send(const std::string& data) {
        if (!is_open_flag) {
            return std::make_tuple(-1, "not open");
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hSerial, data.data(), data.size(), &bytesWritten, NULL)) {
            DWORD error = GetLastError();
            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "write error %lu", error);
            return std::make_tuple(-1, std::string(errorMsg));
        }

        return std::make_tuple((int)bytesWritten, "");
    }

    // Receive data (non-blocking)
    std::tuple<std::string, std::string> receive(int max_size) {
        if (!is_open_flag) {
            return std::make_tuple("", "not open");
        }

        std::vector<char> buffer(max_size);
        DWORD bytesRead = 0;

        if (!ReadFile(hSerial, buffer.data(), buffer.size(), &bytesRead, NULL)) {
            DWORD error = GetLastError();
            char errorMsg[256];
            snprintf(errorMsg, sizeof(errorMsg), "read error %lu", error);
            return std::make_tuple("", std::string(errorMsg));
        }

        if (bytesRead > 0) {
            return std::make_tuple(std::string(buffer.data(), bytesRead), "");
        } else {
            // No data available (non-blocking)
            return std::make_tuple("", "timeout");
        }
    }

    // Close serial port
    void close() {
        if (is_open_flag && hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            is_open_flag = false;
            printf("[LuaSerialPort] Port closed\\n");
        }
    }

    // Check if port is open
    bool is_open() const {
        return is_open_flag;
    }
};
#endif // _WIN32
