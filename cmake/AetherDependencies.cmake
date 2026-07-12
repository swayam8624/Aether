include(FetchContent)

function(aether_resolve_fastgltf)
    if(TARGET fastgltf::fastgltf)
        return()
    endif()

    set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
    set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)
    FetchContent_Declare(fastgltf
        URL https://github.com/spnda/fastgltf/archive/refs/tags/v0.9.0.tar.gz
        URL_HASH SHA256=0bb564e127b14c22f062db50f89381dd2e0a20dbaf4987ca138a4ae8728712f9
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(fastgltf)
endfunction()

function(aether_resolve_zstd)
    if(TARGET libzstd_static)
        return()
    endif()
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
    set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zstd
        URL https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz
        URL_HASH SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR build/cmake
    )
    FetchContent_MakeAvailable(zstd)
endfunction()
