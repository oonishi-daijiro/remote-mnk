#pragma once

#include "utils.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename, typename, bool> class virtual_struct;

template <typename T> struct get_size {
  static constexpr auto value = sizeof(T);
};

template <typename M, typename R, bool UR>
struct get_size<virtual_struct<M, R, UR>> {
  static constexpr auto value = virtual_struct<M, R, UR>::size;
};

template <typename Tpl> struct tuple_element_size;

template <typename... E> struct tuple_element_size<std::tuple<E...>> {
  static constexpr size_t sizes[sizeof...(E) + 1]{0, get_size<E>::value...};
};

template <typename typemap> struct virtual_struct_resolver {

  static constexpr auto get_size() {
    size_t total = 0;
    for (int i = 0; i < std::tuple_size_v<typename typemap::types> + 1; i++) {
      total += tuple_element_size<typename typemap::types>::sizes[i];
    }
    return total;
  }

  template <array_expr K> static constexpr auto get_offset() {
    size_t offset = 0;
    for (int i = 0; i < typemap::template get_index<K> + 1; i++) {
      offset += tuple_element_size<typename typemap::types>::sizes[i];
    }
    return offset;
  }
};

template <typename typemap> struct virtual_union_resolver {

  static constexpr auto get_size() {
    size_t max = 0;
    for (int i = 0; i < std::tuple_size_v<typename typemap::types> + 1; i++) {
      if (tuple_element_size<typename typemap::types>::sizes[i] > max) {
        max = tuple_element_size<typename typemap::types>::sizes[i];
      }
    }
    return max;
  }

  template <array_expr K> static constexpr auto get_offset() { return 0; }
};

template <typename Tpl> struct is_all_trivial_type;

template <typename... T> struct is_all_trivial_type<std::tuple<T...>> {
  static constexpr bool is_trivials[sizeof...(T) + 1]{true,
                                                      std::is_trivial_v<T>...};
  static constexpr auto is_all_trivial() {
    bool init = true;
    for (int i = 0; i < sizeof...(T); i++) {
      init &= is_trivials[i];
    }
    return init;
  }
};

template <typename typemap = TypeMap<>,
          typename Resolver = virtual_struct_resolver<typemap>,
          bool use_ref = false>
class virtual_struct {
public:
  // meta functions
  template <typename T, typename... NT> struct change_parm;

  template <template <typename...> typename T, typename... E, typename... NT>
  struct change_parm<T<E...>, NT...> {
    using type = T<NT...>;
  };

  template <typename T> struct fillter_execlude_virtual_struct {
    static constexpr auto value = true;
  };

  template <typename T, typename R, bool UR>
  struct fillter_execlude_virtual_struct<virtual_struct<T, R, UR>> {
    static constexpr auto value = false;
  };

  using virtual_struct_execluded =
      tuple_utility::fillter<typename typemap::types,
                             fillter_execlude_virtual_struct>::type;

  static_assert(is_all_trivial_type<virtual_struct_execluded>::is_all_trivial(),
                "you can set only virtual_struct/union and trivial type");

  template <typename T>
  static constexpr auto is_virtual_struct =
      !fillter_execlude_virtual_struct<T>::value;

  template <array_expr K> using value_type = typemap::template get_t<K>;

  template <typename SPAN, size_t S> struct reduce_span;

public:
  using map = typemap;
  using resolver = Resolver;

  static constexpr size_t size = resolver::get_size();

  static constexpr std::array<std::byte, resolver::get_size()>
  gen_buffer_array() {
    return std::array<std::byte, size>{};
  }

  template <array_expr K, typename T>
  using add =
      virtual_struct<typename typemap::template add<K, T>,
                     typename change_parm<
                         resolver, typename typemap::template add<K, T>>::type,
                     use_ref>;

  using to_use_ref = virtual_struct<typemap, resolver, true>;

  template <size_t N>
  virtual_struct(std::span<std::byte, N> buf)
    requires(!use_ref)
      : buffer{static_cast<std::span<std::byte>>(buf)} {
    static_assert(N >= size, "no enought buffer length");
  }

  template <size_t N>
  virtual_struct(std::span<std::byte, N> &buf)
    requires(use_ref)
      : buffer{static_cast<std::span<std::byte> &>(buf)} {
    static_assert(N >= size, "no enought buffer length");
  }

  const auto &get_buffer() const { return buffer; }

  template <array_expr K>
  auto get() const -> const value_type<K> &
    requires(!is_virtual_struct<value_type<K>>)
  {
    constexpr auto offset = resolver::template get_offset<K>();
    void *ptr = buffer.data() + offset;
    return *((value_type<K> *)(ptr));
  }

  template <array_expr K>
  auto get() const -> value_type<K>
    requires is_virtual_struct<value_type<K>>
  {
    constexpr auto offset = resolver::template get_offset<K>();
    using internal_map = value_type<K>::map;
    constexpr auto internal_size = value_type<K>::size;
    auto ss = buffer.template subspan<offset, internal_size>();
    return virtual_struct<internal_map, typename value_type<K>::resolver,
                          false>{ss};
  }

  template <array_expr K, typename... T> auto set(T &&...v) {
    void *ptr = buffer.data() + resolver::template get_offset<K>();
    return set_impl<value_type<K>>(ptr, std::forward<T>(v)...);
  }

  void clear() { std::memset(buffer.data(), 0, buffer.size()); }

private:
  template <typename VT, typename... T> void set_impl(void *ptr, T &&...v) {
    new (ptr) VT(std::forward<T>(v)...);
  } // emplace

  template <typename VT, typename T, size_t N>
  void set_impl(void *ptr, T (&p)[N]) {
    std::memcpy(ptr, p, sizeof(T) * N);
  } // array

  using buffer_type =
      std::conditional_t<use_ref, std::span<std::byte> &, std::span<std::byte>>;

  buffer_type buffer;
};

template <typename typemap = TypeMap<>, bool use_ref = false>
using virtual_union =
    virtual_struct<typemap, virtual_union_resolver<typemap>, use_ref>;
