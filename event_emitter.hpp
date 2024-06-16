#pragma once

#include "utils.hpp"

#include <functional>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

template <typename MAP, class EMIT_PROVIDER = void> class event_emitter {
public:
  event_emitter() = default;

  friend EMIT_PROVIDER;

  template <array_expr KEY> void on(MAP::template get_t<KEY> &&func) {
    constexpr auto index = MAP::template get_index<KEY>;
    constexpr std::string_view s = KEY.buf;
    if (s == "data") {
      std::cout << "sus" << std::endl;
    }
    get_callbacks<KEY>().push_back(
        std::forward<MAP::template get_t<KEY>>(func));
  };

protected:
  template <array_expr KEY, typename... T> void emit(T &&...arg) {
    for (auto &callback : get_callbacks<KEY>()) {
      callback(arg...);
    }
  };

private:
  tuple_utility::transform<typename MAP::types, std::vector>::type callbacks =
      {};

  template <typename callback> struct callback_arg_type_impl;
  template <typename... arg>
  struct callback_arg_type_impl<std::function<void(arg...)>> {
    using type = std::tuple<arg...>;
  };
  template <typename T>
  using callback_arg_type = callback_arg_type_impl<T>::type;

  template <typename T> using apply_ref = T &;

  // <void(int),void(float),void(double)> -> <tpl<int>,tpl<float>,tpl<double>>

  template <array_expr KEY>
  constexpr std::vector<typename MAP::template get_t<KEY>> &get_callbacks() {
    constexpr auto index = MAP::template get_index<KEY>;
    return std::get<index>(callbacks);
  }
};
