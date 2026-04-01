#ifndef MODEL_PICKER_DIALOG_H
#define MODEL_PICKER_DIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;

/**
 * ModelPickerDialog — 模型选择与测试窗口
 *
 * 功能：
 *  - 手动输入模型名并添加到列表
 *  - 点击"拉取模型"从 /models 接口自动获取
 *  - 列表项可通过复选框控制是否启用
 *  - 选中单行后点击"测试"按钮，向 API 发一条简单请求
 *    并显示测试状态（成功/失败）及响应时间（ms）
 *
 * 关闭后通过 selectedModel() 获取当前选中的模型名，
 * 通过 models() 获取完整的已配置模型列表（含禁用项）。
 */
class ModelPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModelPickerDialog(const QString &baseUrl,
                               const QString &apiKey,
                               const QStringList &existingModels,
                               const QString &currentModel,
                               QWidget *parent = nullptr);

    /** 当前选中（高亮）的模型名，作为 provider 的 active model */
    QString selectedModel() const;

    /** 完整的已配置模型列表（包含被取消勾选的项） */
    QStringList models() const;

private slots:
    void addManualModel();
    void fetchModels();
    void testSelectedModel();
    void removeSelectedModel();
    void onItemSelectionChanged();
    void acceptSelection();

private:
    void populateList(const QStringList &modelIds, const QString &selectModel = {});
    QString normalizedEndpointBase() const;

    QString m_baseUrl;
    QString m_apiKey;

    QLineEdit     *m_manualEdit      = nullptr;
    QListWidget   *m_modelList       = nullptr;
    QPushButton   *m_fetchButton     = nullptr;
    QPushButton   *m_testButton      = nullptr;
    QPushButton   *m_removeButton    = nullptr;
    QLabel        *m_statusLabel     = nullptr;
    QPushButton   *m_selectButton    = nullptr;
};

#endif // MODEL_PICKER_DIALOG_H
