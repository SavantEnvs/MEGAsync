// Fuzz MEGAupdater's update-manifest reader, UpdateTask::processUpdateFile
// (src/MEGAUpdater/UpdateTask.cpp). The manifest (v.txt) is fetched over plain http://
// (updateCheckUrlForTrack), so its bytes are attacker-controlled up to the RSA signature check,
// and processUpdateFile parses all of it -- version, per-file URL/path/signature lines, the
// base64 signatures (Base64::atob into a fixed 512-byte buffer) -- before that check.
// Input: the manifest file. The installed version file it compares against is created once,
// read-only, in a private $HOME (getAppDataDir() derives the app data folder from $HOME).
#include "UpdateTask.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// macOS-only downloader (MacUtils.mm, Cocoa). processUpdateFile never downloads; the real
// checkForUpdates() path that would is not fuzzed. Network stand-in only.
bool downloadFileSynchronously(std::string, std::string, std::string)
{
    return false;
}

namespace
{
struct ManifestReader: UpdateTask
{
    using UpdateTask::processUpdateFile;
};
} // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    const char* tmp = getenv("TMPDIR");
    std::string home = std::string(tmp && *tmp ? tmp : "/tmp") + "/megaupd-XXXXXX";
    if (!mkdtemp(&home[0]))
    {
        abort();
    }
    std::string dir = home;
    for (const char* part: {"/Library", "/Application Support", "/Mega Limited", "/MEGAsync"})
    {
        dir += part;
        mkdir(dir.c_str(), 0700);
    }
    // Installed version 1: any manifest with version > 1 is parsed in full.
    std::ofstream(dir + "/megasync.version") << "1";
    setenv("HOME", home.c_str(), 1);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return -1; // fmemopen() rejects an empty buffer
    }
    FILE* fd = fmemopen(const_cast<uint8_t*>(data), size, "r");
    if (!fd)
    {
        return -1;
    }
    ManifestReader task;
    (void)task.processUpdateFile(fd);
    fclose(fd);
    return 0;
}
