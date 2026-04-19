#pragma once

#include <QScrollArea>
#include <QMap>

class QHBoxLayout;
class SinkItemWidget;

class SinkListWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit SinkListWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;

public slots:
    void addSink(const QString &id, const QString &name, uint formFactor);
    void removeSink(const QString &id);
    void clear();

signals:
    void sinkSelected(const QString &id);

private slots:
    void onItemClicked(const QString &id);

private:
    QWidget *m_container;
    QHBoxLayout *m_layout;
    QMap<QString, SinkItemWidget*> m_items;
    QString m_selectedId;
};