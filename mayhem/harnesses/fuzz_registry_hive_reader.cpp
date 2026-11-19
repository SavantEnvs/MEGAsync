// Fuzz the Windows registry hive ("regf") reader, src/MEGASync/control/RegistryHiveReader.cpp.
// The hive it parses belongs to another process (the Snipping Tool's UserClasses.dat) and may be
// truncated or mid-rewrite when the app reads it, so the whole file is untrusted input.
// The input is the hive file; lookups mirror the real caller
// (platform/win/PlatformImplementation.cpp) plus paths that stop short / are empty.
#include "RegistryHiveReader.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>

static QStringList managedByApp()
{
    return QStringList{QStringLiteral("Local Settings"),
                       QStringLiteral("Software"),
                       QStringLiteral("Microsoft"),
                       QStringLiteral("Windows"),
                       QStringLiteral("CurrentVersion"),
                       QStringLiteral("AppModel"),
                       QStringLiteral("SystemAppData"),
                       QStringLiteral("Microsoft.ScreenSketch_8wekyb3d8bbwe"),
                       QStringLiteral("PersistedStorageItemTable"),
                       QStringLiteral("ManagedByApp")};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > 0x7fffffff)
    {
        return -1;
    }
    static const QStringList base = managedByApp();
    static const QStringList first = base + QStringList{QStringLiteral("{6D6C522E-2F56-415E-9FC4-7AC2BDD7EE6C}")};
    static const QStringList second = base + QStringList{QStringLiteral("{AD469B09-92F7-4C6E-8047-EBA464162545}")};
    static const QStringList shortPath{QStringLiteral("Local Settings")};

    // Same construction as the caller: a raw view over the file bytes.
    const RegistryHiveReader reader(
        QByteArray::fromRawData(reinterpret_cast<const char*>(data), static_cast<int>(size)));
    (void)reader.isValid();
    for (const QStringList* path: {&first, &second, &base, &shortPath})
    {
        (void)reader.readString(*path, QStringLiteral("Metadata"));
        (void)reader.readString(*path, QStringLiteral("FilePath"));
    }
    (void)reader.readString(QStringList(), QStringLiteral("Metadata"));
    return 0;
}
