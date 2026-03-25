#ifndef TECHARCGAUGE_H
#define TECHARCGAUGE_H

#include <QWidget>
#include <QColor>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>
#include <QtMath>
#include "animationmanager.h"

class TechArcGauge : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)

public:
    explicit TechArcGauge(QWidget *parent = nullptr);
    ~TechArcGauge();

    double value() const { return m_value; }
    double minimum() const { return m_minimum; }
    double maximum() const { return m_maximum; }
    
    double secondValue() const { return m_secondValue; }
    double secondMaximum() const { return m_secondMaximum; }
    
    void setRange(double min, double max);
    void setLabelText(const QString &text);
    void setSuffix(const QString &suffix);
    void setSecondSuffix(const QString &suffix);
    void setPrecision(int precision);
    void setForceControlMode(bool enabled);
    void setModbusAddress(int address) { m_modbusAddress = address; }
    int modbusAddress() const { return m_modbusAddress; }

public slots:
    void setValue(double value);
    void setSecondValue(double value);
    void setSecondMaximum(double max);
    void setMinimum(double min);
    void setMaximum(double max);
    void updateAnimation();
    void updateFromModbus(double value);

signals:
    void valueChanged(double value);
    void secondValueChanged(double value);
    void modbusAddressChanged(int address);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void requestRepaint();
    QColor interpolate(const QColor& start, const QColor& end, double t);

    double m_value;
    double m_minimum;
    double m_maximum;
    
    double m_secondValue;    // 新增：第二数值（如速度）
    double m_secondMaximum;  // 新增：第二数值最大值
    
    int m_precision;
    QString m_suffix;
    QString m_secondSuffix;  // 新增：第二数值单位
    QString m_labelText;
    int m_modbusAddress;

    // 颜色配置
    QColor m_primaryColor;
    QColor m_originalPrimaryColor;
    QColor m_glowColor;
    QColor m_originalGlowColor;

    // 动画相关
    bool m_forceControlEnabled;
    float m_scanLinePhase;
    
    // 重绘控制
    QElapsedTimer m_repaintElapsed;
    bool m_repaintPending;
    const int m_minRepaintIntervalMs = 33; // 约30fps
};

#endif // TECHARCGAUGE_H
