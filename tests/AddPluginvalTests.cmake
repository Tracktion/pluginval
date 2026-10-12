include_guard (GLOBAL)

find_program (PLUGINVAL_PROGRAM pluginval DOC "pluginval executable")

if (NOT PLUGINVAL_PROGRAM)
	message (WARNING "pluginval not found!")
endif ()

set (PLUGINVAL_STRICTNESS 10 CACHE STRING "pluginval strictness level, 1 to 10")

set_property (
	CACHE PLUGINVAL_STRICTNESS
	PROPERTY STRINGS
			 1 2 3 4 5 6 7 8 9 10)

set (PLUGINVAL_REPEATS 0 CACHE STRING "number of times to repeat pluginval tests")

set (PLUGINVAL_SAMPLERATES "44100;44800;96000" CACHE STRING "samplerates to test in pluginval")

set (PLUGINVAL_BLOCKSIZES "1;250;512" CACHE STRING "blocksizes to test in pluginval")

mark_as_advanced (
	PLUGINVAL_PROGRAM PLUGINVAL_STRICTNESS PLUGINVAL_REPEATS 
	PLUGINVAL_SAMPLERATES PLUGINVAL_BLOCKSIZES
)

#

#[[
	add_pluginval_tests (
		pluginTarget
		[TEST_PREFIX <prefix>]
		[LOG_DIR <dir>]
		[NAMES_OUT <var>]
	)

	TEST_PREFIX defaults to ${pluginTarget}.pluginval

	LOG_DIR - relative paths will be evaluated relative to the current binary directory

	NAMES_OUT can be name of a variable to be poplated with test names in calling scope
]]
function (add_pluginval_tests pluginTarget)

	if (NOT TARGET "${pluginTarget}")
		message (
			FATAL_ERROR "${CMAKE_CURRENT_FUNCTION} - target '${pluginTarget}' does not exist!"
		)
	endif ()

	cmake_parse_arguments (ARG "" "TEST_PREFIX;LOG_DIR;NAMES_OUT" "" ${ARGN})

	if (ARG_NAMES_OUT)
		unset ("${ARG_NAMES_OUT}" PARENT_SCOPE)
	endif ()

	if (NOT PLUGINVAL_PROGRAM)
		return ()
	endif ()

	if (NOT ARG_TEST_PREFIX)
		set (ARG_TEST_PREFIX "${pluginTarget}.pluginval")
	endif ()

	list (JOIN PLUGINVAL_SAMPLERATES "," sample_rates)
	list (JOIN PLUGINVAL_BLOCKSIZES "," block_sizes)

	set (test_name "${ARG_TEST_PREFIX}.VST3")

	get_target_property (plugin_artefact "${pluginTarget}_VST3" JUCE_PLUGIN_ARTEFACT_FILE)

	if (ARG_LOG_DIR)
		if (NOT IS_ABSOLUTE "${ARG_LOG_DIR}")
			set (ARG_LOG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ARG_LOG_DIR}")
		endif ()

		set (log_dir_arg --output-dir "${ARG_LOG_DIR}")
	else ()
		unset (log_dir_arg)
	endif ()

	add_test (NAME "${test_name}"
			  COMMAND "${PLUGINVAL_PROGRAM}"
						--strictness-level "${PLUGINVAL_STRICTNESS}"
						--sample-rates "${sample_rates}"
						--block-sizes "${block_sizes}"
						--repeat "${PLUGINVAL_REPEATS}"
						--randomise
						--validate "${plugin_artefact}"
						${log_dir_arg}
	)

	set_tests_properties (
		"${test_name}" PROPERTIES REQUIRED_FILES "${plugin_artefact}"
	)

	message (VERBOSE "Added pluginval test for plugin target ${format_target}")

	if (ARG_NAMES_OUT)
		set ("${ARG_NAMES_OUT}" "${test_name}" PARENT_SCOPE)
	endif ()

endfunction ()
