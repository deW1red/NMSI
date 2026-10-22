# Install script for directory: /home/lab50/nmsi_ws/src/NMSI/Stage/assets

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/lab50/nmsi_ws/src/NMSI/install/stage")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RELEASE")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage/assets" TYPE FILE FILES
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/blue.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/death.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/green.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/logo.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/mains.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/mainspower.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/question_mark.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/red.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/stagelogo.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/stall-old.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/stall.png"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/rgb.txt"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage" TYPE FILE FILES "/home/lab50/nmsi_ws/src/NMSI/Stage/assets/rgb.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/lab50/nmsi_ws/src/NMSI/build/stage/assets/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
