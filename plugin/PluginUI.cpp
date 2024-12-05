#include "PluginUI.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"

// Forward decls.
static void glfw_error_callback(int error, const char *description);
static void glfw_window_close_callback(GLFWwindow *window);

static uint32_t glfw_initialized_cnt = 0;

GlfwBackendExampleUI::GlfwBackendExampleUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
    fWindow(NULL),
    fMyImGuiContext(nullptr)
{
    openEditor();
}

GlfwBackendExampleUI::~GlfwBackendExampleUI()
{
    d_stderr2("UI Destructor invoked");

    if (fWindow)
        glfwSetWindowShouldClose(fWindow, true);

    closeEditor();
}

void GlfwBackendExampleUI::openEditor()
{
    // Initialize GLFW in main thread
    if (!setupGLFW())
        return;

    // Launch drawing thread
    fDrawingThread = std::thread(imgui_drawing_thread, this);
}

void GlfwBackendExampleUI::closeEditor()
{
    DISTRHO_SAFE_ASSERT_RETURN(fWindow != NULL, )

    // Block main thread until drawing thread finishes
    if (fDrawingThread.joinable())
        fDrawingThread.join();

    // OK, now let's clean up GLFW instance
    if (fMyImGuiContext)
    {
        fMyImGuiContext = nullptr;

        glfwDestroyWindow(fWindow);
        if (!--glfw_initialized_cnt)
            glfwTerminate();

        // Manually reset the pointer of GLFW window
        // glfwDestroyWindow() invokes free(), but free() won't reset pointer to NULL.
        // By reset, the destructor can determine if it needs to call closeWindow() in case user forgets.
        fWindow = NULL;
    }
}


bool GlfwBackendExampleUI::setupGLFW()
{
    /**
     * Setup window as well as initialized count.
     * Here I manage a reference count of UI instances and keep glfw initialized
     * when you have multiple instances open
     *
     * This is Justin Frankel's implementation. Noizebox also has this feature
     * included in his modded GLFW (from which I forked)
     */
    if (!glfw_initialized_cnt++)
    {
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            return GLFW_FALSE;
    }

    // Omit explicit version specification to let GLFW guess GL version,
    // or GLFW will fail to load on old environments with GL 2.x
#if 0
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Enable embedded window
    if (!isStandalone())
    {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Do not allow resizing
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Disable decoration. Or you will see a weird titlebar :-)

        glfwWindowHint(GLFW_EMBEDDED_WINDOW, GLFW_TRUE);
        glfwWindowHintVoid(GLFW_PARENT_WINDOW_ID, (void*)getParentWindowHandle());
    }

    fWindow = glfwCreateWindow(getWidth(), getHeight(), DISTRHO_PLUGIN_NAME, NULL, NULL); // This size is only the standalone window's size, NOT editor's size
    if (fWindow == NULL)
        return GLFW_FALSE;

    // Explicitly set window position to avoid occasional misplace (offset)
    glfwSetWindowPos(fWindow, 0, 0);

    // Set window close callback on standalone mode, otherwise the window cannot exit
    if (isStandalone())
    {
        glfwSetWindowCloseCallback(fWindow, glfw_window_close_callback);
    }

    // Store UI pointer into GLFW
    glfwSetWindowUserPointer(fWindow, this);

    return GLFW_TRUE;
}

/**
 * Setup ImGui instance.
 * Must be executed under the drawing thread.
 */
void GlfwBackendExampleUI::setupImGui()
{
    /**
     * The following two functions MUST be executed under the drawing thread.
     * Because only one thread can access the current GLFW context at a time.
     *
     * When you call glfwMakeContextCurrent() in main thread, it will take over
     * the context. So if you then call it in drawing thread, GLFW will throw
     * an error:
     *     Glfw Error 65544: WGL: Failed to make context current:
     *                             The requested resource is in use.
     *
     * What's more, glfwSwapInterval() requires valid current context, otherwise
     * it won't work.
     */
    glfwMakeContextCurrent(fWindow);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    fMyImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(fMyImGuiContext);

    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Set actual editor UI size here
    // TODO: Apply size changes on UI::sizeChanged()
    io.DisplaySize.x = (float)getWidth();
    io.DisplaySize.y = (float)getHeight();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    /**
     * Setup Platform/Renderer backends.
     *
     * NOTICE: We need to register our own GLFW event callbacks.
     *         See glfw_callbacks.cpp for more details.
     *
     * FIXME: The newly included ImGui GLFW callback ImGui_ImplGlfw_WndProc()
     *        is buggy in my project.
     *
     * When I test the plugin with SaviHost, the host UI hangs, plugin UI freezes,
     * and Wine reports the following error:
     *
     *     011c:err:seh:user_callback_handler ignoring exception c0000005
     *
     * On multi-thread environment, the Win32 WndProc callback ImGui_ImplGlfw_WndProc()
     * cannot get the current ImGui context as well. The context is empty in the function.
     * Error code c0000005 means "Memory violation".
     *
     * This is quite similar to the issue described in glfw_callbacks.cpp.
     * 
     * To fix this problem, just restore GLFW window's WndProc hook to its default value
     * soon after ImGui_ImplGlfw_InitForOpenGL().
     */
#if _WIN32  // HACK: Workaround to deal with ImGui_ImplGlfw_WndProc() issue.

    // Get and save the original WndProc hook
    fPrevWndProc = (WNDPROC)::GetWindowLongPtrW(glfwGetWin32Window(fWindow), GWLP_WNDPROC);
    IM_ASSERT(fPrevWndProc != nullptr);

    // Initialize ImGui GLFW backend as usual
    ImGui_ImplGlfw_InitForOpenGL(fWindow, false); // Do not register callbacks automatically

    // Restore our WndProc hook, so we can disable ImGui's own hook (aka. ImGui_ImplGlfw_WndProc()).
    ::SetWindowLongPtrW(glfwGetWin32Window(fWindow), GWLP_WNDPROC, (LONG_PTR)fPrevWndProc);
    fPrevWndProc = nullptr;

    // Initialize OpenGL renderer
    ImGui_ImplOpenGL2_Init();
#else
    ImGui_ImplGlfw_InitForOpenGL(fWindow, false); // Do not register callbacks automatically
    ImGui_ImplOpenGL2_Init();
#endif

    // Register my own callbacks
    _setMyGLFWCallbacks();

    // Load the first font (default font)
    // TODO: Convert my own font.
    //io.Fonts->AddFontFromMemoryCompressedTTF(font_compressed_data, font_compressed_size, 16);
}

void GlfwBackendExampleUI::drawFrame()
{

    // The main drawing process
    // Remember to check myImGuiContext before drawing frames, or ImGui_ImplOpenGL2_NewFrame() may execute
    // on an empty context after closeEditor()!
    if (fMyImGuiContext)
    {
        ImGui::SetCurrentContext(fMyImGuiContext);    

        // Process IO events.
        // NOTICE: IO event should be invoked on main thread. See GlfwBackendExampleUI::uiIdle().
        //glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw main editor window
        ImGui::ShowDemoWindow();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(fWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        //ImGuiIO &io = ImGui::GetIO();
        //glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        static constexpr ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
        // you may need to backup/reset/restore other state, e.g. for current shader using the commented lines below.
        //GLint last_program;
        //glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        //glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        //glUseProgram(last_program);

        // Let GLFW render our UI
        // Omitting those two function calls will end up with a blank window.
        glfwMakeContextCurrent(fWindow);
        glfwSwapBuffers(fWindow);
    }
}

/**
    A parameter has changed on the plugin side.
    This is called by the host to inform the UI about parameter changes.
*/
void GlfwBackendExampleUI::parameterChanged(uint32_t index, float value)
{
    d_stdout("parameterChanged %u %f", index, value);

    switch (index)
    {
    case kParameterWidth:
        setWidth(static_cast<int>(value + 0.5f));
        break;
    case kParameterHeight:
        setHeight(static_cast<int>(value + 0.5f));
        break;
    }
}


void GlfwBackendExampleUI::uiIdle()
{
    DISTRHO_SAFE_ASSERT_RETURN(fWindow != NULL && fMyImGuiContext != nullptr, )

    if (!glfwWindowShouldClose(fWindow))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

        // NOTICE: After applying our own callback, we should invoke glfwPollEvents() in MAIN thread,
        //         not drawing thread.
        //         (According to GLFW's document.)
        // FIXME: This is not fluent enough (event sample rate is obviously lower!)
        glfwPollEvents();
    }
}


void GlfwBackendExampleUI::focus()
{
    // Noop.
}

void GlfwBackendExampleUI::sizeChanged(uint width, uint height)
{
    DISTRHO_SAFE_ASSERT_RETURN(fWindow != NULL && fMyImGuiContext != nullptr, )

    glfwSetWindowSize(fWindow, width, height);

    ImGui::SetCurrentContext(fMyImGuiContext);

    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    io.DisplaySize.x = (float)getWidth();
    io.DisplaySize.y = (float)getHeight();
}

void GlfwBackendExampleUI::titleChanged(const char* title)
{
    (void) title;
}

void GlfwBackendExampleUI::visibilityChanged(bool visibility)
{
    (void) visibility;
}

void GlfwBackendExampleUI::transientParentWindowChanged(const uintptr_t winId)
{
    (void) winId;
}


/**
 * The drawing thread function.
 * It should be a global function rather than a class member.
 *
 * @param editor The active editor instance.

 * FIXME: Try non-static function, and set friend in the UI class?
 */
static void imgui_drawing_thread(GlfwBackendExampleUI *editor)
{
    // Setup ImGui
    editor->setupImGui();

    // Render UI
    while (!glfwWindowShouldClose(editor->getWindow())) {
        editor->drawFrame();
    }

    // Set current context to make sure that the following two shutdown functions
    // can be in right context
    ImGui::SetCurrentContext(editor->getImGuiContext());

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(editor->getImGuiContext());

    d_stderr2("Drawing thread finished!");
}


static void glfw_error_callback(int error, const char *description)
{
    d_stderr("Glfw Error %d: %s\n", error, description);
#if _WIN32
    MessageBoxA(NULL, (LPCSTR)description, "Error", MB_OK);
#endif
}

static void glfw_window_close_callback(GLFWwindow *window)
{
    d_stderr("Window close callback");

    // Explicitly request DISTRHO UI to close, by invoking DISTRHO::UI::hide().
    // Reference: deps/dpf/examples/EmbedExternalUI/EmbedExternalExampleUI.cpp
    static_cast<GlfwBackendExampleUI *>(glfwGetWindowUserPointer(window))->hide();
}

/* ------------------------------------------------------------------------------------------------------------
 * UI entry point, called by DPF to create a new UI instance. */

START_NAMESPACE_DISTRHO

UI* createUI()
{
    d_stderr("Creating UI...");
	return new GlfwBackendExampleUI();
}

END_NAMESPACE_DISTRHO
