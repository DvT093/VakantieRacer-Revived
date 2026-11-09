# vracer-menu: native replacement for the Delphi menu (MenuData/Menu.exe).
#   engine    shared with Europe Racer and A2 Racer IV: ../Davilex-RED/menu/engine
#             (DFM loader, graphic buttons, captions ini, assets/mod overrides,
#             4K painter, SDL3 window and audio)
#   launcher  RED-engine race handshake (game.ini + running the race), also
#             shared: ../Davilex-RED/menu/launcher
#   vracer/   Vakantie Racer screens and flow
set(DAVILEX_RED_DIR ${CMAKE_SOURCE_DIR}/../Davilex-RED CACHE PATH "Shared Davilex RED code")
include(${DAVILEX_RED_DIR}/menu/davi_menu.cmake)

file(GLOB VRACER_MENU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vracer/*.cpp)
add_executable(vracer-menu ${VRACER_MENU_SOURCES})
target_link_libraries(vracer-menu PRIVATE davi_menu_launcher)  # brings davi_menu_engine
# "vracer/<x>.h", and src/gfx_settings.h (graphics.ini), shared with the race.
target_include_directories(vracer-menu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(vracer-menu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
