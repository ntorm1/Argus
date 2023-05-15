#dir -Recurse -Include *.h,*.cpp,*.py | Get-Content | Measure-Object -Line  
# CMakeList.txt : CMake project for FastTest, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}\\Argus\\FastTest\\lib)
set(CMAKE_CXX_STANDARD 20 REQUIRED)
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
set(VCPKG_TOOLCHAIN_PATH "C:\\dev\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake" CACHE FILEPATH "Vcpkg toolchain file")
set(CMAKE_TOOLCHAIN_FILE "C:\\dev\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")
message("Compiler: ${CMAKE_CXX_COMPILER}")

project (Argus)

#----external libraries----#
#Python
find_package(Python 3.10.11 EXACT COMPONENTS Interpreter Development REQUIRED)
if(Python_FOUND)
    message(STATUS "Python found: ${Python_EXECUTABLE}")
else()
    message(FATAL_ERROR "Python not found!")
endif()

#fmt
find_package(fmt CONFIG REQUIRED)

#pybind11
find_package(pybind11 CONFIG REQUIRED)

find_library(GMP_LIBRARY gmp)
if (GMP_LIBRARY)
    message(STATUS "Found GMP library: ${GMP_LIBRARY}")
else()
    message(FATAL_ERROR "GMP library not found")
endif()
#-------------------------#

file(GLOB SRCS src/*.cpp)
include_directories(include)

pybind11_add_module(FastTest ${SRCS})

# Set compiler flags for static linking
if(MINGW)
    target_link_libraries(FastTest PRIVATE -static-libgcc -static-libstdc++)
    set(MINGW_PATH " C:/msys64/mingw64") 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${MINGW_PATH}/include")
else()
    message(FATAL_ERROR "MinGW compiler not found. Please install either MinGW or MSVC and configure your environment.")
endif()

set_target_properties(FastTest PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(FastTest PRIVATE -L${MINGW_PATH}/lib Python::Python fmt::fmt-header-only ${GMP_LIBRARY})