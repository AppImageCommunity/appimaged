# include libappimage
add_subdirectory(libappimage EXCLUDE_FROM_ALL)


# uses libappimage's scripts
include(ExternalProject)
set(USE_SYSTEM_INOTIFY_TOOLS OFF CACHE BOOL "Use system libinotifytools instead of building our own")

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    message(STATUS "Downloading and building inotify-tools")

    # TODO: build out of source
    ExternalProject_Add(inotify-tools-EXTERNAL
        URL https://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz
        URL_HASH SHA512=6074d510e89bba5da0d7c4d86f2562c662868666ba0a7ea5d73e53c010a0050dd1fc01959b22cffdb9b8a35bd1b0b43c04d02d6f19927520f05889e8a9297dfb
        PATCH_COMMAND ${WGET} -N --content-disposition "https://raw.githubusercontent.com/AppImage/external-resources/master/patches/config.guess"
        COMMAND ${WGET} -N --content-disposition "https://raw.githubusercontent.com/AppImage/external-resources/master/patches/config.sub"
        UPDATE_COMMAND ""  # ${MAKE} sure CMake won't try to fetch updates unnecessarily and hence rebuild the dependency every time
        CONFIGURE_COMMAND CC=${CC} CXX=${CXX} CFLAGS=${CFLAGS} LDFLAGS=${LDFLAGS} <SOURCE_DIR>/configure --enable-shared --enable-static --enable-doxygen=no --prefix=<INSTALL_DIR> --libdir=<INSTALL_DIR>/lib ${EXTRA_CONFIGURE_FLAGS}
        BUILD_COMMAND ${MAKE}
        BUILD_IN_SOURCE ON
        INSTALL_COMMAND ${MAKE} install
    )

    import_external_project(
        TARGET_NAME inotify-tools
        EXT_PROJECT_NAME inotify-tools-EXTERNAL
        LIBRARIES "<INSTALL_DIR>/lib/libinotifytools.a"
        INCLUDE_DIRS "<INSTALL_DIR>/include/"
    )
else()
    message(STATUS "Using system inotify-tools")

    import_find_pkg_target(inotify-tools INotify INOTIFYTOOLS)
endif()

import_pkgconfig_target(TARGET_NAME libnotify PKGCONFIG_TARGET libnotify)
