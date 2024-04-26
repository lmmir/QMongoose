#include "qhttpserver.h"
#include "mongoose/mongoose.h"
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QThreadPool>
#include <functional>
#include <set>
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
  std::set<mg_connection *> set_mg_connection;
};

QHttpServer::QHttpServer() : d(new Private) {
  d->threadPool.setMaxThreadCount(100);
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

  } else if (ev == MG_EV_ERROR) {

    this->mgErrorEvent(connection, ev, ev_data);
    if (d->set_mg_connection.find(c) != d->set_mg_connection.end()) {
      d->set_mg_connection.erase(c);
    }

  } else if (ev == MG_EV_ACCEPT) {

    this->mgAcceptEvent(connection, ev, ev_data);

  } else if (ev == MG_EV_POLL) {

    this->mgPollEvent(connection, ev, ev_data);

  } else if (ev == MG_EV_CLOSE) {
    if (c->fn_data != 0) {
      delete (UserData *)c->fn_data;
    }
    if (d->set_mg_connection.find(c) != d->set_mg_connection.end()) {
      d->set_mg_connection.erase(c);
    }

  } else if (ev == MG_EV_HTTP_MSG) {

    if (c->is_closing == 1) {
      return;
    }

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    QString message = QString::fromLocal8Bit(hm->message.buf, hm->message.len);
    mg_mgr *mgr = c->mgr;
    unsigned long id = c->id;

    d->threadPool.start(QRunnableIml::create([this, mgr, id, message]() {
      this->mgHttpMsg(message);
      mg_wakeup(mgr, id, "hi!", 3); // Respond to parent
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
  QElapsedTimer et;
  et.start();
  while (!this->isInterruptionRequested()) {
    mg_mgr_poll(&d->mgr, 50);

    if (et.elapsed() > 1000) {
      qDebug() << d->set_mg_connection.size();
      et.restart();
    }
  }
  mg_mgr_free(&d->mgr);
}

void QHttpServer::mgOpenEvent(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);
  d->set_mg_connection.insert(c);
}

void QHttpServer::mgAcceptEvent(void *connection, int ev, void *ev_data) {
  mg_connection *c = static_cast<mg_connection *>(connection);

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
    if (qAbs(QDateTime::currentDateTime().secsTo(userData->openTime)) > 60000) {
      c->is_closing = 1;
    }
  }
}

void QHttpServer::mgErrorEvent(void *connection, int ev, void *ev_data) {
  ;
  qDebug() << __FUNCTION__;
}

void QHttpServer::mgHttpMsg(const QString &msg) {
  ;
  QThread::msleep(10);
}
