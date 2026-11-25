cmake_minimum_required(VERSION 3.20)
set(AURORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/aurore)
set(AURORE_LIBRARY ${AURORE_DIR}/library)
include(${AURORE_LIBRARY}/cmake/commonOption.cmake)
include(${AURORE_LIBRARY}/cmake/toolchain.cmake)
project(navidrome_nx LANGUAGES C CXX)
set(NAVIDROME_VERSION "1.1.0" CACHE STRING "Navidrome NX release version (major.minor.patch)")
if(NOT NAVIDROME_VERSION MATCHES "^(0|[1-9][0-9]*)[.](0|[1-9][0-9]*)[.](0|[1-9][0-9]*)$")
    message(FATAL_ERROR "NAVIDROME_VERSION must be major.minor.patch")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(AURORE_RESOURCES_ROOT ${CMAKE_BINARY_DIR})
add_subdirectory(${AURORE_LIBRARY} ${CMAKE_BINARY_DIR}/aurore)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MIXER REQUIRED SDL2_mixer)
if(PLATFORM_SWITCH)
    set(MUSIC_MIXER_LIBRARIES ${MIXER_STATIC_LIBRARIES})
else()
    set(MUSIC_MIXER_LIBRARIES ${MIXER_LIBRARIES})
endif()
set(APP_SOURCE src/main.cpp vendor/qrcodegen/qrcodegen.c)
if(PLATFORM_SWITCH)
    list(APPEND APP_SOURCE ${AURORE_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
    set_source_files_properties(${AURORE_LIBRARY}/lib/platforms/switch/switch_wrapper.c PROPERTIES COMPILE_DEFINITIONS DEBUG)
endif()
program_target(navidrome_nx "${APP_SOURCE}")
target_compile_definitions(navidrome_nx PRIVATE NAVIDROME_VERSION="${NAVIDROME_VERSION}")
target_include_directories(navidrome_nx PRIVATE src ${MIXER_INCLUDE_DIRS})
target_link_directories(navidrome_nx PRIVATE ${MIXER_STATIC_LIBRARY_DIRS})
target_link_libraries(navidrome_nx PRIVATE Aurore::aurore ${MUSIC_MIXER_LIBRARIES} jansson curl mbedtls mbedx509 mbedcrypto z)
set(MERGED_RESOURCES ${CMAKE_BINARY_DIR}/resources)
add_custom_target(navidrome_resources
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${AURORE_DIR}/resources ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/xml ${MERGED_RESOURCES}/themes
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/es/app.json ${MERGED_RESOURCES}/i18n/es-419/app.json)
add_dependencies(navidrome_nx navidrome_resources)
if(PLATFORM_SWITCH)
    set(NAVIDROME_CA_BUNDLE "/etc/ssl/certs/ca-certificates.crt" CACHE FILEPATH "CA bundle for GitHub HTTPS on Switch")
    add_custom_command(TARGET navidrome_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${NAVIDROME_CA_BUNDLE}" "${MERGED_RESOURCES}/github-ca.pem")
    add_custom_target(navidrome_nx.nro ALL DEPENDS navidrome_nx
        COMMAND ${NX_NACPTOOL_EXE} --create "Navidrome NX" "baezahuertaelias" "${NAVIDROME_VERSION}" navidrome_nx.nacp
        COMMAND ${NX_ELF2NRO_EXE} navidrome_nx.elf navidrome_nx.nro
            --icon=${CMAKE_CURRENT_SOURCE_DIR}/resources/img/navidrome-icon.jpg
            --nacp=${CMAKE_BINARY_DIR}/navidrome_nx.nacp --romfsdir=${MERGED_RESOURCES})
endif()

if(AURORE_BUILD_TESTS)
    add_executable(navidrome_ui_tests tests/ui_test.cpp vendor/qrcodegen/qrcodegen.c)
    target_compile_definitions(navidrome_ui_tests PRIVATE NAVIDROME_VERSION="${NAVIDROME_VERSION}")
    target_include_directories(navidrome_ui_tests PRIVATE src ${MIXER_INCLUDE_DIRS})
    target_link_libraries(navidrome_ui_tests PRIVATE Aurore::test ${MUSIC_MIXER_LIBRARIES} jansson curl mbedtls mbedx509 mbedcrypto z)
    add_dependencies(navidrome_ui_tests navidrome_resources)
endif()
