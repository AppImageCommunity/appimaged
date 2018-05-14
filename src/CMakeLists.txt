# required for pkg_check_module's IMPORTED_TARGET
cmake_minimum_required(VERSION 3.6)


add_executable(appimaged main.c)

import_pkgconfig_target(libglib2 glib-2.0>=2.40)

target_link_libraries(appimaged PRIVATE libappimage xdg-basedir inotify-tools)


install(
    TARGETS appimaged
    RUNTIME DESTINATION bin COMPONENT appimaged
)
