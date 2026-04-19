#include "SinkItemWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QFontMetrics>

SinkItemWidget::SinkItemWidget(const QString &id, const QString &name, uint formFactor, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
    , m_name(name)
    , m_formFactor(formFactor)
{
    setupUi();
}

void SinkItemWidget::setupUi()
{
    setFixedSize(120, 120); 

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 10, 5, 5);
    layout->setSpacing(5);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setPixmap(getIcon());
    // Allow icon to take up available space
    m_iconLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_nameLabel = new QLabel(m_name, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    
    // Configure font
    QFont font = m_nameLabel->font();
    font.setPointSize(9);
    m_nameLabel->setFont(font);

    // Elide text if too long
    QFontMetrics metrics(font);
    QString elidedText = metrics.elidedText(m_name, Qt::ElideRight, 100);
    m_nameLabel->setText(elidedText);
    m_nameLabel->setToolTip(m_name);
    m_nameLabel->setFixedHeight(20);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_nameLabel);
}

void SinkItemWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void SinkItemWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit clicked(m_id);
}

void SinkItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw border box
    QRect r = rect().adjusted(2, 2, -2, -2);
    if (m_selected) {
        p.setPen(QPen(palette().color(QPalette::Highlight), 2));
        QColor highlightBrush = palette().color(QPalette::Highlight);
        highlightBrush.setAlphaF(0.3);
        p.setBrush(highlightBrush);
    } else {
        p.setPen(QPen(palette().color(QPalette::WindowText), 1));
        p.setBrush(palette().color(QPalette::Window));
    }
    p.drawRect(r); 
}

QPixmap SinkItemWidget::getIcon() const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    
    QPen pen(palette().color(QPalette::WindowText), 2);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    // Form Factors:
    // Speakers = 1
    // Headphones = 3
    // Headset = 5
    
    if (m_formFactor == 3 || m_formFactor == 5) { 
        // Headphones
        
        // Headband
        QPainterPath path;
        path.moveTo(14, 38);
        path.cubicTo(14, 10, 50, 10, 50, 38);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // Ear cups
        QBrush cupBrush(palette().color(QPalette::WindowText), Qt::DiagCrossPattern);
        p.setBrush(cupBrush); 
        
        // Left Cup
        p.save();
        p.translate(14, 38);
        p.rotate(-10);
        p.drawRoundedRect(QRect(-7, 0, 14, 24), 3, 3);
        p.restore();

        // Right Cup
        p.save();
        p.translate(50, 38);
        p.rotate(10);
        p.drawRoundedRect(QRect(-7, 0, 14, 24), 3, 3);
        p.restore();

    } else if (m_formFactor == 1) {
        // Speaker
        
        // Box
        p.setBrush(Qt::NoBrush);
        p.drawRect(16, 10, 32, 44);
        
        // Circle (Woofer)
        QBrush coneBrush(palette().color(QPalette::WindowText), Qt::DiagCrossPattern);
        p.setBrush(coneBrush);
        p.drawEllipse(19, 15, 26, 26);
        
        // Label/Port area at bottom
        p.setBrush(Qt::NoBrush);
        p.drawRect(22, 45, 20, 5);
        
    } else {
        // Generic Device
        p.setBrush(Qt::NoBrush);
        p.drawRect(10, 20, 44, 24);
        
        // Knobs/Buttons
        p.drawEllipse(15, 25, 6, 6);
        p.drawEllipse(43, 25, 6, 6);
    }
    
    return pixmap;
}