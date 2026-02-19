cmake_minimum_required(VERSION 3.16)

project(PicoProjectsSuperbuild NONE)

set(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR})

add_custom_target(build_creepy_halloween_sonar
  COMMAND ${REPO_ROOT}/scripts/build.sh --project creepy_halloween_sonar --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(build_pico2w_lcd_soundboard
  COMMAND ${REPO_ROOT}/scripts/build.sh --project pico2w_lcd_soundboard --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(build_pico_lcd_generative_art
  COMMAND ${REPO_ROOT}/scripts/build.sh --project pico_lcd_generative_art --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(build_pico_am_radio
  COMMAND ${REPO_ROOT}/scripts/build.sh --project pico_am_radio --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(build_kitchen_wall_clock
  COMMAND ${REPO_ROOT}/scripts/build.sh --project kitchen_wall_clock --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(build_all
  COMMAND ${REPO_ROOT}/scripts/build.sh --all --no-flash
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)

add_custom_target(verify_all
  COMMAND ${REPO_ROOT}/scripts/verify.sh --all
  WORKING_DIRECTORY ${REPO_ROOT}
  USES_TERMINAL
)
