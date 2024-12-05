#include "imgui_internal.h"

/**
 * Use TLS (Thread-local Storage) for ImGui's global context pointer
 *
 * ImGui doesn't thread-safe by default. Luckily Omar Cornut has his
 * own workarounds in imgui.cpp:957.
 *
 * STEPS:
 * 1. Add this workaround in imconfig.h:
 *         struct ImGuiContext;
 *         extern thread_local ImGuiContext* MyImGuiTLS;
 *         #define GImGui MyImGuiTLS
 * 2. Define my own MyImGuiTLS in my source.
 * 3. Still use different ImGuiContext for each instance.
      EXAMPLE:
      Each ImguiEditor instance maintains its own ImGuiContext.
 * 4. Use different std::thread instance for each thread. Do NOT use
 *    only one global thread, or unexpected error may occurs.
 *    EXAMPLE:
 *    Here I define std::thread object as ImguiEditor's class member,
 *    so each ImguiEditor instance can have its own drawing thread
 *    instance. No need to maintain a list of opening threads.
 */
thread_local ImGuiContext* MyImGuiTLS;
