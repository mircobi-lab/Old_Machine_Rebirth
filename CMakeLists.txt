cmake_minimum_required(VERSION 3.22)

project(OldMachineRebirth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Per compilare con una copia locale di JUCE: -DFETCHCONTENT_SOURCE_DIR_JUCE=/percorso/JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
# libusb: serve solo per il display del Push 2, compilata statica dentro l'app
set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libusb
    GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
    GIT_TAG        v1.0.30-0
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE libusb)

# Sorgenti condivisi fra app e test. I moduli JUCE sono librerie INTERFACE:
# compilarli in una libreria statica e poi di nuovo nell'app duplicherebbe i
# simboli, quindi si condivide la lista.
set(REBIRTH_CORE_SOURCES
    Source/machine/PadGrid.cpp
    Source/push2/Push2Protocol.cpp
    Source/push2/Push2Display.cpp
    Source/push2/MachineScreen.cpp
    Source/control/Push2Panel.cpp
    Source/control/ControlMap.cpp
    Source/control/OscSurface.cpp
    Source/ui/MBStudioLookAndFeel.cpp)

set(REBIRTH_MODULES
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_osc
    juce::juce_dsp)

# ---------------------------------------------------------------- app
juce_add_gui_app(OldMachineRebirth
    PRODUCT_NAME "Old Machine Rebirth"
    COMPANY_NAME "MBStudio91"
    BUNDLE_ID com.mbstudio91.rebirth
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Serve per campionare dal microfono."
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/brand/icon.png")

juce_add_binary_data(RebirthAssets SOURCES
    Assets/Fonts/BebasNeue-Regular.ttf
    Assets/brand/mbstudio91_wordmark.png)

target_sources(OldMachineRebirth PRIVATE
    Source/Main.cpp
    Source/ui/MainComponent.cpp
    ${REBIRTH_CORE_SOURCES})

target_include_directories(OldMachineRebirth PRIVATE Source)
target_compile_definitions(OldMachineRebirth PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OldMachineRebirth,JUCE_PRODUCT_NAME>")
target_link_libraries(OldMachineRebirth PRIVATE
    usb-1.0
    RebirthAssets
    ${REBIRTH_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- test
file(GLOB REBIRTH_TEST_SOURCES CONFIGURE_DEPENDS Tests/*.cpp)
juce_add_console_app(RebirthTests PRODUCT_NAME "RebirthTests")

target_sources(RebirthTests PRIVATE ${REBIRTH_TEST_SOURCES} ${REBIRTH_CORE_SOURCES})
target_include_directories(RebirthTests PRIVATE Source Tests)
target_compile_definitions(RebirthTests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_APPLICATION_NAME_STRING="RebirthTests")
target_link_libraries(RebirthTests PRIVATE
    usb-1.0
    RebirthAssets
    ${REBIRTH_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME RebirthTests
         COMMAND $<TARGET_FILE:RebirthTests>)

# --------------------------------------------------- banco di comandi per OSC
# Genera il file di sessione di Open Stage Control dalla stessa tabella dei
# comandi che usa il Push, cosi' le due versioni non vanno fuori passo.
juce_add_console_app(OscTemplate PRODUCT_NAME "OscTemplate")
target_sources(OscTemplate PRIVATE tools/osc-template/Main.cpp ${REBIRTH_CORE_SOURCES})
target_include_directories(OscTemplate PRIVATE Source)
target_compile_definitions(OscTemplate PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="OscTemplate")
target_link_libraries(OscTemplate PRIVATE
    usb-1.0
    RebirthAssets
    ${REBIRTH_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
