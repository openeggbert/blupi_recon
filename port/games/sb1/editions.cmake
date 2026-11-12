# The SB1-family editions with their own historical source (port/docs/EDITIONS.md): one program each,
# like blupi2-sdl3. Each compiles the shared historical TUs from its edition's tree, an overlay file
# replacing a tree file of the same name (tools/source_tree.py's rule), and the port's SB1 code
# (games/sb1/game) once more in that context, with its edition's identity:
#   BLUPI_EDITION_KEY  1 eggbert-demo, 2 eggbert-value, 3 blupi-demo
# Desktop only; the historical TUs are never edited, and every edition keeps its own restrictions.
function(blupi_sb1_edition NAME KEY TREE OVERLAY DESCRIPTION PRODUCT)
    set(case_dir ${CMAKE_CURRENT_BINARY_DIR}/compat_case_${NAME})
    file(MAKE_DIRECTORY ${case_dir})
    if(NOT CMAKE_HOST_WIN32)
        file(WRITE ${case_dir}/Windows.h "#include \"${BLUPI_COMMON}/compat/include/windows.h\"\n")
        file(WRITE ${case_dir}/DEF.H "#include \"${TREE}/def.h\"\n")
    endif()
    add_library(${NAME}_context INTERFACE)
    target_include_directories(${NAME}_context BEFORE INTERFACE ${BLUPI_COMMON}/compat/include ${case_dir} ${TREE})
    target_compile_definitions(${NAME}_context INTERFACE BLUPI_EDITION_KEY=${KEY})
    if(MSVC)
        target_compile_options(${NAME}_context INTERFACE
            /FI${BLUPI_COMMON}/compat/include/port_forced.h
            /FI${BLUPI_GAME_DIR}/game/port_switches.h
            /permissive /Zc:forScope- /W0)
    else()
        target_compile_options(${NAME}_context INTERFACE
            "SHELL:-include ${BLUPI_COMMON}/compat/include/port_forced.h"
            "SHELL:-include ${BLUPI_GAME_DIR}/game/port_switches.h"
            -fno-strict-aliasing -fwrapv -fsigned-char -w
            $<$<CXX_COMPILER_ID:GNU>:-fpermissive>
            $<$<CXX_COMPILER_ID:Clang>:-fms-extensions>)
    endif()

    set(tus)
    foreach(tu ${BLUPI_SHARED_TU_NAMES})
        if(OVERLAY AND EXISTS ${OVERLAY}/${tu})
            list(APPEND tus ${OVERLAY}/${tu})
        else()
            list(APPEND tus ${TREE}/${tu})
        endif()
    endforeach()
    add_library(${NAME}_hist OBJECT ${tus})
    target_link_libraries(${NAME}_hist PRIVATE ${NAME}_context)
    if(MSVC)
        target_compile_options(${NAME}_hist PRIVATE /source-charset:.1252 /execution-charset:.1252
            /FI${BLUPI_COMMON}/compat/include/port_hist_stdio.h)
    else()
        target_compile_options(${NAME}_hist PRIVATE
            $<$<CXX_COMPILER_ID:GNU>:-finput-charset=CP1252>
            $<$<CXX_COMPILER_ID:GNU>:-fexec-charset=CP1252>
            "SHELL:-include ${BLUPI_COMMON}/compat/include/port_hist_stdio.h")
    endif()

    add_library(${NAME}_game STATIC ${BLUPI_SB1_PORT_SOURCES} $<TARGET_OBJECTS:${NAME}_hist>)
    target_link_libraries(${NAME}_game PRIVATE ${NAME}_context PUBLIC blupi_platform blupi_app)
    target_compile_definitions(${NAME}_game PRIVATE BLUPI_PORT_VERSION="${BLUPI_PORT_VERSION}")
    target_include_directories(${NAME}_game PRIVATE ${BLUPI_GAME_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_sources(${NAME}_game PRIVATE ${BLUPI_LANGUAGE_INC})

    add_executable(${NAME} ${BLUPI_COMMON}/platform/entry.cpp)
    set_target_properties(${NAME} PROPERTIES OUTPUT_NAME ${NAME})
    target_link_libraries(${NAME} PRIVATE ${NAME}_game SDL3::SDL3)
    if(WIN32)
        set(BLUPI_RC_ICON ${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico)
        set(BLUPI_RC_DESCRIPTION "${DESCRIPTION}")
        set(BLUPI_RC_NAME ${NAME})
        set(BLUPI_RC_PRODUCT "${PRODUCT}")
        configure_file(${BLUPI_COMMON}/platform/blupi_win.rc.in ${CMAKE_CURRENT_BINARY_DIR}/generated/${NAME}_win.rc @ONLY)
        target_sources(${NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/${NAME}_win.rc)
    endif()
    blupi_package_app(${NAME})
endfunction()

if(NOT ANDROID)
    set(BLUPI_REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)
    blupi_sb1_edition(eggbert-demo-sdl3 1 ${BLUPI_HIST_SRC} ${BLUPI_REPO}/games/sb1-eggbert-demo
        "Speedy Eggbert Demo (SDL3 port)" "Speedy Eggbert Demo SDL3 port")
    blupi_sb1_edition(eggbert-value-sdl3 2 ${BLUPI_HIST_SRC} ${BLUPI_REPO}/games/sb1-eggbert-value
        "Speedy Eggbert SE Valueware (SDL3 port)" "Speedy Eggbert SE Valueware SDL3 port")
    blupi_sb1_edition(blupi-demo-sdl3 3 ${BLUPI_REPO}/games/sb1-blupi-demo/src ""
        "Speedy Blupi Demo, June 1998 (SDL3 port)" "Speedy Blupi Demo SDL3 port")
endif()
