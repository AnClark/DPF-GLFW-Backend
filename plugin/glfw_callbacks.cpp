/*
 *  glfw_callbacks.cpp
 *
 *  Copyright (c) 2022 AnClark Liu
 *
 *  This file is part of amsynth.
 *
 *  amsynth is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  amsynth is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with amsynth.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * Workaround for wrong ImGui context issue:
 *   - Empty ImGui context issue on Windows
 *   - Wrong interactions on Linux (e.g. A mouse event has effect on all GLFW instances)
 *
 * On Windows, when I've used dedicated thread for ImGui rendering, and
 * used TLS for GImGui, there's still a bug:
 *
 * When I move mouse into editor window, plugin crashes. GDB shows that it
 * crashes when ImGui's GLFW callback function is accessing backend data
 * via ImGui_ImplGlfw_GetBackendData() (returns current ImGui context).
 *
 * On Linux, when I open more than one instance in DAWs, the plugin won't crash,
 * but when I interact with one instance, the other instances will also respond
 * input events.
 *
 * The two issues above are because GLFW queries window events in main thread,
 * not in drawing thread.
 *   - On Windows, since the two threads have their own GImGui instance, GImGui
 *     will be null in main thread.
 *   - On Linux, it seems that all threads share the same GImGui instance.
 *
 * @bear24rw (Max Thurn) provided a workaround: register our own glfw
 * callbacks to ensure the imgui context is set correctly before fowarding
 * the call to the imgui backend.
 *
 * This workaround doesn't aim at multithreading, but works well for me!
 *
 * Reference: https://github.com/ocornut/imgui/pull/3934#issuecomment-873213161
 *
 */

/*
 *         ========  Special Notice ========
 * After applying our own callbacks, remember to invoke glfwPollEvents() in MAIN thread!
 * Here I invoke it on GlfwBackendExampleUI::uiIdle().
 */

#include "PluginUI.hpp"
#include "backends/imgui_impl_glfw.h"

void GlfwBackendExampleUI::_setMyGLFWCallbacks()
{
    // Define intermediate callback functions.
    // Those intermediates will execute callbacks defined in GlfwBackendExampleUI's
    // instance.
    auto char_callback_func = [](GLFWwindow *w, unsigned int c) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_charCallback(c);
    };
    auto cursor_enter_callback_func = [](GLFWwindow *w, int entered) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_cursorEnterCallback(entered);
    };
    auto mouse_button_callback_func = [](GLFWwindow *w, int button, int action, int mods) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_mouseButtonCallback(button, action, mods);
    };
    auto scroll_callback_func = [](GLFWwindow *w, double xoffset, double yoffset) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_scrollCallback(xoffset, yoffset);
    };
    auto key_callback_func = [](GLFWwindow *w, int key, int scancode, int action, int mods) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_keyCallback(key, scancode, action, mods);
    };
    auto cursor_pos_callback_func = [](GLFWwindow *w, double x, double y) {
        static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(w))->_cursorPosCallback(x, y);
    };

    // Register my own callbacks
    glfwSetCharCallback(fWindow, char_callback_func);
    glfwSetCursorEnterCallback(fWindow, cursor_enter_callback_func);
    glfwSetMouseButtonCallback(fWindow, mouse_button_callback_func);
    glfwSetScrollCallback(fWindow, scroll_callback_func);
    glfwSetKeyCallback(fWindow, key_callback_func);
    glfwSetCursorPosCallback(fWindow, cursor_pos_callback_func);    // On Linux, this callback is essential

    // Set window user pointer to ImguiEditor's current instance
    glfwSetWindowUserPointer(fWindow, this);
}

// ---------- CALLBACKS ----------
// These callbacks will first set correct ImGui context, then invoke ImGui's own
// callbacks. This can prevent GLFW from accessing wrong ImGui instance.

void GlfwBackendExampleUI::_charCallback(unsigned int c)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_CharCallback(fWindow, c);
}

void GlfwBackendExampleUI::_cursorEnterCallback(int entered)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_CursorEnterCallback(fWindow, entered);
}

void GlfwBackendExampleUI::_mouseButtonCallback(int button, int action, int mods)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_MouseButtonCallback(fWindow, button, action, mods);
}

void GlfwBackendExampleUI::_scrollCallback(double xoffset, double yoffset)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_ScrollCallback(fWindow, xoffset, yoffset);
}

void GlfwBackendExampleUI::_keyCallback(int key, int scancode, int action, int mods)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_KeyCallback(fWindow, key, scancode, action, mods);
}

// On Linux, this callback is essential
void GlfwBackendExampleUI::_cursorPosCallback(double x, double y)
{
    ImGui::SetCurrentContext(this->fMyImGuiContext);
    ImGui_ImplGlfw_CursorPosCallback(fWindow, x, y);
}
