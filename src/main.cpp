#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "gui/MainWindow.h"

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    QString dt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString txt = QString("[%1] ").arg(dt);

    switch (type) {
        case QtDebugMsg:
            txt += QString("Debug: %1").arg(msg);
            break;
        case QtWarningMsg:
            txt += QString("Warning: %1").arg(msg);
            break;
        case QtCriticalMsg:
            txt += QString("Critical: %1").arg(msg);
            break;
        case QtFatalMsg:
            txt += QString("Fatal: %1").arg(msg);
            abort();
    }

    QFile outFile("audioman_log.txt");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream ts(&outFile);
        ts << txt << Qt::endl;
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(customMessageHandler);
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icon.png"));

    MainWindow window;
    window.show();

    return app.exec();
}