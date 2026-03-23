#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include "Scanner.h"
#include "AudioRouter.h"
#include "AudioController.h"

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

    // Initialize Controller
    AudioController controller;

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
    out << "COMMANDS:" << Qt::endl;
    out << "  add <ID>             : Route audio to sink" << Qt::endl;
    out << "  rem <ID>             : Stop routing to sink" << Qt::endl;
    out << "  vol <ID> <0.0-1.0>   : Set volume" << Qt::endl;
    out << "  mute <ID>            : Mute sink" << Qt::endl;
    out << "  unmute <ID>          : Unmute sink" << Qt::endl;
    out << "  status <ID>          : Get sink status" << Qt::endl;
    out << "  list                 : List sinks again" << Qt::endl;
    out << "  exit                 : Quit" << Qt::endl;

    // Command loop
    while (true) {
        out << Qt::endl << "> " << Qt::flush;
        QString line = in.readLine();

        if (line.isNull()) break;
        
        QString input = line.trimmed();
        if (input.isEmpty()) continue;

        QStringList parts = input.split(' ', Qt::SkipEmptyParts);
        QString cmd = parts[0].toLower();

        if (cmd == "exit" || cmd == "quit") {
            break;
        }
        else if (cmd == "list") {
            sinks = scanner.getSinks();
            out << "Available Sinks:" << Qt::endl;
            for (auto it = sinks.begin(); it != sinks.end(); ++it) {
                out << "----------------------------------------" << Qt::endl;
                out << "Name: " << it.value() << Qt::endl;
                out << "ID  : " << it.key() << Qt::endl;
            }
        }
        else if (parts.size() >= 2) {
            QString id = parts[1];
            
            if (cmd == "add") {
                router.addSink(id);
                out << "Added sink: " << sinks.value(id, id) << Qt::endl;
            }
            else if (cmd == "rem") {
                router.removeSink(id);
                out << "Removed sink: " << sinks.value(id, id) << Qt::endl;
            }
            else if (cmd == "mute") {
                if (controller.setMute(id, true))
                    out << "Muted " << sinks.value(id, id) << Qt::endl;
                else
                    out << "Failed to mute" << Qt::endl;
            }
            else if (cmd == "unmute") {
                if (controller.setMute(id, false))
                    out << "Unmuted " << sinks.value(id, id) << Qt::endl;
                else
                    out << "Failed to unmute" << Qt::endl;
            }
            else if (cmd == "status") {
                bool active = controller.isSinkActive(id);
                bool muted = controller.getMute(id);
                float vol = controller.getVolume(id);
                out << "Status for " << sinks.value(id, id) << ":" << Qt::endl;
                out << "  Active: " << (active ? "Yes" : "No") << Qt::endl;
                out << "  Muted : " << (muted ? "Yes" : "No") << Qt::endl;
                out << "  Volume: " << (vol * 100.0f) << "%" << Qt::endl;
            }
            else if (cmd == "vol" && parts.size() >= 3) {
                float v = parts[2].toFloat();
                if (controller.setVolume(id, v))
                    out << "Volume set to " << v << Qt::endl;
                else
                    out << "Failed to set volume" << Qt::endl;
            }
            else {
                out << "Unknown command." << Qt::endl;
            }
        }
        else {
            out << "Invalid command syntax." << Qt::endl;
        }
    }

    out << "Stopping router..." << Qt::endl;
    router.stop();

    return 0;
}