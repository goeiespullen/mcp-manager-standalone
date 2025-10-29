#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "UpdateChecker.h"

class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(const UpdateChecker::ReleaseInfo& info,
                         const UpdateChecker::Version& currentVersion,
                         QWidget* parent = nullptr);

private slots:
    void onDownloadClicked();

private:
    UpdateChecker::ReleaseInfo m_releaseInfo;

    QLabel* m_titleLabel;
    QLabel* m_currentVersionLabel;
    QLabel* m_latestVersionLabel;
    QLabel* m_publishedLabel;
    QTextEdit* m_releaseNotesText;
    QPushButton* m_downloadButton;
    QPushButton* m_closeButton;

    void setupUI();
};

#endif // UPDATEDIALOG_H
