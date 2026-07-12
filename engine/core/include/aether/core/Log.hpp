#pragma once

#include <functional>
#include <mutex>
#include <string_view>

namespace aether {

enum class LogLevel { debug, info, warning, error };

class Log final {
  public:
    using Sink = std::function<void(LogLevel, std::string_view)>;

    static Log& instance();
    void setSink(Sink sink);
    void write(LogLevel level, std::string_view message) const;

  private:
    Log();
    mutable std::mutex mutex_;
    Sink sink_;
};

} // namespace aether
