cmake_minimum_required (VERSION 3.20)

project (Glfw_Backend_Example_Plugin
    LANGUAGES C CXX
)

add_subdirectory (deps/dpf)


# Enable -fPIC to let static library link properly
add_compile_options (-fPIC)


#
# Build my own GLFW
#
set(BUILD_SHARED_LIBS       OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES     OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS        OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS         OFF CACHE BOOL "")
set(GLFW_INSTALL            OFF CACHE BOOL "")
set(GLFW_THREAD_LOCAL_DATA  ON  CACHE BOOL "")
set(GLFW_VULKAN_STATIC      OFF CACHE BOOL "")
add_subdirectory(deps/glfw)

# Set GLFW native API exposure
if (WIN32)
  add_compile_definitions (GLFW_EXPOSE_NATIVE_WIN32=1)
else ()
  add_compile_definitions (GLFW_EXPOSE_NATIVE_X11=1)
endif ()

#
# Build plugin
#

set (DEAR_IMGUI_DIR ${PROJECT_SOURCE_DIR}/deps/imgui)
set (DEAR_IMGUI_STUFF
  ${DEAR_IMGUI_DIR}/imgui.cpp
  ${DEAR_IMGUI_DIR}/imgui_demo.cpp
  ${DEAR_IMGUI_DIR}/imgui_draw.cpp
  ${DEAR_IMGUI_DIR}/imgui_tables.cpp
  ${DEAR_IMGUI_DIR}/imgui_widgets.cpp
  ${DEAR_IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${DEAR_IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)

dpf_add_plugin(${PROJECT_NAME}
  TARGETS lv2 vst2 vst3 clap jack
  UI_TYPE external
  FILES_DSP
    plugin/PluginDSP.cpp
  FILES_UI
    plugin/static_instance.cpp
    plugin/PluginUI.cpp
    ${DEAR_IMGUI_STUFF}
    plugin/glfw_callbacks.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    plugin/
    ${DEAR_IMGUI_DIR}
    deps/glfw/include
)

# Include my own ImGui config
# Contains workaround for thread-safety
target_compile_definitions(${PROJECT_NAME}-ui PRIVATE IMGUI_USER_CONFIG="${PROJECT_SOURCE_DIR}/plugin/imconfig.h")

# Link against OpenGL library
if (WIN32)
  set (OPENGL_LIBRARIES -lopengl32)       # Must link against opengl32 to avoid link error
else ()
  set (OPENGL_LIBRARIES -lGL)
endif ()

target_link_directories (${PROJECT_NAME} PUBLIC ${GLFW_BINARY_DIR}/src)
target_link_libraries (${PROJECT_NAME} PRIVATE glfw ${OPENGL_LIBRARIES})
