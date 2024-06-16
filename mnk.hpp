#pragma once

#include <Windows.h>
#include <array>
#include <chrono>
#include <climits>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "event_emitter.hpp"
#include "utils.hpp"

struct normalized_mouse_pos {
  unsigned short dx;
  unsigned short dy;
};
enum mouse_button {
  left_button,
  right_button,
  middle_button,
  x_button_1,
  x_button_2,
  x_buttons = -1 // this is not supporsed to be used.
};

template <auto HOOK_ID, typename callback_map, typename INHERITER>

struct observer {
public:
  observer() = delete;

  static inline std::future<void> start_observe() {
    std::lock_guard lock{mutex_thread_id};

    std::promise<void> prms;
    std::future ftr = prms.get_future();

    auto th = std::thread(set_hook, std::move(prms));
    msgque_tid = GetThreadId(th.native_handle());

    th.detach();
    return ftr;
  };

  static inline void stop_observe() {
    std::lock_guard lock{mutex_thread_id};
    auto res = PostThreadMessage(msgque_tid, WM_QUIT, 0, 0);
    UnhookWindowsHookEx(hook_handle);
  };

  template <array_expr EVENT>
  static inline void on(typename callback_map::template get_t<EVENT> &&func) {
    emitter.on<EVENT>(std::forward<decltype(func)>(func));
  }

protected:
  static inline HHOOK hook_handle;
  static inline event_emitter<callback_map, INHERITER> emitter;

  static inline std::mutex mutex_thread_id;
  static inline DWORD msgque_tid = 0;

  static inline void set_hook(std::promise<void> promise) {
    hook_handle = SetWindowsHookEx(HOOK_ID, INHERITER::hook, NULL, 0);

    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    HANDLE hThreadInitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    SetEvent(hThreadInitEvent);

    while (GetMessage(&msg, NULL, 0, 0) != 0) {
    }
    promise.set_value();
    return;
  }
};

using callback_keydown =
    STR_TYPE_PAIR<"keydown", std::function<void(DWORD, bool)>>;
using callback_keyup = STR_TYPE_PAIR<"keyup", std::function<void(DWORD, bool)>>;

using keyevents_callbacks = TypeMap<callback_keydown, callback_keyup>;

struct keyboard_observer
    : public observer<WH_KEYBOARD_LL, keyevents_callbacks, keyboard_observer> {
public:
  static inline LRESULT __stdcall hook(int nCode, WPARAM wParam,
                                       LPARAM lParam) {
    auto key = (PKBDLLHOOKSTRUCT)lParam;
    if (match(wParam, WM_KEYDOWN, WM_SYSKEYDOWN)) {
      emitter.emit<"keydown">(key->scanCode,
                              ((key->flags) & (KF_EXTENDED >> 8)));
    } else if (match(wParam, WM_KEYUP, WM_SYSKEYUP)) {
      emitter.emit<"keyup">(key->scanCode, ((key->flags) & (KF_EXTENDED >> 8)));
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  };
};

using callback_mousemove =
    STR_TYPE_PAIR<"mousemove", std::function<void(normalized_mouse_pos &)>>;

enum wheel { vertical, horizonal };

using callback_mousewheel =
    STR_TYPE_PAIR<"mousewheel", std::function<void(wheel, short)>>;

using callback_mousedown =
    STR_TYPE_PAIR<"mousedown", std::function<void(mouse_button)>>;
using callback_mouseup =
    STR_TYPE_PAIR<"mouseup", std::function<void(mouse_button)>>;

using mouseevents_callbacks = TypeMap<callback_mousemove, callback_mousewheel,
                                      callback_mousedown, callback_mouseup>;
struct mouse_observer
    : public observer<WH_MOUSE_LL, mouseevents_callbacks, mouse_observer> {
  static inline LRESULT __stdcall hook(int nCode, WPARAM wParam,
                                       LPARAM lParam) {

    const auto mouse_struct = reinterpret_cast<MOUSEHOOKSTRUCT *>(lParam);
    const auto msg = reinterpret_cast<MSG *>(lParam);

#define EXPAND_ALL_MOUSE_EVENT(EVENT)                                          \
  WM_LBUTTON##EVENT, WM_RBUTTON##EVENT, WM_XBUTTON##EVENT, WM_MBUTTON##EVENT

    if (wParam == WM_MOUSEMOVE) {
      emitter.emit<"mousemove">(
          normalize_mouse_pos(mouse_struct->pt.x, mouse_struct->pt.y));

    } else if (wParam == WM_MOUSEWHEEL) {
      emitter.emit<"mousewheel">(wheel::vertical,
                                 GET_WHEEL_DELTA_WPARAM(msg->wParam));
    } else if (wParam == WM_MOUSEHWHEEL) {
      emitter.emit<"mousewheel">(wheel::horizonal,
                                 GET_WHEEL_DELTA_WPARAM(msg->wParam));
    } else if (match(wParam, EXPAND_ALL_MOUSE_EVENT(DOWN)) ||
               match(wParam, EXPAND_ALL_MOUSE_EVENT(UP))) {

      int mousebutton = get_mousebutton_from_wparm(wParam);
      if (mousebutton == x_buttons) {
        mousebutton = (GET_XBUTTON_WPARAM(msg->wParam) == XBUTTON1)
                          ? x_button_1
                          : x_button_2;
      }

      if (match(wParam, EXPAND_ALL_MOUSE_EVENT(DOWN))) {
        emitter.emit<"mousedown">(static_cast<mouse_button>(mousebutton));
      } else if (match(wParam, EXPAND_ALL_MOUSE_EVENT(UP))) {
        emitter.emit<"mouseup">(static_cast<mouse_button>(mousebutton));
      }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
  };

private:
  struct desktop_resolution {
    unsigned short width;
    unsigned short height;

    desktop_resolution() {
      DEVMODE mode;
      EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &mode);
      width = mode.dmPelsWidth;
      height = mode.dmPelsHeight;
    }
  };

  static inline desktop_resolution res{};

  static inline normalized_mouse_pos normalize_mouse_pos(LONG x, LONG y) {
    x = x <= 0 ? 0 : x;
    y = y <= 0 ? 0 : y;
    auto &[width, height] = res;
    x = x > width ? width : x;
    y = y > height ? height : y;

    float normalized_x = (float)x / (float)width;
    float normalized_y = (float)y / (float)height;

    unsigned short normalized_dx = USHRT_MAX * normalized_x;
    unsigned short normalized_dy = USHRT_MAX * normalized_y;

    return {normalized_dx, normalized_dy};
  }

  static inline mouse_button get_mousebutton_from_wparm(WPARAM parm) {

#define EXPAND_ALL_BUTTON_EVENT(BUTTON)                                        \
  WM_##BUTTON##DOWN, WM_##BUTTON##UP, WM_##BUTTON##DBLCLK

    using enum mouse_button;

    if (match(parm, EXPAND_ALL_BUTTON_EVENT(LBUTTON))) {
      return left_button;
    } else if (match(parm, EXPAND_ALL_BUTTON_EVENT(RBUTTON))) {
      return right_button;
    } else if (match(parm, EXPAND_ALL_BUTTON_EVENT(MBUTTON))) {
      return middle_button;
    } else if (match(parm, EXPAND_ALL_BUTTON_EVENT(XBUTTON))) {
      return x_buttons;
    } else {
      return left_button;
    }
  };
};

struct mouse_emulator {

  static inline void move(unsigned short dx, unsigned short dy) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;

    SendInput(1, &input, sizeof(INPUT));
  };

  static inline void down(mouse_button button) {
    using enum mouse_button;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

    switch (button) {
    case left_button:
      input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      break;
    case right_button:
      input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
      break;
    case middle_button:
      input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
      break;
    case x_button_1:
      input.mi.dwFlags = MOUSEEVENTF_XDOWN;
      input.mi.mouseData = XBUTTON1;
      break;
    case x_button_2:
      input.mi.dwFlags = MOUSEEVENTF_XUP;
      input.mi.mouseData = XBUTTON2;
      break;
    case x_buttons: // never
      break;
    }

    SendInput(1, &input, sizeof(input));
  };

  static inline void up(mouse_button button) {
    using enum mouse_button;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

    switch (button) {
    case left_button:
      input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
      break;
    case right_button:
      input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
      break;
    case middle_button:
      input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
      break;
    case x_button_1:
      input.mi.dwFlags |= MOUSEEVENTF_XUP;
      input.mi.mouseData = XBUTTON1;
      break;
    case x_button_2:
      input.mi.dwFlags |= MOUSEEVENTF_XUP;
      input.mi.mouseData = XBUTTON2;
      break;
    case x_buttons: // never
      break;
    }
    SendInput(1, &input, sizeof(input));
  };

  static inline void wheel_v(int dw) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL | MOUSEEVENTF_ABSOLUTE;
    input.mi.mouseData = dw;
    SendInput(1, &input, sizeof(input));
  };

  static inline void wheel_h(int dw) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL | MOUSEEVENTF_ABSOLUTE;
    input.mi.mouseData = dw;
    SendInput(1, &input, sizeof(input));
  };
};

struct keyboard_emulator {
  static inline void down(int scancode, bool is_extended) {
    INPUT input{};
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (is_extended) {
      input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    input.type = INPUT_KEYBOARD;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;
    input.ki.wVk = 0;
    input.ki.wScan = scancode;
    SendInput(1, &input, sizeof(input));
  };

  static inline void up(int scancode, bool is_extended) {
    INPUT input{};
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    if (is_extended) {
      input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    input.type = INPUT_KEYBOARD;
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;
    input.ki.wVk = 0;
    input.ki.wScan = scancode;
    SendInput(1, &input, sizeof(input));
  };
};
