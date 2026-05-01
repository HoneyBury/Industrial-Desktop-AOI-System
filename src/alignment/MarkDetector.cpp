#include "alignment/MarkDetector.h"

namespace alignment {

Result<std::vector<MarkPoint>> MarkDetector::detect(const std::string &imagePath,
                                                    const MarkAlgorithm preferredAlgorithm) const {
  ::MarkDetector detector;
  if (preferredAlgorithm == MarkAlgorithm::BinaryGeometry) {
    return detector.detectCircularMarks(imagePath);
  }

  return detector.detectTemplateMarks(imagePath);
}

} // namespace alignment
