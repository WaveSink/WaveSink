#include "SinkListWidget.h"
#include "SinkItemWidget.h"
#include <QHBoxLayout>
#include <QDebug>

SinkListWidget::SinkListWidget(QWidget *parent)
    : QScrollArea(parent)
    , m_container(new QWidget(this))
    , m_layout(new QHBoxLayout(m_container))
{
    // Setup ScrollArea
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    // Setup Container Layout
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(10);
    m_layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_container->setLayout(m_layout);
    setWidget(m_container);
}

QSize SinkListWidget::sizeHint() const
{
    return m_container->sizeHint();
}

void SinkListWidget::addSink(const QString &id, const QString &name, uint formFactor)
{
    if (m_items.contains(id)) {
        return;
    }

    SinkItemWidget *item = new SinkItemWidget(id, name, formFactor, m_container);
    m_layout->addWidget(item);
    m_items.insert(id, item);

    connect(item, &SinkItemWidget::clicked, this, &SinkListWidget::onItemClicked);
    updateGeometry();
}

void SinkListWidget::removeSink(const QString &id)
{
    if (m_items.contains(id)) {
        SinkItemWidget *item = m_items.take(id);
        m_layout->removeWidget(item);
        item->deleteLater();
        updateGeometry();
    }
}

void SinkListWidget::clear()
{
    QList<SinkItemWidget*> items = m_items.values();
    m_items.clear();
    
    for (SinkItemWidget *item : items) {
        m_layout->removeWidget(item);
        item->deleteLater();
    }
    updateGeometry();
}

void SinkListWidget::onItemClicked(const QString &id)
{
    if (m_selectedId == id) {
        return;
    }

    if (!m_selectedId.isEmpty() && m_items.contains(m_selectedId)) {
        m_items[m_selectedId]->setSelected(false);
    }

    m_selectedId = id;

    if (m_items.contains(m_selectedId)) {
        m_items[m_selectedId]->setSelected(true);
    }

    emit sinkSelected(id);
}