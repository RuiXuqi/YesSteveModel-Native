project (ysm-core CXX)

FILE(GLOB_RECURSE coreCodeFiles CONFIGURE_DEPENDS src/*.cpp src/*.cc)
add_library(ysm-core STATIC ${coreCodeFiles})
unset(coreCodeFiles)
set_property(TARGET ysm-core PROPERTY CXX_STANDARD 23)
target_include_directories(ysm-core PUBLIC src)
target_compile_definitions(ysm-core PUBLIC $<$<CONFIG:Debug>:YSM_DEBUG=1>)
if (YSM_ENABLE_DEBUG_LOG)
    target_compile_definitions(ysm-core PRIVATE
            YSM_ENABLE_DEBUG_LOG=1
            ABSL_MAX_VLOG_VERBOSITY=2)
else ()
    target_compile_definitions(ysm-core PRIVATE
            YSM_ENABLE_DEBUG_LOG=0
            ABSL_MAX_VLOG_VERBOSITY=0)
endif ()
target_link_libraries(ysm-core PRIVATE
        absl::inlined_vector

        minizip
        lzma
        zlib-ng-static
        libzstd_static

        yuv
        avif
        dav1d
        turbojpeg-static
        spng_static
        webp

        opus

        blake3
        cryptopp
)
target_link_libraries(ysm-core PUBLIC
        mimalloc-static
        msft_proxy4
        jni
        optional_ref
        absl::flat_hash_map
        absl::status
        absl::statusor
        absl::log_globals
        absl::log_initialize
        absl::log
        absl::time
        pystring
        magic_enum
        cglm
        cpu_features
        yalantinglibs
        flatbuffers
)
if (YSM_LINUX OR YSM_MACOS)
    find_package(Iconv REQUIRED)
    target_link_libraries(ysm-core PRIVATE Iconv::Iconv)
elseif (YSM_WINDOWS)
    target_link_libraries(ysm-core PRIVATE synchronization) # TODO: 适配 win7 后要处理
endif ()
