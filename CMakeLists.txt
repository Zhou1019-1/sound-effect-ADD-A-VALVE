cmake_minimum_required(VERSION 3.22)
project(AddAValve VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT AddAValve)

include(cmake/Dependencies.cmake)

set(DSP_SOURCES
    src/dsp/ValveParameters.h
    src/dsp/ValveProcessor.h
    src/dsp/ValveProcessor.cpp
)
set(MODEL_SOURCES
    src/model/AudioDocument.h
    src/model/AudioDocument.cpp
    src/model/PresetStore.h
    src/model/PresetStore.cpp
)

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
    ${DSP_SOURCES}
    ${MODEL_SOURCES}
    src/audio/Metering.h
    src/audio/Metering.cpp
    src/audio/PlaybackEngine.h
    src/audio/PlaybackEngine.cpp
    src/audio/AudioExporter.h
    src/audio/AudioExporter.cpp
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
    ${DSP_SOURCES}
    ${MODEL_SOURCES}
    src/audio/Metering.h
    src/audio/Metering.cpp
    src/audio/AudioExporter.h
    src/audio/AudioExporter.cpp
    tests/TestMain.cpp
    tests/ValveProcessorTests.cpp
    tests/AudioDocumentTests.cpp
    tests/MeteringTests.cpp
    tests/AudioExporterTests.cpp
)
target_compile_definitions(AddAValveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(AddAValveTests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

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
    target_compile_options(AddAValve PRIVATE /utf-8)
    target_compile_options(AddAValveTests PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME AddAValveUnitTests COMMAND AddAValveTests)

install(TARGETS AddAValve RUNTIME DESTINATION .)
install(FILES LICENSE THIRD_PARTY_NOTICES.txt DESTINATION .)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "ADD-A-VALVE")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ADD-A-VALVE-${PROJECT_VERSION}-Windows-x64")
include(CPack)
