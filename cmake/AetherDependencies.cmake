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
