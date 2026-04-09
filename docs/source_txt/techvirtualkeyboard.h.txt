#ifndef TECHVIRTUALKEYBOARD_H
/**
 * @file techvirtualkeyboard.h
 * @brief 虚拟键盘控件声明，提供屏幕触控输入的键盘界面。
 *
 * 详细说明: 支持字母/数字输入、特殊键处理与事件回调，适用于触摸屏场景。
 *
 * 使用示例:
 * @code
 * #include "techvirtualkeyboard.h"
 * TechVirtualKeyboard *kb = new TechVirtualKeyboard(parent);
 * connect(kb, &TechVirtualKeyboard::keyPressed, [](QChar c){ qDebug() << c; });
 * @endcode
 */
#define TECHVIRTUALKEYBOARD_H

#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QLineEdit>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

class TechVirtualKeyboard : public QWidget
{
    Q_OBJECT
public:
    /**
     * 功能: 构造虚拟键盘控件，初始化 UI 和样式。
     * 如何使用: 在需要弹出虚拟键盘的界面创建此控件并使用 setTargetLineEdit 绑定目标输入控件。
     * 如何修改: 若需支持更多按键或布局，可在 setupUI 中扩展按钮表与布局。
     */
    explicit TechVirtualKeyboard(QWidget *parent = nullptr);

    /**
     * 功能: 析构函数，释放动画与定时器等资源。
     * 如何使用: 由 Qt 自动销毁或手动 delete。
     * 如何修改: 若增加外部资源，请在析构中一并清理。
     */
    ~TechVirtualKeyboard();

    /**
     * 使用示例:
     * @code
     * auto *kbd = new TechVirtualKeyboard(parent);
     * kbd->setTargetLineEdit(myLineEdit);
     * kbd->showAtWidget(myLineEdit);
     * connect(kbd, &TechVirtualKeyboard::inputConfirmed, [](const QString &t){ qDebug() << t; });
     * @endcode
     */

    // 设置目标LineEdit
    /**
     * 功能: 将虚拟键盘绑定到目标 QLineEdit，以便在确认时把文本写回目标控件。
     * 如何使用: 在弹出键盘前调用 setTargetLineEdit(targetLineEdit)。
     * 如何修改: 若需支持 QTextEdit 或自定义输入控件，改为接受通用接口并实现相应写回逻辑。
     */
    void setTargetLineEdit(QLineEdit *lineEdit);

    // 显示键盘并定位到目标LineEdit下方
    /**
     * 功能: 在指定的目标控件下方显示并定位虚拟键盘（自动处理屏幕边界）。
     * 如何使用: 调用 showAtWidget(targetWidget) 来弹出键盘并定位。
     * 如何修改: 若需要自定义定位策略，可在实现中替换定位算法或提供偏移参数。
     */
    void showAtWidget(QWidget *targetWidget);

    // 设置初始文本（用于编辑现有内容）
    /**
     * 功能: 设置键盘初始显示的文本（例如用于编辑已有内容）。
     * 如何使用: 在 showAtWidget 之前或之后调用以预填充文本。
     * 如何修改: 若需支持光标位置或选中文本，请扩展为设置光标位置的接口。
     */
    void setInitialText(const QString &text);

    /**
     * @brief 设置目标控件并显示键盘
     *
     * 使用示例:
     * @code
     * TechVirtualKeyboard *kbd = new TechVirtualKeyboard(parent);
     * kbd->setTargetLineEdit(myLineEdit);
     * kbd->showAtWidget(myLineEdit);
     * @endcode
     */

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onButtonClicked();
    void onEnterClicked();
    void onDeleteClicked();
    void onNegateClicked();
    void onClearClicked();
    void onBackspaceClicked();
    void updateAnimation();  // 重命名并作为统一接口

private:
    void setupUI();
    void setupConnections();
    void applyTechStyle();
    void updatePreview();
    void applyButtonPressEffect(QPushButton *button);

private:
    QLineEdit *m_targetLineEdit;
    QLineEdit *m_inputPreview;  // 输入预览框
    QGridLayout *m_mainLayout;
    QVBoxLayout *m_outerLayout;

    // 按钮
    QPushButton *m_buttons[10]; // 数字0-9
    QPushButton *m_buttonDecimal;   // 小数点
    QPushButton *m_buttonEnter;     // 确认/回车
    QPushButton *m_buttonDelete;    // 删除
    QPushButton *m_buttonClear;     // 清除所有
    QPushButton *m_buttonBackspace; // 退格
    QPushButton *m_buttonNegate;    // 负号

    // 科技感样式
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_glowColor;
    QGraphicsDropShadowEffect *m_shadowEffect;

    // 动画效果
    bool m_glowEnabled;
    QTimer *m_glowTimer;
    qreal m_glowIntensity;
    bool m_glowDirection;

    // 当前输入的文本
    QString m_currentText;
    bool m_isEditing;  // 是否正在编辑

signals:
    /**
     * @brief 输入确认信号
     * @param text 最终输入文本
     *
     * 使用示例:
     * @code
     * connect(kbd, &TechVirtualKeyboard::inputConfirmed, [](const QString &t){ qDebug() << t; });
     * @endcode
     */
    void inputConfirmed(const QString &text);

    /**
     * @brief 输入取消信号（用户取消操作或关闭键盘）
     */
    void inputCancelled();
};

#endif // TECHVIRTUALKEYBOARD_H
