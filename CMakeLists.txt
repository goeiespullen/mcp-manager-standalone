cmake_minimum_required(VERSION 3.16)
project(mcp-manager VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MCPServer.cpp
    src/MCPServerInstance.cpp
    src/MCPServerManager.cpp
    src/MCPSession.cpp
    src/MCPGateway.cpp
    src/AzureDevOpsClient.cpp
    src/TrafficMonitor.cpp
)

set(HEADERS
    include/MainWindow.h
    include/MCPServer.h
    include/MCPServerInstance.h
    include/MCPServerManager.h
    include/MCPSession.h
    include/MCPGateway.h
    include/AzureDevOpsClient.h
    include/TrafficMonitor.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link Qt libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
