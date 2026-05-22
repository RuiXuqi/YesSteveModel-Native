project (ysm-lib CXX)

FILE(GLOB_RECURSE libCodeFiles CONFIGURE_DEPENDS src/*.cpp src/*.cc)
add_library(ysm-lib SHARED ${libCodeFiles})
set_property(TARGET ysm-lib PROPERTY CXX_STANDARD 23)
set_target_properties(ysm-lib PROPERTIES OUTPUT_NAME "ysm")
if (YSM_ENABLE_DEBUG_LOG)
    target_compile_definitions(ysm-lib PRIVATE
            YSM_ENABLE_DEBUG_LOG=1
            ABSL_MAX_VLOG_VERBOSITY=2)
else ()
    target_compile_definitions(ysm-lib PRIVATE
            YSM_ENABLE_DEBUG_LOG=0
            ABSL_MAX_VLOG_VERBOSITY=0)
endif ()

target_link_libraries(ysm-lib PRIVATE ysm-core yalantinglibs)
if (YSM_LINUX OR YSM_ANDROID)
    target_link_options(ysm-lib PRIVATE -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/version-map.txt)
elseif (YSM_MACOS)
    target_link_options(ysm-lib PRIVATE -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/exported-symbols.txt)
endif()
