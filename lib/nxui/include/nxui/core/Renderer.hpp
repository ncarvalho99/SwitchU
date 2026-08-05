#pragma once
#include <nxui/core/Types.hpp>
#include <nxui/core/GpuDevice.hpp>
#ifdef NXUI_BACKEND_DEKO3D
#include <deko3d.hpp>
#endif
#include <cstdint>
#include <vector>
#include <string>

namespace nxui {

class Texture;
class Font;

struct Vertex2D {
    float x, y;         // Position
    float u, v;         // Texture coordinate
    float r, g, b, a;   // Color (premultiplied alpha)
};
static_assert(sizeof(Vertex2D) == 32);

struct VsUniforms {
    float projection[16];   // Ortho matrix (top-left origin)
};

struct FsUniforms {
    int32_t useTexture;
    float   param1;
    float   param2;
    float   param3;
    float   extra[4 * 4 * 3];
};

// Shader program IDs
enum class ShaderProgram {
    Basic,
    Backdrop,
    BlurH,
    BlurV,
    Wave,
    LiquidGlass,
    Gradient,
    Count
};

struct LiquidGlassSettings {
    float refractionIntensity = 0.08f;
    float blurIntensity = 0.65f;
    float noiseIntensity = 0.0f;

    float glowIntensity = 0.22f;
    float saturation = 0.96f;
    float opacityMultiplier = 0.28f;
    float roughness = 0.015f;

    float animSpeed = 0.0f;
    float time = 0.0f;
    float powerFactor = 6.0f;
    float fPower = 1.0f;

    float refA = 0.7f;
    float refB = 2.3f;
    float refC = 5.2f;
    float refD = 6.9f;

    float glowWeight = 0.14f;
    float glowBias = -0.02f;
    float glowEdge0 = 0.12f;
    float glowEdge1 = -0.08f;

    Color tintBoost {1.f, 1.f, 1.f, 0.8f};
};

// Renderer
class Renderer {
public:
    explicit Renderer(GpuDevice& gpu);
    ~Renderer();

    /// Set the base directory for compiled shader (.dksh) files.
    /// Must be called BEFORE initialize(). Default: "romfs:/shaders/"
    static void setShaderBasePath(const std::string& path) { s_shaderBasePath = path; }
    static const std::string& shaderBasePath() { return s_shaderBasePath; }

    bool initialize();

    // Frame scope
    void beginFrame();

    // Per-frame GPU submission counters, sampled at the previous beginFrame.
    // The settings overlay drops to 10-15 fps and the cause is not obvious from
    // reading the code — every glass panel forces a flush and a pipeline
    // rebind, but so does the cheaper frosted path, so the two guesses I made
    // from structure alone were both wrong. These make it measurable.
    uint32_t lastFrameDrawCalls()     const { return m_lastFrameDrawCalls; }
    uint32_t lastFramePipelineBinds() const { return m_lastFramePipelineBinds; }
    uint32_t lastFrameVertices()      const { return m_lastFrameVertices; }

    // Fullscreen work that does not scale with draw count. GPU time sits near
    // 30ms whether the frame submits 119 draws or 144, which is the signature
    // of a fixed cost. applyBlur runs 15 iterations of two fullscreen passes
    // in this overlay, and captureToOffscreen blits the framebuffer — counting
    // them says whether the cache that is supposed to stop them is working.
    uint32_t lastFrameBlurPasses() const { return m_lastFrameBlurPasses; }
    uint32_t lastFrameCaptures()   const { return m_lastFrameCaptures; }

    // Keep the offscreen backdrop capture across frames. Only set this while
    // whatever the glass samples is genuinely static, or the refraction will
    // show a stale scene.
    void setHoldOffscreenCapture(bool hold) { m_holdOffscreenCapture = hold; }
    bool holdOffscreenCapture() const       { return m_holdOffscreenCapture; }
    void endFrame();

    // 2D drawing
    void drawRect(const Rect& r, const Color& c);
    void drawRectOutline(const Rect& r, const Color& c, float thickness = 1.f);
    void drawRoundedRect(const Rect& r, const Color& c, float radius);
    void drawRoundedRectOutline(const Rect& r, const Color& c, float radius, float thickness = 1.f);
    void drawCircle(const Vec2& center, float radius, const Color& c, int segments = 32);
    void drawTriangle(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Color& c);
    void drawLine(const Vec2& from, const Vec2& to, const Color& c, float thickness = 1.f);
    void drawGradientRect(const Rect& r, const Color& top, const Color& bottom);
    void drawTexture(const Texture* tex, const Rect& dest, const Color& tint = Color::white());
    void drawTextureSub(const Texture* tex, const Rect& src, const Rect& dest, const Color& tint = Color::white());
    void drawTextureRounded(const Texture* tex, const Rect& dest, float radius, const Color& tint = Color::white());
    void drawText(const std::string& text, const Vec2& pos, Font* font, const Color& color, float scale = 1.f);

    // Post-processing
    void captureToOffscreen(bool reuseIfValid = false);
    void copyOffscreen(int srcTarget, int dstTarget);
    void drawOffscreen(int target, const Rect& dest, const Color& tint = Color::white());
    void drawOffscreenRounded(int target, const Rect& dest, float radius,
                              const Color& tint = Color::white());
    void drawLiquidGlass(int target, const Rect& panelRect, float radius,
                         const Color& tint, float opacity = 1.f, float shade = 0.f);
    void applyBlur(float radius = 1.0f, int passes = 2);
    void applyWave(float time, float amplitude, float frequency);

    // Switch shader program (flushes current batch)
    void useShader(ShaderProgram prog);

    // Push custom FS uniforms (flushes current batch)
    void pushFsUniforms(const FsUniforms& fs);

    // Scissor/clip stack
    void pushClipRect(const Rect& r);
    void popClipRect();

    // Screen dimensions
    int width()  const { return m_gpu.width(); }
    int height() const { return m_gpu.height(); }

    // Flush current batch
    void flush();

    LiquidGlassSettings& liquidGlassSettings() { return m_liquidGlassSettings; }
    const LiquidGlassSettings& liquidGlassSettings() const { return m_liquidGlassSettings; }
    void resetLiquidGlassSettings();
    void setLiquidGlassDebugRawBackdrop(bool enabled) { m_liquidGlassDebugRawBackdrop = enabled; }
    bool liquidGlassDebugRawBackdrop() const { return m_liquidGlassDebugRawBackdrop; }

    // Debug
    uint32_t vertexCount() const { return m_vtxCount; }
    void setBoxWireframeEnabled(bool enabled) { m_boxWireframeEnabled = enabled; }
    bool boxWireframeEnabled() const { return m_boxWireframeEnabled; }

    // Texture descriptor management
#ifdef NXUI_BACKEND_DEKO3D
    int registerTexture(const dk::ImageView& view);
    void updateTexture(int slot, const dk::ImageView& view);
#endif
    void bindTexture(int slot);
    void resetTextureSlots();

    GpuDevice& gpu() { return m_gpu; }

    // Offscreen descriptor slots
    int offscreenDescSlot(int target) const { return m_offDescSlot[target]; }

private:
    // Emit geometry helpers
    // Segments per 90-degree corner on rounded geometry.
    static constexpr int kCornerSegs = 8;

    // Rounded fills are a single quad masked by the fragment shader. These
    // describe the shape to it: extent in pixels, and the UV window the quad
    // spans so the shader can normalise fragUV onto the shape. Zero radius
    // means no mask, which is the state every other draw runs in.
    float m_roundRadius = 0.f;
    Vec2  m_roundSize {0.f, 0.f};
    Vec2  m_roundUvMin {0.f, 0.f};
    Vec2  m_roundUvScale {1.f, 1.f};

    // Emits `quad` with the corner mask active, then clears it.
    void drawRoundedMasked(const Rect& dest, float radius, const Color& c,
                           const Rect& uv);

    uint32_t m_frameDrawCalls = 0;
    uint32_t m_framePipelineBinds = 0;
    uint32_t m_peakVtxCount = 0;
    uint32_t m_lastFrameDrawCalls = 0;
    uint32_t m_lastFramePipelineBinds = 0;
    uint32_t m_lastFrameVertices = 0;
    uint32_t m_frameBlurPasses = 0;
    uint32_t m_frameCaptures = 0;
    uint32_t m_lastFrameBlurPasses = 0;
    uint32_t m_lastFrameCaptures = 0;
    bool     m_holdOffscreenCapture = false;

    // Flushes the current batch if `count` more vertices would not fit.
    // addVertex silently drops past the buffer end, so shapes that emit a run
    // of vertices have to make room before starting one.
    void reserveVertices(uint32_t count);
    void addVertex(float x, float y, float u, float v, const Color& c);
    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1, const Color& c);
    void addQuadGrad(float x0, float y0, float x1, float y1,
                     float u0, float v0, float u1, float v1,
                     const Color& cTop, const Color& cBot);

#ifdef NXUI_BACKEND_DEKO3D
    bool loadShaders();
    void setupSampler();
    void updateProjection();
    void bindRenderTarget(int offscreenIdx);
    void restoreRenderTarget();
#endif

    GpuDevice& m_gpu;

#ifdef NXUI_BACKEND_DEKO3D
    static constexpr int SHADER_COUNT = (int)ShaderProgram::Count;
    dk::Shader m_vertShaders[SHADER_COUNT];
    dk::Shader m_fragShaders[SHADER_COUNT];
    ShaderProgram m_curShader = ShaderProgram::Basic;
#endif

    // Batching state
    Vertex2D*  m_vtxBase   = nullptr;
    uint32_t   m_vtxCount  = 0;
    uint32_t   m_vtxBatchStart = 0;
    int        m_curTexSlot = -1;
    bool       m_texturing  = false;

    // Clip stack
    std::vector<Rect> m_clipStack;

    // Texture descriptor tracking
    int m_nextDescSlot = 0;
    static constexpr int WHITE_TEX_SLOT = 0;

    // Offscreen target descriptor slots
    int m_offDescSlot[GpuDevice::NUM_OFFSCREEN] = {};

#ifdef NXUI_BACKEND_DEKO3D
    dk::Image          m_whiteImage;
    dk::UniqueMemBlock m_whiteMemBlock;

    // Track whether any image/sampler descriptor has been written
    // via CPU memcpy since the last GPU-side barrier.  When true,
    // flush() inserts barrier(DkBarrier_None, DkInvalidateFlags_Descriptors)
    // before the next draw call so the GPU re-reads from memory.
    bool m_descDirty = false;
#endif

#ifdef NXUI_BACKEND_SDL2
    // SDL2 backend: textures tracked by slot for binding
    std::vector<SDL_Texture*> m_texSlots;
    SDL_Texture* m_boundTex = nullptr;

    // Vertex buffer (CPU-side for SDL2)
    std::vector<Vertex2D> m_vtxBuf;
#endif

    bool m_boxWireframeEnabled = false;
    bool m_liquidGlassDebugRawBackdrop = false;
    bool m_reusableOffscreenCaptureValid = false;
    LiquidGlassSettings m_liquidGlassSettings;

    static inline std::string s_shaderBasePath = "romfs:/shaders/";
};

} // namespace nxui
