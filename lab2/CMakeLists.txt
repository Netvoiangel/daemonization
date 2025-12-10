cmake_minimum_required(VERSION 3.16)
project(lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror -pedantic)

find_package(Threads REQUIRED)

set(COMMON_SOURCES
  src/common.cpp
  src/ipc_utils.cpp
  src/logging.cpp
  src/wolf_game.cpp
)

set(COMMON_LIBS Threads::Threads)

if(UNIX AND NOT APPLE)
  list(APPEND COMMON_LIBS rt)
endif()

file(GLOB CONN_SOURCES "src/conn_*.cpp")

if(NOT CONN_SOURCES)
  message(FATAL_ERROR "No connection sources (conn_*.cpp) found")
endif()

foreach(CONN_SRC IN LISTS CONN_SOURCES)
  get_filename_component(CONN_BASENAME "${CONN_SRC}" NAME_WE)
  string(REPLACE "conn_" "" TYPE_CODE "${CONN_BASENAME}")

  add_executable(host_${TYPE_CODE} src/host.cpp ${COMMON_SOURCES} "${CONN_SRC}")
  target_compile_definitions(host_${TYPE_CODE} PRIVATE TYPE_CODE="${TYPE_CODE}")
  target_include_directories(host_${TYPE_CODE} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(host_${TYPE_CODE} PRIVATE ${COMMON_LIBS})

  add_executable(client_${TYPE_CODE} src/client.cpp ${COMMON_SOURCES} "${CONN_SRC}")
  target_compile_definitions(client_${TYPE_CODE} PRIVATE TYPE_CODE="${TYPE_CODE}")
  target_include_directories(client_${TYPE_CODE} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(client_${TYPE_CODE} PRIVATE ${COMMON_LIBS})
endforeach()


