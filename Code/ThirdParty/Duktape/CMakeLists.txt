ez_cmake_init()



ez_requires(EZ_3RDPARTY_DUKTAPE_SUPPORT)

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(LIBRARY ${PROJECT_NAME} NO_EZ_PREFIX)

target_compile_definitions(${PROJECT_NAME} PUBLIC BUILDSYSTEM_ENABLE_DUKTAPE_SUPPORT)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4702)
endif()
