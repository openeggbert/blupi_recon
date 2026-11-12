# An edition program starts with its data, shows its title screen and plays the attract-mode demo:
# exit 0, and the frame trace reaches PLAY (WM_PHASE_PLAY 1525) after the title (1524).
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/user)
execute_process(COMMAND ${EXE} --headless --data ${DATA} --user ${WORK}/user --frames 1500
                        --trace ${WORK}/trace.txt
                RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out TIMEOUT 300)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "exit ${result}:\n${out}")
endif()
file(STRINGS ${WORK}/trace.txt lines)
set(title FALSE)
set(play FALSE)
foreach(line IN LISTS lines)
    if(line MATCHES "^[0-9]+ 1524 ")
        set(title TRUE)
    elseif(line MATCHES "^[0-9]+ 1525 " AND title)
        set(play TRUE)
    endif()
endforeach()
if(NOT title OR NOT play)
    message(FATAL_ERROR "title ${title}, attract-mode play ${play} (trace ${WORK}/trace.txt)")
endif()
message(STATUS "title screen, then the attract-mode demo in play")
