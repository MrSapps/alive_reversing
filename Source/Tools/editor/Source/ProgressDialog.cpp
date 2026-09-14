#include "ProgressDialog.hpp"
#include "ui_ProgressDialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::SetLabel(const QString& text)
{
    ui->label->setText(text);
}
