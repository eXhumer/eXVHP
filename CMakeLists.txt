cmake_minimum_required(VERSION 3.22)

project(
eXVHP
    LANGUAGES CXX
    VERSION 2.0.0
)

find_package(Qt6 COMPONENTS Core Network REQUIRED)

qt_wrap_cpp(
LIB_MOC
    Include/${PROJECT_NAME}/Service.hxx
)

add_library(
${PROJECT_NAME}
    ${LIB_MOC}
    Source/MediaService.cxx
)

target_link_libraries(
${PROJECT_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Network
)

target_include_directories(
${PROJECT_NAME}
    PRIVATE
        Include/${PROJECT_NAME}/

    PUBLIC
        Include/
)
