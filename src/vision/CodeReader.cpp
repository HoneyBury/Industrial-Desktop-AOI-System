#include "vision/CodeReader.h"

Result<std::string> CodeReader::readQrCode(const std::string &imagePath) const {
  return Result<std::string>::success("DEMO-CODE-001", "Stub QR reader completed for " + imagePath);
}

