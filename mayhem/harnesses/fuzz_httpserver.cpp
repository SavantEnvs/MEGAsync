// libFuzzer harness for MEGAsync's localhost webclient HTTP bridge (src/MEGASync/control/HTTPServer.cpp).
// Any web page the user visits can POST to this server, so the request body is untrusted. Each input
// is sent as the body of one POST over a real loopback TCP connection:
// readClient -> processPostRequest -> processRequest -> the request handlers
// (externalDownloadRequest, the 52f4b333 fix, and the others).
// Kept across inputs on purpose: the app objects and an offline (never logged-in) MegaApi, as in
// upstream's UnitTests main.cpp (MegaApplication constructed, never initialize()d). Each request
// uses a fresh connection. HTTPServer's process-global request state (the static
// webDataRequests / webTransferStateRequests maps, filled by the "l", "d" and upload/sync
// requests and read by the "t"/"sf" queries, and the isFirstWebDownloadDone flag) is reset after
// every input, so each input's result depends only on that input.
#include "MegaApplication.h"
#include "Platform.h"
#include "control/HTTPServer.h"
#include "megaapi.h"
#include "private_access.h"

#include <QCoreApplication>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static void silencedHandler(QtMsgType type, const QMessageLogContext&, const QString&)
{
    if (type == QtFatalMsg)
    {
        abort();
    }
}

static HTTPServer* gServer = nullptr;

// HTTPServer's private static request state (control/HTTPServer.h).
using WebDataRequestsPtr = QMultiMap<QString, RequestData*>*;
using WebTransferStateRequestsPtr = QMap<mega::MegaHandle, RequestTransferData*>*;
MAYHEM_PRIVATE_ACCESS(FirstWebDownloadDoneTag, bool*, &HTTPServer::isFirstWebDownloadDone);
MAYHEM_PRIVATE_ACCESS(WebDataRequestsTag, WebDataRequestsPtr, &HTTPServer::webDataRequests);
MAYHEM_PRIVATE_ACCESS(WebTransferStateRequestsTag, WebTransferStateRequestsPtr, &HTTPServer::webTransferStateRequests);

static void resetServerState()
{
    *get(FirstWebDownloadDoneTag()) = false;
    auto* data = get(WebDataRequestsTag());
    qDeleteAll(*data);
    data->clear();
    auto* transfers = get(WebTransferStateRequestsTag());
    qDeleteAll(*transfers);
    transfers->clear();
}

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
    new MegaApplication(qargc, qargv);
    if (cwd)
    {
        if (chdir(cwd) != 0)
        {
            abort();
        }
        free(cwd);
    }
    static QTemporaryDir dir;
    auto* api = new mega::MegaApi("FUZZKEY", dir.path().toUtf8().constData(), "fuzz");
    mega::MegaApi::setLogLevel(mega::MegaApi::LOG_LEVEL_FATAL);
    gServer = new HTTPServer(api, 0);
    if (!gServer->isListening())
    {
        fprintf(stderr, "HTTPServer not listening\n");
        abort();
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    QByteArray body(reinterpret_cast<const char*>(data), static_cast<int>(size));
    // The server counts Content-length in QString (UTF-16) units.
    int clen = QString::fromUtf8(body.constData(), body.size()).size();
    QByteArray msg = "POST / HTTP/1.1\r\nOrigin: https://mega.nz\r\nContent-length: " +
                     QByteArray::number(clen) + "\r\n\r\n" + body;
    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, gServer->serverPort());
    // Event-loop passes, not wall-clock: loopback connect and the server's synchronous handling
    // finish within a few passes.
    for (int i = 0; i < 2000 && sock.state() != QAbstractSocket::ConnectedState; ++i)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    if (sock.state() != QAbstractSocket::ConnectedState)
    {
        resetServerState();
        return 0;
    }
    sock.write(msg);
    sock.flush();
    for (int i = 0; i < 2000 && sock.state() == QAbstractSocket::ConnectedState && sock.bytesAvailable() == 0; ++i)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    sock.abort();
    for (int i = 0; i < 4; ++i)
    {
        QCoreApplication::processEvents();
    }
    resetServerState();
    return 0;
}
