AUX_SOURCE_DIRECTORY(. SRC_LIST)
add_library(emshell ${SRC_LIST})

add_custom_command(TARGET emshell
    POST_BUILD
    COMMENT "[ generate doc ]"
    COMMAND doxygen 2>/dev/null
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    #VERBATIM
    )

add_custom_command(TARGET emshell
    POST_BUILD
    COMMENT "[ astyle code ]"
    COMMAND astyle src/*.c src/*.h 1>/dev/null 2>/dev/null
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    #VERBATIM
    )

