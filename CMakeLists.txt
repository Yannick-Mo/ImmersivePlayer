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

# 第三方库路径（每个库可独立配置，通过 -D 命令行覆盖）
set(GLM_DIR    "D:/YannickLibS/glm"                CACHE PATH "Path to GLM (header-only math library)")
set(ASSIMP_DIR "D:/YannickLibS/Assimp"             CACHE PATH "Path to Assimp library root")
set(FFMPEG_DIR "D:/YannickLibS/ffmpeg_win64_shared" CACHE PATH "Path to FFmpeg shared libraries root")

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
add_executable(ImmersivePlayer ${SOURCES} app.rc)
set_target_properties(ImmersivePlayer PROPERTIES WIN32_EXECUTABLE TRUE)

# 全局预处理器定义
if(MSVC)
    target_compile_definitions(ImmersivePlayer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN __STDC_CONSTANT_MACROS)
else()
    target_compile_definitions(ImmersivePlayer PRIVATE __STDC_CONSTANT_MACROS)
endif()

# 头文件目录
target_include_directories(ImmersivePlayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GLM_DIR}
    ${ASSIMP_DIR}/include
)

# 链接库
target_link_libraries(ImmersivePlayer PRIVATE
    ${ASSIMP_DIR}/lib/assimp-vc143-mt.lib
    opengl32.lib
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swscale
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# 构建后复制 Assimp DLL
add_custom_command(TARGET ImmersivePlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_DIR}/bin/assimp-vc143-mt.dll"
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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/swresample-6.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/avfilter-11.dll"
        "$<TARGET_FILE_DIR:ImmersivePlayer>/"
    COMMENT "Copying FFmpeg DLLs"
)

# ── 安装规则 ──────────────────────────────────────────
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

install(TARGETS ImmersivePlayer
    RUNTIME DESTINATION .
    BUNDLE  DESTINATION .
)

install(FILES
    "${ASSIMP_DIR}/bin/assimp-vc143-mt.dll"
    "${FFMPEG_DIR}/bin/avcodec-62.dll"
    "${FFMPEG_DIR}/bin/avformat-62.dll"
    "${FFMPEG_DIR}/bin/avutil-60.dll"
    "${FFMPEG_DIR}/bin/swscale-9.dll"
    "${FFMPEG_DIR}/bin/swresample-6.dll"
    "${FFMPEG_DIR}/bin/avfilter-11.dll"
    DESTINATION .
)

# ── CPack ─────────────────────────────────────────────
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "ImmersivePlayer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "ImmersivePlayer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Immersive 3D Video Player")
set(CPACK_NSIS_DISPLAY_NAME "ImmersivePlayer ${PROJECT_VERSION}")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS/$STARTMENU_FOLDER/ImmersivePlayer.lnk' '$INSTDIR/ImmersivePlayer.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS/$STARTMENU_FOLDER/ImmersivePlayer.lnk'"
)
include(CPack)