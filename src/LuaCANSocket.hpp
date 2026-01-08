#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <tuple>
#include <cstdio>

// Include official PCAN-Basic headers (downloaded by build system)
#include "PCANBasic.h"

// Lua-accessible CAN Socket using PCAN-Basic API
class LuaCANSocket {
private:
    // Function pointers for runtime DLL loading
    typedef TPCANStatus (*CAN_Initialize_t)(TPCANHandle, TPCANBaudrate, TPCANType, DWORD, WORD);
    typedef TPCANStatus (*CAN_Uninitialize_t)(TPCANHandle);
    typedef TPCANStatus (*CAN_Read_t)(TPCANHandle, TPCANMsg*, TPCANTimestamp*);
    typedef TPCANStatus (*CAN_Write_t)(TPCANHandle, TPCANMsg*);
    typedef TPCANStatus (*CAN_GetStatus_t)(TPCANHandle);
    
    static HMODULE pcan_module;
    static CAN_Initialize_t CAN_Initialize_Func;
    static CAN_Uninitialize_t CAN_Uninitialize_Func;
    static CAN_Read_t CAN_Read_Func;
    static CAN_Write_t CAN_Write_Func;
    static CAN_GetStatus_t CAN_GetStatus_Func;
    static bool pcan_loaded;
    
    TPCANHandle channel = PCAN_NONEBUS;
    bool is_open_flag = false;
    
    // Runtime loading of PCAN-Basic DLL
    static bool LoadPCANLibrary() {
        if (pcan_loaded) return pcan_module != nullptr;
        
        pcan_module = LoadLibraryA("PCANBasic.dll");
        if (!pcan_module) {
            printf("[CANSocket] PCANBasic.dll not found. Install PCAN drivers from peak-system.com\n");
            pcan_loaded = true;
            return false;
        }
        
        CAN_Initialize_Func = (CAN_Initialize_t)GetProcAddress(pcan_module, "CAN_Initialize");
        CAN_Uninitialize_Func = (CAN_Uninitialize_t)GetProcAddress(pcan_module, "CAN_Uninitialize");
        CAN_Read_Func = (CAN_Read_t)GetProcAddress(pcan_module, "CAN_Read");
        CAN_Write_Func = (CAN_Write_t)GetProcAddress(pcan_module, "CAN_Write");
        CAN_GetStatus_Func = (CAN_GetStatus_t)GetProcAddress(pcan_module, "CAN_GetStatus");
        
        if (!CAN_Initialize_Func || !CAN_Uninitialize_Func || !CAN_Read_Func || 
            !CAN_Write_Func || !CAN_GetStatus_Func) {
            printf("[CANSocket] Failed to load PCAN functions\n");
            FreeLibrary(pcan_module);
            pcan_module = nullptr;
            pcan_loaded = true;
            return false;
        }
        
        printf("[CANSocket] PCAN-Basic loaded successfully\n");
        pcan_loaded = true;
        return true;
    }
    
    TPCANBaudrate BaudrateFromInt(int baud) {
        switch (baud) {
            case 1000000: return PCAN_BAUD_1M;
            case 800000: return PCAN_BAUD_800K;
            case 500000: return PCAN_BAUD_500K;
            case 250000: return PCAN_BAUD_250K;
            case 125000: return PCAN_BAUD_125K;
            case 100000: return PCAN_BAUD_100K;
            case 50000: return PCAN_BAUD_50K;
            case 20000: return PCAN_BAUD_20K;
            case 10000: return PCAN_BAUD_10K;
            default: return PCAN_BAUD_500K;
        }
    }
    
public:
    LuaCANSocket() = default;
    
    ~LuaCANSocket() {
        close();
    }
    
    // Open CAN channel
    bool open(const std::string& channel_name, int baudrate = 500000) {
        if (is_open_flag) {
            printf("[CANSocket] Channel already open\n");
            return false;
        }
        
        if (!LoadPCANLibrary()) {
            return false;
        }
        
        // Parse channel name
        if (channel_name == "PCAN_USBBUS1") channel = PCAN_USBBUS1;
        else if (channel_name == "PCAN_USBBUS2") channel = PCAN_USBBUS2;
        else if (channel_name == "PCAN_USBBUS3") channel = PCAN_USBBUS3;
        else if (channel_name == "PCAN_USBBUS4") channel = PCAN_USBBUS4;
        else if (channel_name == "PCAN_USBBUS5") channel = PCAN_USBBUS5;
        else if (channel_name == "PCAN_USBBUS6") channel = PCAN_USBBUS6;
        else if (channel_name == "PCAN_USBBUS7") channel = PCAN_USBBUS7;
        else if (channel_name == "PCAN_USBBUS8") channel = PCAN_USBBUS8;
        else {
            printf("[CANSocket] Unknown channel: %s\n", channel_name.c_str());
            return false;
        }
        
        TPCANBaudrate baud = BaudrateFromInt(baudrate);
        TPCANStatus result = CAN_Initialize_Func(channel, baud, 0, 0, 0);
        
        if (result != PCAN_ERROR_OK) {
            printf("[CANSocket] Failed to initialize channel %s: error 0x%X\n", 
                   channel_name.c_str(), result);
            return false;
        }
        
        is_open_flag = true;
        printf("[CANSocket] Opened %s at %d bps\n", channel_name.c_str(), baudrate);
        return true;
    }
    
    // Send CAN frame
    std::tuple<bool, std::string> send(int can_id, const std::string& data, 
                                       bool extended = false, bool rtr = false) {
        if (!is_open_flag) {
            return std::make_tuple(false, "not open");
        }
        
        if (data.size() > 8) {
            return std::make_tuple(false, "data too long");
        }
        
        TPCANMsg msg;
        msg.ID = can_id;
        msg.MSGTYPE = PCAN_MESSAGE_STANDARD;
        if (extended) msg.MSGTYPE |= PCAN_MESSAGE_EXTENDED;
        if (rtr) msg.MSGTYPE |= PCAN_MESSAGE_RTR;
        msg.LEN = data.size();
        
        for (size_t i = 0; i < data.size(); i++) {
            msg.DATA[i] = (BYTE)data[i];
        }
        
        TPCANStatus result = CAN_Write_Func(channel, &msg);
        
        if (result != PCAN_ERROR_OK) {
            char errorMsg[64];
            snprintf(errorMsg, sizeof(errorMsg), "write error 0x%X", result);
            return std::make_tuple(false, std::string(errorMsg));
        }
        
        return std::make_tuple(true, "");
    }
    
    // Receive CAN frame (non-blocking)
    std::tuple<double, int, std::string, bool, bool, std::string> receive() {
        if (!is_open_flag) {
            return std::make_tuple(0.0, 0, "", false, false, "not open");
        }
        
        TPCANMsg msg;
        TPCANTimestamp timestamp;
        
        TPCANStatus result = CAN_Read_Func(channel, &msg, &timestamp);
        
        if (result == PCAN_ERROR_QRCVEMPTY) {
            return std::make_tuple(0.0, 0, "", false, false, "timeout");
        }
        
        if (result != PCAN_ERROR_OK) {
            char errorMsg[64];
            snprintf(errorMsg, sizeof(errorMsg), "read error 0x%X", result);
            return std::make_tuple(0.0, 0, "", false, false, std::string(errorMsg));
        }
        
        // Convert timestamp to milliseconds
        double ts = timestamp.millis + (timestamp.millis_overflow * 4294967.296) + 
                   (timestamp.micros / 1000.0);
        
        std::string data_str((char*)msg.DATA, msg.LEN);
        bool ext = (msg.MSGTYPE & PCAN_MESSAGE_EXTENDED) != 0;
        bool rtr_flag = (msg.MSGTYPE & PCAN_MESSAGE_RTR) != 0;
        
        return std::make_tuple(ts, (int)msg.ID, data_str, ext, rtr_flag, "");
    }
    
    // Get CAN bus status
    std::string get_status() {
        if (!is_open_flag) {
            return "not open";
        }
        
        TPCANStatus status = CAN_GetStatus_Func(channel);
        
        if (status == PCAN_ERROR_OK) return "ok";
        if (status == PCAN_ERROR_BUSOFF) return "bus-off";
        
        char statusStr[32];
        snprintf(statusStr, sizeof(statusStr), "error 0x%X", status);
        return std::string(statusStr);
    }
    
    // Close CAN channel
    void close() {
        if (is_open_flag && pcan_module) {
            CAN_Uninitialize_Func(channel);
            channel = PCAN_NONEBUS;
            is_open_flag = false;
            printf("[CANSocket] Channel closed\n");
        }
    }
    
    // Check if channel is open
    bool is_open() const {
        return is_open_flag;
    }
};

// Static member initialization
HMODULE LuaCANSocket::pcan_module = nullptr;
LuaCANSocket::CAN_Initialize_t LuaCANSocket::CAN_Initialize_Func = nullptr;
LuaCANSocket::CAN_Uninitialize_t LuaCANSocket::CAN_Uninitialize_Func = nullptr;
LuaCANSocket::CAN_Read_t LuaCANSocket::CAN_Read_Func = nullptr;
LuaCANSocket::CAN_Write_t LuaCANSocket::CAN_Write_Func = nullptr;
LuaCANSocket::CAN_GetStatus_t LuaCANSocket::CAN_GetStatus_Func = nullptr;
bool LuaCANSocket::pcan_loaded = false;

#endif // _WIN32
