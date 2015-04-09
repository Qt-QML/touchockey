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

#include "httpserver.h"

#include <QTcpSocket>
#include <QSslSocket>
#include <QSslConfiguration>

HttpConnection::HttpConnection(QTcpSocket *socket)
    : m_socket(socket)
{
    // FIXME: Handle connection close -> delete
    connect(socket, &QTcpSocket::readyRead, this, &HttpConnection::onReadyRead);
    onReadyRead();
}

HttpConnection::~HttpConnection()
{
}

void HttpConnection::onReadyRead()
{
    while (m_socket->bytesAvailable()) {
        QByteArray tmp = m_socket->read(512);
        m_buffer += tmp;
        conversation += tmp;
        if (state < State::Body) {
            int crPos = m_buffer.indexOf('\r');
            while (crPos > -1 && crPos + 1 < m_buffer.size() && m_buffer[crPos + 1] == '\n') {
                QByteArray line = m_buffer.left(crPos);
                m_buffer.remove(0, crPos + 2);
                crPos = m_buffer.indexOf('\r');

                if (line.isEmpty())
                    state = State::Body;
                else if (state == State::ParsingRequestLine)
                    processRequestLine(line);
                else if (state == State::ParsingHeaders)
                    processHeaderLine(line);
            }
        }
        if (state == State::Body) {
            uint64_t contentLength = m_request.header(QNetworkRequest::ContentLengthHeader).toLongLong();
            if (m_buffer.size() >= contentLength) {
                m_body = m_buffer.left(contentLength);
                m_buffer.remove(0, contentLength);

                emit requestReady();
                if (!m_socket->isOpen())
                    deleteLater();
                else
                    state = State::ParsingRequestLine;
                return;
            }
        }
    }
}

void HttpConnection::processRequestLine(const QByteArray &line)
{
    Q_ASSERT(state == State::ParsingRequestLine);
    auto tokens = line.split(' ');
    m_method = tokens[0];
    QUrl requestUrl(tokens[1]);
    m_request.setUrl(requestUrl);
    state = State::ParsingHeaders;
}

void HttpConnection::processHeaderLine(const QByteArray &line)
{
    Q_ASSERT(state == State::ParsingHeaders);
    int colonPos = line.indexOf(':');
    QByteArray name = line.left(colonPos);
    QByteArray value = line.mid(colonPos + 2);
    m_request.setRawHeader(name, value);
}

HttpServer::HttpServer()
{
    connect(this, &HttpServer::newConnection, this, &HttpServer::onNewConnection);
}

void HttpServer::onNewConnection()
{
    QTcpSocket *client = nextPendingConnection();
    auto *connection = new HttpConnection(client);
    connect(connection, &HttpConnection::requestReady, this, &HttpServer::onRequestReady);
}

void HttpServer::incomingConnection(qintptr socket)
{
    // return QTcpServer::incomingConnection(socket);

    QSslSocket *pSslSocket = new QSslSocket();

    if (Q_LIKELY(pSslSocket)) {
        pSslSocket->setSslConfiguration(QSslConfiguration::defaultConfiguration());

        if (Q_LIKELY(pSslSocket->setSocketDescriptor(socket))) {
            connect(pSslSocket, &QSslSocket::peerVerifyError, [](const QSslError &error){
                qDebug() << __func__ << error;
            });

            typedef void (QSslSocket::* sslErrorsSignal)(const QList<QSslError> &);
            connect(pSslSocket, static_cast<sslErrorsSignal>(&QSslSocket::sslErrors),  [](const QList<QSslError> &errors){
                qDebug() << __func__ << errors;
            });
            // connect(pSslSocket, &QSslSocket::encrypted,  []{
            //     qDebug() << __func__ << "QSslSocket::encrypted!!";
            // });

            addPendingConnection(pSslSocket);

            pSslSocket->setProtocol(QSsl::AnyProtocol);
            pSslSocket->setLocalCertificate("cert.pem");
            pSslSocket->setPrivateKey("key.pem");

            pSslSocket->startServerEncryption();
        } else {
            qFatal("NNNNNNNOOOOOO!");
           delete pSslSocket;
        }
    }
}


void HttpServer::onRequestReady()
{
    auto *connection = static_cast<HttpConnection*>(sender());
    emit normalHttpRequest(connection->method(), connection->request(), connection->body(), connection->socket(), connection);
}
