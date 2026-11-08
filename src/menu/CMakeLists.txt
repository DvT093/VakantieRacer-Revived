# vracer-menu: native replacement for the Delphi menu (MenuData/Menu.exe).
#   engine/   generic Delphi-form menu engine (DFM loader, graphic buttons,
#             captions ini, assets/mod overrides, SDL3 window and audio)
#   launcher/ RED-engine race handshake (game.ini + running the race)
#   vracer/   Vakantie Racer screens and flow
find_package(SDL3 REQUIRED CONFIG)
find_package(Freetype REQUIRED)

add_library(davi_menu STATIC
  engine/dfm.cpp engine/ini.cpp engine/text.cpp engine/assets.cpp
  engine/form.cpp engine/app.cpp
  launcher/race_launcher.cpp)
target_include_directories(davi_menu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(davi_menu PUBLIC SDL3::SDL3 Freetype::Freetype)

file(GLOB VRACER_MENU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vracer/*.cpp)
add_executable(vracer-menu ${VRACER_MENU_SOURCES})
target_link_libraries(vracer-menu PRIVATE davi_menu)
set_target_properties(vracer-menu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
