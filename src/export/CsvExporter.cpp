#include "export/CsvExporter.h"

#include <fstream>
#include <sstream>

std::string CsvExporter::escapeCsvField(const std::string &field) {
  if (field.find(',') == std::string::npos &&
      field.find('"') == std::string::npos &&
      field.find('\n') == std::string::npos) {
    return field;
  }

  std::string escaped = field;
  // Double any embedded double-quotes.
  for (size_t i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '"') {
      escaped.insert(i, "\"");
      ++i;
    }
  }

  return '"' + escaped + '"';
}

int CsvExporter::exportInspectionRecords(const std::string &filePath,
                                         const std::vector<InspectionRecord> &records) {
  std::ofstream file(filePath, std::ios::out | std::ios::trunc);
  if (!file.is_open()) {
    return -1;
  }

  // Header row
  file << "Board ID,Timestamp,Program,Decision,AI Label,Confidence,Marks,ROIs,Image Path\n";

  for (const auto &r : records) {
    file << escapeCsvField(r.boardId) << ','
         << escapeCsvField(r.timestamp) << ','
         << escapeCsvField(r.programName) << ','
         << escapeCsvField(r.finalDecision) << ','
         << escapeCsvField(r.aiLabel) << ','
         << r.aiConfidence << ','
         << r.marksCount << ','
         << r.roisCount << ','
         << escapeCsvField(r.imagePath) << '\n';
  }

  file.close();
  return static_cast<int>(records.size());
}
