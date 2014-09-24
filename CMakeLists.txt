CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(phoned)

SET(CMAKE_INSTALL_PREFIX "/usr")

# -----------------------------------------------------------------------------
# Required platform modules
# -----------------------------------------------------------------------------
INCLUDE(FindPkgConfig)
INCLUDE(GNUInstallDirs)

PKG_CHECK_MODULES(glib REQUIRED glib-2.0)
PKG_CHECK_MODULES(gio REQUIRED gio-2.0)
PKG_CHECK_MODULES(dbus REQUIRED dbus-1)
PKG_CHECK_MODULES(libebook-contacts REQUIRED libebook-contacts-1.2)
PKG_CHECK_MODULES(aul REQUIRED aul)

INCLUDE_DIRECTORIES(
  ${glib_INCLUDE_DIRS}
  ${gio_INCLUDE_DIRS}
  ${dbus_INCLUDE_DIRS}
  ${libebook-contacts_INCLUDE_DIRS}
  ${aul_INCLUDE_DIRS}
)

# -----------------------------------------------------------------------------
# Determine the time tracing option
# -----------------------------------------------------------------------------

OPTION(ENABLE_TIME_TRACER "TIME TRACING" OFF)

IF(ENABLE_TIME_TRACER)
    MESSAGE(STATUS "Time tracer enabled")
    ADD_DEFINITIONS("-DENABLE_TIME_TRACER")
ELSE(ENABLE_TIME_TRACER)
    MESSAGE(STATUS "Time tracer disabled")
ENDIF(ENABLE_TIME_TRACER)

# -----------------------------------------------------------------------------
# Set build type (Release by default)
# -----------------------------------------------------------------------------
IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
     SET(CMAKE_BUILD_TYPE Release)
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")

MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

# -----------------------------------------------------------------------------
# CFlags
# -----------------------------------------------------------------------------
SET(CMAKE_C_FLAGS_PROFILING    "-O0 -g -pg")
SET(CMAKE_CXX_FLAGS_PROFILING  "-O0 -std=c++0x -g -pg")
SET(CMAKE_C_FLAGS_DEBUG        "-O0 -g")
SET(CMAKE_CXX_FLAGS_DEBUG      "-O0 -std=c++0x -g")
SET(CMAKE_C_FLAGS_RELEASE      "-O2 -g")
SET(CMAKE_CXX_FLAGS_RELEASE    "-O2 -std=c++0x -g")
ADD_DEFINITIONS("-DCLIENT_IPC_THREAD")
ADD_DEFINITIONS("-DEXPORT_API=")
ADD_DEFINITIONS("-Wall")
ADD_DEFINITIONS("-D_FILE_OFFSET_BITS=64")
#ADD_DEFINITIONS("-Werror")
#ADD_DEFINITIONS("-Wextra")
ADD_DEFINITIONS("-DAPPLICATION_API_BACKWARD_COMPATIBILITY")
ADD_DEFINITIONS("-std=c++11")

# -----------------------------------------------------------------------------
# Global variables
# -----------------------------------------------------------------------------

SET(DESTINATION_PREFIX bin)
SET(DBUS_SERVICE_PREFIX share/dbus-1/services)
SET(SYSTEMD_SERVICE_PREFIX lib/systemd/user)

# -----------------------------------------------------------------------------
# Macros for pkgconfig
# -----------------------------------------------------------------------------
SET(PKGCONFIG_DIR ${CMAKE_SOURCE_DIR}/pkgconfig)

MACRO(configure_and_install_pkg PKG_FILE)
    CONFIGURE_FILE(${PKGCONFIG_DIR}/${PKG_FILE}.in
               ${PKGCONFIG_DIR}/${PKG_FILE} @ONLY)
    INSTALL(FILES ${PKGCONFIG_DIR}/${PKG_FILE} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
ENDMACRO(configure_and_install_pkg)

SET(TARGET_NAME phoned)

configure_and_install_pkg(phoned.pc)

SET(SRCS src/phoned.cpp
         src/phone.cpp
         src/connman.cpp
         src/bluez.cpp
         src/obex.cpp
         src/ofono.cpp
         src/utils.cpp
)

ADD_EXECUTABLE(${TARGET_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${TARGET_NAME}
                      ${glib_LDFLAGS}
                      ${gio_LDFLAGS}
                      ${dbus_LDFLAGS}
                      ${libebook-contacts_LDFLAGS}
                      ${aul_LDFLAGS}
)
INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${DESTINATION_PREFIX})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/org.tizen.phone.service DESTINATION ${DBUS_SERVICE_PREFIX})
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/phoned.service DESTINATION ${SYSTEMD_SERVICE_PREFIX})

