cmake_minimum_required (VERSION 3.5)

project(IO-Multiplexin)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  #set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

add_executable(server_select server_select.cpp)
add_executable(client client.cpp)
add_executable(server_poll server_poll.cpp)
add_executable(server_epoll server_epoll.cpp)