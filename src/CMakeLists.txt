# required for pkg_check_module's IMPORTED_TARGET
cmake_minimum_required(VERSION 3.6)

import_pkgconfig_target(TARGET_NAME libglib PKGCONFIG_TARGET glib-2.0>=2.40)
import_pkgconfig_target(TARGET_NAME libgobject PKGCONFIG_TARGET gobject-2.0>=2.40)
import_pkgconfig_target(TARGET_NAME libgio PKGCONFIG_TARGET gio-2.0>=2.40)

add_executable(appimaged main.c notify.c notify.h)
target_link_libraries(appimaged PRIVATE inotify-tools libappimage_static xdg-basedir dl libglib)

install(
    TARGETS appimaged
    RUNTIME DESTINATION bin COMPONENT appimaged
)
