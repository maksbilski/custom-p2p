cmake_minimum_required(VERSION 3.14)
project(p2p_resource_sync)

message(STATUS "Configuring project: ${PROJECT_NAME}")

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Main library
message(STATUS "Configuring main library...")
add_library(p2p_resource_sync
    "src/local_resource_manager.cpp"
    "src/remote_resource_manager.cpp"
    "src/announcement_broadcaster.cpp"
    "src/announcement_receiver.cpp"
    "src/tcp_server.cpp"
    "src/resource_downloader.cpp"
)

message(STATUS "Setting include directories...")
target_include_directories(p2p_resource_sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Main executable
message(STATUS "Configuring main executable...")
add_executable(p2p_resource_sync_app
    "main.cpp"
)

message(STATUS "Linking main executable...")
target_link_libraries(p2p_resource_sync_app
    PRIVATE p2p_resource_sync
)

# Testing
message(STATUS "Configuring testing...")
enable_testing()

# Find GTest package
find_package(GTest REQUIRED)

# Function to create test target
function(add_test_executable TEST_NAME TEST_SOURCE)
    message(STATUS "Configuring test: ${TEST_NAME}")
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME}
        PRIVATE p2p_resource_sync
        PRIVATE GTest::GTest
        PRIVATE GTest::Main
    )
    gtest_discover_tests(${TEST_NAME}
        PROPERTIES
        ENVIRONMENT GTEST_COLOR=1
        VERBOSITY verbose
    )
endfunction()

# Individual test targets
add_test_executable(resource_downloader_test "tests/resource_downloader_test.cpp")
add_test_executable(local_resource_manager_test "tests/local_resource_manager_test.cpp")
add_test_executable(remote_resource_manager_test "tests/remote_resource_manager_test.cpp")
add_test_executable(announcement_broadcaster_test "tests/announcement_broadcaster_test.cpp")
add_test_executable(tcp_server_test "tests/tcp_server_test.cpp")

# All tests target (optional)
message(STATUS "Configuring all tests executable...")
add_executable(p2p_resource_sync_tests
    "tests/local_resource_manager_test.cpp"
    "tests/remote_resource_manager_test.cpp"
    "tests/announcement_test.cpp"
    "tests/tcp_server_test.cpp"
    "tests/resource_downloader_test.cpp"
)

message(STATUS "Linking all tests executable...")
target_link_libraries(p2p_resource_sync_tests
    PRIVATE p2p_resource_sync
    PRIVATE GTest::GTest
    PRIVATE GTest::Main
)

gtest_discover_tests(p2p_resource_sync_tests)
message(STATUS "Configuration complete")
