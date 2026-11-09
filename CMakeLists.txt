cmake_minimum_required(VERSION 3.25)
project(vracer_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

# SwARc (lifter, x87 model, runtime, Win32 HLE), sibling checkout.
set(SWARC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SwARc" CACHE PATH "SwARc source tree")

enable_testing()
add_subdirectory("${SWARC_DIR}" swarc)

# spel.dat lifted to C++ in generated/ (recomp_lift, see CLAUDE.md).
recomp_add_lifted(vracer_lifted "${CMAKE_CURRENT_SOURCE_DIR}/generated")
if(TARGET vracer_lifted)
  add_executable(vracer src/main.cpp src/gfx_resolution.cpp $<TARGET_OBJECTS:vracer_lifted>)
  target_link_libraries(vracer PRIVATE recomp_win32 recomp_d3d7 recomp_dinput8 recomp_dsound)
  set_target_properties(vracer PROPERTIES ENABLE_EXPORTS ON)
endif()

set(VRACER_GAME_EXE "" CACHE FILEPATH "spel.dat for EXE-derived tests")
recomp_add_lift_tests(EXE "${VRACER_GAME_EXE}"
  SYMBOLS "${CMAKE_CURRENT_SOURCE_DIR}/symbols"
  MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/lifter.manifest")

# Native menu (replaces MenuData/Menu.exe), see src/menu/.
add_subdirectory(src/menu)
