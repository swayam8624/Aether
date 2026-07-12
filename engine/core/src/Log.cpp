#include <aether/core/Log.hpp>

#include <iostream>

namespace aether {
namespace {
const char* label(LogLevel level) {
    switch (level) {
    case LogLevel::debug:
        return "DEBUG";
    case LogLevel::info:
        return "INFO";
    case LogLevel::warning:
        return "WARN";
    case LogLevel::error:
        return "ERROR";
    }
    return "UNKNOWN";
}
} // namespace

Log& Log::instance() {
    static Log log;
    return log;
}

Log::Log()
    : sink_([](LogLevel level, std::string_view message) {
          std::clog << "[AETHER][" << label(level) << "] " << message << '\n';
      }) {}

void Log::setSink(Sink sink) {
    std::scoped_lock lock(mutex_);
    sink_ = std::move(sink);
}

void Log::write(LogLevel level, std::string_view message) const {
    std::scoped_lock lock(mutex_);
    sink_(level, message);
}
} // namespace aether
