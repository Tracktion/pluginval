# A minimal, fully deterministic gain effect used to dogfood the acceptance
# feature's audio-input path. Built only when PLUGINVAL_BUILD_TEST_PLUGINS is ON.

juce_add_plugin(pluginval_gain
    PRODUCT_NAME                "pluginval Gain"
    COMPANY_NAME                Tracktion
    BUNDLE_ID                   com.Tracktion.pluginvalGain
    PLUGIN_MANUFACTURER_CODE    Trkt
    PLUGIN_CODE                 Pgn1
    FORMATS                     VST3 $<$<PLATFORM_ID:Darwin>:AU>
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(pluginval_gain PRIVATE
    GainPlugin.cpp
    GainPlugin.h)

target_compile_features(pluginval_gain PRIVATE cxx_std_20)

target_compile_definitions(pluginval_gain PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(pluginval_gain PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
