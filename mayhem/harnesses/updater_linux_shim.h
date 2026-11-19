// Force-included (-include) when compiling src/MEGAUpdater/UpdateTask.cpp on Linux, which
// upstream only builds for Windows and macOS. On non-Windows, checkForUpdates() reads
// `routing.updateTrack`, which only the macOS branch declares (MacUpdateRouting). Declare the
// non-arm64 track so the file compiles; checkForUpdates() is not reached by the fuzz target.
#pragma once
static const struct
{
    const char* updateTrack;
} routing = {"msyncv2"};
