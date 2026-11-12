# Golden-trace gate (port/docs/ROADMAP.md stage 0.2).
# Runs the game headless with ARGS and a checkpoint trace (every EVERY frames:
# frame, phase, state hash, screen hash) and compares it with GOLDEN.
#   -DUPDATE=ON rewrites GOLDEN instead (only when a change is intended and
#   explained; faithful-mode changes are regressions by definition).
# On a mismatch it reports the first differing checkpoint; find the field with
#   blupi-sdl3 <same args> --state-dump <frame>:dump.txt
# on both builds and diff the dumps.
#   -DSTATE_ONLY=ON compares frame, phase and state hash only (runs whose
#   cursor legitimately differs from the golden run).
separate_arguments(args NATIVE_COMMAND "${ARGS}")
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
execute_process(
    COMMAND ${EXE} --headless --data ${DATA} --user ${WORK}/user ${args}
            --trace ${WORK}/trace.txt --trace-every ${EVERY}
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET TIMEOUT 900)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "game exited with ${rc}")
endif()
if(UPDATE)
    file(COPY_FILE ${WORK}/trace.txt ${GOLDEN})
    message(STATUS "updated ${GOLDEN}")
    return()
endif()
# The goldens are recorded with original, unplayed data. The game also reads its players, saves and
# user missions from the data folder (the port's user folder first, then DATA/), so a copy that has
# been played gives other traces: a mismatch names such files when there are any.
file(GLOB played RELATIVE ${DATA} ${DATA}/[Dd][Aa][Tt][Aa]/*)
list(FILTER played INCLUDE REGEX "/([Ii][Nn][Ff][Oo][0-9]+|[Ss][0-9]+-[0-9]+|[Uu][0-9]+-[0-9]+|[Tt][Ii][Mm][Ee]|[Bb][Ee][Nn][Cc][Hh])\\.[Bb][Ll][Pp]$")
set(hint "")
if(played)
    list(JOIN played ", " played)
    string(CONCAT hint "\nThe data folder holds files the game writes when it is played (${played}); the goldens\n"
           "were recorded with an unplayed copy (an Eggbert release ships INFO001.BLP and INFO002.BLP).")
endif()
file(STRINGS ${WORK}/trace.txt got)
file(STRINGS ${GOLDEN} want)
list(LENGTH got ngot)
list(LENGTH want nwant)
if(NOT ngot EQUAL nwant)
    message(FATAL_ERROR "trace has ${ngot} checkpoints, golden has ${nwant}${hint}")
endif()
math(EXPR last "${nwant} - 1")
foreach(i RANGE ${last})
    list(GET got ${i} g)
    list(GET want ${i} w)
    if(STATE_ONLY)
        string(REGEX REPLACE " [0-9a-f]+$" "" g "${g}")
        string(REGEX REPLACE " [0-9a-f]+$" "" w "${w}")
    endif()
    if(NOT g STREQUAL w)
        message(FATAL_ERROR "first difference at checkpoint ${i}:\n  golden: ${w}\n  now:    ${g}\n"
                            "(columns: frame phase state-hash screen-hash; trace in ${WORK})${hint}")
    endif()
endforeach()
message(STATUS "${nwant} checkpoints identical to ${GOLDEN}")
