#include <iostream>
#include <cstddef>
#include "telemetry_defs.h"

int main() {
    std::cout << "=== Struct Size and Offset Analysis ===" << std::endl << std::endl;

    // IMU
    std::cout << "IMUData:" << std::endl;
    std::cout << "  Total size: " << sizeof(IMUData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(IMUData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(IMUData, time) << std::endl;
    std::cout << "  accelX offset: " << offsetof(IMUData, accelX) << std::endl;
    std::cout << "  accelY offset: " << offsetof(IMUData, accelY) << std::endl;
    std::cout << "  accelZ offset: " << offsetof(IMUData, accelZ) << std::endl;
    std::cout << "  gyroX offset: " << offsetof(IMUData, gyroX) << std::endl;
    std::cout << "  gyroY offset: " << offsetof(IMUData, gyroY) << std::endl;
    std::cout << "  gyroZ offset: " << offsetof(IMUData, gyroZ) << std::endl << std::endl;

    // GPS
    std::cout << "GPSData:" << std::endl;
    std::cout << "  Total size: " << sizeof(GPSData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(GPSData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(GPSData, time) << std::endl;
    std::cout << "  latitude offset: " << offsetof(GPSData, latitude) << std::endl;
    std::cout << "  longitude offset: " << offsetof(GPSData, longitude) << std::endl;
    std::cout << "  altitude offset: " << offsetof(GPSData, altitude) << std::endl;
    std::cout << "  speed offset: " << offsetof(GPSData, speed) << std::endl << std::endl;

    // Battery
    std::cout << "BatteryData:" << std::endl;
    std::cout << "  Total size: " << sizeof(BatteryData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(BatteryData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(BatteryData, time) << std::endl;
    std::cout << "  voltage offset: " << offsetof(BatteryData, voltage) << std::endl;
    std::cout << "  current offset: " << offsetof(BatteryData, current) << std::endl;
    std::cout << "  temperature offset: " << offsetof(BatteryData, temperature) << std::endl;
    std::cout << "  percentage offset: " << offsetof(BatteryData, percentage) << std::endl;
    std::cout << "  health offset: " << offsetof(BatteryData, health) << std::endl;
    std::cout << "  cycleCount offset: " << offsetof(BatteryData, cycleCount) << std::endl << std::endl;

    // LIDAR
    std::cout << "LIDARData:" << std::endl;
    std::cout << "  Total size: " << sizeof(LIDARData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(LIDARData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(LIDARData, time) << std::endl;
    std::cout << "  range offset: " << offsetof(LIDARData, range) << std::endl;
    std::cout << "  intensity offset: " << offsetof(LIDARData, intensity) << std::endl;
    std::cout << "  angleX offset: " << offsetof(LIDARData, angleX) << std::endl;
    std::cout << "  angleY offset: " << offsetof(LIDARData, angleY) << std::endl;
    std::cout << "  quality offset: " << offsetof(LIDARData, quality) << std::endl;
    std::cout << "  flags offset: " << offsetof(LIDARData, flags) << std::endl << std::endl;

    // RADAR
    std::cout << "RADARData:" << std::endl;
    std::cout << "  Total size: " << sizeof(RADARData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(RADARData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(RADARData, time) << std::endl;
    std::cout << "  range offset: " << offsetof(RADARData, range) << std::endl;
    std::cout << "  velocity offset: " << offsetof(RADARData, velocity) << std::endl;
    std::cout << "  azimuth offset: " << offsetof(RADARData, azimuth) << std::endl;
    std::cout << "  elevation offset: " << offsetof(RADARData, elevation) << std::endl;
    std::cout << "  signalStrength offset: " << offsetof(RADARData, signalStrength) << std::endl;
    std::cout << "  targetCount offset: " << offsetof(RADARData, targetCount) << std::endl;
    std::cout << "  trackID offset: " << offsetof(RADARData, trackID) << std::endl << std::endl;

    // State
    std::cout << "StateData:" << std::endl;
    std::cout << "  Total size: " << sizeof(StateData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(StateData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(StateData, time) << std::endl;
    std::cout << "  systemMode offset: " << offsetof(StateData, systemMode) << std::endl;
    std::cout << "  armed offset: " << offsetof(StateData, armed) << std::endl;
    std::cout << "  statusFlags offset: " << offsetof(StateData, statusFlags) << std::endl;
    std::cout << "  errorCode offset: " << offsetof(StateData, errorCode) << std::endl;
    std::cout << "  uptime offset: " << offsetof(StateData, uptime) << std::endl;
    std::cout << "  cpuUsage offset: " << offsetof(StateData, cpuUsage) << std::endl;
    std::cout << "  memoryUsage offset: " << offsetof(StateData, memoryUsage) << std::endl << std::endl;

    // Debug
    std::cout << "DebugData:" << std::endl;
    std::cout << "  Total size: " << sizeof(DebugData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(DebugData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(DebugData, time) << std::endl;
    std::cout << "  counter offset: " << offsetof(DebugData, counter) << std::endl;
    std::cout << "  eventID offset: " << offsetof(DebugData, eventID) << std::endl;
    std::cout << "  priority offset: " << offsetof(DebugData, priority) << std::endl;
    std::cout << "  subsystem offset: " << offsetof(DebugData, subsystem) << std::endl;
    std::cout << "  value1 offset: " << offsetof(DebugData, value1) << std::endl;
    std::cout << "  value2 offset: " << offsetof(DebugData, value2) << std::endl;
    std::cout << "  metric offset: " << offsetof(DebugData, metric) << std::endl << std::endl;

    // Motor
    std::cout << "MotorData:" << std::endl;
    std::cout << "  Total size: " << sizeof(MotorData) << " bytes" << std::endl;
    std::cout << "  header offset: " << offsetof(MotorData, header) << std::endl;
    std::cout << "  time offset: " << offsetof(MotorData, time) << std::endl;
    std::cout << "  rpm offset: " << offsetof(MotorData, rpm) << std::endl;
    std::cout << "  torque offset: " << offsetof(MotorData, torque) << std::endl;
    std::cout << "  power offset: " << offsetof(MotorData, power) << std::endl;
    std::cout << "  temperature offset: " << offsetof(MotorData, temperature) << std::endl;
    std::cout << "  throttle offset: " << offsetof(MotorData, throttle) << std::endl;
    std::cout << "  faults offset: " << offsetof(MotorData, faults) << std::endl;
    std::cout << "  totalRotations offset: " << offsetof(MotorData, totalRotations) << std::endl << std::endl;

    return 0;
}
