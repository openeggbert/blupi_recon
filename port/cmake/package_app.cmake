# Desktop runtime layout, shared by SB1 and SB2. Android packages SB1 as libmain.
function(blupi_package_app target)
    if(ANDROID)
        return()
    endif()
    if(EMSCRIPTEN)
        # The web build (port/docs/WEB.md): <program>.js and .wasm, started by port/web/loader.js
        # once the player's game folder is in the browser's storage (IDBFS under /persist).
        # ASYNCIFY lets the launcher's blocking loop yield to the browser while it waits.
        set_target_properties(${target} PROPERTIES SUFFIX ".js")
        target_link_options(${target} PRIVATE
            -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=5MB
            -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sFORCE_FILESYSTEM=1 -sENVIRONMENT=web -lidbfs.js
            "-sEXPORTED_RUNTIME_METHODS=['FS','callMain','addRunDependency','removeRunDependency']")
        return()
    endif()
    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:${target}>)
    endif()
    if(MINGW)
        target_link_options(${target} PRIVATE -static)
    endif()
    if(BLUPI_PORTABLE AND UNIX AND NOT APPLE)
        set_target_properties(${target} PROPERTIES BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "$ORIGIN")
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared>
                $<TARGET_FILE_DIR:${target}>/$<TARGET_SONAME_FILE_NAME:SDL3::SDL3-shared>)
    endif()
    if(BLUPI_BUNDLE_SOUNDFONT AND BLUPI_SOUNDFONT_FILE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${BLUPI_SOUNDFONT_FILE} $<TARGET_FILE_DIR:${target}>/soundfont.sf2)
    endif()
endfunction()
