#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include "Scanner.h"
#include "AudioRouter.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    QTextStream in(stdin);

    out << "========================================" << Qt::endl;
    out << "       AudioMan Routing Test            " << Qt::endl;
    out << "========================================" << Qt::endl;

    // Initialize Scanner to find devices
    Scanner scanner;
    
    // Initialize Router
    AudioRouter router;

    // Get and list available sinks
    out << "Scanning for output devices..." << Qt::endl;
    QMap<QString, QString> sinks = scanner.getSinks();

    if (sinks.isEmpty()) {
        out << "No output devices found!" << Qt::endl;
        return 0;
    }

    out << "Available Sinks:" << Qt::endl;
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        out << "----------------------------------------" << Qt::endl;
        out << "Name: " << it.value() << Qt::endl;
        out << "ID  : " << it.key() << Qt::endl;
    }
    out << "----------------------------------------" << Qt::endl;

    // Start the router (capturing system audio)
    router.start();
    out << Qt::endl << "Router started (Capturing System Audio)." << Qt::endl;
    out << "Paste a Sink ID from the list above to route audio to it." << Qt::endl;
    out << "You can add multiple sinks." << Qt::endl;
    out << "Type 'exit' or 'quit' to stop." << Qt::endl;

    // Command loop
    while (true) {
        out << Qt::endl << "Enter Sink ID: " << Qt::flush;
        QString line = in.readLine();

        if (line.isNull()) break;
        
        QString input = line.trimmed();
        
        if (input.compare("exit", Qt::CaseInsensitive) == 0 || 
            input.compare("quit", Qt::CaseInsensitive) == 0) {
            break;
        }

        if (input.isEmpty()) continue;

        // Check if it's a known sink for better feedback
        if (sinks.contains(input)) {
            out << "Adding sink: " << sinks[input] << Qt::endl;
        } else {
            out << "Adding sink with ID: " << input << Qt::endl;
        }

        router.addSink(input);
        out << "Audio should now be playing on this device." << Qt::endl;
    }

    out << "Stopping router..." << Qt::endl;
    router.stop();

    return 0;
}