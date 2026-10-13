# A minimal plugin with only discrete (bool / choice) parameters, used to
# dogfood the "Discrete parameter state restoration" test.
# Built only when PLUGINVAL_BUILD_TEST_PLUGINS is ON.

set(discrete_params_formats VST3)

if (APPLE)
    list(APPEND discrete_params_formats AU)
endif()

juce_add_plugin(pluginval_discrete_params
    PRODUCT_NAME                "pluginval Discrete Params"
    COMPANY_NAME                Tracktion
    BUNDLE_ID                   com.Tracktion.pluginvalDiscreteParams
    PLUGIN_MANUFACTURER_CODE    Trkt
    PLUGIN_CODE                 Pdp1
    FORMATS                     ${discrete_params_formats}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(pluginval_discrete_params PRIVATE
    DiscreteParamsPlugin.cpp
    DiscreteParamsPlugin.h)

target_compile_features(pluginval_discrete_params PRIVATE cxx_std_20)

target_compile_definitions(pluginval_discrete_params PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(pluginval_discrete_params PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
