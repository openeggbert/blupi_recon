# Higher-resolution images (port/docs/HD.md): the game runs headless with --hd-scale SCALE and --hd-check,
# which compares, at every Display, the mirrored frame with the exact enlargement of the faithful one.
# With PACK_IMAGES, a synthetic pack of those images enlarged (tools/make_hd_test_pack.py) is made
# first (once per build tree) and used: the frames must still be the enlargements, and the images must
# come from the pack.
#   -DEXE -DDATA -DWORK -DSCALE -DARGS [-DPYTHON -DTOOL -DPACK_IMAGES=name,name,... -DPACK_DIR=dir]
separate_arguments(args NATIVE_COMMAND "${ARGS}")
file(REMOVE_RECURSE ${WORK}/user)
file(MAKE_DIRECTORY ${WORK}/user)
set(pack_args "")
if(PACK_IMAGES)
    set(pack ${PACK_DIR})
    if(NOT EXISTS ${pack}/done)
        file(REMOVE_RECURSE ${pack})
        execute_process(COMMAND ${PYTHON} ${TOOL} --game ${DATA} --scale ${SCALE} --output ${pack} --only ${PACK_IMAGES}
                        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "pack: ${rc}\n${out}")
        endif()
        file(WRITE ${pack}/done "")
    endif()
    set(pack_args --hd-pack ${pack})
endif()
execute_process(COMMAND ${EXE} --headless --data ${DATA} --user ${WORK}/user --hd-scale ${SCALE} ${pack_args}
                        --hd-check ${args}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 900)
string(REGEX MATCH "hd check: ([0-9]+) displays compared, ([0-9]+) not the enlargement[^\n]*, ([0-9]+) images from the pack" line "${out}")
if(NOT rc EQUAL 0 OR NOT line)
    message(FATAL_ERROR "exit ${rc}\n${out}\n${err}")
endif()
if(CMAKE_MATCH_1 EQUAL 0 OR NOT CMAKE_MATCH_2 EQUAL 0)
    message(FATAL_ERROR "${line}")
endif()
if(PACK_IMAGES AND CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "no image came from the pack: ${line}")
endif()
message(STATUS "${line}")
