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
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(JSON_BuildTests OFF)
    set(JSON_Install OFF)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(colony_organism STATIC
    src/Effects.cpp
    src/Injuries.cpp
    src/InjuryProfile.cpp
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
