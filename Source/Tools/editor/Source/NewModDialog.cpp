#include "NewModDialog.hpp"
#include "ui_NewModDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include "ProgressDialog.hpp"

NewModDialog::NewModDialog(QWidget* pParent)
    : QDialog(pParent), ui(new Ui::NewModDialog)
{
    ui->setupUi(this);

    // Current OS user as a sensible default author, same idiom used cross-platform by other
    // tools ("USER" on Linux/Mac, "USERNAME" on Windows) - the user can still freely change it.
#ifdef _WIN32
    ui->txtAuthor->setText(qEnvironmentVariable("USERNAME"));
#else
    ui->txtAuthor->setText(qEnvironmentVariable("USER"));
#endif

    // Default parent directory for the new mod, used both for the directory preview below and
    // as the starting point if the user browses for a location - same default location logic
    // as "last_open_dir" elsewhere in the editor, just without depending on that setting.
    mParentDir = QDir::homePath();

    // Pick a non-colliding default name: "New mod", then "New mod (1)", "New mod (2)", ... up to
    // a small bound, checking against sibling directories in the default parent dir.
    QString defaultName = tr("New mod");
    for (int i = 0; QDir(mParentDir).exists(defaultName); ++i)
    {
        defaultName = tr("New mod (%1)").arg(i + 1);
        if (i >= 100)
        {
            // Give up trying to find a free name and just let UpdateDirectoryPreview/CreateNew
            // report the collision as normal, rather than looping indefinitely.
            break;
        }
    }
    ui->txtName->setText(defaultName);

    UpdateDirectoryPreview();
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
    const bool baseOn = ui->chkBaseOn->isChecked();
    const QString dir = ui->txtDirectory->text();
    const QString name = ui->txtName->text().trimmed();
    const QString author = ui->txtAuthor->text().trimmed();
    const QString baseOnDir = ui->txtBaseOn->text();

    bool copyFailed = false;
    mCreatedMod = ExecASync<std::unique_ptr<EditorMod>>("Creating mod...", [&]()
        {
            auto mod = EditorMod::CreateNew(dir, name, author, game);
            // The mod itself is already created at this point either way - a failed copy leaves a
            // real, usable (if empty) mod behind rather than rolling anything back, same as
            // CreateNew's own "no transactional rollback" behavior for a failed directory/modinfo.json
            // write.
            if (mod && baseOn)
            {
                copyFailed = !EditorMod::CopyLevelsFrom(baseOnDir, mod->mDirectory);
            }
            return mod;
        });

    if (!mCreatedMod)
    {
        QMessageBox::warning(this, tr("New Mod"), tr("Failed to create the mod - the folder may already contain files, or couldn't be written to."));
        return;
    }

    if (copyFailed)
    {
        QMessageBox::warning(this, tr("New Mod"), tr("The mod was created, but copying levels from the chosen project failed partway through."));
    }

    accept();
}
