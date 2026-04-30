#pragma once

#include "common/Result.h"

#include <string>

class CodeReader {
public:
  Result<std::string> readQrCode(const std::string &imagePath) const;
};

