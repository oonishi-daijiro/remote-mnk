#pragma once

#include <WinSock2.h>

#include <cstddef>
#include <iostream>
#include <iterator>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace sockutil {

inline std::system_error create_std_error_of_WSA(std::string error_message) {
  std::error_code code(WSAGetLastError(), std::system_category());
  return {code, error_message};
};

class socket_err {
public:
  socket_err(int err = 0) : err(err) {}

  socket_err(const socket_err &src) { this->err = src.err; };
  socket_err &operator=(const socket_err &src) {
    this->err = src.err;
    return *this;
  };

  socket_err(socket_err &&src) {
    src.err = 0;
    this->err = src.err;
  };

  socket_err &operator=(socket_err &&rv) {
    rv.err = 0;
    this->err = rv.err;
    return *this;
  };

  socket_err &operator=(int err) {
    this->err = err;
    return *this;
  }

  constexpr int check(std::string error_message = "") noexcept(false) {
    if (err == SOCKET_ERROR) {
      throw create_std_error_of_WSA(error_message);
    }
    return err;
  };

private:
  int err;
};

class sock {
private:
  SOCKET socket_handle = INVALID_SOCKET;
  socket_err err;

public:
  sock(int af, int type, int protocol) noexcept(false) {
    socket_handle = socket(af, type, protocol);
    if (socket_handle == INVALID_SOCKET) {
      throw create_std_error_of_WSA("Failed to create socket");
    }
  }

  sock(SOCKET handle) noexcept(false) : socket_handle(handle) {

    if (handle == INVALID_SOCKET) {
      throw create_std_error_of_WSA("Failed to create socket");
    }
  }

  sock() : socket_handle(INVALID_SOCKET){};

  ~sock() {
    if (socket_handle != INVALID_SOCKET) {
      closesocket(socket_handle);
    }
  }

  sock(const sock &) = delete;
  sock &operator=(const sock &) = delete;

  sock(sock &&src) {
    this->socket_handle = src.socket_handle;
    src.socket_handle = INVALID_SOCKET;
  }

  sock &operator=(sock &&src) {
    this->socket_handle = src.socket_handle;
    src.socket_handle = INVALID_SOCKET;
    return *this;
  }

  sock &connect_to(auto &address) noexcept(false) {
    err = connect(socket_handle, reinterpret_cast<sockaddr *>(&address),
                  sizeof(address));
    err.check("Failed to connect");
    return *this;
  }

  template <typename T> auto send_data(T &&p, int flags = 0) noexcept {
    auto span = std::span{std::forward<T>(p)};
    auto data = (char *)span.data();
    auto size = span.size();
    return send(socket_handle, data, size, flags);
  }

  sock &bind_sock(auto &address) noexcept(false) {
    err = bind(socket_handle, reinterpret_cast<sockaddr *>(&address),
               sizeof(address));
    err.check("Failed to bind");
    return *this;
  }

  sock &shutdown_sock(int how) noexcept(false) {
    err = shutdown(socket_handle, how);
    err.check("Failed to shutdown");
    return *this;
  }

  sock &listen_to(int backlog) noexcept(false) {
    err = listen(socket_handle, backlog);
    err.check("Failed to listen");
    return *this;
  }
  sock &accept_connection(struct sockaddr &address,
                          int *addrlen = NULL) noexcept(false) {
    err = accept(socket_handle, &address, addrlen);
    err.check("Failder to accept connection");
    return *this;
  }

  sock accept_connection() {
    SOCKET client_socket = accept(socket_handle, NULL, NULL);
    return {client_socket};
  }

  int receive(auto &buffer, int flags) noexcept {
    auto p = std::begin(buffer);
    static_assert(std::contiguous_iterator<decltype(p)>,
                  "please set contiguous_iterator.");
    auto size = std::size(buffer);
    return recv(socket_handle, (char *)&(*p), size, flags);
  }

  int receive(sock &socket, byte *ptr, int size, int flags) noexcept {
    return recv(socket, (char *)(ptr), size, flags);
  }

  void close() {
    closesocket(socket_handle);
    socket_handle = INVALID_SOCKET;
  }

  operator const SOCKET() const { return socket_handle; }
};

} // namespace sockutil
