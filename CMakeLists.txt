cmake_minimum_required(VERSION 3.20)
project(ScarletMesa VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(SCARLET_BUNDLE_SDL "Build the pinned SDL2 source statically for distribution" OFF)
option(SCARLET_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
include(CTest)

if(NOT SCARLET_BUNDLE_SDL)
    find_package(SDL2 2.0.18 QUIET CONFIG)
    if(TARGET SDL2::SDL2)
        set(SCARLET_SDL_TARGET SDL2::SDL2)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2>=2.0.18)
            if(TARGET PkgConfig::SDL2)
                set(SCARLET_SDL_TARGET PkgConfig::SDL2)
            endif()
        endif()
    endif()
endif()

if(NOT SCARLET_SDL_TARGET)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
    set(SDL_WERROR OFF CACHE BOOL "" FORCE)
    # System window/audio drivers are loaded dynamically where SDL supports it.
    # Bundling SDL itself avoids requiring an installed SDL2 or sdl2-compat.
    FetchContent_Declare(SDL2
        URL https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165)
    FetchContent_MakeAvailable(SDL2)
    set(SCARLET_SDL_TARGET SDL2::SDL2-static)
    configure_file("${sdl2_SOURCE_DIR}/LICENSE.txt" "${CMAKE_BINARY_DIR}/SDL-LICENSE.txt" COPYONLY)
endif()

add_library(scarlet-simulation STATIC src/game.cpp)
target_include_directories(scarlet-simulation PUBLIC src)

add_executable(scarlet-mesa src/main.cpp src/render.cpp src/audio.cpp)
target_link_libraries(scarlet-mesa PRIVATE scarlet-simulation ${SCARLET_SDL_TARGET})
target_compile_definitions(scarlet-mesa PRIVATE SDL_MAIN_HANDLED)
if(UNIX AND NOT APPLE AND SCARLET_BUNDLE_SDL)
    # Keep GCC runtime versions from restricting release portability.
    target_link_options(scarlet-mesa PRIVATE -static-libgcc -static-libstdc++)
endif()

foreach(target scarlet-simulation scarlet-mesa)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(SCARLET_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC -fsanitize=address,undefined)
        endif()
    endif()
endforeach()

add_custom_target(scarlet-assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:scarlet-mesa>/assets"
    COMMENT "Copying game assets")
add_dependencies(scarlet-mesa scarlet-assets)

if(BUILD_TESTING)
    add_executable(game_tests tests/game_tests.cpp)
    target_link_libraries(game_tests PRIVATE scarlet-simulation)
    add_test(NAME simulation COMMAND game_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME application-smoke COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app.py"
            "$<TARGET_FILE:scarlet-mesa>" "${CMAKE_BINARY_DIR}/smoke-artifacts")
    else()
        add_test(NAME application-smoke COMMAND scarlet-mesa --smoke-test "${CMAKE_BINARY_DIR}/smoke-artifacts")
    endif()
    set_tests_properties(application-smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
        TIMEOUT 90)
endif()

install(TARGETS scarlet-mesa RUNTIME DESTINATION .)
install(DIRECTORY assets DESTINATION .)
install(FILES README.md LICENSE NOTICE.md CREDITS.md THIRD_PARTY_NOTICES.md
        DESTINATION . OPTIONAL)
install(FILES "${CMAKE_BINARY_DIR}/SDL-LICENSE.txt" DESTINATION . OPTIONAL)
install(DIRECTORY docs DESTINATION . OPTIONAL)
