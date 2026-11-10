cmake_minimum_required(VERSION 3.25)
project(vracer_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

# SwARc (lifter, x87 model, runtime, Win32 HLE) and Davilex-RED (shared
# menu engine): the submodules in external/ when they are checked out
# (git clone --recursive), else sibling checkouts next to this repo.
function(vracer_default_dir var external sibling)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/${external}/CMakeLists.txt"
     OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/${external}/menu/davi_menu.cmake")
    set(${var} "${CMAKE_CURRENT_SOURCE_DIR}/external/${external}" PARENT_SCOPE)
  else()
    set(${var} "${CMAKE_CURRENT_SOURCE_DIR}/../${sibling}" PARENT_SCOPE)
  endif()
endfunction()
vracer_default_dir(_swarc_default SwARc SwARc-dx5)
vracer_default_dir(_davilex_red_default Davilex-RED Davilex-RED)
set(SWARC_DIR "${_swarc_default}" CACHE PATH "SwARc source tree (branch legacy-dx5)")
set(DAVILEX_RED_DIR "${_davilex_red_default}" CACHE PATH "Shared Davilex RED code")

enable_testing()
add_subdirectory("${SWARC_DIR}" swarc)

# spel.dat lifted to C++ in generated/ (recomp_lift, see CLAUDE.md).
recomp_add_lifted(vracer_lifted "${CMAKE_CURRENT_SOURCE_DIR}/generated")
if(TARGET vracer_lifted)
  # src/game: hand-written replacements of lifted functions (lifter.manifest
  # "replace"), one file per original source file; src/red: RED-engine code
  # shared with other Davilex racers (race mod layer).
  file(GLOB VRACER_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp src/red/*.cpp)
  add_executable(vracer src/main.cpp src/gfx_resolution.cpp ${VRACER_GAME_SOURCES} $<TARGET_OBJECTS:vracer_lifted>)
  target_include_directories(vracer PRIVATE src)
  target_link_libraries(vracer PRIVATE recomp_win32 recomp_d3d7 recomp_dinput8 recomp_dsound)
  set_target_properties(vracer PROPERTIES ENABLE_EXPORTS ON)
endif()

set(VRACER_GAME_EXE "" CACHE FILEPATH "spel.dat for EXE-derived tests")
recomp_add_lift_tests(EXE "${VRACER_GAME_EXE}"
  SYMBOLS "${CMAKE_CURRENT_SOURCE_DIR}/symbols"
  MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/lifter.manifest")

# Native menu (replaces MenuData/Menu.exe), see src/menu/.
add_subdirectory(src/menu)
