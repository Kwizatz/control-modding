cmake_minimum_required(VERSION 3.21)
project(control-modding)
set(CMAKE_CXX_STANDARD 17)

find_package (Python3 COMPONENTS Interpreter)

get_filename_component(PYTHON_PATH "${Python3_EXECUTABLE}" DIRECTORY)
find_program(GDB_EXECUTABLE gdb PATHS ENV MSYSTEM_PREFIX ENV ProgramFiles PATH_SUFFIXES bin usr/bin)
find_program(7Z_EXECUTABLE 7z PATHS ENV MSYSTEM_PREFIX ENV ProgramFiles PATH_SUFFIXES bin usr/bin)
find_program(BASH_EXECUTABLE bash HINTS ENV MSYSTEM_PREFIX)
find_program(AUTOPEP8_EXECUTABLE autopep8 HINTS ENV MSYSTEM_PREFIX "${PYTHON_PATH}/Scripts")

set(DEBUG_PATH "${CMAKE_BINARY_DIR}/bin")
if(CMAKE_GENERATOR MATCHES "(MSYS|MinGW) Makefiles")
  set(DEBUG_PATH "${DEBUG_PATH};$ENV{MSYSTEM_PREFIX}/bin")
  set(USE_EXTERNAL_CONSOLE "true")
  else()
  set(DEBUG_PATH "$ENV{PATH}:${DEBUG_PATH}")
  set(USE_EXTERNAL_CONSOLE "false")
  endif()

add_subdirectory(tool)

if(7Z_EXECUTABLE)
  add_custom_target(generate-blender-release
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/control-modding-blender-addon.zip
  COMMAND ${7Z_EXECUTABLE} a ${CMAKE_SOURCE_DIR}/control-modding-blender-addon.zip -mx=9 *.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/blender/addons/io_binfbx
  COMMENT "Generating Blender Addon Release")
endif()
configure_file("cmake/launch.json.in" "${CMAKE_SOURCE_DIR}/.vscode/launch.json")
configure_file("cmake/settings.json.in" "${CMAKE_SOURCE_DIR}/.vscode/settings.json")
configure_file("cmake/c_cpp_properties.json.in" "${CMAKE_SOURCE_DIR}/.vscode/c_cpp_properties.json")
