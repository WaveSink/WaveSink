#pragma once

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPropertyAnimation>
#include "../Scanner.h"
#include "../AudioRouter.h"
#include "../AudioController.h"

class SinkListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSinkAdded(const QString &id);
    void onSinkRemoved(const QString &id);
    void onSinkSelected(const QString &id);
    void onPlayToggled(bool checked);
    void onVolumeChanged(int value);
    void onEqValueChanged();
    void onEqToggled(bool checked);

private:
    void setupUi();
    void loadInitialSinks();

    SinkListWidget *m_sinkList;
    Scanner *m_scanner;
    AudioRouter *m_router;
    AudioController *m_controller;

    QString m_currentSinkId;

    QWidget *m_detailsWidget;
    QCheckBox *m_playCheckBox;
    QSlider *m_volumeSlider;
    QLabel *m_volumeLabel;
    QPropertyAnimation *m_detailsAnimation;

    class QPushButton *m_eqToggleButton;
    QWidget *m_eqContainer;

    QList<QSlider*> m_eqSliders;
    QMap<QString, QList<int>> m_eqSettings;
};