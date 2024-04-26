#include "qhttpserver.h"
#include "mongoose/mongoose.h"
#include <QDebug>
#include <QThreadPool>
#include <functional>
class QRunnableIml : public QRunnable {
public:
  static QRunnable *create(std::function<void()> functionToRun) {
    class FunctionRunnable : public QRunnable {
      std::function<void()> m_functionToRun;

    public:
      FunctionRunnable(std::function<void()> functionToRun)
          : m_functionToRun(std::move(functionToRun)) {
        this->setAutoDelete(true);
      }
      void run() override { m_functionToRun(); }
    };

    return new FunctionRunnable(std::move(functionToRun));
  }
};

struct QHttpServer::Private {
  QThreadPool threadPool;
  struct mg_mgr mgr;
};

QHttpServer::QHttpServer() : d(new Private) {
  d->threadPool.setMaxThreadCount(5);
  mg_mgr_init(&d->mgr);
}

QHttpServer::~QHttpServer() {

  d->threadPool.waitForDone();
  mg_mgr_free(&d->mgr);
}

int QHttpServer::init(QString ip, int port) {
  static QHttpServer *instance = this;
  mg_connection *mc = mg_http_listen(
      &d->mgr, QString("http://%1:%2").arg(ip).arg(port).toStdString().c_str(),
      [](mg_connection *c, int ev, void *ev_data) {
        QHttpServer *p = static_cast<QHttpServer *>(c->fn_data);
        p->mg_event_handler(c, ev, ev_data);
      },
      this);
  mg_wakeup_init(&d->mgr); // Initialise wakeup socket pair
}

void QHttpServer::exec(bool block) {
  while (true) {
    mg_mgr_poll(&d->mgr, 50);
  }
}

void QHttpServer::mg_event_handler(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    QString message = QString::fromLocal8Bit(hm->message.buf, hm->message.len);
    unsigned long conn_id = c->id;
    mg_mgr *mgr = c->mgr;
    d->threadPool.start(QRunnableIml::create([=]() {
      mg_wakeup(mgr, conn_id, "hi!", 3); // Respond to parent
    }));
    qDebug() << "MG_EV_HTTP_MSG";
  } else if (ev == MG_EV_WAKEUP) {
    struct mg_str *data = (struct mg_str *)ev_data;
    mg_http_reply(c, 200, "", "Result: %.*s\n", data->len, data->buf);
  }
}
