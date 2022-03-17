#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <filesystem>
#include <thread>

class Options {
public:
  static const int DEFAULT_PORT = 8080;
  Options() = default;
  Options(int argc, char** argv);
  auto overrideConfig(int /*argc*/, char** /*argv*/) -> bool;
  [[nodiscard]] auto cfgFile() const noexcept -> const std::string&;
  [[nodiscard]] auto docRoot() const -> std::filesystem::path;
  [[nodiscard]] auto threads() -> std::size_t;
  [[nodiscard]] auto endpoint() const -> boost::asio::ip::tcp::endpoint;

private:
  std::string cfgFile_{};
  std::string address_{"0.0.0.0"};
  int port_{DEFAULT_PORT};
  std::string docRoot_{"./"};
  int threads_{static_cast<int>(std::thread::hardware_concurrency())};
};