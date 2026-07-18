#pragma once

#include "src/config/settings.hpp"
#include "src/engine/snapshot.hpp"

namespace vectra { class Esp { public: static void Draw(const FrameSnapshot& frame, const Settings& settings, bool debugAnnotations = false); }; }
