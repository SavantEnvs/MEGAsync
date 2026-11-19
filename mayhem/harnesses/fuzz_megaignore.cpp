// libFuzzer harness for MEGAsync's .megaignore reader (src/MEGASync/syncs/control/MegaIgnoreManager.cpp
// and MegaIgnoreRules.cpp). A .megaignore lives inside each synced folder and is synced like any other
// file, so its contents come from whoever can write to the cloud folder (a share, another client).
// Each input is written as <scratch>/.megaignore and read through the manager's own entry point,
// MegaIgnoreManager(folder, false) -> setInputDirPath -> parseIgnoresFile, then every parsed rule is
// queried and re-serialized with applyChanges() (MegaIgnoreRule::getModifiedRule for each rule).
// Kept across inputs on purpose: the app objects and an offline (never logged-in) MegaApi, as in
// upstream's UnitTests main.cpp; parseIgnoresFile only calls the MegaApi's stateless getCRC(). The
// scratch files are rewritten for every input, so nothing an input does reaches the next one.
#include "MegaApplication.h"
#include "Platform.h"
#include "megaapi.h"
#include "private_access.h"
#include "syncs/control/MegaIgnoreManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// MegaApplication::megaApi is only set by initialize(), which needs a login flow; the reader calls
// MegaSyncApp->getMegaApi()->getCRC(), so give the constructed app an offline MegaApi.
MAYHEM_PRIVATE_ACCESS(AppMegaApiTag, mega::MegaApi* MegaApplication::*, &MegaApplication::megaApi);

static void silencedHandler(QtMsgType type, const QMessageLogContext&, const QString&)
{
    if (type == QtFatalMsg)
    {
        abort();
    }
}

static QString gFolder;
static QString gIgnoreFile;
static QString gOutputFile;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    setenv("QT_QPA_PLATFORM", "offscreen", 0);
    qInstallMessageHandler(silencedHandler);
    Platform::create();
    static int qargc = 1;
    static char* qargv[] = {(*argv)[0], nullptr};
    // The MegaApplication constructor changes the working directory; restore it so the fuzzer's
    // relative corpus/artifact paths keep working.
    char* cwd = getcwd(nullptr, 0);
    auto* app = new MegaApplication(qargc, qargv);
    if (cwd)
    {
        if (chdir(cwd) != 0)
        {
            abort();
        }
        free(cwd);
    }
    static QTemporaryDir sdkDir;
    static QTemporaryDir scratch;
    if (!sdkDir.isValid() || !scratch.isValid())
    {
        fprintf(stderr, "no scratch directory\n");
        abort();
    }
    auto* api = new mega::MegaApi("FUZZKEY", sdkDir.path().toUtf8().constData(), "fuzz");
    mega::MegaApi::setLogLevel(mega::MegaApi::LOG_LEVEL_FATAL);
    app->*get(AppMegaApiTag()) = api;
    gFolder = scratch.path();
    gIgnoreFile = gFolder + QDir::separator() + QString::fromUtf8(MegaIgnoreManager::MEGA_IGNORE_FILE_NAME);
    gOutputFile = gFolder + QDir::separator() + QString::fromUtf8("out.megaignore");
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    QFile::remove(gOutputFile);
    {
        QFile in(gIgnoreFile);
        if (!in.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            abort();
        }
        in.write(reinterpret_cast<const char*>(data), static_cast<qint64>(size));
    }

    MegaIgnoreManager manager(gFolder, false);
    for (const auto& rule : manager.getAllRules())
    {
        (void)rule->isValid();
        (void)rule->ruleType();
        (void)rule->getDisplayText();
        (void)manager.findRule(rule->getModifiedRule());
    }
    (void)manager.getExcludedExtensions();
    (void)manager.getLowLimitRule();
    (void)manager.getHighLimitRule();
    (void)manager.getIgnoreSymLink();
    (void)manager.enabledRulesCount();
    (void)manager.getNameRulesCount();
    // Re-serialize every rule into a separate output file (the exclusions dialog's save path).
    manager.setOutputIgnorePath(gOutputFile);
    (void)manager.applyChanges();
    return 0;
}
