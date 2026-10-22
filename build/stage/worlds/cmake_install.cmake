# Install script for directory: /home/lab50/nmsi_ws/src/NMSI/Stage/worlds

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage/worlds" TYPE FILE FILES
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/amcl-sonar.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/autolab.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/camera.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/everything.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/lsp_test.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/mbicp.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/nd.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/roomba.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/simple.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/test.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/uoa_robotics_lab.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/vfh.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/wavefront-remote.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/wavefront.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/wifi.cfg"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/SFU.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/autolab.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/camera.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/circuit.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/everything.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/fasr.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/fasr2.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/fasr_plan.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/large.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/lsp_test.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/mbicp.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/pioneer_flocking.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/pioneer_follow.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/pioneer_walle.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/roomba.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/sensor_noise_demo.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/sensor_noise_module_demo.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/simple.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/uoa_robotics_lab.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/wifi.world"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/beacons.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/chatterbox.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/hokuyo.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/irobot.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/map.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/objects.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/pantilt.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/pioneer.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/sick.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/ubot.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/uoa_robotics_lab_models.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/walle.inc"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/cfggen.sh"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/test.sh"
    "/home/lab50/nmsi_ws/src/NMSI/Stage/worlds/worldgen.sh"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/lab50/nmsi_ws/src/NMSI/build/stage/worlds/benchmark/cmake_install.cmake")
  include("/home/lab50/nmsi_ws/src/NMSI/build/stage/worlds/bitmaps/cmake_install.cmake")
  include("/home/lab50/nmsi_ws/src/NMSI/build/stage/worlds/wifi/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/lab50/nmsi_ws/src/NMSI/build/stage/worlds/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
