#ifndef TECHCHAMFERTOOLBUTTON_H
#define TECHCHAMFERTOOLBUTTON_H

#include <QColor>
#include <QToolButton>

class TechChamferToolButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor checkedFillColor READ checkedFillColor WRITE setCheckedFillColor NOTIFY checkedFillColorChanged)
    Q_PROPERTY(QColor checkedBorderColor READ checkedBorderColor WRITE setCheckedBorderColor NOTIFY checkedBorderColorChanged)
    Q_PROPERTY(int chamferSize READ chamferSize WRITE setChamferSize NOTIFY chamferSizeChanged)

public:
    explicit TechChamferToolButton(QWidget *parent = nullptr);

    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor &color);

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &color);

    QColor checkedFillColor() const { return m_checkedFillColor; }
    void setCheckedFillColor(const QColor &color);

    QColor checkedBorderColor() const { return m_checkedBorderColor; }
    void setCheckedBorderColor(const QColor &color);

    int chamferSize() const { return m_chamferSize; }
    void setChamferSize(int size);

    void setColors(const QColor &fill, const QColor &border);

signals:
    void fillColorChanged();
    void borderColorChanged();
    void checkedFillColorChanged();
    void checkedBorderColorChanged();
    void chamferSizeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    qreal effectiveSlant() const;
    qreal effectiveCornerCut() const;
    QColor effectiveFillColor() const;
    QColor effectiveBorderColor() const;
    QColor effectiveTextColor() const;
    void drawContent(QPainter &painter, const QRect &rect) const;

    QColor m_fillColor = QColor(8, 18, 32, 173);
    QColor m_borderColor = QColor(0, 220, 255, 180);
    QColor m_checkedFillColor = QColor(0, 130, 200, 224);
    QColor m_checkedBorderColor = QColor(120, 240, 255, 255);
    int m_chamferSize = 0;
};

#endif // TECHCHAMFERTOOLBUTTON_H
