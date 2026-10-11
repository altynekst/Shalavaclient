cmake_minimum_required(VERSION 3.16)
project(ShalawaLoader LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL_LIB_DIR "lib64")
else()
    set(SDL_LIB_DIR "lib32")
endif()

add_executable(ShalawaLoader WIN32
    main.cpp
    Utils.cpp
    Network.cpp
    Config.cpp
    Auth.cpp
    Client.cpp
    UI.cpp
    Crypto.cpp
    eastereggs.cpp
    antifrida.cpp
    AntiDebug.cpp
    AdvancedProtection.cpp
    Heartbeat.cpp
    Integrity.cpp
    HardenedProtection.cpp
    ../src/engine/external/imgui/imgui.cpp
    ../src/engine/external/imgui/imgui_draw.cpp
    ../src/engine/external/imgui/imgui_tables.cpp
    ../src/engine/external/imgui/imgui_widgets.cpp
    ../src/engine/external/imgui/imgui_impl_sdl2.cpp
    ../src/engine/external/imgui/imgui_impl_opengl3.cpp
    ../src/engine/external/json-parser/json.c
)

target_include_directories(ShalawaLoader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../src/engine/external/imgui
    ../src/engine/external/json-parser
    ../ddnet-libs/sdl/include/windows
)

if(WIN32)
    target_link_libraries(ShalawaLoader PRIVATE
        opengl32
        dwmapi
        winhttp
        windowscodecs
        crypt32
        psapi
        bcrypt
        ole32
    )
    
    target_link_directories(ShalawaLoader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../ddnet-libs/sdl/windows/${SDL_LIB_DIR}
    )
    
    target_link_libraries(ShalawaLoader PRIVATE SDL2)

    add_custom_command(TARGET ShalawaLoader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../ddnet-libs/sdl/windows/${SDL_LIB_DIR}/SDL2.dll"
        "$<TARGET_FILE_DIR:ShalawaLoader>/SDL2.dll"
    )
endif()