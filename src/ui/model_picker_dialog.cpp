#include "src/ui/model_picker_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "src/ui/lucide_icon_factory.h"

// ── helpers ──────────────────────────────────────────────────────────────────

namespace {

QString normalizeBase(QString url)
{
    url = url.trimmed();
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    // 去掉尾部的 /chat/completions 或 /responses，保留到 /v1 级别
    for (const QString &suffix : { QStringLiteral("/chat/completions"),
                                   QStringLiteral("/responses") }) {
        if (url.endsWith(suffix)) {
            url.chop(suffix.length());
            break;
        }
    }
    return url;
}

} // namespace

// ── ModelPickerDialog ─────────────────────────────────────────────────────────

ModelPickerDialog::ModelPickerDialog(const QString &baseUrl,
                                     const QString &apiKey,
                                     const QStringList &existingModels,
                                     const QString &currentModel,
                                     QWidget *parent)
    : QDialog(parent)
    , m_baseUrl(baseUrl)
    , m_apiKey(apiKey)
{
    setObjectName(QStringLiteral("modelPickerDialog"));
    setWindowTitle(tr("Select Models"));
    setWindowIcon(LucideIconFactory::icon(LucideIconFactory::IconType::Ai, QColor("#315efb"), 20));
    resize(560, 480);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 14);
    mainLayout->setSpacing(12);

    // ── 标题卡片 ──────────────────────────────────────────────────────────────
    auto *headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("headerCard"));
    auto *headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(16, 14, 16, 14);
    headerLayout->setSpacing(10);

    auto *iconLabel = new QLabel(headerCard);
    iconLabel->setObjectName(QStringLiteral("dialogIconBadge"));
    iconLabel->setPixmap(LucideIconFactory::pixmap(LucideIconFactory::IconType::Ai, 22, QColor("#ffffff")));
    iconLabel->setFixedSize(42, 42);
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto *titleLbl = new QLabel(tr("Model Manager"), headerCard);
    titleLbl->setObjectName(QStringLiteral("dialogTitle"));
    auto *hintLbl = new QLabel(tr("Add models manually or fetch from the provider. Check/uncheck to enable. Select one as the active model."), headerCard);
    hintLbl->setObjectName(QStringLiteral("dialogHint"));
    hintLbl->setWordWrap(true);
    titleCol->addWidget(titleLbl);
    titleCol->addWidget(hintLbl);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleCol, 1);
    mainLayout->addWidget(headerCard);

    // ── 手动添加行 ────────────────────────────────────────────────────────────
    auto *addRow = new QHBoxLayout();
    addRow->setSpacing(8);
    m_manualEdit = new QLineEdit(this);
    m_manualEdit->setPlaceholderText(tr("Enter model name, e.g. gpt-4o-mini"));
    auto *addButton = new QPushButton(tr("Add"), this);
    addButton->setObjectName(QStringLiteral("dialogSecondaryButton"));
    addButton->setIcon(LucideIconFactory::icon(LucideIconFactory::IconType::StatusOk, QColor("#198754"), 16));
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setFixedWidth(80);
    addRow->addWidget(m_manualEdit, 1);
    addRow->addWidget(addButton);
    mainLayout->addLayout(addRow);

    // ── 模型列表 ──────────────────────────────────────────────────────────────
    m_modelList = new QListWidget(this);
    m_modelList->setObjectName(QStringLiteral("codeTree"));
    m_modelList->setAlternatingRowColors(true);
    m_modelList->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_modelList, 1);

    // ── 操作按钮行 ────────────────────────────────────────────────────────────
    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);

    m_fetchButton = new QPushButton(
        LucideIconFactory::icon(LucideIconFactory::IconType::Open, QColor("#2d6cdf"), 16),
        tr("Fetch Models"),
        this);
    m_fetchButton->setObjectName(QStringLiteral("secondaryActionButton"));
    m_fetchButton->setCursor(Qt::PointingHandCursor);
    m_fetchButton->setToolTip(tr("Fetch available models from the provider's /models endpoint"));

    m_testButton = new QPushButton(
        LucideIconFactory::icon(LucideIconFactory::IconType::Sparkles, QColor("#315efb"), 16),
        tr("Test Selected"),
        this);
    m_testButton->setObjectName(QStringLiteral("secondaryActionButton"));
    m_testButton->setCursor(Qt::PointingHandCursor);
    m_testButton->setEnabled(false);
    m_testButton->setToolTip(tr("Send a test request with the selected model and show status/latency"));

    m_removeButton = new QPushButton(
        LucideIconFactory::icon(LucideIconFactory::IconType::Exit, QColor("#c25027"), 16),
        tr("Remove"),
        this);
    m_removeButton->setObjectName(QStringLiteral("dialogSecondaryButton"));
    m_removeButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setEnabled(false);

    actionRow->addWidget(m_fetchButton);
    actionRow->addWidget(m_testButton);
    actionRow->addWidget(m_removeButton);
    actionRow->addStretch();
    mainLayout->addLayout(actionRow);

    // ── 状态标签 ──────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("dialogHint"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setText(tr("No test run yet."));
    mainLayout->addWidget(m_statusLabel);

    // ── 底部按钮 ──────────────────────────────────────────────────────────────
    auto *btnBox = new QDialogButtonBox(this);
    m_selectButton = btnBox->addButton(tr("Use Selected"), QDialogButtonBox::AcceptRole);
    m_selectButton->setObjectName(QStringLiteral("dialogPrimaryButton"));
    m_selectButton->setEnabled(false);
    auto *cancelButton = btnBox->addButton(QDialogButtonBox::Cancel);
    cancelButton->setObjectName(QStringLiteral("dialogSecondaryButton"));
    cancelButton->setText(tr("Cancel"));
    mainLayout->addWidget(btnBox);

    // ── 初始填充 ──────────────────────────────────────────────────────────────
    populateList(existingModels, currentModel);

    // ── 信号连接 ──────────────────────────────────────────────────────────────
    connect(addButton,       &QPushButton::clicked, this, &ModelPickerDialog::addManualModel);
    connect(m_manualEdit,    &QLineEdit::returnPressed, this, &ModelPickerDialog::addManualModel);
    connect(m_fetchButton,   &QPushButton::clicked, this, &ModelPickerDialog::fetchModels);
    connect(m_testButton,    &QPushButton::clicked, this, &ModelPickerDialog::testSelectedModel);
    connect(m_removeButton,  &QPushButton::clicked, this, &ModelPickerDialog::removeSelectedModel);
    connect(m_modelList,     &QListWidget::itemSelectionChanged, this, &ModelPickerDialog::onItemSelectionChanged);
    connect(m_selectButton,  &QPushButton::clicked, this, &ModelPickerDialog::acceptSelection);
    connect(cancelButton,    &QPushButton::clicked, this, &QDialog::reject);
}

// ── public ───────────────────────────────────────────────────────────────────

QString ModelPickerDialog::selectedModel() const
{
    const QList<QListWidgetItem *> selected = m_modelList->selectedItems();
    if (selected.isEmpty()) {
        return QString();
    }
    return selected.first()->text();
}

QStringList ModelPickerDialog::models() const
{
    QStringList result;
    for (int i = 0; i < m_modelList->count(); ++i) {
        result.append(m_modelList->item(i)->text());
    }
    return result;
}

// ── private slots ─────────────────────────────────────────────────────────────

void ModelPickerDialog::addManualModel()
{
    const QString name = m_manualEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    // 去重检查
    for (int i = 0; i < m_modelList->count(); ++i) {
        if (m_modelList->item(i)->text() == name) {
            m_modelList->setCurrentRow(i);
            m_manualEdit->clear();
            return;
        }
    }
    auto *item = new QListWidgetItem(name);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
    m_modelList->addItem(item);
    m_modelList->setCurrentItem(item);
    m_manualEdit->clear();
}

void ModelPickerDialog::fetchModels()
{
    const QString base = normalizedEndpointBase();
    if (base.isEmpty() || m_apiKey.trimmed().isEmpty()) {
        m_statusLabel->setText(tr("Base URL or API Key is missing; cannot fetch models."));
        return;
    }

    m_fetchButton->setEnabled(false);
    m_statusLabel->setText(tr("Fetching models..."));

    const QUrl url(base + QStringLiteral("/models"));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkAccessManager manager;
    QPointer<QNetworkReply> reply = manager.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer,  &QTimer::timeout,          &loop, &QEventLoop::quit);
    QObject::connect(reply,   &QNetworkReply::finished,  &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    m_fetchButton->setEnabled(true);

    if (!reply) {
        m_statusLabel->setText(tr("Request was canceled."));
        return;
    }
    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        m_statusLabel->setText(tr("Fetch timed out."));
        return;
    }
    timer.stop();

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        m_statusLabel->setText(tr("Failed to parse response from /models."));
        return;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        const QString msg = root.value(QStringLiteral("error")).toObject()
                                .value(QStringLiteral("message")).toString(tr("Unknown error"));
        m_statusLabel->setText(tr("API error: %1").arg(msg));
        return;
    }

    const QJsonArray dataArr = root.value(QStringLiteral("data")).toArray();
    QStringList fetched;
    for (const QJsonValue &v : dataArr) {
        const QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            fetched.append(id);
        }
    }
    fetched.sort(Qt::CaseInsensitive);

    if (fetched.isEmpty()) {
        m_statusLabel->setText(tr("No models returned by the provider."));
        return;
    }

    // 合并到现有列表（已有的保留，新的追加）
    QStringList existing = models();
    QStringList toAdd;
    for (const QString &id : std::as_const(fetched)) {
        if (!existing.contains(id)) {
            toAdd.append(id);
        }
    }
    for (const QString &id : std::as_const(toAdd)) {
        auto *item = new QListWidgetItem(id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        m_modelList->addItem(item);
    }
    m_statusLabel->setText(tr("Fetched %1 model(s); %2 new added.").arg(fetched.size()).arg(toAdd.size()));
}

void ModelPickerDialog::testSelectedModel()
{
    const QString modelName = selectedModel();
    if (modelName.isEmpty()) {
        return;
    }
    const QString base = normalizedEndpointBase();
    if (base.isEmpty() || m_apiKey.trimmed().isEmpty()) {
        m_statusLabel->setText(tr("Base URL or API Key is missing; cannot test."));
        return;
    }

    m_testButton->setEnabled(false);
    m_statusLabel->setText(tr("Testing model \"%1\"...").arg(modelName));

    // 构建最小请求
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] = QStringLiteral("You are a test assistant. Reply only with the word OK.");

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")]    = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = QStringLiteral("ping");

    QJsonObject body;
    body[QStringLiteral("model")]       = modelName;
    body[QStringLiteral("messages")]    = QJsonArray{ sysMsg, userMsg };
    body[QStringLiteral("max_tokens")]  = 16;
    body[QStringLiteral("temperature")] = 0.0;

    const QUrl url(base + QStringLiteral("/chat/completions"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());

    QNetworkAccessManager manager;
    QElapsedTimer elapsed;
    elapsed.start();
    QPointer<QNetworkReply> reply =
        manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout,         &loop, &QEventLoop::quit);
    QObject::connect(reply,  &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();

    m_testButton->setEnabled(true);

    if (!reply) {
        m_statusLabel->setText(tr("Test canceled."));
        return;
    }
    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        m_statusLabel->setText(tr("Test timed out (>30 s)."));
        return;
    }
    timer.stop();
    const qint64 ms = elapsed.elapsed();

    const QByteArray data      = reply->readAll();
    const int        httpCode  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool       netOk     = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    bool success = false;
    QString detail;

    if (netOk && pe.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("error"))) {
            detail = root.value(QStringLiteral("error")).toObject()
                         .value(QStringLiteral("message")).toString(tr("API error"));
        } else if (!root.value(QStringLiteral("choices")).toArray().isEmpty()) {
            success = true;
            detail  = tr("Response received.");
        } else {
            detail = tr("Unexpected response format.");
        }
    } else if (!netOk) {
        detail = tr("HTTP %1").arg(httpCode);
    } else {
        detail = tr("Invalid JSON response.");
    }

    if (success) {
        m_statusLabel->setText(
            tr("OK  |  Model: %1  |  Latency: %2 ms  |  %3")
                .arg(modelName)
                .arg(ms)
                .arg(detail));
    } else {
        m_statusLabel->setText(
            tr("FAILED  |  Model: %1  |  Latency: %2 ms  |  %3")
                .arg(modelName)
                .arg(ms)
                .arg(detail));
    }
}

void ModelPickerDialog::removeSelectedModel()
{
    const QList<QListWidgetItem *> selected = m_modelList->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    delete m_modelList->takeItem(m_modelList->row(selected.first()));
}

void ModelPickerDialog::onItemSelectionChanged()
{
    const bool hasSelection = !m_modelList->selectedItems().isEmpty();
    m_testButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_selectButton->setEnabled(hasSelection);
}

void ModelPickerDialog::acceptSelection()
{
    if (selectedModel().isEmpty()) {
        return;
    }
    accept();
}

// ── private helpers ───────────────────────────────────────────────────────────

void ModelPickerDialog::populateList(const QStringList &modelIds, const QString &selectModel)
{
    m_modelList->clear();
    for (const QString &id : modelIds) {
        auto *item = new QListWidgetItem(id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        m_modelList->addItem(item);
        if (!selectModel.isEmpty() && id == selectModel) {
            m_modelList->setCurrentItem(item);
        }
    }
    if (m_modelList->currentItem() == nullptr && m_modelList->count() > 0) {
        m_modelList->setCurrentRow(0);
    }
}

QString ModelPickerDialog::normalizedEndpointBase() const
{
    return normalizeBase(m_baseUrl);
}
