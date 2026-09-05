#include "PlatformPickerScreen.hpp"

#include "details/GameMetadataClient.hpp"
#include "SettingsGlassTuning.hpp"
#include "themeshop/ThemeHttp.hpp"

#include <nlohmann/json.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/core/Renderer.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>

namespace {
constexpr int kColumns = 3;
constexpr int kRows = 4;
constexpr int kCount = kColumns * kRows;

std::string encodeUrlComponent(const std::string& text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 3);
    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back((char)ch);
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}
}

PlatformPickerScreen::PlatformPickerScreen(nxui::GpuDevice& gpu, nxui::Renderer& renderer,
                                           nxui::ThreadPool& threadPool)
    : m_gpu(gpu), m_renderer(renderer), m_threadPool(threadPool),
      m_platforms{{
          {"PC", "pc", "pc.png"},
          {"PlayStation", "playstation", "psx.png"},
          {"PlayStation 2", "playstation-2", "ps2.png"},
          {"PlayStation 3", "playstation-3", "ps3.png"},
          {"PSP", "psp", "psp.png"},
          {"PS Vita", "playstation-vita", "psvita.png"},
          {"Dreamcast", "dreamcast", "dreamcast.png"},
          {"Nintendo 64", "nintendo-64", "nintendo_64.png"},
          {"GameCube", "gamecube", "gamecube.png"},
          {"Wii", "wii", "wii.png"},
          {"Nintendo DS", "nintendo-ds", "nintendo_ds.png"},
          {"Game Boy Advance", "game-boy-advance", "gameboy_advance.png"},
      }} {
    setRect({0.f, 0.f, 1280.f, 720.f});
    setVisible(false);
    setFocusable(true);
    setFrameworkTouchEnabled(false);
    addAction(static_cast<std::uint64_t>(nxui::Button::DLeft), [this]() { moveSelection(-1, 0); });
    addAction(static_cast<std::uint64_t>(nxui::Button::LStickL), [this]() { moveSelection(-1, 0); });
    addAction(static_cast<std::uint64_t>(nxui::Button::LStickR), [this]() { moveSelection(1, 0); });
    addAction(static_cast<std::uint64_t>(nxui::Button::LStickU), [this]() { moveSelection(0, -1); });
    addAction(static_cast<std::uint64_t>(nxui::Button::LStickD), [this]() { moveSelection(0, 1); });
    addAction(static_cast<std::uint64_t>(nxui::Button::DRight), [this]() { moveSelection(1, 0); });
    addAction(static_cast<std::uint64_t>(nxui::Button::DUp), [this]() { moveSelection(0, -1); });
    addAction(static_cast<std::uint64_t>(nxui::Button::DDown), [this]() { moveSelection(0, 1); });
    addAction(static_cast<std::uint64_t>(nxui::Button::A), [this]() { selectCurrent(); });
    addAction(static_cast<std::uint64_t>(nxui::Button::B), [this]() { hide(); });
}

void PlatformPickerScreen::showForTitle(std::string title) {
    const std::uint64_t currentGen = ++m_generation;
    for (auto& future : m_availabilityFutures)
        if (future.valid()) m_retiredAvailabilityFutures.push_back(std::move(future));
    m_title = std::move(title);
    m_selected = 0;
    m_spinner = 0.f;
    m_active = true;
    m_backdropCacheValid = false;
    m_cachedPreBlurRadius = -1.f;
    m_cachedBlurIterations = -1;
    setVisible(true);

    const auto cacheIt = m_availabilityCache.find(m_title);
    const bool hasCache = cacheIt != m_availabilityCache.end();

    for (std::size_t i = 0; i < m_platforms.size(); ++i) {
        auto& platform = m_platforms[i];
        if (!platform.icon.empty()) {
            const std::string base = m_assetBase.empty() ? "romfs:" : m_assetBase;
            platform.texture.loadFromFile(m_gpu, m_renderer,
                                          base + "/icons/consoles/" + platform.icon, 256);
        }

        if (hasCache && cacheIt->second[i] != Availability::Checking) {
            platform.availability = cacheIt->second[i];
            m_availabilityResults[i] = std::make_shared<std::atomic<Availability>>(cacheIt->second[i]);
            continue;
        }

        platform.availability = Availability::Checking;
        m_availabilityResults[i] = std::make_shared<std::atomic<Availability>>(Availability::Checking);
        const std::string queryTitle = m_title;
        const std::string slug = platform.slug;
        const auto result = m_availabilityResults[i];
        m_availabilityFutures[i] = m_threadPool.submit([this, currentGen, queryTitle, slug, result]() {
            if (!m_active || m_generation != currentGen) return;
            try {
                const std::string url = std::string(GameMetadataClient::kServiceUrl)
                    + "/v1/availability?title=" + encodeUrlComponent(queryTitle)
                    + "&platform=" + encodeUrlComponent(slug);
                if (!m_active || m_generation != currentGen) return;
                const std::string jsonText = themeshop::http::getText(url);
                if (!m_active || m_generation != currentGen) return;
                const bool found = nlohmann::json::parse(jsonText).value("found", false);
                result->store(found ? Availability::Available : Availability::Unavailable);
            } catch (...) {
                if (m_active && m_generation == currentGen) {
                    result->store(Availability::Unavailable);
                }
            }
        });
    }
}

void PlatformPickerScreen::hide() {
    if (!m_active) return;
    m_active = false;
    ++m_generation;
    setVisible(false);
    if (m_closedCb) m_closedCb();
}

void PlatformPickerScreen::wait() {
    for (auto& future : m_availabilityFutures)
        if (future.valid()) future.wait();
    for (auto& future : m_retiredAvailabilityFutures)
        if (future.valid()) future.wait();
    m_retiredAvailabilityFutures.clear();
}

nxui::Rect PlatformPickerScreen::containRect(const nxui::Texture& texture, const nxui::Rect& area) {
    if (!texture.valid()) return area;
    const float scale = std::min(area.width / texture.width(), area.height / texture.height());
    const float width = texture.width() * scale;
    const float height = texture.height() * scale;
    return {area.x + (area.width - width) * 0.5f, area.y + (area.height - height) * 0.5f,
            width, height};
}

void PlatformPickerScreen::onContentRender(nxui::Renderer& renderer) {
    if (!m_active || !m_theme) return;

    // Same real liquid-glass backdrop capture/blur used by the settings and
    // game-details dossiers, instead of a flat frosted rect: a plain dark
    // panel made dark console logos (PS1, GameCube, N64...) disappear against
    // it, so this samples and blurs the actual scene behind the picker.
    const auto& tuning = settings::debug::settingsGlassTuning();
    const bool needsBackdropRefresh = !m_backdropCacheValid
        || std::abs(m_cachedPreBlurRadius - tuning.preBlurRadius) > 0.001f
        || m_cachedBlurIterations != tuning.blurIterations;
    if (needsBackdropRefresh) {
        renderer.captureToOffscreenSharp();
        if (tuning.blurIterations > 0 && tuning.preBlurRadius > 0.001f) {
            renderer.applyBlur(tuning.preBlurRadius, tuning.blurIterations);
        }
        renderer.copyOffscreen(nxui::GpuDevice::OFF_SHARP_A, nxui::GpuDevice::OFF_SETTINGS);
        m_backdropCacheValid = true;
        m_cachedPreBlurRadius = tuning.preBlurRadius;
        m_cachedBlurIterations = tuning.blurIterations;
    }

    renderer.drawRect(rect(), nxui::Color(0.f, 0.f, 0.f, 0.42f));

    const nxui::Rect panel{70.f, 38.f, 1140.f, 644.f};

    nxui::LiquidGlassSettings savedGlass = renderer.liquidGlassSettings();
    auto& glass = renderer.liquidGlassSettings();
    glass.refractionIntensity = std::clamp(tuning.refractionIntensity, 0.0f, 1.5f);
    glass.blurIntensity = std::max(0.0f, tuning.shaderBlurIntensity);
    glass.noiseIntensity = 0.0f;
    glass.glowIntensity = std::max(0.0f, tuning.glowIntensity);
    glass.saturation = std::max(0.0f, tuning.saturation);
    glass.opacityMultiplier = 1.0f;
    glass.roughness = std::max(0.0f, tuning.roughness);
    glass.powerFactor = std::max(1.001f, tuning.powerFactor);

    const nxui::Color glassTint = m_theme->panelBase.withAlpha(
        m_theme->mode == nxui::ThemeMode::Dark
            ? std::clamp(tuning.tintAlphaDark, 0.0f, 1.0f)
            : std::clamp(tuning.tintAlphaLight, 0.0f, 1.0f));
    const nxui::Rect glassRect = panel.shrunk(std::max(0.0f, tuning.inset));
    const float glassRadius = std::max(12.0f, 28.f - std::max(0.0f, tuning.inset) * 0.5f);

    renderer.drawLiquidGlass(nxui::GpuDevice::OFF_SETTINGS, glassRect, glassRadius, glassTint, 1.f,
                             std::clamp(tuning.shade, 0.0f, 1.0f));

    const nxui::Color panelBorder = m_theme->panelBorder.withAlpha(
        m_theme->mode == nxui::ThemeMode::Dark ? 0.32f : 0.40f);
    const nxui::Color panelHighlight = m_theme->panelHighlight.withAlpha(0.12f);
    renderer.drawRoundedRectOutline(glassRect,
                                    panelBorder.withAlpha(std::clamp(panelBorder.a * 0.90f, 0.14f, 0.34f)),
                                    glassRadius, 1.2f);
    renderer.drawRoundedRectOutline(glassRect.shrunk(1.5f),
                                    panelHighlight.withAlpha(std::clamp(panelHighlight.a * 0.90f, 0.04f, 0.10f)),
                                    std::max(0.0f, glassRadius - 1.5f), 1.0f);
    renderer.liquidGlassSettings() = savedGlass;

    auto& i18n = nxui::I18n::instance();
    if (m_font)
        renderer.drawText(i18n.tr("platform_picker.title", "Original Platform"), {108.f, 68.f},
                          m_font, m_theme->textPrimary, 1.f);
    if (m_smallFont) {
        renderer.drawText(m_title, {108.f, 110.f}, m_smallFont, m_theme->textSecondary, 0.78f);
        renderer.drawText(i18n.tr("platform_picker.subtitle", "Checking metadata availability"),
                          {108.f, 139.f}, m_smallFont, m_theme->textSecondary, 0.72f);
    }
    constexpr float cardW = 320.f;
    constexpr float cardH = 105.f;
    constexpr float gapX = 34.f;
    constexpr float gapY = 17.f;
    for (int i = 0; i < kCount; ++i) {
        const nxui::Rect card{108.f + (i % kColumns) * (cardW + gapX),
                              180.f + (i / kColumns) * (cardH + gapY), cardW, cardH};
        m_cardRects[(std::size_t)i] = card;
        const auto& platform = m_platforms[(std::size_t)i];
        const nxui::Color status = platform.availability == Availability::Available
            ? nxui::Color(0.25f, 0.92f, 0.48f, 1.f)
            : platform.availability == Availability::Unavailable
                ? nxui::Color(1.f, 0.30f, 0.28f, 1.f)
                : m_theme->cursorNormal.withAlpha(0.75f + 0.20f * std::sin(m_spinner * 5.f));
        renderer.drawRoundedRect(card, m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark ? 0.14f : 0.20f), 16.f);
        renderer.drawRoundedRectOutline(card, m_theme->panelBorder.withAlpha(0.26f), 16.f, 1.f);
        // Availability status pill inside card
        renderer.drawCircle({card.x + 22.f, card.y + 67.f}, 4.5f, status, 16);
        if (m_smallFont) {
            renderer.drawText(i18n.tr("platform_picker.name." + platform.slug, platform.label),
                              {card.x + 22.f, card.y + 27.f}, m_smallFont, m_theme->textPrimary, 0.78f);
            renderer.drawText(platform.availability == Availability::Checking
                              ? i18n.tr("platform_picker.checking", "Checking...")
                              : platform.availability == Availability::Available
                              ? i18n.tr("platform_picker.available", "Metadata available")
                              : i18n.tr("platform_picker.unavailable", "Metadata unavailable"),
                              {card.x + 34.f, card.y + 61.f}, m_smallFont, status, 0.62f);
        }
        const nxui::Rect logoArea{card.right() - 130.f, card.y + 14.f, 104.f, card.height - 28.f};
        if (platform.texture.valid())
            renderer.drawTexture(&platform.texture, containRect(platform.texture, logoArea),
                                 nxui::Color::white());
        else if (platform.slug == "pc" && m_font)
            renderer.drawText("PC", {logoArea.x + 30.f, logoArea.y + 25.f}, m_font,
                              m_theme->textPrimary, 0.82f);
        if (i == m_selected)
            renderer.drawRoundedRectOutline(card.expanded(4.f), m_theme->cursorNormal, 19.f, 4.f);
    }
}

void PlatformPickerScreen::onContentUpdate(float dt) {
    if (!m_active) return;
    m_spinner += dt;
    for (auto& future : m_retiredAvailabilityFutures) {
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            future.get();
    }
    m_retiredAvailabilityFutures.erase(
        std::remove_if(m_retiredAvailabilityFutures.begin(), m_retiredAvailabilityFutures.end(),
                       [](const auto& future) { return !future.valid(); }),
        m_retiredAvailabilityFutures.end());
    for (std::size_t i = 0; i < m_availabilityFutures.size(); ++i) {
        auto& future = m_availabilityFutures[i];
        if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            future.get();
            if (m_active) {
                const Availability avail = m_availabilityResults[i]->load();
                m_platforms[i].availability = avail;
                if (avail != Availability::Checking) {
                    m_availabilityCache[m_title][i] = avail;
                }
            }
        }
    }
}

void PlatformPickerScreen::moveSelection(int dx, int dy) {
    const int target = m_selected + dx + dy * kColumns;
    if (target >= 0 && target < kCount) m_selected = target;
}

void PlatformPickerScreen::selectCurrent() {
    if (!m_active) return;
    if (m_selectedCb) m_selectedCb(m_platforms[(std::size_t)m_selected].slug);
}

void PlatformPickerScreen::handleTouch(nxui::Input& input) {
    if (!m_active || !input.touchUp()) return;
    for (int i = 0; i < kCount; ++i) {
        if (!m_cardRects[(std::size_t)i].contains(input.touchX(), input.touchY())) continue;
        m_selected = i;
        selectCurrent();
        return;
    }
}
