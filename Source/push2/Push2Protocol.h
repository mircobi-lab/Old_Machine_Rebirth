#pragma once

// Protocollo Ableton Push 2, solo la parte documentata da Ableton:
// https://github.com/Ableton/push-interface (MIDI and Display Interface Manual + Push2-map.json)
// Niente JUCE qui dentro, cosi' si prova con un semplice compilatore C++.

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace push2
{
// ---------------------------------------------------------------- USB
constexpr std::uint16_t vendorId  = 0x2982;
constexpr std::uint16_t productId = 0x1967;
constexpr int displayInterface    = 0;
constexpr std::uint8_t displayEndpoint = 0x01;

// ---------------------------------------------------------------- display
constexpr int displayWidth  = 960;
constexpr int displayHeight = 160;
constexpr int lineBytes     = 2048;                      // 1920 di pixel + 128 di riempimento
constexpr int frameBytes    = displayHeight * lineBytes; // 327680
constexpr int transferBytes = 16384;                     // 20 trasferimenti per frame

const std::array<std::uint8_t, 16>& frameHeader();

// Converte un'immagine RGB 8 bit (3 byte per pixel, righe di width*3 byte)
// nel buffer del Push 2: BGR565 little endian, righe da 2048 byte, XOR E7 F3 E7 FF.
std::vector<std::uint8_t> encodeFrame (const std::uint8_t* rgb, int width, int height, int rowStride);

// Un pixel RGB -> 16 bit nel formato del Push 2 (blu nei bit alti, rosso nei bassi)
std::uint16_t packPixel (std::uint8_t r, std::uint8_t g, std::uint8_t b);

// Inverso di encodeFrame: dal frame del Push a RGB 8 bit, righe di displayWidth*3 byte.
// Vuoto se il frame non ha la misura giusta.
std::vector<std::uint8_t> decodeFrame (const std::uint8_t* frame, int size);

// ---------------------------------------------------------------- MIDI
// Pad: riga 0 = in alto (92..99), riga 7 = in basso (36..43); colonna 0 = sinistra
int padNote (int row, int column);
std::optional<std::pair<int, int>> padPosition (int note); // {riga, colonna}

constexpr int upperDisplayButtonCC = 102; // 102..109 sopra il display
constexpr int lowerDisplayButtonCC = 20;  // 20..27 sotto il display

// Gli 8 rotativi in alto (Push2-map.json): girandoli mandano CC 71..78,
// toccandoli mandano le note 0..7 con velocity 127, lasciandoli velocity 0
constexpr int trackEncoderCC = 71;
constexpr int trackEncoderTouchNote = 0;
constexpr int numTrackEncoders = 8;

// Quanti scatti e in che verso: il valore e' relativo, in complemento a due
int encoderDelta (int ccValue);

constexpr int masterEncoderCC = 79;
constexpr int tempoEncoderCC  = 14;
constexpr int swingEncoderCC  = 15;

// Colonna a destra dei pad: in Live lancia le scene. CC 36 e' in basso, 43 in alto.
constexpr int numSceneButtons = 8;
int sceneButtonCC (int rowFromTop); // 0 = in alto

// Striscia verticale a sinistra dei pad: nota 12 al tocco, poi pitch bend
// oppure CC 1 (mod wheel) secondo come e' configurata
constexpr int touchStripNote = 12;
constexpr int modWheelCC = 1;
constexpr std::uint8_t stripPitchBend = 0x68; // di serie: punto, ritorno al centro
constexpr std::uint8_t stripModWheel  = 0x0C; // barra dal basso, senza ritorno

// Pedali: jack 1 (a destra, vicino all'interruttore) e jack 2 (a sinistra)
constexpr int pedal1CC = 64; // Sustain
constexpr int pedal2CC = 69; // Hold 2

// Altri tasti usati
constexpr int shiftCC = 49, masterCC = 28, tapTempoCC = 3, metronomeCC = 9;
constexpr int playCC = 85, stopCC = 29, muteCC = 60, soloCC = 61;
constexpr int leftCC = 44, rightCC = 45, upCC = 46, downCC = 47;
constexpr int octaveDownCC = 54, octaveUpCC = 55;


// Indici della palette di fabbrica garantiti dal manuale
constexpr int colourBlack = 0;
constexpr int colourWhite = 122;
constexpr int colourRed   = 127;

using Bytes = std::vector<std::uint8_t>;

Bytes identityRequest();
Bytes setAftertouchMode (bool polyphonic);       // comando 0x1E: 0 = channel pressure, 1 = poly

// Comando 0x0A. Il Push si accende in Live mode, e in Live mode quello che arriva
// sulla porta User lo butta via: per parlarci dalla User bisogna dirglielo.
// Il sysex invece lo ascolta da tutte e due le porte, sempre.
constexpr std::uint8_t midiModeLive = 0, midiModeUser = 1, midiModeDual = 2;
Bytes setMidiMode (std::uint8_t mode);
Bytes setTouchStripConfig (std::uint8_t flags);  // comando 0x17: vedi stripPitchBend / stripModWheel                     // F0 7E 01 06 01 F7
Bytes setPaletteEntry (int index, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t w);
Bytes reapplyPalette();                      // comando 0x05
Bytes setLedBrightness (int value);          // comando 0x06, 0..127 (a USB il Push limita a 8)
Bytes setDisplayBrightness (int value);      // comando 0x08, 0..255 (a USB il Push limita a 100)

struct Identity
{
    int major = 0, minor = 0, build = 0;
    std::uint32_t serial = 0;
    int boardRevision = 0;
};

// Accetta il messaggio completo F0 ... F7 oppure senza F0/F7 (come lo passa JUCE)
std::optional<Identity> parseIdentityReply (const std::uint8_t* data, int size);
} // namespace push2
