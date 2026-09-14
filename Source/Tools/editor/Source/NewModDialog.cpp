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

    const GameType game = ui->cmbGame->currentText() == "AE" ? GameType::eAe : GameType::eAo;
    mCreatedMod = EditorMod::CreateNew(ui->txtDirectory->text(), ui->txtName->text().trimmed(), ui->txtAuthor->text().trimmed(), game);
    if (!mCreatedMod)
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Failed to create the mod - the folder may already contain files, or couldn't be written to."));
        return;
    }

    accept();
}
