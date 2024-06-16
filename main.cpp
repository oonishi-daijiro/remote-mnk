#include <WinSock2.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "app_defines.hpp"
#include "bth.hpp"
#include "client.hpp"
#include "mnk.hpp"
#include "protocol.hpp"
#include "server.hpp"
#include "socket_util.hpp"
#include "utils.hpp"
#include "virtual_struct.hpp"

void runas_client();
void runas_server();

void func(DWORD scancode) { std::cout << "keydown" << std::endl; }

int main(int len, char **argv) noexcept(true) {
  if (len < 2)
    return 1;

  std::string_view mode = argv[1];
  std::cout << "runas :" << mode << std::endl;
  std::setlocale(LC_CTYPE, ".UTF-8");
  std::cout << std::boolalpha;

  WSAData wsa_data;
  auto startup_res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_res) {
    std::cout << "faild to start wsa." << std::endl;
    WSACleanup();
    return 1;
  }

  try {
    if (mode == "server") {
      runas_server();
    } else if (mode == "client") {
      runas_client();
    }

  } catch (std::system_error &err) {
    std::cout << err.what() << ' ' << err.code() << std::endl;
  } catch (std::runtime_error &err) {
    std::cout << "Runtime error." << err.what() << std::endl;
  }

  WSACleanup();
}

// client is emits mnk input to server.
void runas_client() {

  auto ovserve_end = keyboard_observer::start_observe();
  auto mouse_ovserve_end = mouse_observer::start_observe();

  auto all_bth_devices = lookup_bluetooth_socket();

  auto device_name = L"ONISHIDAIJIRO";
  auto bth_socket =
      std::find(all_bth_devices.begin(), all_bth_devices.end(), device_name);
  if (bth_socket == all_bth_devices.end()) {
    throw std::runtime_error("Cannnot to find device");
  }

  bth_socket->socket_address->serviceClassId = bth_service::GUID_service_class;
  bth_socket->socket_address->port = 0;
  bth_socket->socket_address->addressFamily = AF_BTH;

  bluetooth_client client;

  using namespace protocol;
  auto protocol_buffer = remote_mnk_protocol::gen_buffer_array();
  auto buffer_span = std::span{protocol_buffer};

  remote_mnk_protocol rmnk_proto{buffer_span};

  auto mouse_proto = rmnk_proto.get<"input">().get<"mouse">();
  auto keyboard_proto = rmnk_proto.get<"input">().get<"keyboard">();

  auto future_conncction_close = client.connect(*bth_socket->socket_address);

  auto send = [&]() {
    auto start = std::chrono::system_clock::now();
    client.send(protocol_buffer);
    auto end = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                       start)
              << std::endl;
    rmnk_proto.clear();
  };

  mouse_observer::on<"mousemove">([&](normalized_mouse_pos &pos) {
    auto &[x, y] = pos;
    rmnk_proto.set<"input-type">(input_type::mouse);
    mouse_proto.set<"event">(mouse_protocol_proto::event::move);
    mouse_proto.get<"data">().set<"move">(x, y);
    send();
  });

  mouse_observer::on<"mousewheel">([&](auto wheel, auto dw) {
    rmnk_proto.set<"input-type">(input_type::mouse);
    mouse_proto.set<"event">(mouse_protocol_proto::event::wheel);
    using enum mouse_protocol_proto::data::wheel::direction;

    mouse_proto.get<"data">().get<"wheel">().set<"delta-wheel">(dw);
    if (wheel == wheel::horizonal) {
      mouse_proto.get<"data">().get<"wheel">().set<"direction">(horizonal);
    } else if (wheel == wheel::vertical) {
      mouse_proto.get<"data">().get<"wheel">().set<"direction">(vertical);
    }
    send();
  });

  mouse_observer::on<"mousedown">([&](auto button) {
    rmnk_proto.set<"input-type">(input_type::mouse);
    mouse_proto.set<"event">(mouse_protocol_proto::event::button);
    auto button_data = mouse_proto.get<"data">().get<"button">();
    button_data.set<"button">(button);
    button_data.set<"action">(mouse_protocol_proto::data::button::action::down);
    send();
  });

  mouse_observer::on<"mouseup">([&](auto button) {
    rmnk_proto.set<"input-type">(input_type::mouse);
    mouse_proto.set<"event">(mouse_protocol_proto::event::button);
    auto button_data = mouse_proto.get<"data">().get<"button">();
    button_data.set<"button">(button);
    button_data.set<"action">(mouse_protocol_proto::data::button::action::up);
    send();
  });

  keyboard_observer::on<"keydown">([&](auto scancode, auto is_extended_key) {
    rmnk_proto.set<"input-type">(input_type::keyboard);
    keyboard_proto.set<"action">(keyboard_protocol_proto::button_action::down);
    keyboard_proto.set<"scancode">(scancode);
    keyboard_proto.set<"is-extended">(is_extended_key);
    send();
  });

  keyboard_observer::on<"keyup">([&](auto scancode, auto is_extended_key) {
    rmnk_proto.set<"input-type">(input_type::keyboard);
    keyboard_proto.set<"action">(keyboard_protocol_proto::button_action::up);
    keyboard_proto.set<"scancode">(scancode);
    keyboard_proto.set<"is-extended">(is_extended_key);
    send();
  });

  client.on<"data">([](auto data) { std::cout << data.data() << std::endl; });

  client.on<"end">([&]() {
    client.close();
    keyboard_observer::stop_observe();
    mouse_observer::stop_observe();
  });

  client.on<"error">([]() {
    keyboard_observer::stop_observe();
    mouse_observer::stop_observe();
  });

  future_conncction_close.wait();
  ovserve_end.wait();
  mouse_ovserve_end.wait();
};

// server is receive client's input and emulate to own pc.
void runas_server() {

  bluetooth_server bth_server{512};
  using namespace protocol;

  bth_server.on<"data">([&](sockutil::sock &sock, std::span<std::byte> &data) {
    auto start = std::chrono::system_clock::now();
    remote_mnk_protocol rmnk_proto{data};

    auto input_type = rmnk_proto.get<"input-type">();
    auto mouse_proto = rmnk_proto.get<"input">().get<"mouse">();
    auto keyboard_proto = rmnk_proto.get<"input">().get<"keyboard">();

    if (input_type == input_type::mouse) {
      auto event = mouse_proto.get<"event">();
      auto data = mouse_proto.get<"data">();
      using enum mouse_protocol_proto::data::button::action;

      if (event == mouse_protocol_proto::event::button) {
        auto button_data = data.get<"button">();

        if (button_data.get<"action">() == down) {
          mouse_emulator::down(button_data.get<"button">());
        } else if (button_data.get<"action">() == up) {
          mouse_emulator::up(button_data.get<"button">());
        }
      } else if (event == mouse_protocol_proto::event::move) {
        auto &[x, y] = data.get<"move">();
        mouse_emulator::move(x, y);
      } else if (event == mouse_protocol_proto::event::wheel) {
        auto &direction = data.get<"wheel">().get<"direction">();
        auto &dw = data.get<"wheel">().get<"delta-wheel">();
        using enum mouse_protocol_proto::data::wheel::direction;

        if (direction == vertical) {
          mouse_emulator::wheel_v(dw);
        } else if (direction == horizonal) {
          mouse_emulator::wheel_h(dw);
        }
      }
    } else if (input_type == input_type::keyboard) {
      auto &action = keyboard_proto.get<"action">();
      auto &scancode = keyboard_proto.get<"scancode">();
      auto &is_extended = keyboard_proto.get<"is-extended">();

      using enum keyboard_protocol_proto::button_action;
      if (action == down) {
        keyboard_emulator::down(scancode, is_extended);
      } else if (action == up) {
        keyboard_emulator::up(scancode, is_extended);
      }
    }
  });

  bth_server.on<"end">(
      [&](auto &client_socket) { client_socket.shutdown_sock(SD_BOTH); });

  bth_server.on<"error">([](auto &client_socket, auto errorcode) {
    std::cout << "Failed with an error." << WSAGetLastError() << std::endl;
    client_socket.shutdown_sock(SD_BOTH);
  });

  auto server_listen_future = bth_server.listen();
  server_listen_future.wait();
};
