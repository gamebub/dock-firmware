execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE STRING_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX REPLACE "(..)" "0x\\1, " BYTE_HASH "${STRING_HASH}")
string(REGEX REPLACE ", $" "" BYTE_HASH "${BYTE_HASH}")

configure_file(
    ${INCLUDE}/git_commit.h.in
    ${CMAKE_BINARY_DIR}/generated/git_commit.h
)
