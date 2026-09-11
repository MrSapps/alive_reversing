#pragma once

#include <QByteArray>
#include <QIODevice>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstring>
#include <optional>

namespace Automation
{
    // Arbitrary sanity cap so a corrupt/malicious length prefix can't trigger a huge allocation.
    // Generous enough for a full-screen PNG screenshot base64-encoded into JSON.
    constexpr qint64 kMaxFrameSize = 64 * 1024 * 1024;

    // Frame layout (both directions): 4-byte little-endian length prefix, then that many
    // bytes of UTF-8 JSON. No alignment/padding.
    inline void WriteFrame(QIODevice* device, const nlohmann::json& msg)
    {
        const std::string payload = msg.dump();
        const uint32_t length = static_cast<uint32_t>(payload.size());

        QByteArray header(reinterpret_cast<const char*>(&length), sizeof(length));
        device->write(header);
        device->write(payload.data(), static_cast<qint64>(payload.size()));
    }

    // Accumulates bytes across possibly-partial reads and yields complete JSON frames
    // as they become available. A single readyRead()/recv() can deliver a partial frame,
    // several full frames, or a mix, so callers must loop TryTakeFrame() until it returns
    // nullopt.
    class FrameReader final
    {
    public:
        void Append(const QByteArray& data)
        {
            mBuffer.append(data);
        }

        // Returns nullopt if no complete frame is buffered yet. A returned json with
        // is_discarded() true means a frame was received but failed to parse.
        std::optional<nlohmann::json> TryTakeFrame()
        {
            constexpr int kHeaderSize = sizeof(uint32_t);
            if (mBuffer.size() < kHeaderSize)
            {
                return std::nullopt;
            }

            uint32_t length = 0;
            std::memcpy(&length, mBuffer.constData(), kHeaderSize);

            if (static_cast<qint64>(length) > kMaxFrameSize)
            {
                // Desync/garbage on the wire - drop everything buffered so we don't wait
                // forever for bytes that will never arrive.
                mBuffer.clear();
                return nlohmann::json(nlohmann::json::value_t::discarded);
            }

            if (mBuffer.size() < kHeaderSize + static_cast<int>(length))
            {
                return std::nullopt;
            }

            const QByteArray payload = mBuffer.mid(kHeaderSize, static_cast<int>(length));
            mBuffer.remove(0, kHeaderSize + static_cast<int>(length));

            return nlohmann::json::parse(payload.constData(), payload.constData() + payload.size(), nullptr, false);
        }

    private:
        QByteArray mBuffer;
    };
}
