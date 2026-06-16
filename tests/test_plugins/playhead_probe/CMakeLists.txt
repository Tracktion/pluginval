# A minimal, deterministic plugin that writes the host transport into its output,
# used to dogfood the acceptance feature's fixed-playhead support. Built only when
# PLUGINVAL_BUILD_TEST_PLUGINS is ON.

juce_add_plugin(pluginval_playhead_probe
    PRODUCT_NAME                "pluginval Playhead Probe"
    COMPANY_NAME                Tracktion
    BUNDLE_ID                   com.Tracktion.pluginvalPlayheadProbe
    PLUGIN_MANUFACTURER_CODE    Trkt
    PLUGIN_CODE                 Pph1
    FORMATS                     VST3 $<$<PLATFORM_ID:Darwin>:AU>
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    VST3_CATEGORIES             Generator
    AU_MAIN_TYPE                kAudioUnitType_Generator
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(pluginval_playhead_probe PRIVATE
    PlayheadProbePlugin.cpp
    PlayheadProbePlugin.h)

target_compile_features(pluginval_playhead_probe PRIVATE cxx_std_20)

target_compile_definitions(pluginval_playhead_probe PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(pluginval_playhead_probe PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
