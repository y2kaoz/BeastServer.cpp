#include "Y2KaoZ/Network/Tcp/Http/AcceptorHandler.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/HttpSessionHandler.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/WebSocketSession.hpp"
#include "Y2KaoZ/Network/Tcp/Http/WebSocket/WebSocketSessions.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/log/trivial.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
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

auto main(int /*argc*/, char** /*argv*/) -> int {
  try {
  const std::uint16_t PORT = 8080;

  std::size_t numThreads = std::thread::hardware_concurrency();
  boost::asio::io_context ioContext{static_cast<int>(numThreads > 0 ? numThreads : 1)};

  boost::asio::signal_set signals(ioContext, SIGINT, SIGTERM);
  signals.async_wait([&ioContext](auto, auto) { ioContext.stop(); });

  auto endpoint = boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address("0.0.0.0"), PORT};

      BOOST_LOG_TRIVIAL(info) << "This server is accepting connections to: '" << endpoint << "' with "
                            << numThreads << " threads.";

  std::make_shared<Y2KaoZ::Network::Tcp::Acceptor>(
    ioContext,
    endpoint,
    std::make_shared<Y2KaoZ::Network::Tcp::Http::AcceptorHandler>(
      ".",
      std::make_shared<Y2KaoZ::Network::Tcp::Http::WebSocket::HttpSessionHandler>(
        std::make_shared<ChatWebSocketSessionHandler>())))
    ->accept();

  std::vector<std::thread> threads;
  threads.reserve(numThreads - 1);
  for (auto i = numThreads - 1; i > 0; --i) {
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
