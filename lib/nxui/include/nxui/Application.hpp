#pragma once
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Input.hpp>
#include <functional>
#include <memory>

namespace nxui {

class Activity;

/// Top-level application object.
/// Owns the GPU device, Renderer, and Input, runs the main loop,
/// and delegates lifecycle events to the attached Activity.
///
/// Usage:
///   nxui::Application app;
///   app.setActivity(std::make_unique<MyActivity>());
///   if (app.initialize()) app.run();
///   app.shutdown();
class Application {
public:
    Application() = default;
    ~Application();

    /// Onde a Application relata o que acontece fora do controle dela. O
    /// pedido de encerramento do sistema so ia para svcOutputDebugString, que
    /// nao esta em lugar nenhum quando se abre o log depois: o menu saia
    /// limpo, o daemon relancava, e o registro terminava sem uma palavra --
    /// indistinguivel de uma queda.
    void setLogSink(std::function<void(const char*)> sink) { m_logSink = std::move(sink); }

    /// Attach the main activity. Must be called before initialize().
    void setActivity(std::unique_ptr<Activity> activity);

    /// Queue an activity change from inside the main loop.
    void requestActivity(std::unique_ptr<Activity> activity);

    /// Initialise GPU, Renderer, Input, then call activity->onCreate().
    bool initialize();

    /// Enter the main loop (blocks until requestExit() is called).
    void run();

    /// Shutdown: activity->onDestroy(), then GPU/Renderer cleanup.
    void shutdown();

    // Accessors used by Activity
    GpuDevice& gpu()       { return m_gpu; }
    Renderer&  renderer()  { return *m_renderer; }
    Input&     input()     { return m_input; }

    void requestExit()       { m_running = false; }
    bool isRunning() const   { return m_running; }

    /// Disable/enable GPU rendering.  When disabled the main loop still
    /// runs input + update, but skips beginFrame/endFrame so the GPU is
    /// free for whichever app currently owns the foreground.
    void setRenderEnabled(bool e) { m_renderEnabled = e; }
    bool renderEnabled() const    { return m_renderEnabled; }

private:
    void dispatchInput();
    bool applyPendingActivity();

    GpuDevice  m_gpu;
    std::unique_ptr<Renderer> m_renderer;
    Input      m_input;

    std::unique_ptr<Activity> m_activity;
    std::unique_ptr<Activity> m_pendingActivity;
    bool m_running = true;
    std::function<void(const char*)> m_logSink;
    bool m_renderEnabled = true;
    int  m_navDebounce = 0;
    // Frames a direction has been held for. Navigation used to fire only on the
    // frame a button went down, so holding one moved a single icon and stopped.
    int  m_navHoldFrames = 0;
};

} // namespace nxui
