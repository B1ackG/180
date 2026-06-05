#ifndef TECHPINYINKEYBOARD_H
#define TECHPINYINKEYBOARD_H

#include <QWidget>
#include <QMap>
#include <QVector>

class QGridLayout;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QPlainTextEdit;

class TechPinyinKeyboard : public QWidget
{
    Q_OBJECT
public:
    explicit TechPinyinKeyboard(QWidget *parent = nullptr);

    void setTargetTextEdit(QTextEdit *textEdit);
    void setTargetPlainTextEdit(QPlainTextEdit *plainTextEdit);
    void showAtWidget(QWidget *targetWidget);

private slots:
    void onLetterClicked();
    void onCandidateClicked();
    void onBackspaceClicked();
    void onClearClicked();
    void onSpaceClicked();
    void onConfirmClicked();

private:
    void setupUI();
    void setupConnections();
    void updatePreview();
    void updateCandidates();
    void commitCandidate(const QString &candidate);
    QStringList fallbackCandidatesForPinyin(const QString &pinyin) const;

    QString targetText() const;
    void setTargetText(const QString &text);

private:
    QTextEdit *m_targetTextEdit = nullptr;
    QPlainTextEdit *m_targetPlainTextEdit = nullptr;

    QLineEdit *m_textPreview = nullptr;
    QLineEdit *m_pinyinPreview = nullptr;
    QGridLayout *m_layout = nullptr;

    QVector<QPushButton*> m_candidateButtons;
    QVector<QPushButton*> m_letterButtons;

    QPushButton *m_buttonBackspace = nullptr;
    QPushButton *m_buttonClear = nullptr;
    QPushButton *m_buttonSpace = nullptr;
    QPushButton *m_buttonConfirm = nullptr;

    QString m_textBuffer;
    QString m_currentPinyin;
    QMap<QString, QStringList> m_dictionary;
};

#endif // TECHPINYINKEYBOARD_H
