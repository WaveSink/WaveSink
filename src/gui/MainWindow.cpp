#include "MainWindow.h"
#include "SinkListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_sinkList(nullptr)
    , m_scanner(new Scanner(this))
    , m_router(new AudioRouter(this))
    , m_controller(new AudioController(this))
{
    setupUi();

    connect(m_scanner, &Scanner::sinkAdded, this, &MainWindow::onSinkAdded);
    connect(m_scanner, &Scanner::sinkRemoved, this, &MainWindow::onSinkRemoved);
    connect(m_sinkList, &SinkListWidget::sinkSelected, this, &MainWindow::onSinkSelected);

    loadInitialSinks();
    m_router->start();
}

MainWindow::~MainWindow()
{
    m_router->stop();
}

void MainWindow::setupUi()
{
    setWindowTitle("AudioMan - Sink Manager");

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(2);

    QLabel *titleLabel = new QLabel("Available Audio Devices", centralWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    m_sinkList = new SinkListWidget(centralWidget);
    layout->addWidget(m_sinkList);

    m_detailsWidget = new QWidget(centralWidget);
    QVBoxLayout *detailsLayout = new QVBoxLayout(m_detailsWidget);
    detailsLayout->setContentsMargins(0, 10, 0, 0);

    m_playCheckBox = new QCheckBox("Play on this device", m_detailsWidget);
    connect(m_playCheckBox, &QCheckBox::toggled, this, &MainWindow::onPlayToggled);
    detailsLayout->addWidget(m_playCheckBox);

    QHBoxLayout *volumeLayout = new QHBoxLayout();
    QLabel *volTextLabel = new QLabel("Volume", m_detailsWidget);
    m_volumeLabel = new QLabel("100%", m_detailsWidget);
    
    volumeLayout->addWidget(volTextLabel);
    volumeLayout->addStretch();
    volumeLayout->addWidget(m_volumeLabel);

    detailsLayout->addLayout(volumeLayout);

    m_volumeSlider = new QSlider(Qt::Horizontal, m_detailsWidget);
    m_volumeSlider->setRange(0, 100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    detailsLayout->addWidget(m_volumeSlider);

    layout->addWidget(m_detailsWidget);
    
    m_detailsAnimation = new QPropertyAnimation(m_detailsWidget, "maximumHeight", this);
    m_detailsAnimation->setDuration(200);
    m_detailsWidget->setMaximumHeight(0);

    layout->addStretch(1);
}

void MainWindow::loadInitialSinks()
{
    if (!m_scanner || !m_sinkList) return;

    QMap<QString, QString> sinks = m_scanner->getSinks();
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        QString id = it.key();
        QString name = it.value();
        uint formFactor = m_scanner->getFormFactor(id);
        m_sinkList->addSink(id, name, formFactor);
    }
}

void MainWindow::onSinkAdded(const QString &id)
{
    // Need to fetch details for the new sink
    // Since signals are queued, the map inside Scanner should be updated already
    QMap<QString, QString> sinks = m_scanner->getSinks();
    if (sinks.contains(id)) {
        QString name = sinks.value(id);
        uint formFactor = m_scanner->getFormFactor(id);
        m_sinkList->addSink(id, name, formFactor);
    }
}

void MainWindow::onSinkRemoved(const QString &id)
{
    m_sinkList->removeSink(id);
}

void MainWindow::onSinkSelected(const QString &id)
{
    m_currentSinkId = id;

    // Update UI based on sink status
    bool isActive = m_router->hasSink(id);
    m_playCheckBox->blockSignals(true);
    m_playCheckBox->setChecked(isActive);
    m_playCheckBox->blockSignals(false);

    float vol = m_controller->getVolume(id);
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(vol * 100);
    m_volumeSlider->blockSignals(false);
    m_volumeLabel->setText(QString::number(int(vol * 100)) + "%");

    // Expand details if not already
    if (m_detailsWidget->maximumHeight() == 0) {
        m_detailsWidget->show();
        m_detailsAnimation->setStartValue(0);
        m_detailsAnimation->setEndValue(m_detailsWidget->sizeHint().height());
        m_detailsAnimation->start();
    }
}

void MainWindow::onPlayToggled(bool checked)
{
    if (m_currentSinkId.isEmpty()) return;

    if (checked) {
        m_router->addSink(m_currentSinkId);
    } else {
        m_router->removeSink(m_currentSinkId);
    }
}

void MainWindow::onVolumeChanged(int value)
{
    if (m_currentSinkId.isEmpty()) return;

    float vol = value / 100.0f;
    m_controller->setVolume(m_currentSinkId, vol);
    m_volumeLabel->setText(QString::number(value) + "%");
}
