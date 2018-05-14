# handles installation of resources

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/appimaged.service
    DESTINATION lib/systemd/user/
    COMPONENT appimaged
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/appimaged.desktop
    DESTINATION share/applications
    COMPONENT appimaged
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/appimaged.svg
    DESTINATION share/icons/hicolor/scalable/apps
    COMPONENT appimaged
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/usr/share/metainfo/appimaged.appdata.xml
    DESTINATION share/metainfo
    COMPONENT appimaged
)
