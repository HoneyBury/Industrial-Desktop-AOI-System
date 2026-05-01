#pragma once

#include <QElapsedTimer>

#include "infrastructure/Logger.h"

namespace LaserSpc::App {

template <typename Func, typename DetailFunc>
auto executeQueryWithPerf(const QString& label, Func&& func, DetailFunc&& detailFunc) {
    QElapsedTimer timer;
    timer.start();
    auto result = func();
    LaserSpc::Infrastructure::Logger::perf(label, timer.elapsed(), detailFunc(result));
    return result;
}

}  // namespace LaserSpc::App
