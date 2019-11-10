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
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/appimaged.png
    DESTINATION share/icons/hicolor/128x128/apps
    COMPONENT appimaged
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/usr/share/metainfo/appimaged.appdata.xml
    DESTINATION share/metainfo
    COMPONENT appimaged
)
