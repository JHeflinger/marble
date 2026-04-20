cmake_minimum_required(VERSION 3.20)
project(Marble)

add_custom_target(run_app
    COMMAND ${CMAKE_SOURCE_DIR}/run.bat
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_executable(marble_exe IMPORTED GLOBAL)
set_target_properties(marble_exe PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/build/bin.exe"
)

add_dependencies(marble_exe run_app)