#include "mainwindow.h"

#include "qhttpserver.h"
#include <QApplication>
int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  QHttpServer s;
  s.init();
  s.start();
  MainWindow w;
  w.show();
  return a.exec();
}
