#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "Scanner.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "Starting AudioMan Scanner Test...";

    Scanner scanner;

    QObject::connect(&scanner, &Scanner::sinkAdded, [](const QString &id){
        qDebug() << "Sink Added:" << id;
    });

    QObject::connect(&scanner, &Scanner::sinkRemoved, [](const QString &id){
        qDebug() << "Sink Removed:" << id;
    });

    QObject::connect(&scanner, &Scanner::sourceAdded, [](const QString &id){
        qDebug() << "Source Added:" << id;
    });

    QObject::connect(&scanner, &Scanner::sourceRemoved, [](const QString &id){
        qDebug() << "Source Removed:" << id;
    });

    // Schedule a listing of devices after a short delay to allow enumeration
    QTimer::singleShot(2000, &scanner, [&scanner](){
        qDebug() << "\n--- Current Sinks ---";
        for (const auto& sink : scanner.getSinks()) {
            qDebug() << sink;
        }

        qDebug() << "\n--- Current Sources (Devices & Apps) ---";
        for (const auto& source : scanner.getSources()) {
            qDebug() << source;
        }
        qDebug() << "--------------------------------------\n";
    });

    return app.exec();
}
