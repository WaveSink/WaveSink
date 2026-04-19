#pragma once

#include <QWidget>
#include <QString>

class QLabel;

class SinkItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SinkItemWidget(const QString &id, const QString &name, uint formFactor, QWidget *parent = nullptr);

    QString id() const { return m_id; }

    bool isSelected() const { return m_selected; }
    void setSelected(bool selected);

signals:
    void clicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_id;
    QString m_name;
    uint m_formFactor;
    bool m_selected = false;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    
    void setupUi();
    QPixmap getIcon() const;
};