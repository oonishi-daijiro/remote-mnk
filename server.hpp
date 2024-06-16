#pragma once

#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <span>
#include <system_error>
#include <thread>

#include <WinSock2.h>
#include <bluetoothapis.h>
#include <vector>
#include <ws2bth.h>

#include "app_defines.hpp"
#include "event_emitter.hpp"
#include "socket_util.hpp"
#include "utils.hpp"

using server_callback_data = STR_TYPE_PAIR<
    "data", std::function<void(sockutil::sock &, std::span<std::byte> &)>>;

using server_callback_end =
    STR_TYPE_PAIR<"end", std::function<void(sockutil::sock &)>>;
using error_callback_end =
    STR_TYPE_PAIR<"error", std::function<void(sockutil::sock &, int)>>;
using connect_callback =
    STR_TYPE_PAIR<"connect", std::function<void(sockutil::sock &)>>;

using callbacks_t = TypeMap<server_callback_data, server_callback_end,
                            error_callback_end, connect_callback>;

class bluetooth_server : public event_emitter<callbacks_t, bluetooth_server> {
public:
  bluetooth_server(size_t buffer_size = 256) noexcept(false)
      : socket(new sockutil::sock(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM)),
        buf(buffer_size) {

    socket->bind_sock(bind_address);

    int size = sizeof(SOCKADDR_BTH);
    auto get_sockname_err = getsockname(
        *socket, reinterpret_cast<SOCKADDR *>(&bind_address), &size);

    if (get_sockname_err) {
      std::error_code code(WSAGetLastError(), std::system_category());
      throw std::system_error(code, "Failed to get socket name");
    }

    auto set_service_err =
        WSASetServiceW(&service_query, RNRSERVICE_REGISTER, 0);

    if (set_service_err) {
      std::error_code code(WSAGetLastError(), std::system_category());
      throw std::system_error(code, "Failed to set service");
    }
  }

  [[nodiscard]] std::future<void> listen() noexcept(false) {
    std::promise<void> promise;
    std::future future = promise.get_future();
    std::cout << "listen" << std::endl;
    socket->listen_to(10);

    auto th = std::thread(&bluetooth_server::accept_and_receive, this,
                          std::move(promise));
    th.detach();
    return future;
  }

  void resize_buffer(auto size) { buf.resize(size); }

  void close() {
    std::cout << "close" << std::endl;
    should_continue_to_accept = false;
    std::cout << should_continue_to_accept << std::endl;
  };

private:
  void accept_and_receive(std::promise<void> promise) noexcept {
    std::span sp{buf};

    while (should_continue_to_accept) {
      auto client_socket = socket->accept_connection();
      emit<"connect">(client_socket);

      int last_response = 0;
      for (int res = client_socket.receive(buf, 0); res > 0;
           res = client_socket.receive(buf, 0), last_response = res) {

        emit<"data">(client_socket, sp);

        for (auto &e : buf) {
          e = (std::byte)0;
        }
      }

      if (last_response == 0) {
        emit<"end">(client_socket);
      } else if (last_response == SOCKET_ERROR) {
        emit<"error">(client_socket, WSAGetLastError());
      }
    }
    promise.set_value();
  }

  std::vector<std::byte> buf;
  bool should_continue_to_accept = true;
  std::shared_ptr<sockutil::sock> socket;

  SOCKADDR_BTH bind_address{.addressFamily = AF_BTH, .port = BT_PORT_ANY};

  CSADDR_INFO addr_info = {
      .LocalAddr{
          .lpSockaddr = (LPSOCKADDR)(&bind_address),
          .iSockaddrLength = sizeof(SOCKADDR_BTH),
      },
      .RemoteAddr{.lpSockaddr = (LPSOCKADDR)(&bind_address),
                  .iSockaddrLength = sizeof(SOCKADDR_BTH)},
      .iSocketType = SOCK_STREAM,
      .iProtocol = BTHPROTO_RFCOMM

  };

  WSAQUERYSETW service_query = {
      .dwSize = sizeof(WSAQUERYSETW),
      .lpszServiceInstanceName = const_cast<wchar_t *>(L"Bluetooth remote mnk"),
      .lpServiceClassId = (LPGUID)&bth_service::GUID_service_class,
      .lpszComment = const_cast<wchar_t *>(
          L"A remote mouse and keyboard by using bluetooth SPP."),
      .dwNameSpace = NS_BTH,
      .dwNumberOfCsAddrs = 1,
      .lpcsaBuffer = const_cast<CSADDR_INFO *>(&addr_info),
  };
};
