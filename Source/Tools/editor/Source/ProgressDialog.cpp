#include "ProgressDialog.hpp"
#include "ui_ProgressDialog.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

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

void ExecASyncImpl(const QString& dialogTitle, const std::function<void()>& fnDoWork)
{
    ProgressDialog dlg;
    dlg.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    dlg.setWindowTitle(dialogTitle);
    dlg.SetLabel(dialogTitle);

    QFuture<void> f = QtConcurrent::run([&]() { fnDoWork(); });

    QFutureWatcher<void> watcher;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &dlg, &QDialog::close);
    watcher.setFuture(f);

    dlg.exec();
}
