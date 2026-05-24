cmake_minimum_required(VERSION 3.16)
project(ImmersivePlayer VERSION 1.0.0 LANGUAGES CXX C)

# C++ 标准配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 自动 MOC、UIC、RCC
set(CMAKE_AUTOMOC ON) 
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 第三方库根目录（可通过 -D 命令行覆盖）
set(YAROLIBS_DIR "D:/YaroLibS" CACHE PATH "Path to YaroLibS third-party libraries")
set(FFMPEG_DIR "D:/YaroLibS/ffmpeg_win64_shared" CACHE PATH "Path to FFmpeg shared libraries")

# 查找 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    OpenGL
    OpenGLWidgets
)

# 查找 FFmpeg
find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_options("$<$<CONFIG:Release>:/O2>$<$<CONFIG:RelWithDebInfo>:/O2>$<$<CONFIG:MinSizeRel>:/O1>")
else()
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
endif() 

# 收集源文件
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# 创建可执行文件
add_executable(ImmersivePlayer ${SOURCES})

# 全局预处理器定义
if(MSVC)
    target_compile_definitions(ImmersivePlayer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN __STDC_CONSTANT_MACROS)
else()
    target_compile_definitions(ImmersivePlayer PRIVATE __STDC_CONSTANT_MACROS)
endif()

# 头文件目录
target_include_directories(ImmersivePlayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${YAROLIBS_DIR}/glm
    ${YAROLIBS_DIR}/Assimp/include
)

# 链接库
target_link_libraries(ImmersivePlayer PRIVATE
    ${YAROLIBS_DIR}/Assimp/lib/assimp-vc143-mt.lib
    opengl32.lib
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# 着色器热重载选项
option(IMMERSIVEPLAYER_SHADER_HOT_RELOAD "Enable shader hot-reload" ON)
if(IMMERSIVEPLAYER_SHADER_HOT_RELOAD)
    target_compile_definitions(ImmersivePlayer PRIVATE IMMERSIVEPLAYER_SHADER_HOT_RELOAD)
endif()

# 构建后复制 Assimp DLL
add_custom_command(TARGET ImmersivePlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${YAROLIBS_DIR}/Assimp/bin/assimp-vc143-mt.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMENT "Copying assimp DLL"
)
# 构建后复制 FFmpeg DLL
add_custom_command(TARGET ImmersivePlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/avcodec-62.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/avformat-62.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/avutil-60.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/swscale-9.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMENT "Copying FFmpeg DLLs"
)

# 复制着色器目录（如果存在）
if(EXISTS "${CMAKE_SOURCE_DIR}/shaders")
    add_custom_command(TARGET ImmersivePlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:ImmersivePlayer>/shaders"
        COMMENT "Copying shaders to build directory"
    )
endif()

# 复制资源目录（如果存在）
if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
    add_custom_command(TARGET ImmersivePlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:ImmersivePlayer>/resources"
        COMMENT "Copying resources to build directory"
    )
endif()
