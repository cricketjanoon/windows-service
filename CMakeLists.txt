cmake_minimum_required(VERSION 3.10)
project(TestService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# Source files
add_executable(test_service test_service.cpp)

# Link Windows libraries
if(WIN32)
    target_link_libraries(test_service 
        user32
        advapi32
    )
endif()

# Set output directory
set_target_properties(test_service PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
