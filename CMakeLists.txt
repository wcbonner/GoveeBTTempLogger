# CMakeList.txt : CMake project for influxd-wind-svg, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.12.0)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("goveebttemplogger"
    VERSION 2.20230918-1
    DESCRIPTION "Govee H5074, H5075, H5100, H5174, H5177, H5179, H5181, H5182, and H5183 Bluetooth Low Energy Temperature and Humidity Logger"
    HOMEPAGE_URL https://github.com/wcbonner/GoveeBTTempLogger
)

configure_file(goveebttemplogger-version.h.in goveebttemplogger-version.h)

# Add source to this project's executable.
add_executable (goveebttemplogger "goveebttemplogger.cpp" "uuid.c" "uuid.h")

add_executable(gvh-organizelogs "gvh-organizelogs.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET goveebttemplogger PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(goveebttemplogger PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           ${EXTRA_INCLUDES}
                           )

# TODO: Add tests and install targets if needed.

install(TARGETS goveebttemplogger
    DESTINATION bin
    RUNTIME DESTINATION "/usr/local/bin/"
    LIBRARY DESTINATION "/usr/local/lib/"
)

install(FILES "GoveeBTTempLogger/usr/local/lib/systemd/system/goveebttemplogger.service"
    DESTINATION "/usr/local/lib/systemd/system"
    COMPONENT "goveebttemplogger"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "wcbonner@users.noreply.github.com")
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_DEPENDS curl,influxdb)
set(CPACK_DEBIAN_PACKAGE_SECTION custom)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/GoveeBTTempLogger/DEBIAN/postinst" "${CMAKE_CURRENT_SOURCE_DIR}/GoveeBTTempLogger/DEBIAN/prerm" "${CMAKE_CURRENT_SOURCE_DIR}/GoveeBTTempLogger/DEBIAN/postrm")
include(CPack)
