# required for pkg_check_module's IMPORTED_TARGET
cmake_minimum_required(VERSION 3.6)


# FIXME: why are these lines necessary? The
include_directories(${inotify-tools_INCLUDE_DIRS})
include_directories(${libglib_INCLUDE_DIRS})


add_executable(appimaged main.c)

target_link_libraries(appimaged PRIVATE libappimage_static inotify-tools xdg-basedir)


install(
    TARGETS appimaged
    RUNTIME DESTINATION bin COMPONENT appimaged
)
