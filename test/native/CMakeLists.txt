cmake_minimum_required(VERSION 3.20)

project(jinggua_native_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(jinggua_native_tests
  test_coin.cpp
  test_coin_animation.cpp
  test_divination_flow.cpp
  test_hexagram.cpp
  test_imu.cpp
  test_main.cpp
  test_microphone_research.cpp
  test_ordering.cpp
  test_state_machine.cpp
  test_transform.cpp
  test_yao.cpp
  ../../src/application/divination_session.cpp
  ../../src/application/input_event.cpp
  ../../src/application/state_machine.cpp
  ../../src/data/hexagrams.cpp
  ../../src/data/trigrams.cpp
  ../../src/domain/coin.cpp
  ../../src/domain/divination.cpp
  ../../src/domain/hexagram.cpp
  ../../src/domain/trigram.cpp
  ../../src/domain/yao.cpp
  ../../src/hardware/buttons.cpp
  ../../src/hardware/display.cpp
  ../../src/hardware/font_data.cpp
  ../../src/hardware/imu.cpp
  ../../src/hardware/microphone_research.cpp
  ../../src/hardware/random.cpp
  ../../src/ui/renderer.cpp
  ../../src/ui/screens.cpp
  ../../src/ui/coin_animation.cpp
)

target_include_directories(jinggua_native_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../include
  ${CMAKE_CURRENT_LIST_DIR}
)

if(MSVC)
  target_compile_options(jinggua_native_tests PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(jinggua_native_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
  )
endif()

enable_testing()
add_test(NAME jinggua_native_tests COMMAND jinggua_native_tests)
