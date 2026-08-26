#pragma once
#include <switchu/smi_protocol.hpp>
#include <switch.h>
#include <cstring>
#include <vector>

namespace switchu::smi {

class StorageWriter {
public:
    explicit StorageWriter(SystemMessage msg) {
        m_buf.resize(sizeof(CommandHeader), 0);
        CommandHeader hdr{kCommandMagic, static_cast<uint32_t>(msg)};
        std::memcpy(m_buf.data(), &hdr, sizeof(hdr));
        m_pos = sizeof(CommandHeader);
    }

    explicit StorageWriter(Result rc) {
        m_buf.resize(sizeof(CommandHeader), 0);
        CommandHeader hdr{kCommandMagic, static_cast<uint32_t>(rc)};
        std::memcpy(m_buf.data(), &hdr, sizeof(hdr));
        m_pos = sizeof(CommandHeader);
    }

    template<typename T>
    void push(const T& val) {
        if (m_pos + sizeof(T) > kStorageSize) return;
        m_buf.resize(m_pos + sizeof(T));
        std::memcpy(m_buf.data() + m_pos, &val, sizeof(T));
        m_pos += sizeof(T);
    }

    void pushBytes(const void* data, size_t len) {
        if (m_pos + len > kStorageSize) return;
        m_buf.resize(m_pos + len);
        std::memcpy(m_buf.data() + m_pos, data, len);
        m_pos += len;
    }

    Result createStorage(AppletStorage& st) const {
        const s64 size = static_cast<s64>(m_buf.size());
        Result rc = appletCreateStorage(&st, size);
        if (R_SUCCEEDED(rc)) {
            rc = appletStorageWrite(&st, 0, m_buf.data(), size);
            if (R_FAILED(rc))
                appletStorageClose(&st);
        }
        return rc;
    }

private:
    std::vector<uint8_t> m_buf;
    size_t m_pos = 0;
};

class StorageReader {
public:
    explicit StorageReader(AppletStorage& st) {
        s64 sz = 0;
        if (R_SUCCEEDED(appletStorageGetSize(&st, &sz)) && sz > 0) {
            const size_t readSize = static_cast<size_t>(sz) < kStorageSize
                ? static_cast<size_t>(sz) : kStorageSize;
            m_buf.resize(readSize);
            if (R_FAILED(appletStorageRead(&st, 0, m_buf.data(), readSize)))
                m_buf.clear();
        }
        appletStorageClose(&st);
        m_pos = sizeof(CommandHeader);
    }

    bool valid() const {
        if (m_buf.size() < sizeof(CommandHeader)) return false;
        CommandHeader hdr;
        std::memcpy(&hdr, m_buf.data(), sizeof(hdr));
        return hdr.magic == kCommandMagic;
    }

    uint32_t messageOrResult() const {
        CommandHeader hdr;
        std::memcpy(&hdr, m_buf.data(), sizeof(hdr));
        return hdr.message;
    }

    SystemMessage systemMessage() const {
        return static_cast<SystemMessage>(messageOrResult());
    }

    template<typename T>
    T pop() {
        T val{};
        if (m_pos + sizeof(T) <= m_buf.size()) {
            std::memcpy(&val, m_buf.data() + m_pos, sizeof(T));
            m_pos += sizeof(T);
        }
        return val;
    }

    const uint8_t* rawAt(size_t offset) const {
        if (offset >= m_buf.size()) return nullptr;
        return m_buf.data() + offset;
    }

    size_t remaining() const {
        return (m_pos < m_buf.size()) ? m_buf.size() - m_pos : 0;
    }

    size_t position() const { return m_pos; }

private:
    std::vector<uint8_t> m_buf;
    size_t m_pos = 0;
};

using StorageFn = Result(*)(AppletStorage*);

inline Result loopWaitStorage(StorageFn fn, AppletStorage* st, bool wait) {
    if (!wait) return fn(st);
    for (uint32_t i = 0; i < kMaxRetries; ++i) {
        if (R_SUCCEEDED(fn(st))) return 0;
        svcSleepThread(kRetrySleepNs);
    }
    return MAKERESULT(Module_Libnx, 0xFF);
}

}
