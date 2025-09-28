cmake_minimum_required(VERSION 3.18.1)

# --- Llama.cpp 특정 CMake 옵션 ---
set(LLAMA_STANDALONE OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_K_QUANTS ON CACHE BOOL "" FORCE)
set(LLAMA_NATIVE OFF CACHE BOOL "" FORCE) # 수정됨
# --- Llama.cpp 특정 CMake 옵션 끝 ---

add_subdirectory(llama_cpp)

add_library(
             testapp-native
             SHARED
             native-lib.cpp
             )

find_library(
              log-lib
              log
              )

target_link_libraries(
                       testapp-native
                       ${log-lib}
                       android
                       llama
                       )