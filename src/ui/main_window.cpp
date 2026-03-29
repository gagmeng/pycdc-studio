#include "src/ui/main_window.h"

#include <QAction>
#include <QBrush>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "src/app/app_context.h"
#include "src/model/code_object_node.h"
#include "src/model/project_session.h"
#include "src/service/decompiler_service.h"
#include "src/service/fallback_service.h"
#include "src/ui/lucide_icon_factory.h"
#include "src/ui/settings_dialog.h"

namespace {
constexpr int kNodeIdRole = Qt::UserRole + 1;

QString joinOrDash(const QStringList &values)
{
    return values.isEmpty() ? QStringLiteral("-") : values.join(QStringLiteral(", "));
}

bool isSupportedBytecodeFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();
    return info.exists() && info.isFile() && (suffix == QStringLiteral("pyc") || suffix == QStringLiteral("pyo"));
}

bool isDroppedPathSupported(const QString &path)
{
    const QFileInfo info(path);
    return (info.exists() && info.isDir()) || isSupportedBytecodeFile(path);
}

QStringList collectBytecodeFilesFromPath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return {};
    }
    if (info.isFile()) {
        return isSupportedBytecodeFile(path) ? QStringList{info.absoluteFilePath()} : QStringList{};
    }
    if (!info.isDir()) {
        return {};
    }

    QStringList results;
    QDirIterator it(info.absoluteFilePath(),
                    QStringList{QStringLiteral("*.pyc"), QStringLiteral("*.pyo")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        results.append(QDir::cleanPath(it.next()));
    }
    return results;
}

QStringList collectDroppedBytecodeFiles(const QList<QUrl> &urls)
{
    QSet<QString> uniquePaths;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localPath = url.toLocalFile();
        const QStringList discovered = collectBytecodeFilesFromPath(localPath);
        for (const QString &filePath : discovered) {
            uniquePaths.insert(filePath);
        }
    }

    QStringList results = uniquePaths.values();
    results.sort(Qt::CaseInsensitive);
    return results;
}

bool nodeExistsRecursive(const CodeObjectNode &node, const QString &nodeId)
{
    if (node.id == nodeId) {
        return true;
    }
    for (const CodeObjectNode &child : node.children) {
        if (nodeExistsRecursive(child, nodeId)) {
            return true;
        }
    }
    return false;
}

const CodeObjectNode *findRootNodeForSelection(const QList<CodeObjectNode> &roots, const QString &nodeId)
{
    for (const CodeObjectNode &root : roots) {
        if (nodeExistsRecursive(root, nodeId)) {
            return &root;
        }
    }
    return nullptr;
}

void collectAiSectionsForRoot(const QList<CodeObjectNode> &nodes, QStringList &sections)
{
    for (const CodeObjectNode &node : nodes) {
        if (!node.aiSource.trimmed().isEmpty() && node.objectType != QStringLiteral("module")) {
            QString section;
            section += QStringLiteral("# [%1] %2\n").arg(node.objectType, node.qualifiedName);
            section += node.aiSource.trimmed();
            sections.append(section);
        }
        if (!node.children.isEmpty()) {
            collectAiSectionsForRoot(node.children, sections);
        }
    }
}

QString buildMergedTextForRoot(const CodeObjectNode &root)
{
    if (root.objectType == QStringLiteral("module") && !root.aiSource.trimmed().isEmpty()) {
        return root.aiSource.trimmed();
    }

    QStringList aiSections;
    collectAiSectionsForRoot(root.children, aiSections);
    if (aiSections.isEmpty()) {
        return root.nativeSource;
    }

    QString document = root.nativeSource.trimmed();
    if (document.isEmpty()) {
        document = QStringLiteral("# Native source is unavailable for this file.\n");
    }
    if (!document.endsWith(QLatin1Char('\n'))) {
        document.append(QLatin1Char('\n'));
    }
    document.append(QStringLiteral("\n# --- AI fallback patches ---\n"));
    document.append(QStringLiteral("# These snippets are reconstructed per code object and are not yet merged inline.\n\n"));
    document.append(aiSections.join(QStringLiteral("\n\n")));
    return document.trimmed();
}

LucideIconFactory::IconType nodeIconType(const QString &objectType)
{
    const QString lowered = objectType.toLower();
    if (lowered == QStringLiteral("module")) {
        return LucideIconFactory::IconType::ModuleNode;
    }
    if (lowered == QStringLiteral("class")) {
        return LucideIconFactory::IconType::ClassNode;
    }
    if (lowered == QStringLiteral("lambda")) {
        return LucideIconFactory::IconType::LambdaNode;
    }
    if (lowered == QStringLiteral("comprehension")) {
        return LucideIconFactory::IconType::ComprehensionNode;
    }
    return LucideIconFactory::IconType::FunctionNode;
}

QColor nodeAccentColor(const QString &objectType)
{
    const QString lowered = objectType.toLower();
    if (lowered == QStringLiteral("module")) {
        return QColor("#2d6cdf");
    }
    if (lowered == QStringLiteral("class")) {
        return QColor("#5b6cff");
    }
    if (lowered == QStringLiteral("lambda")) {
        return QColor("#b26b00");
    }
    if (lowered == QStringLiteral("comprehension")) {
        return QColor("#078c76");
    }
    return QColor("#1b7f6b");
}

LucideIconFactory::IconType statusIconType(CodeObjectNode::Status status)
{
    switch (status) {
    case CodeObjectNode::Status::NativeOk:
        return LucideIconFactory::IconType::StatusOk;
    case CodeObjectNode::Status::AiReconstructed:
        return LucideIconFactory::IconType::StatusAi;
    case CodeObjectNode::Status::NativeFailed:
    case CodeObjectNode::Status::Partial:
    case CodeObjectNode::Status::Unknown:
    default:
        return LucideIconFactory::IconType::StatusWarning;
    }
}

QColor statusColor(CodeObjectNode::Status status)
{
    switch (status) {
    case CodeObjectNode::Status::NativeOk:
        return QColor("#198754");
    case CodeObjectNode::Status::AiReconstructed:
        return QColor("#315efb");
    case CodeObjectNode::Status::Partial:
        return QColor("#9a6700");
    case CodeObjectNode::Status::NativeFailed:
        return QColor("#c25027");
    case CodeObjectNode::Status::Unknown:
    default:
        return QColor("#7a8898");
    }
}
}

MainWindow::MainWindow(AppContext *context, QWidget *parent)
    : QMainWindow(parent)
    , m_context(context)
{
    buildUi();
    buildMenus();
    connectSignals();
    refreshTree();
    refreshSourceViews();
    refreshInspectViews();
    refreshStatus();
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("pycdc-studio"));
    setWindowIcon(LucideIconFactory::appIcon());
    resize(1440, 900);
    setMinimumSize(1180, 760);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralPanel"));
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(14);

    auto *heroCard = new QFrame(central);
    heroCard->setObjectName(QStringLiteral("heroCard"));
    auto *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(22, 18, 22, 18);
    heroLayout->setSpacing(14);

    auto *heroTop = new QHBoxLayout();
    heroTop->setSpacing(14);

    auto *heroIconBadge = new QLabel(heroCard);
    heroIconBadge->setObjectName(QStringLiteral("heroIconBadge"));
    heroIconBadge->setPixmap(LucideIconFactory::pixmap(LucideIconFactory::IconType::App, 30, QColor("#ffffff")));
    heroIconBadge->setFixedSize(58, 58);
    heroIconBadge->setAlignment(Qt::AlignCenter);

    auto *heroTextLayout = new QVBoxLayout();
    heroTextLayout->setSpacing(6);

    auto *heroTitle = new QLabel(tr("pycdc-studio"), heroCard);
    heroTitle->setObjectName(QStringLiteral("heroTitle"));
    auto *heroSubtitle = new QLabel(tr("A desktop workspace for Python bytecode analysis, native decompilation, and AI-assisted fallback reconstruction."), heroCard);
    heroSubtitle->setObjectName(QStringLiteral("heroSubtitle"));
    heroSubtitle->setWordWrap(true);

    heroTextLayout->addWidget(heroTitle);
    heroTextLayout->addWidget(heroSubtitle);
    heroTop->addWidget(heroIconBadge, 0, Qt::AlignTop);
    heroTop->addLayout(heroTextLayout, 1);

    auto *heroActions = new QVBoxLayout();
    heroActions->setSpacing(8);
    heroActions->setAlignment(Qt::AlignTop);

    auto *retryButton = new QPushButton(LucideIconFactory::icon(LucideIconFactory::IconType::Sparkles, QColor("#ffffff"), 18),
                                        tr("Retry with AI"),
                                        heroCard);
    retryButton->setObjectName(QStringLiteral("primaryActionButton"));
    retryButton->setCursor(Qt::PointingHandCursor);
    retryButton->setMinimumHeight(42);
    retryButton->setToolTip(tr("Send the currently selected code object to AI fallback reconstruction"));

    auto *retryHint = new QLabel(tr("Works on the selected function, method, class body, or module node."), heroCard);
    retryHint->setObjectName(QStringLiteral("heroActionHint"));
    retryHint->setWordWrap(true);

    heroActions->addWidget(retryButton);
    heroActions->addWidget(retryHint);
    heroTop->addLayout(heroActions, 0);
    heroLayout->addLayout(heroTop);

    auto createInfoChip = [&](LucideIconFactory::IconType iconType,
                              const QColor &iconColor,
                              const QString &title,
                              const QString &caption) {
        auto *chip = new QFrame(heroCard);
        chip->setObjectName(QStringLiteral("infoChip"));
        auto *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(12, 10, 12, 10);
        chipLayout->setSpacing(10);

        auto *iconLabel = new QLabel(chip);
        iconLabel->setObjectName(QStringLiteral("infoChipIcon"));
        iconLabel->setPixmap(LucideIconFactory::pixmap(iconType, 18, iconColor));
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setFixedSize(34, 34);

        auto *textLayout = new QVBoxLayout();
        textLayout->setSpacing(1);

        auto *titleLabel = new QLabel(title, chip);
        titleLabel->setObjectName(QStringLiteral("infoChipTitle"));
        auto *captionLabel = new QLabel(caption, chip);
        captionLabel->setObjectName(QStringLiteral("infoChipCaption"));
        captionLabel->setWordWrap(true);

        textLayout->addWidget(titleLabel);
        textLayout->addWidget(captionLabel);
        chipLayout->addWidget(iconLabel, 0, Qt::AlignTop);
        chipLayout->addLayout(textLayout, 1);
        return chip;
    };

    auto *chipRow = new QHBoxLayout();
    chipRow->setSpacing(10);
    chipRow->addWidget(createInfoChip(LucideIconFactory::IconType::Open,
                                      QColor("#2d6cdf"),
                                      tr("Drop bytecode or a folder"),
                                      tr("Drag .pyc/.pyo files or a folder. Directories are scanned recursively for supported bytecode.")),
                       1);
    chipRow->addWidget(createInfoChip(LucideIconFactory::IconType::Native,
                                      QColor("#1b7f6b"),
                                      tr("Compare outputs"),
                                      tr("Switch between merged, native, and AI-reconstructed source with shared metadata.")),
                       1);
    chipRow->addWidget(createInfoChip(LucideIconFactory::IconType::Prompt,
                                      QColor("#9a6700"),
                                      tr("Inspect prompt context"),
                                      tr("Review metadata, disassembly, and the exact AI prompt used for reconstruction.")),
                       1);
    heroLayout->addLayout(chipRow);
    layout->addWidget(heroCard);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setObjectName(QStringLiteral("workspaceSplitter"));
    layout->addWidget(splitter, 1);

    m_treeWidget = new QTreeWidget(splitter);
    m_treeWidget->setObjectName(QStringLiteral("codeTree"));
    m_treeWidget->setColumnCount(3);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setUniformRowHeights(true);
    m_treeWidget->setIconSize(QSize(18, 18));
    m_treeWidget->setHeaderLabels({ tr("Name"), tr("Type"), tr("Status") });
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    m_sourceTabs = new QTabWidget(splitter);
    m_sourceTabs->setDocumentMode(true);
    m_sourceTabs->setIconSize(QSize(16, 16));
    m_sourceTabs->tabBar()->setDrawBase(false);
    m_mergedEdit = new QPlainTextEdit(m_sourceTabs);
    m_nativeEdit = new QPlainTextEdit(m_sourceTabs);
    m_aiEdit = new QPlainTextEdit(m_sourceTabs);
    for (QPlainTextEdit *edit : {m_mergedEdit, m_nativeEdit, m_aiEdit}) {
        edit->setReadOnly(true);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        edit->setFont(monoFont);
    }
    m_sourceTabs->addTab(m_mergedEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Merged, QColor("#245ba7")), tr("Merged"));
    m_sourceTabs->addTab(m_nativeEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Native, QColor("#1b7f6b")), tr("Native"));
    m_sourceTabs->addTab(m_aiEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Ai, QColor("#315efb")), tr("AI"));

    m_inspectTabs = new QTabWidget(splitter);
    m_inspectTabs->setDocumentMode(true);
    m_inspectTabs->setIconSize(QSize(16, 16));
    m_inspectTabs->tabBar()->setDrawBase(false);
    m_disassemblyEdit = new QPlainTextEdit(m_inspectTabs);
    m_metadataEdit = new QPlainTextEdit(m_inspectTabs);
    m_promptEdit = new QPlainTextEdit(m_inspectTabs);
    m_logEdit = new QPlainTextEdit(m_inspectTabs);
    for (QPlainTextEdit *edit : {m_disassemblyEdit, m_metadataEdit, m_promptEdit, m_logEdit}) {
        edit->setReadOnly(true);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        edit->setFont(monoFont);
    }
    m_inspectTabs->addTab(m_disassemblyEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Disassembly, QColor("#245ba7")), tr("Disassembly"));
    m_inspectTabs->addTab(m_metadataEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Metadata, QColor("#6a55d9")), tr("Metadata"));
    m_inspectTabs->addTab(m_promptEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Prompt, QColor("#9a6700")), tr("Prompt"));
    m_inspectTabs->addTab(m_logEdit, LucideIconFactory::icon(LucideIconFactory::IconType::Log, QColor("#6b7685")), tr("Log"));

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    splitter->setStretchFactor(2, 4);
    splitter->setSizes({300, 640, 580});

    setCentralWidget(central);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->showMessage(tr("Ready"));

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #edf3f9;
        }
        QWidget#centralPanel {
            background: transparent;
        }
        QFrame#heroCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #f9fbff,
                                        stop:1 #eef5ff);
            border: 1px solid #d7e2f0;
            border-radius: 20px;
        }
        QLabel#heroIconBadge {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #2f7cff,
                                        stop:1 #153a74);
            border: 1px solid rgba(255, 255, 255, 0.32);
            border-radius: 16px;
        }
        QLabel#heroTitle {
            color: #13253d;
            font-size: 28px;
            font-weight: 700;
        }
        QLabel#heroSubtitle {
            color: #5c6f87;
            font-size: 13px;
        }
        QLabel#heroActionHint {
            color: #667a92;
            font-size: 12px;
        }
        QPushButton#primaryActionButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #3a7dff,
                                        stop:1 #2454e6);
            color: #ffffff;
            border: none;
            border-radius: 14px;
            padding: 10px 18px;
            font-size: 13px;
            font-weight: 700;
            min-width: 180px;
        }
        QPushButton#primaryActionButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #4b89ff,
                                        stop:1 #315efb);
        }
        QPushButton#primaryActionButton:pressed {
            background: #214cd0;
        }
        QFrame#infoChip {
            background: rgba(255, 255, 255, 0.86);
            border: 1px solid #dbe6f3;
            border-radius: 16px;
        }
        QLabel#infoChipIcon {
            background: #f4f8ff;
            border: 1px solid #deebfb;
            border-radius: 10px;
        }
        QLabel#infoChipTitle {
            color: #18304a;
            font-size: 13px;
            font-weight: 650;
        }
        QLabel#infoChipCaption {
            color: #667a92;
            font-size: 12px;
        }
        QMenuBar {
            background: transparent;
            border: none;
            padding: 4px 8px;
        }
        QMenuBar::item {
            padding: 6px 10px;
            border-radius: 8px;
            color: #23364f;
        }
        QMenuBar::item:selected {
            background: #e6eef8;
        }
        QMenu {
            background: #ffffff;
            border: 1px solid #d7e2f0;
            padding: 6px;
            border-radius: 12px;
        }
        QMenu::item {
            padding: 7px 18px;
            border-radius: 8px;
        }
        QMenu::item:selected {
            background: #e6eef8;
        }
        QTreeWidget, QPlainTextEdit {
            background: #fbfdff;
            color: #17304a;
            border: 1px solid #d7e2f0;
            border-radius: 16px;
            selection-background-color: #d7e7ff;
            selection-color: #17304a;
            alternate-background-color: #f3f7fc;
        }
        QTreeWidget::item {
            padding: 6px 4px;
        }
        QTreeWidget::item:selected {
            background: #dfeafe;
            color: #13253d;
        }
        QHeaderView {
            background: transparent;
        }
        QHeaderView::section {
            background: transparent;
            color: #5a6d84;
            border: none;
            padding: 10px 10px 12px 10px;
            font-weight: 650;
        }
        QTabWidget::pane {
            border: 1px solid #d7e2f0;
            border-top: none;
            border-radius: 16px;
            background: #fbfdff;
            top: 6px;
        }
        QTabBar::tab {
            background: transparent;
            color: #647891;
            padding: 8px 14px;
            margin-right: 8px;
            margin-top: 6px;
            border: 1px solid transparent;
            border-radius: 999px;
            min-width: 92px;
        }
        QTabBar::tab:selected {
            background: #edf4ff;
            color: #18304a;
            border-color: #d8e7ff;
        }
        QTabBar::tab:hover:!selected {
            background: #f4f8ff;
            color: #23364f;
        }
        QSplitter::handle {
            background: transparent;
            width: 10px;
        }
        QStatusBar {
            background: #ffffff;
            color: #5a6d84;
            border-top: 1px solid #d7e2f0;
        }
    )"));

    connect(retryButton, &QPushButton::clicked, this, &MainWindow::retryCurrentNodeWithAi);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("File"));

    m_openAction = fileMenu->addAction(LucideIconFactory::icon(LucideIconFactory::IconType::Open, QColor("#2d6cdf")),
                                       tr("Open .pyc..."));
    m_openAction->setShortcut(QKeySequence::Open);

    m_settingsAction = fileMenu->addAction(LucideIconFactory::icon(LucideIconFactory::IconType::Settings, QColor("#5b6cff")),
                                           tr("Settings..."));
    m_settingsAction->setShortcut(QKeySequence::Preferences);

    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction(LucideIconFactory::icon(LucideIconFactory::IconType::Exit, QColor("#6b7685")),
                                       tr("Exit"));
    m_exitAction->setShortcut(QKeySequence::Quit);

    m_retryAiAction = new QAction(LucideIconFactory::icon(LucideIconFactory::IconType::Sparkles, QColor("#315efb")),
                                  tr("Retry with AI"),
                                  this);
    m_retryAiAction->setShortcut(QKeySequence(tr("Ctrl+R")));
    addAction(m_retryAiAction);
}

void MainWindow::connectSignals()
{
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openPycFile);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    connect(m_retryAiAction, &QAction::triggered, this, &MainWindow::retryCurrentNodeWithAi);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_treeWidget, &QTreeWidget::currentItemChanged, this, &MainWindow::updateNodeDetails);

    if (!m_context) {
        return;
    }

    ProjectSession &session = m_context->session();
    connect(&session, &ProjectSession::treeChanged, this, &MainWindow::refreshTree);
    connect(&session, &ProjectSession::sourcesChanged, this, &MainWindow::refreshSourceViews);
    connect(&session, &ProjectSession::inspectChanged, this, &MainWindow::refreshInspectViews);
    connect(&session, &ProjectSession::logChanged, this, &MainWindow::refreshInspectViews);
    connect(&session, &ProjectSession::statusMessageChanged, this, &MainWindow::refreshStatus);
    connect(&session, &ProjectSession::fileChanged, this, &MainWindow::refreshStatus);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event || !event->mimeData() || !event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile() && isDroppedPathSupported(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event || !event->mimeData() || !event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    const QStringList bytecodeFiles = collectDroppedBytecodeFiles(urls);
    if (bytecodeFiles.isEmpty()) {
        if (m_context) {
            m_context->session().setStatusMessage(tr("No .pyc or .pyo files were found in the dropped selection."));
            m_context->session().appendLogLine(tr("[drop] no supported bytecode files found"));
        }
        return;
    }

    const bool success = bytecodeFiles.size() == 1
        ? openPycFileFromPath(bytecodeFiles.first())
        : m_context && m_context->decompilerService().decompileFiles(bytecodeFiles);
    if (success) {
        event->acceptProposedAction();
        if (m_context) {
            if (bytecodeFiles.size() == 1) {
                m_context->session().setStatusMessage(tr("Opened dropped bytecode file: %1").arg(QFileInfo(bytecodeFiles.first()).fileName()));
            } else {
                m_context->session().setStatusMessage(tr("Loaded %1 bytecode files into the workspace.").arg(bytecodeFiles.size()));
                m_context->session().appendLogLine(tr("[drop] discovered %1 supported bytecode files").arg(bytecodeFiles.size()));
                const int previewCount = qMin(bytecodeFiles.size(), 12);
                for (int index = 0; index < previewCount; ++index) {
                    m_context->session().appendLogLine(tr("[drop] %1").arg(bytecodeFiles.at(index)));
                }
                if (bytecodeFiles.size() > previewCount) {
                    m_context->session().appendLogLine(tr("[drop] ... and %1 more").arg(bytecodeFiles.size() - previewCount));
                }
            }
        }
        return;
    }

    if (m_context) {
        m_context->session().setStatusMessage(tr("Unable to open the dropped bytecode selection."));
    }
}

void MainWindow::openPycFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          tr("Open Python Bytecode"),
                                                          QString(),
                                                          tr("Python Bytecode (*.pyc *.pyo);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    openPycFileFromPath(filePath);
}

bool MainWindow::openPycFileFromPath(const QString &filePath)
{
    if (!m_context) {
        return false;
    }
    if (!isSupportedBytecodeFile(filePath)) {
        return false;
    }

    return m_context->decompilerService().decompileFile(filePath);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (m_context) {
        m_context->aiClient().reloadFromEnvironment();
        m_context->session().setStatusMessage(tr("AI settings saved."));
        m_context->session().appendLogLine(tr("[settings] AI provider configuration updated"));
    }
}

void MainWindow::retryCurrentNodeWithAi()
{
    if (!m_context || !m_treeWidget->currentItem()) {
        return;
    }

    const QString nodeId = m_treeWidget->currentItem()->data(0, kNodeIdRole).toString();
    m_context->fallbackService().retryNodeWithAi(nodeId);
}

void MainWindow::refreshTree()
{
    const QString previousNodeId = m_treeWidget->currentItem()
        ? m_treeWidget->currentItem()->data(0, kNodeIdRole).toString()
        : QString();

    m_treeWidget->clear();
    if (!m_context) {
        return;
    }

    const QList<CodeObjectNode> tree = m_context->session().codeObjectTree();
    for (const CodeObjectNode &node : tree) {
        populateTreeItem(nullptr, node);
    }

    QTreeWidgetItem *targetItem = previousNodeId.isEmpty() ? nullptr : findTreeItemByNodeId(previousNodeId);
    if (!targetItem && m_treeWidget->topLevelItemCount() > 0) {
        targetItem = m_treeWidget->topLevelItem(0);
    }
    if (targetItem) {
        m_treeWidget->setCurrentItem(targetItem);
    }
    m_treeWidget->expandToDepth(1);
}

void MainWindow::populateTreeItem(QTreeWidgetItem *parentItem, const CodeObjectNode &node)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, node.displayName.isEmpty() ? node.qualifiedName : node.displayName);
    item->setText(1, node.objectType);
    item->setText(2, node.statusText());
    item->setData(0, kNodeIdRole, node.id);
    item->setToolTip(0, node.qualifiedName);
    item->setIcon(0, LucideIconFactory::icon(nodeIconType(node.objectType), nodeAccentColor(node.objectType), 18));
    item->setIcon(2, LucideIconFactory::icon(statusIconType(node.status), statusColor(node.status), 16));
    item->setForeground(1, QBrush(QColor("#6b7f95")));
    item->setForeground(2, QBrush(statusColor(node.status)));

    if (parentItem) {
        parentItem->addChild(item);
    } else {
        m_treeWidget->addTopLevelItem(item);
    }

    for (const CodeObjectNode &child : node.children) {
        populateTreeItem(item, child);
    }
}

QTreeWidgetItem *MainWindow::findTreeItemByNodeId(const QString &nodeId) const
{
    if (nodeId.isEmpty()) {
        return nullptr;
    }

    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        if ((*it)->data(0, kNodeIdRole).toString() == nodeId) {
            return *it;
        }
        ++it;
    }
    return nullptr;
}

void MainWindow::refreshSourceViews()
{
    if (!m_context) {
        return;
    }

    if (QTreeWidgetItem *current = m_treeWidget->currentItem()) {
        updateNodeDetails(current, nullptr);
        return;
    }

    const ProjectSession &session = m_context->session();
    m_mergedEdit->setPlainText(session.mergedSource());
    m_nativeEdit->setPlainText(session.nativeSource());
    m_aiEdit->clear();
}

void MainWindow::refreshInspectViews()
{
    if (!m_context) {
        return;
    }

    if (QTreeWidgetItem *current = m_treeWidget->currentItem()) {
        updateNodeDetails(current, nullptr);
        return;
    }

    const ProjectSession &session = m_context->session();
    m_disassemblyEdit->setPlainText(session.disassemblyText());
    m_promptEdit->setPlainText(session.promptPreviewText());
    m_logEdit->setPlainText(session.logText());
    m_metadataEdit->clear();
}

void MainWindow::refreshStatus()
{
    if (!m_context) {
        statusBar()->showMessage(tr("Ready"));
        return;
    }

    const ProjectSession &session = m_context->session();
    const QString fileName = session.openedFilePath().isEmpty()
        ? tr("No file")
        : QFileInfo(session.openedFilePath()).fileName();
    const QString message = session.statusMessage().isEmpty()
        ? tr("Ready")
        : QStringLiteral("%1 | %2").arg(fileName, session.statusMessage());
    statusBar()->showMessage(message);
}

void MainWindow::updateNodeDetails(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (!m_context || !current) {
        m_metadataEdit->clear();
        m_aiEdit->clear();
        return;
    }

    const QString nodeId = current->data(0, kNodeIdRole).toString();
    const ProjectSession &session = m_context->session();
    const CodeObjectNode *node = session.findNodeById(nodeId);
    if (!node) {
        m_metadataEdit->clear();
        m_aiEdit->clear();
        return;
    }

    const CodeObjectNode *rootNode = findRootNodeForSelection(session.codeObjectTree(), nodeId);
    if (rootNode) {
        m_mergedEdit->setPlainText(buildMergedTextForRoot(*rootNode));
        m_nativeEdit->setPlainText(rootNode->nativeSource);
        m_disassemblyEdit->setPlainText(node->disassembly.isEmpty()
                                            ? rootNode->disassembly
                                            : node->disassembly);
    } else {
        m_mergedEdit->setPlainText(session.mergedSource());
        m_nativeEdit->setPlainText(session.nativeSource());
        m_disassemblyEdit->setPlainText(node->disassembly.isEmpty() ? session.disassemblyText()
                                                                    : node->disassembly);
    }

    QString metadata;
    metadata += tr("Qualified Name: %1\n").arg(node->qualifiedName);
    metadata += tr("Type: %1\n").arg(node->objectType);
    metadata += tr("Status: %1\n").arg(node->statusText());
    metadata += tr("Source File: %1\n").arg(node->sourceFile);
    metadata += tr("First Line: %1\n").arg(node->firstLine >= 0 ? QString::number(node->firstLine) : QStringLiteral("-"));
    metadata += tr("co_names: %1\n").arg(joinOrDash(node->coNames));
    metadata += tr("co_varnames: %1\n").arg(joinOrDash(node->coVarNames));
    metadata += tr("co_freevars: %1\n").arg(joinOrDash(node->coFreeVars));
    metadata += tr("co_cellvars: %1\n").arg(joinOrDash(node->coCellVars));
    metadata += tr("co_consts: %1\n").arg(joinOrDash(node->coConstsPreview));
    if (!node->nativeError.trimmed().isEmpty()) {
        metadata += tr("\nNative Error:\n%1").arg(node->nativeError);
    }
    m_metadataEdit->setPlainText(metadata);
    m_promptEdit->setPlainText(session.promptPreviewText());
    m_logEdit->setPlainText(session.logText());
    m_aiEdit->setPlainText(node->aiSource);
}
