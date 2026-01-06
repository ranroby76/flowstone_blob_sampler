cmake_minimum_required(VERSION 3.20)
project(KortezzBlobSampler LANGUAGES CXX)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Must use the real installed FlowStone SDK header
set(FLOWSTONE_SDK_ROOT "D:/Workspace/FLOWSTONE_SDK" CACHE PATH "Path to FlowStone SDK root folder")

add_library(KortezzBlobSampler SHARED
    "${SRC_DIR}/BlobSampler.cpp"
)

target_include_directories(KortezzBlobSampler PRIVATE
    "${FLOWSTONE_SDK_ROOT}"   # provides FlowStoneSdk.h
    "${SRC_DIR}"
)

set_target_properties(KortezzBlobSampler PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    OUTPUT_NAME "KortezzBlobSampler"
)

# FlowStone SDK primitives use .fs32/.fs64 extensions (DLL under the hood)
if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(KortezzBlobSampler PROPERTIES SUFFIX ".fs64")
    else()
        set_target_properties(KortezzBlobSampler PROPERTIES SUFFIX ".fs32")
    endif()
endif()

if (MSVC)
    target_compile_definitions(KortezzBlobSampler PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    # IMPORTANT:
    # Do NOT force /permissive- here because the official FlowStoneSdk.h
    # uses helper functions that write through a const pointer.
    # (The SDK example projects rely on permissive MSVC behavior.)
    target_compile_options(KortezzBlobSampler PRIVATE
        /W4
        /EHsc
        /MD
    )
endif()