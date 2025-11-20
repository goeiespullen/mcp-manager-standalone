cmake_minimum_required(VERSION 3.16)
project(mcp-manager VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Find OpenSSL for encryption
find_package(OpenSSL REQUIRED)

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
    src/CredentialsTab.cpp
    src/Keystore.cpp
    src/UpdateChecker.cpp
    src/UpdateDialog.cpp
    src/Logger.cpp
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
    include/CredentialsTab.h
    include/Keystore.h
    include/Version.h
    include/UpdateChecker.h
    include/UpdateDialog.h
    include/Logger.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link Qt libraries and OpenSSL
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Test executable for keystore compatibility
add_executable(test_keystore test_keystore.cpp src/Keystore.cpp include/Keystore.h)
target_include_directories(test_keystore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_keystore
    Qt6::Core
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Import credentials utility
add_executable(import_credentials
    import_credentials.cpp
    src/Keystore.cpp
    src/Logger.cpp
)
target_include_directories(import_credentials PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(import_credentials
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)
