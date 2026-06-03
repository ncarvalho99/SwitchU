#pragma once

#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <switch.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

class UserAvatarButton : public nxui::GlassWidget {
public:
    using ActivateCallback = std::function<void()>;

    UserAvatarButton();

    void loadAvatar(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                    const void* data, std::size_t size);

    void setUid(AccountUid uid) { m_uid = uid; }
    AccountUid uid() const { return m_uid; }

    void setNickname(const std::string& nickname) { m_nickname = nickname; }
    const std::string& nickname() const { return m_nickname; }
    void setChromeEnabled(bool enabled);

    void setOnActivate(ActivateCallback cb) { m_onActivate = std::move(cb); }

    bool isFocusable() const override { return m_focusable; }
    void setFocusable(bool focusable) { m_focusable = focusable; }
    void onFocusGained() override { m_focused = true; }
    void onFocusLost() override { m_focused = false; }
    bool activate() override {
        if (!m_onActivate)
            return false;
        m_onActivate();
        return true;
    }

protected:
    void onContentRender(nxui::Renderer& ren) override;

private:
    nxui::Texture m_avatarTexture;
    AccountUid m_uid = {};
    std::string m_nickname;
    ActivateCallback m_onActivate;
    bool m_focusable = true;
    bool m_focused = false;
    bool m_chromeEnabled = true;
};
