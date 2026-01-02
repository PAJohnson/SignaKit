#include "SignalConfigLoader.h"
#include <cassert>
#include <iostream>

int main() {
  std::vector<PacketDefinition> packets;

  bool result = SignalConfigLoader::Load("signals.yaml", packets);

  if (!result) {
    std::cerr << "Failed to load signals.yaml" << std::endl;
    return 1;
  }

  // Count total signals across all packets
  size_t totalSignals = 0;
  for (const auto &pkt : packets) {
    totalSignals += pkt.signals.size();
  }

  std::cout << "Loaded " << packets.size() << " packets and " << totalSignals
            << " signals." << std::endl;

  assert(packets.size() >= 2);
  assert(totalSignals >= 10);

  // Verify IMU packet
  bool foundIMU = false;
  for (const auto &pkt : packets) {
    if (pkt.id == "IMU") {
      foundIMU = true;
      assert(pkt.headerString == "IMU");
      assert(pkt.signals.size() == 6); // accelX, accelY, accelZ, gyroX, gyroY, gyroZ

      std::cout << "IMU packet has " << pkt.signals.size() << " signals:" << std::endl;
      for (const auto &sig : pkt.signals) {
        std::cout << "  - " << sig.key << " (type: " << sig.type
                  << ", offset: " << sig.offset << ")" << std::endl;
      }
    }
  }
  assert(foundIMU);

  // Verify IMU.accelX signal
  bool foundAccelX = false;
  for (const auto &pkt : packets) {
    if (pkt.id == "IMU") {
      for (const auto &sig : pkt.signals) {
        if (sig.key == "IMU.accelX") {
          foundAccelX = true;
          assert(sig.name == "accelX");
          assert(sig.type == "float");
          assert(sig.offset == 12);
          assert(sig.timeOffset == 4);
          assert(sig.timeType == "double");
        }
      }
    }
  }
  assert(foundAccelX);

  std::cout << "Test Passed!" << std::endl;
  return 0;
}
