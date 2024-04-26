#include "qhttpserver.h"
#include "mongoose/mongoose.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
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
typedef struct {
  QHttpServer *instance;
  QDateTime openTime;
} UserData;

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
        UserData *p = static_cast<UserData *>(c->fn_data);
        instance->mg_event_handler(c, ev, ev_data);
      },
      0);
  mg_wakeup_init(&d->mgr); // Initialise wakeup socket pair
  return 0;
}

void QHttpServer::mg_event_handler(void *connection, int ev, void *ev_data) {
  static unsigned long long requestNumber = 0;
  static unsigned long long closeNumber = 0;
  mg_connection *c = static_cast<mg_connection *>(connection);
  if (ev == MG_EV_OPEN) {

    this->mgOpenEvent(connection, ev, ev_data);

  } else if (ev == MG_EV_ACCEPT) {

    this->mgAcceptEvent(connection, ev, ev_data);

  } else if (ev == MG_EV_POLL) {

    this->mgPollEvent(connection, ev, ev_data);

  } else if (ev == MG_EV_CLOSE) {
    if (c->fn_data != 0) {
      delete (UserData *)c->fn_data;
    }

  } else if (ev == MG_EV_HTTP_MSG) {

    if (c->is_closing == 1) {
      return;
    }

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    QString message = QString::fromLocal8Bit(hm->message.buf, hm->message.len);
    d->threadPool.start(QRunnableIml::create([=]() {
      this->mgHttpMsg(message);
      mg_wakeup(c->mgr, c->id, "hi!", 3); // Respond to parent
    }));

  } else if (ev == MG_EV_WAKEUP) {
    struct mg_str *data = (struct mg_str *)ev_data;
    mg_http_reply(c, 200,
                  "Content-Type:application/"
                  "json;charset=UTF-8\r\nAccess-Control-Allow-Origin:*"
                  "\r\nAccess-Control-Allow-Methods:*\r\nAccess-Control-Allow-"
                  "Headers:Origin, X-Requested-With, Content-Type, Accept, "
                  "Authorization\r\nAccess-Control-Allow-Credentials:"
                  "true\r\n",
                  "Result: %.*s\n", data->len, data->buf);
    c->is_draining = 1;
  }
}

void QHttpServer::run() {

  while (!this->isInterruptionRequested()) {
    mg_mgr_poll(&d->mgr, 50);
  }
  mg_mgr_free(&d->mgr);
}

void QHttpServer::mgOpenEvent(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);
}

void QHttpServer::mgAcceptEvent(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);
  if (d->threadPool.activeThreadCount() >= d->threadPool.maxThreadCount()) {
    c->is_closing = 1;
    return;
  }

  if (c->fn_data == 0) {
    c->fn_data = new UserData;
    UserData *userData = static_cast<UserData *>(c->fn_data);
    userData->openTime = QDateTime::currentDateTime();
  };
}

void QHttpServer::mgPollEvent(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);
  UserData *userData = static_cast<UserData *>(c->fn_data);
  if (userData != 0) {
    if (qAbs(QDateTime::currentDateTime().secsTo(userData->openTime)) > 3) {
      c->is_closing = 1;
    }
  }
}

void QHttpServer::mgHttpMsg(const QString &msg) {
  ;
  QThread::msleep(10);
}
