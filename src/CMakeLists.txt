set(LLVM_DIR "../llvm-project/build/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

set(LLVM_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../llvm-project/llvm")
set(KALEIDOSCOPE_INCLUDE "${LLVM_SOURCE_DIR}/examples/Kaleidoscope/include")

message(STATUS "Buscando KaleidoscopeJIT en: ${KALEIDOSCOPE_INCLUDE}")
message(STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

add_executable(comp main.cpp)

target_include_directories(comp
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${LLVM_INCLUDE_DIRS}
        ${KALEIDOSCOPE_INCLUDE}
)
set_target_properties(comp PROPERTIES ENABLE_EXPORTS ON)

llvm_map_components_to_libnames(llvm_libs
  Core
  Support
  Passes
  Analysis
  ExecutionEngine
  OrcJIT
  OrcShared
  X86CodeGen
  X86AsmParser
  X86Desc
  X86Info
  MC
  AsmParser
  SelectionDAG
)

# Linkea SOLO la variable que contiene todo
target_link_libraries(comp PRIVATE ${llvm_libs} "-rdynamic")

