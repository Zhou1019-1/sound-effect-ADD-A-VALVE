include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG d6181bde38d858c283c3b7bf699ce6340c050b5d
    GIT_SHALLOW FALSE
)

FetchContent_GetProperties(JUCE)
if(NOT juce_POPULATED)
    FetchContent_Populate(JUCE)
    add_subdirectory("${juce_SOURCE_DIR}" "${juce_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()

if(WIN32 AND MSVC)
    FetchContent_Declare(
        LAME
        GIT_REPOSITORY https://github.com/lameproject/lame.git
        GIT_TAG 1f5cc9487284d5950343aa5d4f70de433468070a
        GIT_SHALLOW FALSE
    )
    FetchContent_Populate(LAME)

    set(ADD_A_VALVE_LAME_SOURCE_DIR "${lame_SOURCE_DIR}")
    set(ADD_A_VALVE_LAME_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/lame.exe")
    set(ADD_A_VALVE_VCVARS "${CMAKE_GENERATOR_INSTANCE}/VC/Auxiliary/Build/vcvars64.bat")

    add_custom_command(
        OUTPUT "${ADD_A_VALVE_LAME_EXECUTABLE}"
        COMMAND "${ADD_A_VALVE_VCVARS}"
        COMMAND cmd.exe /d /c nmake /nologo /f Makefile.MSVC
                MSVCVER=Win64 MACHINE=/machine:X64 ASM=NO MMX=NO GTK=NO
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${ADD_A_VALVE_LAME_SOURCE_DIR}/output/lame.exe" "${ADD_A_VALVE_LAME_EXECUTABLE}"
        DEPENDS "${ADD_A_VALVE_LAME_SOURCE_DIR}/Makefile.MSVC"
        WORKING_DIRECTORY "${ADD_A_VALVE_LAME_SOURCE_DIR}"
        COMMENT "Building bundled LAME 3.100.1 x64 encoder"
        VERBATIM
    )
    add_custom_target(LameEncoder DEPENDS "${ADD_A_VALVE_LAME_EXECUTABLE}")
endif()
