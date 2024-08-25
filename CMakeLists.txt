cmake_minimum_required(VERSION 3.15)

project(Moses VERSION 0.0.1)

add_subdirectory(JUCE) # Use our patched JUCE 6.1.6, which skips the unset(xxx) calls

juce_add_plugin(Moses
    COMPANY_NAME Juce-Dev                       # Specify the name of the plugin's author (I put Juce-Dev to have my dev VST grouped together)
    IS_SYNTH FALSE                              # Is this a synth or an effect?
    IS_EFFECT TRUE                              # Is this plugin an effect?
    NEEDS_MIDI_INPUT FALSE                      # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE                     # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    COPY_PLUGIN_AFTER_BUILD TRUE                # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE Juce               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Mses                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3 Standalone                     # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "Moses")                 # The name of the final executable, which can differ from the target name

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(Moses INTERFACE ${SourceFiles})

# binary data seems to break the crossbuild because it needs libfreetype6 at build time (lmk if you know how to fix this)
# Uncomment this if you want to use binary data, then just add your assets to <repo>/Resources/,
#add #include "BinaryData.h" in the relevant source files, and run a build.

#file(GLOB_RECURSE AssetFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*")
#juce_add_binary_data(Assets SOURCES ${AssetFiles})

target_compile_definitions(Moses
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0
)

add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)
set(VST3_COPY_DIR "C:/Program Files/VST")

target_link_libraries(Moses
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
