#include "UpdateDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>

UpdateDialog::UpdateDialog(const UpdateChecker::ReleaseInfo& info,
                          const UpdateChecker::Version& currentVersion,
                          QWidget* parent)
    : QDialog(parent)
    , m_releaseInfo(info)
{
    setWindowTitle("MCP Manager - Update Available");
    setMinimumWidth(500);
    setMinimumHeight(400);

    setupUI();

    // Set version labels
    m_currentVersionLabel->setText(QString("Current Version: <b>%1</b>").arg(currentVersion.string));
    m_latestVersionLabel->setText(QString("Latest Version: <b>%1</b>").arg(info.version.string));

    // Set published date
    if (info.publishedAt.isValid()) {
        QString dateStr = info.publishedAt.toString("yyyy-MM-dd HH:mm");
        m_publishedLabel->setText(QString("Published: %1").arg(dateStr));
    } else {
        m_publishedLabel->hide();
    }

    // Set release notes
    m_releaseNotesText->setPlainText(info.releaseNotes);
}

void UpdateDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Title
    m_titleLabel = new QLabel("A new version of MCP Manager is available!");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    mainLayout->addSpacing(10);

    // Version info group
    QGroupBox* versionGroup = new QGroupBox("Version Information");
    QVBoxLayout* versionLayout = new QVBoxLayout(versionGroup);

    m_currentVersionLabel = new QLabel();
    m_latestVersionLabel = new QLabel();
    m_publishedLabel = new QLabel();

    versionLayout->addWidget(m_currentVersionLabel);
    versionLayout->addWidget(m_latestVersionLabel);
    versionLayout->addWidget(m_publishedLabel);

    mainLayout->addWidget(versionGroup);

    // Release notes
    QGroupBox* notesGroup = new QGroupBox("Release Notes");
    QVBoxLayout* notesLayout = new QVBoxLayout(notesGroup);

    m_releaseNotesText = new QTextEdit();
    m_releaseNotesText->setReadOnly(true);
    m_releaseNotesText->setMinimumHeight(150);

    notesLayout->addWidget(m_releaseNotesText);

    mainLayout->addWidget(notesGroup);

    // Installation instructions
    QLabel* instructionsLabel = new QLabel(
        "<b>To update:</b><br>"
        "1. Click 'View Release' to open the GitHub release page<br>"
        "2. Download the source code (zip or tar.gz)<br>"
        "3. Extract and rebuild using ./build.sh (Linux/macOS) or build.bat (Windows)<br>"
        "4. Or check the release page for pre-built binaries (if available)"
    );
    instructionsLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; border-radius: 5px; }");
    instructionsLabel->setWordWrap(true);
    mainLayout->addWidget(instructionsLabel);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_downloadButton = new QPushButton("View Release on GitHub");
    m_downloadButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px 16px; }");
    connect(m_downloadButton, &QPushButton::clicked, this, &UpdateDialog::onDownloadClicked);

    m_closeButton = new QPushButton("Close");
    m_closeButton->setStyleSheet("QPushButton { padding: 8px 16px; }");
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);
}

void UpdateDialog::onDownloadClicked()
{
    // Open the release page which has both download links and instructions
    QDesktopServices::openUrl(QUrl(m_releaseInfo.url));

    accept();
}
