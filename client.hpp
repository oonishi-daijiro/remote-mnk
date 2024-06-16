#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <span>
#include <thread>
#include <utility>

#include <WinSock2.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

#include "event_emitter.hpp"
#include "socket_util.hpp"
#include "utils.hpp"

using client_callback_data =
    STR_TYPE_PAIR<"data", std::function<void(std::span<std::byte> &)>>;
using client_callback_end = STR_TYPE_PAIR<"end", std::function<void()>>;
using client_callback_error = STR_TYPE_PAIR<"error", std::function<void()>>;

using client_callbacks =
    TypeMap<client_callback_data, client_callback_end, client_callback_error>;

class bluetooth_client
    : public event_emitter<client_callbacks, bluetooth_client> {
public:
  bluetooth_client(size_t bufsize = 256)
      : socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM), buf(bufsize){};

  [[nodiscard]] std::future<void> connect(SOCKADDR_BTH &address) {
    std::cout << "attempt to connect to server" << std::endl;
    socket.connect_to(address);
    std::promise<void> prms;
    auto ftr = prms.get_future();
    auto th = std::thread(&bluetooth_client::receive, this, std::move(prms));
    th.detach();
    return ftr;
  };

  void close() {
    std::cout << "close" << std::endl;
    should_continue_to_receive = false;
    socket.shutdown_sock(SD_BOTH);
    socket.close();
  }

  template <typename T> void send(T &&data) noexcept(false) {
    socket.send_data(std::forward<T>(data));
  };

  void resize_buffer(auto size) { buf.resize(size); }

private:
  sockutil::sock socket;
  bool should_continue_to_receive = true;
  std::vector<std::byte> buf;

  void receive(std::promise<void> promise) {
    std::span<std::byte> spn{buf};

    while (should_continue_to_receive) {
      auto res = socket.receive(buf, 0);
      if (res > 0) {
        emit<"data">(spn);
      } else if (res == 0) {
        emit<"end">();
        break;
      } else if (res == -1) {
        emit<"error">();
        break;
      }

      for (auto &e : buf) {
        e = (std::byte)0;
      }
    }
    promise.set_value();
  };
};
