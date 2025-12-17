cmake_minimum_required(VERSION 3.10)
project(TestService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where Boost is installed
set(BOOST_ROOT "C:/libs/boost_1_90_0")
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_NO_BOOST_CMAKE ON)  # Use FindBoost instead of BoostConfig

# Help CMake find Boost libraries - try common library locations
if(EXISTS "${BOOST_ROOT}/stage/lib")
    set(Boost_LIBRARY_DIR "${BOOST_ROOT}/stage/lib")
elseif(EXISTS "${BOOST_ROOT}/lib")
    set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib")
elseif(EXISTS "${BOOST_ROOT}/lib64")
    set(Boost_LIBRARY_DIR "${BOOST_ROOT}/lib64")
endif()

# Find Boost - try with system component first, fallback to header-only
find_package(Boost QUIET COMPONENTS system)
if(NOT Boost_FOUND)
    # If system library not found, try header-only
    message(STATUS "Boost system library not found, trying header-only mode...")
    find_package(Boost QUIET)
    if(Boost_FOUND)
        message(STATUS "Using Boost in header-only mode (Beast is header-only)")
        set(BOOST_HEADER_ONLY TRUE)
    else()
        message(FATAL_ERROR "Boost not found! Please install Boost or set BOOST_ROOT")
    endif()
else()
    message(STATUS "Found Boost with system library")
    set(BOOST_HEADER_ONLY FALSE)
endif()

# Windows-specific settings
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Source files
add_executable(test_service test_service.cpp)

# Include Boost headers
target_include_directories(test_service PRIVATE ${Boost_INCLUDE_DIRS})

# If using header-only Boost, define this to avoid needing system library
if(BOOST_HEADER_ONLY)
    target_compile_definitions(test_service PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
endif()

# Link Windows and Boost libraries
if(WIN32)
    target_link_libraries(test_service 
        PRIVATE
            ws2_32
            user32
            advapi32
    )
    # Only link Boost libraries if system component was found
    if(NOT BOOST_HEADER_ONLY AND Boost_LIBRARIES)
        target_link_libraries(test_service PRIVATE ${Boost_LIBRARIES})
    endif()
endif()

# Set output directory
set_target_properties(test_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
