cmake_minimum_required(VERSION 3.22)
project(AddAValve VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT AddAValve)

include(cmake/Dependencies.cmake)

# ------------------------------------------------- 外接引擎（可选） ---------
#  ADDAVALVE_EXTERNAL_DSP=ON：不编译 src/dsp/ValveProcessor.cpp，ValveProcessor 的实现
#  改由 external/engine_adapter.cpp 转调 addavalve.h 的 C ABI（同名同语义，调用点零改动）。
#  需要 ADDAVALVE_ENGINE_DIR 指向一个同时含 addavalve.h、导入库、运行库的目录。
option(ADDAVALVE_EXTERNAL_DSP "Drive the tone stage from an external engine library (addavalve)" OFF)
set(ADDAVALVE_ENGINE_DIR "" CACHE PATH "External engine dir: addavalve.h + import library + runtime DLL")
set(ADDAVALVE_ENGINE_DLL "" CACHE FILEPATH "External engine runtime DLL (default: <ADDAVALVE_ENGINE_DIR>/addavalve.dll)")

# ---------------------------------------------------------------- 分层 -----
#  AddAValveCore          静态库：DSP + 计量 + 播放引擎 + 文档模型 + 导出（不含任何 GUI 符号）
#  AddAValve              GUI 应用：只通过 AddAValveCore 使用核心层
#  AddAValveTests         单元测试
#  AddAValveCoreApiSmoke  冒烟目标：只 include <core_api.h>，验证公共契约自足
#
#  源文件列表必须在被引用之前定义：ADDAVALVE_CORE_SOURCES 由
#  DSP_SOURCES / MODEL_SOURCES / ADDAVALVE_AUDIO_SOURCES 拼成，三者都在
#  add_library(AddAValveCore) 之前定义。

set(DSP_SOURCES
    src/dsp/ValveParameters.h
    src/dsp/ValveProcessor.h
    src/dsp/ValveProcessor.cpp
)
if(ADDAVALVE_EXTERNAL_DSP)
    # 内置 DSP 退出编译，换成转调外接引擎的实现（同名同语义，调用点零改动）
    list(REMOVE_ITEM DSP_SOURCES src/dsp/ValveProcessor.cpp)
    list(APPEND DSP_SOURCES external/engine_adapter.cpp)
endif()
set(MODEL_SOURCES
    src/model/AudioDocument.h
    src/model/AudioDocument.cpp
    src/model/PresetStore.h
    src/model/PresetStore.cpp
)
set(ADDAVALVE_AUDIO_SOURCES
    src/audio/Metering.h
    src/audio/Metering.cpp
    src/audio/PlaybackEngine.h
    src/audio/PlaybackEngine.cpp
    src/audio/AudioExporter.h
    src/audio/AudioExporter.cpp
)

set(ADDAVALVE_CORE_SOURCES ${DSP_SOURCES} ${MODEL_SOURCES} ${ADDAVALVE_AUDIO_SOURCES})

# 核心层专用 JUCE 聚合头：静态库目标无法用 juce_generate_juce_header()，
# 这里在 configure 阶段生成一份固定的（只含核心层用到的模块），详见文件头注释。
set(ADDAVALVE_CORE_JUCE_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/AddAValveCore_artefacts/JuceLibraryCode")
file(MAKE_DIRECTORY "${ADDAVALVE_CORE_JUCE_HEADER_DIR}")
configure_file(cmake/JuceHeaderCore.h "${ADDAVALVE_CORE_JUCE_HEADER_DIR}/JuceHeader.h" COPYONLY)

add_library(AddAValveCoreJuceHeader INTERFACE)
target_include_directories(AddAValveCoreJuceHeader INTERFACE "${ADDAVALVE_CORE_JUCE_HEADER_DIR}")

add_library(AddAValveCore STATIC ${ADDAVALVE_CORE_SOURCES})
target_include_directories(AddAValveCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_compile_definitions(AddAValveCore PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(AddAValveCore PUBLIC
    AddAValveCoreJuceHeader
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- 外接引擎接线：头文件搜索路径 + 导入库 + 运行库 ----------------------
if(ADDAVALVE_EXTERNAL_DSP)
    if(NOT ADDAVALVE_ENGINE_DIR)
        message(FATAL_ERROR
            "ADDAVALVE_EXTERNAL_DSP=ON 需要 -DADDAVALVE_ENGINE_DIR=<dir>"
            "（目录里要有 addavalve.h 与导入库）")
    endif()

    # 1) 头文件：external/engine_adapter.cpp 里是 #include <addavalve.h>
    target_include_directories(AddAValveCore PRIVATE "${ADDAVALVE_ENGINE_DIR}")
    # ValveProcessor.h 的公共声明里带 ADDAVALVE_EXTERNAL_DSP 分支，必须传给所有使用者
    target_compile_definitions(AddAValveCore PUBLIC ADDAVALVE_EXTERNAL_DSP=1)

    # 2) 导入库（MSVC 是 addavalve.lib，MinGW 是 libaddavalve.a）
    find_library(ADDAVALVE_ENGINE_LIBRARY
        NAMES addavalve libaddavalve
        PATHS "${ADDAVALVE_ENGINE_DIR}"
        NO_DEFAULT_PATH)
    if(NOT ADDAVALVE_ENGINE_LIBRARY)
        message(FATAL_ERROR "在 ${ADDAVALVE_ENGINE_DIR} 里找不到 addavalve 导入库")
    endif()
    target_link_libraries(AddAValveCore PUBLIC "${ADDAVALVE_ENGINE_LIBRARY}")
    message(STATUS "外接引擎：${ADDAVALVE_ENGINE_LIBRARY}")

    # 3) 运行库：默认 <ENGINE_DIR>/addavalve.dll
    if(NOT ADDAVALVE_ENGINE_DLL)
        set(ADDAVALVE_ENGINE_DLL "${ADDAVALVE_ENGINE_DIR}/addavalve.dll"
            CACHE FILEPATH "External engine runtime DLL" FORCE)
    endif()
    if(WIN32 AND NOT EXISTS "${ADDAVALVE_ENGINE_DLL}")
        message(FATAL_ERROR
            "找不到运行库 ${ADDAVALVE_ENGINE_DLL}；用 -DADDAVALVE_ENGINE_DLL=<path> 指定")
    endif()
    message(STATUS "外接引擎运行库：${ADDAVALVE_ENGINE_DLL}")
endif()
# ---------------------------------------------------------------------------

set(ADD_A_VALVE_ICON "${CMAKE_CURRENT_BINARY_DIR}/app-icon.png")
configure_file("icon/章鱼哥胆机icon.png" "${ADD_A_VALVE_ICON}" COPYONLY)
juce_add_gui_app(AddAValve
    PRODUCT_NAME "ADD A VALVE"
    COMPANY_NAME "ADD A VALVE"
    BUNDLE_ID "com.addavalve.editor"
    ICON_BIG "${ADD_A_VALVE_ICON}"
    ICON_SMALL "${ADD_A_VALVE_ICON}"
)
juce_generate_juce_header(AddAValve)
target_sources(AddAValve PRIVATE
    src/app/Main.cpp
    src/app/MainComponent.h
    src/app/MainComponent.cpp
    src/ui/ControlPanel.h
    src/ui/ControlPanel.cpp
    src/ui/MeterPanel.h
    src/ui/MeterPanel.cpp
    src/ui/WaveformView.h
    src/ui/WaveformView.cpp
)
target_compile_definitions(AddAValve PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AddAValve,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)
target_link_libraries(AddAValve PRIVATE
    AddAValveCore
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_add_console_app(AddAValveTests PRODUCT_NAME "ADD A VALVE Tests")
juce_generate_juce_header(AddAValveTests)
target_sources(AddAValveTests PRIVATE
    tests/TestMain.cpp
    tests/ValveProcessorTests.cpp
    tests/AudioDocumentTests.cpp
    tests/MeteringTests.cpp
    tests/AudioExporterTests.cpp
)
target_compile_definitions(AddAValveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(AddAValveTests PRIVATE
    AddAValveCore
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# 公共头冒烟目标：不生成自己的 JuceHeader.h、不直接链任何 juce:: 模块，
# 所有依赖只能来自 AddAValveCore 的 PUBLIC 接口。core/core_api.h 少写一条
# include 路径、或 src/ core/ 漏进 PUBLIC，这里立刻编不过。
juce_add_console_app(AddAValveCoreApiSmoke PRODUCT_NAME "ADD A VALVE Core API Smoke")
target_sources(AddAValveCoreApiSmoke PRIVATE tests/CoreApiSmoke.cpp)
target_link_libraries(AddAValveCoreApiSmoke PRIVATE AddAValveCore)

if(TARGET LameEncoder)
    add_dependencies(AddAValve LameEncoder)
    add_dependencies(AddAValveTests LameEncoder)
    add_custom_command(TARGET AddAValve POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ADD_A_VALVE_LAME_EXECUTABLE}" "$<TARGET_FILE_DIR:AddAValve>/lame.exe")
    add_custom_command(TARGET AddAValveTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ADD_A_VALVE_LAME_EXECUTABLE}" "$<TARGET_FILE_DIR:AddAValveTests>/lame.exe")
    install(FILES "${ADD_A_VALVE_LAME_EXECUTABLE}" DESTINATION .)
    install(FILES "${ADD_A_VALVE_LAME_SOURCE_DIR}/COPYING" DESTINATION . RENAME "LAME_COPYING.txt")
    install(FILES "${ADD_A_VALVE_LAME_SOURCE_DIR}/LICENSE" DESTINATION . RENAME "LAME_LICENSE.txt")
endif()

if(MSVC)
    target_compile_options(AddAValveCore PRIVATE /utf-8)
    target_compile_options(AddAValve PRIVATE /utf-8)
    target_compile_options(AddAValveTests PRIVATE /utf-8)
    target_compile_options(AddAValveCoreApiSmoke PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME AddAValveUnitTests COMMAND AddAValveTests)
add_test(NAME AddAValveCoreApiSmoke COMMAND AddAValveCoreApiSmoke)

# ---- 外接引擎的运行时部署：三个可执行目标旁边各放一份 DLL，并进安装包 ------
if(ADDAVALVE_EXTERNAL_DSP AND WIN32)
    foreach(_target AddAValve AddAValveTests AddAValveCoreApiSmoke)
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${ADDAVALVE_ENGINE_DLL}" "$<TARGET_FILE_DIR:${_target}>"
            COMMENT "复制外接引擎运行库 → ${_target}")
    endforeach()
endif()

install(TARGETS AddAValve RUNTIME DESTINATION .)
install(FILES LICENSE THIRD_PARTY_NOTICES.txt DESTINATION .)
if(ADDAVALVE_EXTERNAL_DSP AND WIN32)
    install(FILES "${ADDAVALVE_ENGINE_DLL}" DESTINATION .)
endif()
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "ADD-A-VALVE")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ADD-A-VALVE-${PROJECT_VERSION}-Windows-x64")
include(CPack)
