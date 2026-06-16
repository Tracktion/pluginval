# Acceptance self-tests: drive the dogfood plugins through the full record/compare
# path against checked-in reference WAVs.
#
# The configs are checked-in templates (*.json.in). At configure time the dogfood
# plugin artefact paths, the input directory and the (source-tree) reference
# directory are substituted in. An artefact path may contain a generator
# expression (e.g. $<CONFIG> on multi-config generators), so we resolve it via
# file(GENERATE).

get_target_property(tone_artefact  pluginval_tone_generator_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
get_target_property(gain_artefact  pluginval_gain_VST3           JUCE_PLUGIN_ARTEFACT_FILE)
get_target_property(probe_artefact pluginval_playhead_probe_VST3 JUCE_PLUGIN_ARTEFACT_FILE)

set(ACCEPTANCE_REF_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/refs")
set(ACCEPTANCE_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/inputs")
set(TONE_GENERATOR_PLUGIN "${tone_artefact}")
set(GAIN_PLUGIN           "${gain_artefact}")
set(PLAYHEAD_PROBE_PLUGIN "${probe_artefact}")

set(acceptance_cases
    sine-440        # state.parameters path (waveform/frequency/gain by name)
    square-220      # state.parameters path, bit-exact square wave
    square-state    # state.file (setStateInformation) + parameter override precedence
    gain-half       # input.audio effect path: gain a full-height sine, length from input
    playhead-120)   # fixed-playhead path: probe writes the host transport into the output

foreach(case ${acceptance_cases})
    # Step 1: @-substitute the reference dir (and the possibly-genex plugin path).
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${case}.json.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${case}.json.gen" @ONLY)

    # Step 2: resolve any generator expression left in the plugin path. The output
    # path is per-config so multi-config generators don't collide on one file.
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${case}.json"
                  INPUT  "${CMAKE_CURRENT_BINARY_DIR}/${case}.json.gen")

    add_test(NAME "pluginval.acceptance.${case}"
             COMMAND pluginval test "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${case}.json")
endforeach()
