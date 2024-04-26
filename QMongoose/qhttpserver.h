#ifndef QHTTPSERVER_H
#define QHTTPSERVER_H

#include <QObject>
#include <QRunnable>
#include <QThread>
class QHttpServer : public QThread {
  Q_OBJECT
public:
  QHttpServer();
  ~QHttpServer();
  int init(QString ip = "127.0.0.1", int port = 8081);
  void test(int *, int);

private:
  void mg_event_handler(void *c, int ev, void *ev_data);

  void httpRequest(QString message);
  void run() override;

  void mgOpenEvent(void *connection, int ev, void *ev_data);
  void mgAcceptEvent(void *connection, int ev, void *ev_data);
  void mgPollEvent(void *connection, int ev, void *ev_data);

private:
  struct Private;
  QScopedPointer<Private> d;
};

#endif // QHTTPSERVER_H
