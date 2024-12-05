// synthv1_dpfui.h
//
/****************************************************************************
   Copyright (C) 2023, AnClark Liu. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __synthv1_dpfui_h
#define __synthv1_dpfui_h


#include "DistrhoUI.hpp"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui.h>

#include <thread>


// -----------------------------------------------------------------------------------------------------------
// SynthV1PluginUI - DPF Plugin UI interface.


class GlfwBackendExampleUI : public UI {

    // ----------------------------------------------------------------------------------------------------------------
    std::thread fDrawingThread;

    GLFWwindow *fWindow;
    ImGuiContext *fMyImGuiContext = nullptr;

public:
    GlfwBackendExampleUI();
    ~GlfwBackendExampleUI();

    GLFWwindow *getWindow() { return this->fWindow; }
    ImGuiContext *getImGuiContext() { return this->fMyImGuiContext; }

    void openEditor();
    void closeEditor();

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // DSP/Plugin Callbacks

    void parameterChanged(uint32_t index, float value) override;
    //void programLoaded(uint32_t index) override;
    //void stateChanged(const char* key, const char* value) override;

    // ----------------------------------------------------------------------------------------------------------------
    // External window overrides

    void focus() override;
    void sizeChanged(uint width, uint height) override;
    void titleChanged(const char* const title) override;
    void transientParentWindowChanged(const uintptr_t winId) override;
    void visibilityChanged(const bool visible) override;
    void uiIdle() override;

public:

    // ----------------------------------------------------------------------------------------------------------------
    // Renderer methods. Invoked by drawing thread.
    // NOTE: C++ does not support assigning a static function as friend function. So I have to use public function.
    
    bool setupGLFW();
    void setupImGui();
    void drawFrame();

private:

    // ----------------------------------------------------------------------------------------------------------------
    // GLFW Callbacks.

    void _setMyGLFWCallbacks();

    void _charCallback(unsigned int c);
    void _cursorEnterCallback(int entered);
    void _mouseButtonCallback(int button, int action, int mods);
    void _scrollCallback(double xoffset, double yoffset);
    void _keyCallback(int key, int scancode, int action, int mods);
    void _cursorPosCallback(double x, double y);

#if _WIN32
    WNDPROC fPrevWndProc;
#endif

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlfwBackendExampleUI)
};

// -----------------------------------------------------------------------------------------------------------

static void imgui_drawing_thread(GlfwBackendExampleUI *editorInstance);

#endif// __synthv1_dpfui_h

// end of synthv1_dpfui.h