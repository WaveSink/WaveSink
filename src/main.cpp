#include <QCoreApplication>
#include <QDebug>
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

    return app.exec();
}
