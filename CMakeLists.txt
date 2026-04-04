cmake_minimum_required(VERSION 3.16)
project(railway_dem_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

include_directories(
    bsw/dem/include
    bsw/nvm/include
    bsw/kavach/include
    bsw/can/include
)

set(DEM_SOURCES
    bsw/dem/src/DemCore.cpp
    bsw/dem/src/EvtLogger.cpp
)

set(NVM_SOURCES
    bsw/nvm/src/NvmStorage.cpp
)

set(KAVACH_SOURCES
    bsw/kavach/src/KavachEth.cpp
    bsw/kavach/src/KavachConditions.cpp
)

add_executable(rail_kavach_cpp
    app/diag_app/main.cpp
    ${DEM_SOURCES}
    ${NVM_SOURCES}
    ${KAVACH_SOURCES}
)
