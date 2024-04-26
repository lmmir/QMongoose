#ifndef QHTTPSERVER_H
#define QHTTPSERVER_H

#include <QObject>
#include <QRunnable>
class QHttpServer : public QObject {
  Q_OBJECT
public:
  QHttpServer();
  ~QHttpServer();
  int init(QString ip = "127.0.0.1", int port = 8081);
  void test(int *, int);
  void exec(bool block = false);

private:
  void mg_event_handler(void *c, int ev, void *ev_data);

  void httpRequest(QString message);

private:
  struct Private;
  QScopedPointer<Private> d;
};

#endif // QHTTPSERVER_H
