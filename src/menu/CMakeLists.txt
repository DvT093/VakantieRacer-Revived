# vracer-menu: native replacement for the Delphi menu (MenuData/Menu.exe).
#   engine/   generic Delphi-form menu engine (DFM loader, graphic buttons,
#             captions ini, assets/mod overrides, SDL3 window and audio)
#   launcher/ RED-engine race handshake (game.ini + running the race)
#   vracer/   Vakantie Racer screens and flow
find_package(SDL3 REQUIRED CONFIG)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MENU_FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)

add_library(davi_menu STATIC
  engine/dfm.cpp engine/ini.cpp engine/text.cpp engine/assets.cpp
  engine/form.cpp engine/app.cpp engine/video.cpp
  launcher/race_launcher.cpp)
target_include_directories(davi_menu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(davi_menu PUBLIC SDL3::SDL3 Freetype::Freetype PkgConfig::MENU_FFMPEG)
if(APPLE)
  target_link_libraries(davi_menu PUBLIC objc)  # app.cpp: activation handover to the race
endif()

file(GLOB VRACER_MENU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vracer/*.cpp)
add_executable(vracer-menu ${VRACER_MENU_SOURCES})
target_link_libraries(vracer-menu PRIVATE davi_menu)
# src/gfx_settings.h (graphics.ini), shared with the race.
target_include_directories(vracer-menu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(vracer-menu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
