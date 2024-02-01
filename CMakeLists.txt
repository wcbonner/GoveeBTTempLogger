# CMakeList.txt : CMake project for GoveeBTTempLogger, include source and define
# project specific logic here.
#

# This set of commands should build the package
# git clone https://github.com/wcbonner/GoveeBTTempLogger.git
# cmake -S GoveeBTTempLogger -B GoveeBTTempLogger/build
# cmake --build GoveeBTTempLogger/build
# pushd GoveeBTTempLogger/build && cpack . && popd

cmake_minimum_required(VERSION 3.12.0)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (POLICY CMP0115)
  cmake_policy(SET CMP0115 NEW)
endif()

# I'm printing a bunch of stuff in an attempt to figure out if I can recognize building on a Raspberry Pi and only run the POST_BUILD command on the Pi.
message(STATUS "CMAKE_INSTALL_PREFIX=`${CMAKE_INSTALL_PREFIX}`")
cmake_host_system_information(RESULT MY_OS_NAME QUERY OS_NAME)
message(STATUS "OS_NAME=`${MY_OS_NAME}`")
cmake_host_system_information(RESULT MY_OS_RELEASE QUERY OS_RELEASE)
message(STATUS "OS_RELEASE=`${MY_OS_RELEASE}`")
cmake_host_system_information(RESULT MY_OS_VERSION QUERY OS_VERSION)
message(STATUS "OS_VERSION=`${MY_OS_VERSION}`")
cmake_host_system_information(RESULT MY_OS_PLATFORM QUERY OS_PLATFORM)
message(STATUS "OS_PLATFORM=`${MY_OS_PLATFORM}`")
cmake_host_system_information(RESULT MY_PROCESSOR_DESCRIPTION QUERY PROCESSOR_DESCRIPTION)
message(STATUS "PROCESSOR_DESCRIPTION=`${MY_PROCESSOR_DESCRIPTION}`")
if (CMAKE_VERSION VERSION_GREATER 3.22)
    cmake_host_system_information(RESULT PRETTY_NAME QUERY DISTRIB_PRETTY_NAME)
    message(STATUS "${PRETTY_NAME}")
    cmake_host_system_information(RESULT DISTRO QUERY DISTRIB_INFO)
    foreach(VAR IN LISTS DISTRO)
        message(STATUS "${VAR}=`${${VAR}}`")
    endforeach()
endif()

project (GoveeBTTempLogger
    VERSION 2.20240131.0
    DESCRIPTION "Listen and log Govee Thermometer Bluetooth Low Energy Advertisments"
    HOMEPAGE_URL https://github.com/wcbonner/GoveeBTTempLogger
)

configure_file(goveebttemplogger-version.h.in goveebttemplogger-version.h)

# Add source to this project's executable.
add_executable (goveebttemplogger goveebttemplogger.cpp goveebttemplogger-version.h att-types.h uuid.c uuid.h WimISO8601.cpp WimISO8601.h)
target_link_libraries(goveebttemplogger -lbluetooth -lstdc++fs)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET goveebttemplogger PROPERTY CXX_STANDARD 17)
endif()

target_include_directories(goveebttemplogger PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           ${EXTRA_INCLUDES}
                           )

if(EXISTS "/etc/rpi-issue")
	add_custom_command(TARGET goveebttemplogger POST_BUILD
	    COMMAND sudo setcap 'cap_net_raw,cap_net_admin+eip' $<TARGET_FILE:goveebttemplogger>
	    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	    COMMENT "Setting Raw Priveleges on $<TARGET_FILE:goveebttemplogger>"
	)
endif()

add_executable(gvh-organizelogs gvh-organizelogs.cpp goveebttemplogger-version.h)
target_link_libraries(gvh-organizelogs -lbluetooth -lstdc++fs)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET gvh-organizelogs PROPERTY CXX_STANDARD 17)
endif()

target_include_directories(gvh-organizelogs PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           ${EXTRA_INCLUDES}
                           )

# TODO: Add tests and install targets if needed.
include(CTest)
add_test(NAME goveebttemplogger COMMAND goveebttemplogger --help)
add_test(NAME gvh-organizelogs COMMAND gvh-organizelogs --help)

install(TARGETS goveebttemplogger gvh-organizelogs
    DESTINATION bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES goveebttemplogger.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
    COMPONENT "goveebttemplogger"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "wcbonner@users.noreply.github.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "William C Bonner <${CPACK_PACKAGE_CONTACT}>")
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_SECTION custom)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst" "${CMAKE_CURRENT_SOURCE_DIR}/prerm" "${CMAKE_CURRENT_SOURCE_DIR}/postrm")
set(CPACK_STRIP_FILES YES)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
message(STATUS "Components to pack: ${CPACK_COMPONENTS_ALL}")
include(CPack)
