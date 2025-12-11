set(LLVM_DIR "../llvm-project/build/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

add_executable(comp main.cpp)

target_include_directories(comp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(comp PRIVATE LLVMCore LLVMSupport)

