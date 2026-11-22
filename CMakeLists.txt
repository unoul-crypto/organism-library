cmake_minimum_required(VERSION 3.21)
project(organism_library VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL AND NOT EMSCRIPTEN)
    include(CTest)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.11 CONFIG QUIET)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    set(JSON_BuildTests OFF)
    set(JSON_Install OFF)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(colony_organism STATIC
    src/Injuries.cpp
    src/OrganismCodec.cpp
)
target_include_directories(colony_organism PUBLIC include)
target_compile_features(colony_organism PUBLIC cxx_std_17)
target_link_libraries(colony_organism PUBLIC nlohmann_json::nlohmann_json)

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(organism_library_test tests/Organism.cpp)
    target_link_libraries(organism_library_test PRIVATE colony_organism)
    add_test(NAME organism_library COMMAND organism_library_test)
endif()
