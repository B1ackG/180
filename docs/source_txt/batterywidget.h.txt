#ifndef BATTERYWIDGET_H
#define BATTERYWIDGET_H

#include <QWidget>

class QTimer;

/**
 * @brief 电池电量 Widget，可用于 Qt Designer 提升。
 * 推荐放入一个布局管理器中，建议固定大小 60x30 或类似比例。
 */
class BatteryWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(bool charging READ isCharging WRITE setCharging NOTIFY chargingChanged)

public:
    explicit BatteryWidget(QWidget *parent = nullptr);

    double level() const { return m_level; }
    void setLevel(double val);

    bool isCharging() const { return m_charging; }
    void setCharging(bool val);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void levelChanged();
    void chargingChanged();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor batteryColor() const;

    double m_level = 100.0;
    bool m_charging = false;
    qreal m_pulseOpacity = 1.0;
    bool m_pulseDown = false;
    QTimer *m_pulseTimer = nullptr;
};

#endif // BATTERYWIDGET_H
