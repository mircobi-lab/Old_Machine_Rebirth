#include "Push2Protocol.h"

#include <algorithm>

namespace push2
{
namespace
{
const Bytes sysexPrefix { 0xF0, 0x00, 0x21, 0x1D, 0x01, 0x01 };

Bytes command (std::uint8_t id, std::initializer_list<std::uint8_t> args)
{
    Bytes m (sysexPrefix);
    m.push_back (id);
    m.insert (m.end(), args.begin(), args.end());
    m.push_back (0xF7);
    return m;
}

std::uint8_t lo7 (std::uint8_t v) { return static_cast<std::uint8_t> (v & 0x7F); }
std::uint8_t hi1 (std::uint8_t v) { return static_cast<std::uint8_t> (v >> 7); }

constexpr std::array<std::uint8_t, 4> xorPattern { 0xE7, 0xF3, 0xE7, 0xFF };
} // namespace

const std::array<std::uint8_t, 16>& frameHeader()
{
    static const std::array<std::uint8_t, 16> header { 0xFF, 0xCC, 0xAA, 0x88 };
    return header;
}

std::uint16_t packPixel (std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return static_cast<std::uint16_t> (((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));
}

std::vector<std::uint8_t> encodeFrame (const std::uint8_t* rgb, int width, int height, int rowStride)
{
    std::vector<std::uint8_t> frame (frameBytes, 0);
    const int w = std::min (width, displayWidth);
    const int h = std::min (height, displayHeight);

    for (int y = 0; y < h; ++y)
    {
        const std::uint8_t* src = rgb + static_cast<std::size_t> (y) * static_cast<std::size_t> (rowStride);
        std::uint8_t* dst = frame.data() + static_cast<std::size_t> (y) * lineBytes;

        for (int x = 0; x < w; ++x)
        {
            const auto p = packPixel (src[x * 3], src[x * 3 + 1], src[x * 3 + 2]);
            dst[x * 2]     = static_cast<std::uint8_t> (p & 0xFF);
            dst[x * 2 + 1] = static_cast<std::uint8_t> (p >> 8);
        }
    }

    for (std::size_t i = 0; i < frame.size(); ++i)
        frame[i] ^= xorPattern[i % 4];

    return frame;
}

std::vector<std::uint8_t> decodeFrame (const std::uint8_t* frame, int size)
{
    if (frame == nullptr || size < frameBytes)
        return {};

    std::vector<std::uint8_t> rgb ((std::size_t) displayWidth * displayHeight * 3, 0);

    for (int y = 0; y < displayHeight; ++y)
    {
        const std::uint8_t* src = frame + (std::size_t) y * lineBytes;
        std::uint8_t* dst = rgb.data() + (std::size_t) y * displayWidth * 3;

        for (int x = 0; x < displayWidth; ++x)
        {
            // le righe sono multiple di 4, quindi il pattern riparte uguale a ogni riga
            const auto lo = (std::uint8_t) (src[x * 2]     ^ xorPattern[(x * 2) % 4]);
            const auto hi = (std::uint8_t) (src[x * 2 + 1] ^ xorPattern[(x * 2 + 1) % 4]);
            const auto p = (std::uint16_t) (lo | (hi << 8));

            const int r5 = p & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p >> 11;
            dst[x * 3]     = (std::uint8_t) ((r5 << 3) | (r5 >> 2));
            dst[x * 3 + 1] = (std::uint8_t) ((g6 << 2) | (g6 >> 4));
            dst[x * 3 + 2] = (std::uint8_t) ((b5 << 3) | (b5 >> 2));
        }
    }
    return rgb;
}

int padNote (int row, int column)
{
    return 92 - row * 8 + column;
}

std::optional<std::pair<int, int>> padPosition (int note)
{
    if (note < 36 || note > 99)
        return std::nullopt;
    const int fromBottom = (note - 36) / 8;
    return std::make_pair (7 - fromBottom, (note - 36) % 8);
}

int encoderDelta (int ccValue)
{
    return ccValue < 64 ? ccValue : ccValue - 128;
}

int sceneButtonCC (int rowFromTop)
{
    return 43 - rowFromTop;
}

Bytes setAftertouchMode (bool polyphonic)
{
    return command (0x1E, { polyphonic ? (std::uint8_t) 1 : (std::uint8_t) 0 });
}

Bytes setMidiMode (std::uint8_t mode)
{
    return command (0x0A, { static_cast<std::uint8_t> (mode & 0x7F) });
}

Bytes setTouchStripConfig (std::uint8_t flags)
{
    return command (0x17, { static_cast<std::uint8_t> (flags & 0x7F) });
}

Bytes identityRequest()
{
    return { 0xF0, 0x7E, 0x01, 0x06, 0x01, 0xF7 };
}

Bytes setPaletteEntry (int index, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t w)
{
    return command (0x03, { static_cast<std::uint8_t> (index & 0x7F),
                            lo7 (r), hi1 (r), lo7 (g), hi1 (g), lo7 (b), hi1 (b), lo7 (w), hi1 (w) });
}

Bytes reapplyPalette()
{
    return command (0x05, {});
}

Bytes setLedBrightness (int value)
{
    return command (0x06, { static_cast<std::uint8_t> (std::clamp (value, 0, 127)) });
}

Bytes setDisplayBrightness (int value)
{
    const auto v = static_cast<std::uint8_t> (std::clamp (value, 0, 255));
    return command (0x08, { lo7 (v), hi1 (v) });
}

std::optional<Identity> parseIdentityReply (const std::uint8_t* data, int size)
{
    if (size > 0 && data[0] == 0xF0) { ++data; --size; }
    if (size > 0 && data[size - 1] == 0xF7) --size;

    // 7E 01 06 02 | 00 21 1D | 67 32 | 02 00 | maj min | build lsb msb | serial x5 | board
    if (size < 21)
        return std::nullopt;
    if (data[0] != 0x7E || data[2] != 0x06 || data[3] != 0x02)
        return std::nullopt;
    if (data[4] != 0x00 || data[5] != 0x21 || data[6] != 0x1D)
        return std::nullopt;

    Identity id;
    id.major = data[11];
    id.minor = data[12];
    id.build = data[13] | (data[14] << 7);
    id.serial = static_cast<std::uint32_t> (data[15])
              | (static_cast<std::uint32_t> (data[16]) << 7)
              | (static_cast<std::uint32_t> (data[17]) << 14)
              | (static_cast<std::uint32_t> (data[18]) << 21)
              | (static_cast<std::uint32_t> (data[19]) << 28);
    id.boardRevision = data[20];
    return id;
}
} // namespace push2
