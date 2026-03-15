#include "file_target.hpp"

namespace lumina::common::logger {

FileTarget::FileTarget(
    lumina::platform::common::PlatformServices &platform_services,
    const std::string &file_path)
    : platform_services_(platform_services), file_handle_(nullptr) {
  // Open file in append mode using platform services
  if (platform_services_.LuminaCreateFile != nullptr) {
    file_handle_ = platform_services_.LuminaCreateFile(file_path.c_str());
  }
}

FileTarget::~FileTarget() {
  // Close file handle if it's valid
  if (file_handle_ != nullptr &&
      platform_services_.LuminaCloseFile != nullptr) {
    platform_services_.LuminaCloseFile(file_handle_);
    file_handle_ = nullptr;
  }
}

auto FileTarget::Write([[maybe_unused]] LogLevel level,
                       const std::string &message) -> void {
  if (file_handle_ == nullptr ||
      platform_services_.LuminaWriteFile == nullptr) {
    return;
  }

  // Thread-safe write using mutex
  std::lock_guard<std::mutex> lock(write_mutex_);
  platform_services_.LuminaWriteFile(file_handle_, message.c_str(),
                                     message.length());
}

} // namespace lumina::common::logger
