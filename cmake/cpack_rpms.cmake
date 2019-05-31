# required for DEB-DEFAULT to work as intended
cmake_minimum_required(VERSION 3.6)

# allow building RPM packages on non-RPM systems
if(DEFINED ENV{ARCH})
    set(CPACK_RPM_PACKAGE_ARCHITECTURE $ENV{ARCH})
    if(CPACK_RPM_PACKAGE_ARCHITECTURE MATCHES "i686")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "i386")
    elseif(CPACK_RPM_PACKAGE_ARCHITECTURE MATCHES "amd64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    endif()
endif()

# override default package naming
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# versioning
set(CPACK_PACKAGE_VERSION ${APPIMAGED_VERSION})

# use git hash as package release
set(CPACK_RPM_PACKAGE_RELEASE "git${APPIMAGED_GIT_COMMIT}")

# append build ID, similar to AppImage naming
if(DEFINED ENV{TRAVIS_BUILD_NUMBER})
    set(CPACK_RPM_PACKAGE_RELEASE "${CPACK_RPM_PACKAGE_RELEASE}~travis$ENV{TRAVIS_BUILD_NUMBER}")
else()
    set(CPACK_RPM_PACKAGE_RELEASE "${CPACK_RPM_PACKAGE_RELEASE}~local")
endif()

# Exclude well known paths or file crash will be reported at the moment of installing
set(
    CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/icons
    /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps
    /usr/share/applications
    /usr/share/metainfo
    /usr/lib/systemd
    /usr/lib/systemd/user
)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

set(CPACK_RPM_PACKAGE_NAME "appimaged")
set(CPACK_RPM_PACKAGE_SUMMARY
    "Optional AppImage daemon for desktop integration. Integrates AppImages into the desktop, e.g., installs icons and menu entries.")

set(CPACK_RPM_PACKAGE_REQUIRES "libarchive, glib2, fuse")

set(CPACK_COMPONENTS_ALL appimaged)
set(CPACK_RPM_COMPONENT_INSTALL ON)
