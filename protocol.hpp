#pragma once

#include "mnk.hpp"
#include "utils.hpp"
#include "virtual_struct.hpp"
#include <cstdint>

namespace protocol {

struct mouse_protocol_proto { // mouse_protocol
  enum event { move, button, wheel };
  union data { // mouse_protocol_data
    uint16_t move[2];

    struct button { // mouse_button_data
      mouse_button button;
      enum action { down, up };
    };

    struct wheel {
      enum direction { vertical, horizonal };
      int16_t delta_wheel;
    };
  };
};
using mouse_button_data = virtual_struct<>::
    add<"button", decltype(mouse_protocol_proto::data::button::button)>::add<
        "action", mouse_protocol_proto::data::button::action>;
using mouse_wheel_data =
    virtual_struct<>::add<"direction",
                          mouse_protocol_proto::data::wheel::direction>::
        add<"delta-wheel",
            decltype(mouse_protocol_proto::data::wheel::delta_wheel)>;

using mouse_protocol_data =
    virtual_union<>::add<"move", decltype(mouse_protocol_proto::data::move)>::
        add<"button", mouse_button_data>::add<"wheel", mouse_wheel_data>;

using mouse_protocol = virtual_struct<>::add<
    "event", mouse_protocol_proto::event>::add<"data", mouse_protocol_data>;

struct keyboard_protocol_proto {
  enum button_action { down, up };
  int scancode;
  bool is_extended;
};

using keyboard_protocol = virtual_struct<>::add<"scancode", int>::add<
    "is-extended", bool>::add<"action", keyboard_protocol_proto::button_action>;

enum input_type { mouse = 1, keyboard };

using input =
    virtual_union<>::add<"mouse", mouse_protocol>::add<"keyboard",
                                                       keyboard_protocol>;
using remote_mnk_protocol = virtual_struct<>::add<
    "input-type", input_type>::add<"input", input>;

}; // namespace protocol
