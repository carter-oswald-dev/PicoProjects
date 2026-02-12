if(TARGET shared_lcd_driver)
  return()
endif()

add_library(shared_lcd_driver STATIC
  src/LcdDriver.c
)

target_include_directories(shared_lcd_driver
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(shared_lcd_driver
  PUBLIC
    pico_stdlib
    hardware_gpio
    hardware_pwm
    hardware_spi
)

add_library(shared_ui_text STATIC
  src/UiText.cpp
)

target_include_directories(shared_ui_text
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(shared_ui_text
  PUBLIC
    pico_stdlib
)
