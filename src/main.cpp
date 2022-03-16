#include "Y2KaoZ/Network/Tcp/Http/AcceptorHandler.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/HttpSessionHandler.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/WebSocketSession.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/WebSocketSessions.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/log/trivial.hpp>
#include <gsl/gsl_util>
#include <thread>

class ChatWebSocketSessionHandler : public Y2KaoZ::Network::Tcp::Http::WebSocket::WebSocketSession::Handler {
public:
  using WebSocketSession = Y2KaoZ::Network::Tcp::Http::WebSocket::WebSocketSession;
  void onHandler(gsl::not_null<WebSocketSession*> session, Ptr oldHandler, Ptr newHandler) final {
    BOOST_LOG_TRIVIAL(trace) << "WebSocket session '" << session << "' switched handler from '" << oldHandler
                             << "' to '" << newHandler << "'";
    if (oldHandler.get() == this) {
      sessions_.erase(session);
    }
    if (newHandler.get() == this) {
      sessions_.insert(session);
    }
  }
  auto onError(gsl::not_null<WebSocketSession*> session, const std::string& where, boost::system::error_code ec)
    -> bool final {
    if (ec != boost::asio::error::operation_aborted && ec != boost::beast::websocket::error::closed) {
      BOOST_LOG_TRIVIAL(error) << "WebSocket session '" << session << "' " << where << ": '" << ec.what() << "'";
    }
    return true;
  }
  void onStart(gsl::not_null<WebSocketSession*> session) final {
    if (session->isOpen() && sessions_.insert(session)) {
      BOOST_LOG_TRIVIAL(trace) << "WebSocket session  '" << session << "' started.";
    }
  }
  void onClose(gsl::not_null<WebSocketSession*> session) final {
    BOOST_LOG_TRIVIAL(trace) << "WebSocket session  '" << session << "' closed.";
  }
  void onRead(gsl::not_null<WebSocketSession*> session, const std::string& message) final {
    BOOST_LOG_TRIVIAL(trace) << "WebSocket session  '" << session << "' got a message '" << message << "' from client.";
    send(message);
  }
  void onSend(gsl::not_null<WebSocketSession*> session, const std::string& message) final {
    BOOST_LOG_TRIVIAL(trace) << "Server sent '" << message << "' to WebSocket session '" << session << "'.";
  }
  void onDestroy(gsl::not_null<WebSocketSession*> session) noexcept final {
    BOOST_LOG_TRIVIAL(trace) << "WebSocket session  '" << session << "' destroyed.";
    sessions_.erase(session);
  }

private:
  Y2KaoZ::Network::Tcp::Http::WebSocket::WebSocketSessions sessions_{};
  void send(std::string message) {
    BOOST_LOG_TRIVIAL(trace) << "Server is sending '" << message << "' to all WebSocket sessions.";
    const auto sharedMessage = std::make_shared<const std::string>(std::move(message));
    for (auto const& weakPtr : sessions_.sessions()) {
      if (auto sessionPtr = weakPtr.lock()) {
        sessionPtr->send(sharedMessage);
      }
    }
  }
};

class Options {
public:
  static const int DEFAULT_PORT = 8080;

  Options(int /*argc*/, char** /*argv*/) {
  }
  auto overrideConfig(int /*argc*/, char** /*argv*/) -> bool {
    return true;
  }
  [[nodiscard]] auto cfgFile() const noexcept -> const std::string& {
    return cfgFile_;
  }
  [[nodiscard]] auto docRoot() const -> std::filesystem::path {
    return std::filesystem::canonical(docRoot_);
  }
  [[nodiscard]] auto threads() const -> std::size_t {
    return threads_ > 0 ? threads_ : 1;
  }
  [[nodiscard]] auto endpoint() const -> boost::asio::ip::tcp::endpoint {
    const auto address = boost::asio::ip::make_address(address_);
    return boost::asio::ip::tcp::endpoint{address, gsl::narrow_cast<std::uint16_t>(port_)};
  }

private:
  std::string cfgFile_{};
  std::string address_{"0.0.0.0"};
  int port_{DEFAULT_PORT};
  std::string docRoot_{"."};
  std::size_t threads_{std::thread::hardware_concurrency()};
};

auto main(int argc, char** argv) -> int {
  try {
    Options options(argc, argv);
    if (!options.overrideConfig(argc, argv)) {
      return EXIT_FAILURE;
    }

    boost::asio::io_context ioContext{static_cast<int>(options.threads())};

    boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
    signals.async_wait([&ioContext](auto, auto) { ioContext.stop(); });

    auto endpoint = options.endpoint();

    BOOST_LOG_TRIVIAL(info) << "This server is accepting connections to: '" << endpoint << "' with "
                            << options.threads() << " threads.";

    std::make_shared<Y2KaoZ::Network::Tcp::Acceptor>(
      ioContext,
      endpoint,
      std::make_shared<Y2KaoZ::Network::Tcp::Http::AcceptorHandler>(
        options.docRoot(),
        std::make_shared<Y2KaoZ::Network::Tcp::Http::WebSocket::HttpSessionHandler>(
          std::make_shared<ChatWebSocketSessionHandler>())))
      ->accept();

    std::vector<std::thread> threads;
    threads.reserve(options.threads() - 1);
    for (auto i = options.threads() - 1; i > 0; --i) {
      threads.emplace_back([&] { ioContext.run(); });
    }
    ioContext.run();
    for (auto& thread : threads) {
      thread.join();
    }
    BOOST_LOG_TRIVIAL(info) << "The server is all done.";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(fatal) << "Exception: " << e.what();
    return EXIT_FAILURE;
  }
}
