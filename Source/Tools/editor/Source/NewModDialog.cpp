#include "NewModDialog.hpp"
#include "ui_NewModDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>

NewModDialog::NewModDialog(QWidget* pParent)
    : QDialog(pParent), ui(new Ui::NewModDialog)
{
    ui->setupUi(this);
}

NewModDialog::~NewModDialog()
{
    delete ui;
}

void NewModDialog::on_btnBrowse_clicked()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a location for the new mod"));
    if (dir.isEmpty())
    {
        return;
    }
    mParentDir = dir;
    UpdateDirectoryPreview();
}

void NewModDialog::on_txtName_textChanged(const QString& /*text*/)
{
    UpdateDirectoryPreview();
}

void NewModDialog::on_chkBaseOn_toggled(bool checked)
{
    ui->txtBaseOn->setEnabled(checked);
    ui->btnBrowseBaseOn->setEnabled(checked);
}

void NewModDialog::on_btnBrowseBaseOn_clicked()
{
    // Accepts any folder with a levels/ subfolder - a mod root and a relive_data/<ao|ae> root
    // (the converted base game) are structurally identical after EditorMod/data_conversion.cpp
    // both moved to a "levels/" layer, so this one browse control covers "base on an existing
    // mod" and "base on the base game" without needing to special-case either.
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a mod or base game folder to copy levels from"));
    if (dir.isEmpty())
    {
        return;
    }
    if (!QDir(dir + "/levels").exists())
    {
        QMessageBox::warning(this, tr("New Mod"), tr("That folder has no \"levels\" subfolder - pick a mod's root folder or a relive_data/AO or relive_data/AE folder instead."));
        return;
    }
    ui->txtBaseOn->setText(dir);
}

void NewModDialog::UpdateDirectoryPreview()
{
    if (mParentDir.isEmpty())
    {
        return;
    }

    QString sanitizedName = ui->txtName->text().trimmed();
    // Strip characters that aren't safe as a directory name, matching the same "don't trust
    // free text as a path component" reasoning level abbreviations already get.
    static const QString kInvalidChars = "\\/:*?\"<>|";
    for (const QChar& c : kInvalidChars)
    {
        sanitizedName.remove(c);
    }
    if (sanitizedName.isEmpty())
    {
        sanitizedName = "NewMod";
    }

    ui->txtDirectory->setText(QDir(mParentDir).filePath(sanitizedName));
}

void NewModDialog::on_buttonBox_accepted()
{
    if (ui->txtName->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Please enter a mod name."));
        return;
    }
    if (ui->txtDirectory->text().isEmpty())
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Please choose a location."));
        return;
    }

    if (ui->chkBaseOn->isChecked() && ui->txtBaseOn->text().isEmpty())
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Please choose a project to base this mod on, or uncheck the option."));
        return;
    }

    const GameType game = ui->cmbGame->currentText() == "AE" ? GameType::eAe : GameType::eAo;
    mCreatedMod = EditorMod::CreateNew(ui->txtDirectory->text(), ui->txtName->text().trimmed(), ui->txtAuthor->text().trimmed(), game);
    if (!mCreatedMod)
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Failed to create the mod - the folder may already contain files, or couldn't be written to."));
        return;
    }

    // The mod itself is already created at this point either way - a failed copy leaves a real,
    // usable (if empty) mod behind rather than rolling anything back, same as CreateNew's own
    // "no transactional rollback" behavior for a failed directory/modinfo.json write.
    if (ui->chkBaseOn->isChecked())
    {
        if (!EditorMod::CopyLevelsFrom(ui->txtBaseOn->text(), mCreatedMod->mDirectory))
        {
            QMessageBox::warning(this, tr("New Mod"), tr("The mod was created, but copying levels from the chosen project failed partway through."));
        }
    }

    accept();
}
