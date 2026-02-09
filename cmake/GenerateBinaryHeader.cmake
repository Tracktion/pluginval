# GenerateBinaryHeader.cmake
#
# Converts a binary file into a C++ header containing a byte array.
# Similar to xxd -i but cross-platform (pure CMake).
#
# Usage:
#   cmake -DINPUT_FILE=/path/to/binary -DOUTPUT_FILE=/path/to/output.h -P GenerateBinaryHeader.cmake

if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE must be defined")
endif()

file(READ "${INPUT_FILE}" BINARY_DATA HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," HEX_ARRAY "${BINARY_DATA}")
file(SIZE "${INPUT_FILE}" BINARY_SIZE)

file(WRITE "${OUTPUT_FILE}"
    "#pragma once\n"
    "#include <cstddef>\n"
    "static const unsigned char vstvalidator_binary[] = {\n${HEX_ARRAY}\n};\n"
    "static const size_t vstvalidator_binary_len = ${BINARY_SIZE};\n")
