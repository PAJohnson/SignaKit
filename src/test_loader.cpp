#include "SignalConfigLoader.h"
#include <cassert>
#include <iostream>

int main() {
  std::vector<PacketDefinition> packets;
  std::vector<SignalDefinition> signals;

  bool result = SignalConfigLoader::Load("signals.yaml", packets, signals);

  if (!result) {
    std::cerr << "Failed to load signals.yaml" << std::endl;
    return 1;
  }

  std::cout << "Loaded " << packets.size() << " packets and " << signals.size()
            << " signals." << std::endl;

  assert(packets.size() >= 2);
  assert(signals.size() >= 10);

  // Verify IMU packet
  bool foundIMU = false;
  for (const auto &pkt : packets) {
    if (pkt.id == "IMU") {
      foundIMU = true;
      assert(pkt.headerString == "IMU");
      // assert(pkt.sizeCheck == 36); // Just checking logic
    }
  }
  assert(foundIMU);

  // Verify Signal
  bool foundAccelX = false;
  for (const auto &sig : signals) {
    if (sig.key == "IMU.accelX") {
      foundAccelX = true;
      assert(sig.packetId == "IMU");
      assert(sig.valueField == "accelX");
    }
  }
  assert(foundAccelX);

  std::cout << "Test Passed!" << std::endl;
  return 0;
}
