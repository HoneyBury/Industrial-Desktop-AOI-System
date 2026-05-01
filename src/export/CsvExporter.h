#pragma once

#include "database/DatabaseManager.h"

#include <string>
#include <vector>

class CsvExporter {
public:
  // Writes inspection records to a CSV file. Returns the number of rows
  // written (excluding the header), or -1 on failure.
  static int exportInspectionRecords(const std::string &filePath,
                                     const std::vector<InspectionRecord> &records);

private:
  static std::string escapeCsvField(const std::string &field);
};
