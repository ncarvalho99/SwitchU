#pragma once

#include <algorithm>
#include <string>

class ThemeTransferState {
public:
    enum class Phase {
        Idle,
        Running,
        Succeeded,
        Failed,
    };

    void reset() {
        m_phase = Phase::Idle;
        m_label.clear();
        m_progress01 = -1.f;
    }

    void begin(std::string label) {
        m_phase = Phase::Running;
        m_label = std::move(label);
        m_progress01 = -1.f;
    }

    void update(std::string label, float progress01 = -1.f) {
        m_phase = Phase::Running;
        m_label = std::move(label);
        m_progress01 = progress01 < 0.f ? -1.f : std::clamp(progress01, 0.f, 1.f);
    }

    void succeed(std::string label = {}) {
        m_phase = Phase::Succeeded;
        if (!label.empty())
            m_label = std::move(label);
        m_progress01 = 1.f;
    }

    void fail(std::string label) {
        m_phase = Phase::Failed;
        m_label = std::move(label);
        m_progress01 = -1.f;
    }

    void setProgress(float progress01) {
        m_progress01 = std::clamp(progress01, 0.f, 1.f);
    }

    void setLabel(std::string label) {
        m_label = std::move(label);
    }

    Phase phase() const {
        return m_phase;
    }

    bool isRunning() const {
        return m_phase == Phase::Running;
    }

    bool isReady() const {
        return m_phase == Phase::Succeeded;
    }

    bool hasFailed() const {
        return m_phase == Phase::Failed;
    }

    const std::string& label() const {
        return m_label;
    }

    float progress01() const {
        return m_progress01;
    }

private:
    Phase m_phase = Phase::Idle;
    std::string m_label;
    float m_progress01 = -1.f;
};