#pragma once

#include "CrossPointWebServer.h"

// Myne-specific alias for the web server class.
// CrossPointWebServer.h holds the canonical declaration; the simulator library
// (crosspoint-simulator) provides the host-native POSIX implementation under
// that class name, so the canonical header must use CrossPointWebServer.
using MyneWebServer = CrossPointWebServer;
