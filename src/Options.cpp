#include "Options.hpp"
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <gsl/gsl_util>
#include <iostream>
#include <stdexcept>

namespace po = boost::program_options;

Options::Options(int argc, char** argv) {
  // See if there is a configuration file in the command line arguments.
  po::options_description options("Initial options.");
  options.add_options()("config,c", po::value<std::string>(&cfgFile_)->default_value(cfgFile_), "configuration file");
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);
  } catch (const std::exception& e) {
    //... do nothing for now ...
  }

  try {
    po::variables_map vm;
    if (cfgFile_.empty()) {
      return;
    }
    if (!std::filesystem::exists(cfgFile_)) {
      throw std::runtime_error("Configuration file '" + cfgFile_ + "' not found.");
    }
    po::options_description cfgOptions("Configuration file options");
    // clang-format off
    cfgOptions.add_options()
      ("network.address", po::value<std::string>(&address_)->default_value(address_), "Server's listening address.")
      ("network.port", po::value<int>(&port_)->default_value(port_), "Server's listening port.")
      ("server.threads", po::value<int>(&threads_)->default_value(threads_), "Server's threads.")
      ("http.appRoot", po::value<std::string>(&appRoot_)->default_value(appRoot_), "Websocket app document root.")
      ("http.docRoot", po::value<std::string>(&docRoot_)->default_value(docRoot_), "Http document root.")
      ;
    // clang-format on
    po::store(po::parse_config_file(cfgFile_.c_str(), cfgOptions), vm);
    po::notify(vm);
  } catch (const boost::program_options::unknown_option& e) {
    throw std::runtime_error("Configuration file '" + cfgFile_ + "' unknown option: " + e.what());
  }
}

auto Options::overrideConfig(int argc, char** argv) -> bool {
  po::variables_map vm;
  po::options_description cmdOptions("Command line options");
  // clang-format off
  cmdOptions.add_options()
    ("help,h", "produce this help message")
    ("config,c", po::value<std::string>(&cfgFile_)->default_value(cfgFile_), "configuration file")
    ("address", po::value<std::string>(&address_)->default_value(address_), "Server's listening address.")
    ("port", po::value<int>(&port_)->default_value(port_), "Server's listening port.")
    ("threads", po::value<int>(&threads_)->default_value(threads_), "Server's threads.")
    ("appRoot", po::value<std::string>(&appRoot_)->default_value(appRoot_), "Websocket app document root.")
    ("docRoot", po::value<std::string>(&docRoot_)->default_value(docRoot_), "Http document root.")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdOptions), vm);
    po::notify(vm);
    if (vm.count("help") != 0U) {
      std::cout << cmdOptions << "\n";
      return false;
    }
    return true;
  } catch (const boost::program_options::unknown_option& e) {
    BOOST_LOG_TRIVIAL(error) << "Command line unknown option: " << e.what();
    return false;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(fatal) << "Command line unknown exception: " << e.what();
    return false;
  }
}

auto Options::cfgFile() const noexcept -> const std::string& {
  return cfgFile_;
}

auto Options::appRoot() const -> std::filesystem::path {
  return std::filesystem::canonical(appRoot_);
}

auto Options::docRoot() const -> std::filesystem::path {
  return std::filesystem::canonical(docRoot_);
}

auto Options::threads() -> std::size_t {
  if (threads_ < 1) {
    threads_ = static_cast<int>(std::thread::hardware_concurrency());
  }
  return threads_ > 0 ? static_cast<std::size_t>(threads_) : 1;
}

auto Options::endpoint() const -> boost::asio::ip::tcp::endpoint {
  const auto address = boost::asio::ip::make_address(address_);
  return boost::asio::ip::tcp::endpoint{address, gsl::narrow_cast<std::uint16_t>(port_)};
}
