#pragma once

#include <QDialog>
#include <memory>
#include "EditorMod.hpp"

namespace Ui
{
    class NewModDialog;
}

// Collects name/author/target game/on-disk location for a brand new mod and creates it
// (EditorMod::CreateNew) on accept. Mirrors AddObjectDialog/ChangeMapSizeDialog's small
// .ui-based form pattern rather than chaining QInputDialogs, since this has several related
// fields at once.
class NewModDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit NewModDialog(QWidget* pParent);
    ~NewModDialog();

    // Valid only after exec() returned QDialog::Accepted.
    std::unique_ptr<EditorMod> TakeMod()
    {
        return std::move(mCreatedMod);
    }

private slots:
    void on_btnBrowse_clicked();
    void on_btnBrowseBaseOn_clicked();
    void on_chkBaseOn_toggled(bool checked);
    void on_txtName_textChanged(const QString& text);
    void on_buttonBox_accepted();

private:
    void UpdateDirectoryPreview();

    Ui::NewModDialog* ui = nullptr;
    std::unique_ptr<EditorMod> mCreatedMod;
    QString mParentDir;
};
