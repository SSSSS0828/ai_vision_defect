cmake_minimum_required(VERSION 3.16)
project(AIVisionDefect VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Qt5 ───────────────────────────────────────────────────────────────────
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Charts)

# ─── OpenCV ────────────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED)

# ─── Sources ───────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ImageRenderer.cpp
    src/InferenceEngine.cpp
    src/ImagePipeline.cpp
    src/FramePool.cpp
    src/DefectOverlay.cpp
    src/StatisticsPanel.cpp
    src/YoloDetector.cpp
)

set(HEADERS
    include/MainWindow.h
    include/ImageRenderer.h
    include/InferenceEngine.h
    include/ImagePipeline.h
    include/FramePool.h
    include/DefectOverlay.h
    include/StatisticsPanel.h
    include/YoloDetector.h
    include/Types.h
)

# ─── Executable ────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Charts
    ${OpenCV_LIBS}
)

# ─── YOLO / DNN backend (optional CUDA) ────────────────────────────────────
# If you have CUDA-enabled OpenCV, set OPENCV_DNN_CUDA=ON
option(OPENCV_DNN_CUDA "Enable CUDA backend for OpenCV DNN" OFF)
if(OPENCV_DNN_CUDA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CUDA)
endif()

# ─── Install ───────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY resources/ DESTINATION share/${PROJECT_NAME}/resources)
