#include "lumina_terminate.hpp"
#include "logger/logger.hpp"

namespace lumina::common {
auto FlushLoggerIfReady() noexcept -> void {
  // Guard in case terminate is called before logger is initialized
  if (logger::Logger::IsInitialized()) {
    logger::Logger::Instance().Flush();
  }
}
} // namespace lumina::common
