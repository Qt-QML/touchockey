// The MIT License (MIT)
//
// Copyright (c) 2015 Jocelyn Turcotte
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <QGuiApplication>
#include <QFile>
// #include <QQmlApplicationEngine>
// #include <QtQml/qtqmlglobal.h>
// #include <box2dplugin.h>
#include "httpserver.h"
// #include "lightedimageitem.h"
// #include "playerbox2dbody.h"
// #include "qscreensaver.h"

// #include "qqrencode.h"
// #include <QImage>
#include <QNetworkInterface>
#include <QTcpSocket>
// #include <QQuickImageProvider>

// class ImageProvider : public QQuickImageProvider
// {
// public:
//     ImageProvider(QUrl url)
//         : QQuickImageProvider(QQuickImageProvider::Image)
//         , m_url(std::move(url))
//     { }
//     QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize) override
//     {
//         QImage image;
//         if (id == QLatin1String("connectQr")) {
//             QQREncode encoder;
//             encoder.encode(m_url.toString());
//             image = encoder.toQImage();
//         }
//         if (!requestedSize.isEmpty())
//             image = image.scaled(requestedSize);
//         if (size)
//             *size = image.size();
//         // Make sure that it keeps its opaque flag
//         return image.convertToFormat(QImage::Format_RGB32);
//     }

// private:
//     QUrl m_url;
// };

int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);
    // QScreenSaver screenSaver;
    // screenSaver.setScreenSaverEnabled(false);

    // Box2DPlugin plugin;
    // plugin.registerTypes("Box2DStatic");
    // qmlRegisterType<LightedImageItem>("main", 1, 0, "LightedImage");
    // qmlRegisterType<LightGroup>("main", 1, 0, "LightGroup");
    // qmlRegisterType<PlayerBox2DBody>("main", 1, 0, "PlayerBox2DBody");

    // QUrl url{QStringLiteral("http://localhost:1234")};
    // for (const QHostAddress &address : QNetworkInterface::allAddresses())
    //     if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost)) {
    //         url.setHost(address.toString());
    //         break;
    //     }

    // QQmlApplicationEngine engine;
    // engine.addImageProvider("main", new ImageProvider{url});
    // engine.load(QUrl{"qrc:/qml/main.qml"});
    // engine.rootObjects().first()->setProperty("connectUrl", url);
    // engine.rootObjects().first()->setProperty("lowfi", LOWFI);

    // Use a blocking queued connection to make sure that we've initialized the QML Connection before emitting any message from the server thread.
    // GameServer server(1234);
    // QObject::connect(&server, SIGNAL(playerConnected(const QVariant &)), engine.rootObjects().first(), SLOT(onPlayerConnected(const QVariant &)), Qt::BlockingQueuedConnection);
    // QObject::connect(&server, SIGNAL(playerDisconnected(const QVariant &)), engine.rootObjects().first(), SLOT(onPlayerDisconnected(const QVariant &)));

    HttpServer httpServer;

    unsigned reqCounter = 0;
    int port = 5566;
    if (httpServer.listen(QHostAddress::Any, port)) {
        qDebug() << "HTTP Server listening on port" << port;
        HttpServer::connect(&httpServer, &HttpServer::normalHttpRequest, [&](const QByteArray &method, const QNetworkRequest &request, const QByteArray &body, QTcpSocket *connection, HttpConnection*httpConnection) {
            qWarning() << method << request.url();
            auto checkContents = [&] {
                const unsigned *currentInt = reinterpret_cast<const unsigned*>(body.constData());
                for (unsigned i = 0; i < body.size() / sizeof(unsigned); ++i) {
                    if (*(currentInt++) != i) {
                        qWarning() << "Error at" << i << *(currentInt-1);
                        return false;
                    }
                }
                return true;
            };
            if (body.size() != 5*1024*1024) {
                qWarning("Incorrect size: %d", body.size());
                connection->write("HTTP/1.1 400 Incorrect size\r\n");            
            } else if (!checkContents()) {
                qWarning() << "Incorrect contents" << connection->localPort() << connection->peerPort();
                connection->write("HTTP/1.1 400 Incorrect contents\r\n");            
                QFile yo("incorrect.dat");
                yo.open(QFile::WriteOnly);
                yo.write(body);
                yo.close();
                QFile ya("conversation.dat");
                ya.open(QFile::WriteOnly);
                ya.write(httpConnection->conversation);
                ya.close();
            } else 
            {
                // if ((reqCounter++)%10)
                    connection->write("HTTP/1.1 200 OK\r\n");            
                // else {
                //     connection->write("HTTP/1.1 401 Not Authorized\r\n");            
                //     connection->write("WWW-Authenticate: Basic realm=\"Boo\"\r\n");            
                // }
            }

            // bool shouldKeepAlive = ((reqCounter++)%10);

            connection->write("Content-Length: 0\r\n");
            // if (shouldKeepAlive)
                connection->write("Connection: Keep-Alive\r\n");
            // else
                // connection->write("Connection: close\r\n");
            connection->write("\r\n");
            connection->flush();
            // if (shouldKeepAlive)

                // MOUAHAHAHAHAHAHA!
                connection->close();
        });
    }

    return a.exec();
}
