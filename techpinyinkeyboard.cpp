#include "techpinyinkeyboard.h"

#include <QApplication>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTextEdit>
#include <QVBoxLayout>

TechPinyinKeyboard::TechPinyinKeyboard(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(620, 420);

    m_dictionary.insert("ni", {"你", "呢", "泥", "拟", "妮", "逆"});
    m_dictionary.insert("hao", {"好", "号", "浩", "豪", "毫", "昊"});
    m_dictionary.insert("ming", {"名", "明", "鸣", "铭", "命", "冥"});
    m_dictionary.insert("cheng", {"称", "成", "城", "程", "承", "诚"});
    m_dictionary.insert("kong", {"控", "空", "孔", "恐", "倥", "崆"});
    m_dictionary.insert("jian", {"件", "键", "建", "间", "见", "坚"});
    m_dictionary.insert("she", {"设", "射", "涉", "舍", "社", "蛇"});
    m_dictionary.insert("bei", {"备", "北", "被", "倍", "背", "贝"});
    m_dictionary.insert("xi", {"系", "西", "希", "席", "细", "习"});
    m_dictionary.insert("tong", {"统", "通", "同", "童", "铜", "桐"});

    setupUI();
    setupConnections();
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::setTargetTextEdit(QTextEdit *textEdit)
{
    m_targetTextEdit = textEdit;
    m_targetPlainTextEdit = nullptr;
    m_textBuffer = targetText();
    m_currentPinyin.clear();
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::setTargetPlainTextEdit(QPlainTextEdit *plainTextEdit)
{
    m_targetPlainTextEdit = plainTextEdit;
    m_targetTextEdit = nullptr;
    m_textBuffer = targetText();
    m_currentPinyin.clear();
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::showAtWidget(QWidget *targetWidget)
{
    if (!targetWidget) {
        return;
    }

    QPoint globalPos = targetWidget->mapToGlobal(QPoint(0, targetWidget->height() + 5));
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect screenRect = screen->availableGeometry();

    if (globalPos.x() + width() > screenRect.right()) {
        globalPos.setX(screenRect.right() - width());
    }
    if (globalPos.y() + height() > screenRect.bottom()) {
        globalPos.setY(targetWidget->mapToGlobal(QPoint(0, 0)).y() - height() - 5);
    }
    if (globalPos.x() < screenRect.left()) {
        globalPos.setX(screenRect.left());
    }

    move(globalPos);
    show();
    raise();
    activateWindow();
}

void TechPinyinKeyboard::onLetterClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    m_currentPinyin += button->text().toLower();
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::onCandidateClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }
    const QString candidate = button->text().trimmed();
    if (!candidate.isEmpty()) {
        commitCandidate(candidate);
    }
}

void TechPinyinKeyboard::onBackspaceClicked()
{
    if (!m_currentPinyin.isEmpty()) {
        m_currentPinyin.chop(1);
    } else if (!m_textBuffer.isEmpty()) {
        m_textBuffer.chop(1);
    }
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::onClearClicked()
{
    m_currentPinyin.clear();
    m_textBuffer.clear();
    updateCandidates();
    updatePreview();
}

void TechPinyinKeyboard::onSpaceClicked()
{
    if (!m_currentPinyin.isEmpty()) {
        const QStringList candidates = m_dictionary.value(m_currentPinyin.toLower());
        if (!candidates.isEmpty()) {
            commitCandidate(candidates.first());
        } else {
            m_textBuffer += m_currentPinyin;
            m_currentPinyin.clear();
            updateCandidates();
            updatePreview();
        }
        return;
    }

    m_textBuffer += " ";
    updatePreview();
}

void TechPinyinKeyboard::onConfirmClicked()
{
    if (!m_currentPinyin.isEmpty()) {
        const QStringList candidates = m_dictionary.value(m_currentPinyin.toLower());
        if (!candidates.isEmpty()) {
            m_textBuffer += candidates.first();
        } else {
            m_textBuffer += m_currentPinyin;
        }
        m_currentPinyin.clear();
    }

    setTargetText(m_textBuffer);
    hide();
}

void TechPinyinKeyboard::setupUI()
{
    setStyleSheet(
        "QWidget { background-color: #1f2a38; color: white; }"
        "QLineEdit { background-color: #101820; color: #d8f0ff; border: 1px solid #3d6a8f; border-radius: 4px; padding: 4px 6px; }"
        "QPushButton { background-color: #2c4661; border: 1px solid #4f7aa0; border-radius: 4px; color: white; min-height: 36px; }"
        "QPushButton:hover { background-color: #3c5f82; }"
        "QPushButton:pressed { background-color: #1f3650; }"
    );

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    m_textPreview = new QLineEdit(this);
    m_textPreview->setReadOnly(true);
    m_textPreview->setPlaceholderText("文本预览");
    outer->addWidget(m_textPreview);

    m_pinyinPreview = new QLineEdit(this);
    m_pinyinPreview->setReadOnly(true);
    m_pinyinPreview->setPlaceholderText("拼音缓冲");
    outer->addWidget(m_pinyinPreview);

    auto *candidateLayout = new QHBoxLayout();
    candidateLayout->setSpacing(6);
    for (int i = 0; i < 6; ++i) {
        auto *btn = new QPushButton(this);
        btn->setText(QString::number(i + 1));
        btn->setMinimumWidth(85);
        m_candidateButtons.append(btn);
        candidateLayout->addWidget(btn);
    }
    outer->addLayout(candidateLayout);

    m_layout = new QGridLayout();
    m_layout->setHorizontalSpacing(6);
    m_layout->setVerticalSpacing(6);
    outer->addLayout(m_layout);

    const QString row1 = "qwertyuiop";
    const QString row2 = "asdfghjkl";
    const QString row3 = "zxcvbnm";

    int col = 0;
    for (const QChar &c : row1) {
        auto *btn = new QPushButton(QString(c), this);
        m_letterButtons.append(btn);
        m_layout->addWidget(btn, 0, col++);
    }

    col = 0;
    for (const QChar &c : row2) {
        auto *btn = new QPushButton(QString(c), this);
        m_letterButtons.append(btn);
        m_layout->addWidget(btn, 1, col++);
    }

    col = 0;
    for (const QChar &c : row3) {
        auto *btn = new QPushButton(QString(c), this);
        m_letterButtons.append(btn);
        m_layout->addWidget(btn, 2, col++);
    }

    m_buttonBackspace = new QPushButton("退格", this);
    m_buttonClear = new QPushButton("清空", this);
    m_buttonSpace = new QPushButton("空格/上屏", this);
    m_buttonConfirm = new QPushButton("确定", this);

    m_layout->addWidget(m_buttonBackspace, 3, 0, 1, 2);
    m_layout->addWidget(m_buttonClear, 3, 2, 1, 2);
    m_layout->addWidget(m_buttonSpace, 3, 4, 1, 3);
    m_layout->addWidget(m_buttonConfirm, 3, 7, 1, 3);
}

void TechPinyinKeyboard::setupConnections()
{
    for (QPushButton *btn : qAsConst(m_letterButtons)) {
        connect(btn, &QPushButton::clicked, this, &TechPinyinKeyboard::onLetterClicked);
    }
    for (QPushButton *btn : qAsConst(m_candidateButtons)) {
        connect(btn, &QPushButton::clicked, this, &TechPinyinKeyboard::onCandidateClicked);
    }

    connect(m_buttonBackspace, &QPushButton::clicked, this, &TechPinyinKeyboard::onBackspaceClicked);
    connect(m_buttonClear, &QPushButton::clicked, this, &TechPinyinKeyboard::onClearClicked);
    connect(m_buttonSpace, &QPushButton::clicked, this, &TechPinyinKeyboard::onSpaceClicked);
    connect(m_buttonConfirm, &QPushButton::clicked, this, &TechPinyinKeyboard::onConfirmClicked);
}

void TechPinyinKeyboard::updatePreview()
{
    if (m_textPreview) {
        m_textPreview->setText(m_textBuffer);
    }
    if (m_pinyinPreview) {
        m_pinyinPreview->setText(m_currentPinyin);
    }
}

void TechPinyinKeyboard::updateCandidates()
{
    QStringList candidates;
    if (!m_currentPinyin.isEmpty()) {
        candidates = m_dictionary.value(m_currentPinyin.toLower());
        if (candidates.isEmpty()) {
            candidates = fallbackCandidatesForPinyin(m_currentPinyin);
        }
    }

    for (int i = 0; i < m_candidateButtons.size(); ++i) {
        const bool valid = i < candidates.size();
        m_candidateButtons[i]->setEnabled(valid);
        m_candidateButtons[i]->setText(valid ? candidates[i] : QString::number(i + 1));
    }
}

void TechPinyinKeyboard::commitCandidate(const QString &candidate)
{
    m_textBuffer += candidate;
    m_currentPinyin.clear();
    updateCandidates();
    updatePreview();
}

QStringList TechPinyinKeyboard::fallbackCandidatesForPinyin(const QString &pinyin) const
{
    if (pinyin.isEmpty()) {
        return QStringList();
    }
    return QStringList{pinyin.left(1).toUpper() + pinyin.mid(1)};
}

QString TechPinyinKeyboard::targetText() const
{
    if (m_targetTextEdit) {
        return m_targetTextEdit->toPlainText();
    }
    if (m_targetPlainTextEdit) {
        return m_targetPlainTextEdit->toPlainText();
    }
    return QString();
}

void TechPinyinKeyboard::setTargetText(const QString &text)
{
    if (m_targetTextEdit) {
        m_targetTextEdit->setPlainText(text);
    } else if (m_targetPlainTextEdit) {
        m_targetPlainTextEdit->setPlainText(text);
    }
}
