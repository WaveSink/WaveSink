cmake_minimum_required(VERSION 3.16)
project(AudioMan VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automated build steps
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt 6.10 components
# Adjust components based on whether you are using Widgets, QML/Quick, etc.
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets)

# Add your source files
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Scanner.h
    src/Scanner.cpp
    src/AudioRouter.h
    src/AudioRouter.cpp
    src/AudioController.h
    src/AudioController.cpp
)

# Link Qt and Windows Native Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ole32
    mmdevapi
)
