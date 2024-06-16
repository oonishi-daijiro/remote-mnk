cmake_minimum_required(VERSION 3.26)
project(remote_mnk)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CXX_STANDARD C++20)
if(MSVC)
   add_compile_options(/std:c++20)
endif()


add_executable(remote_mnk main.cpp)
target_link_libraries(remote_mnk ws2_32.lib Bthprops user32.lib gdi32.lib)
