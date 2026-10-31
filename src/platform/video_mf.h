// After-credits video on Windows: Media Foundation IMFSourceReader decodes the MP4 (H.264) to RGB32 frames
// on the CPU; the DX12 renderer uploads the current frame to a texture and draws it full screen.
// If Media Foundation or the H.264 decoder is missing (e.g. Wine without the MF codecs), start() returns false and
// the game falls back to its in-engine closing shot for the same length (read from the MP4 header).
#pragma once
#include <string>
#include "../game/game.h"
namespace tz {
VideoPlayer* createMediaFoundationPlayer();  // never null; start() reports whether it can play
}
