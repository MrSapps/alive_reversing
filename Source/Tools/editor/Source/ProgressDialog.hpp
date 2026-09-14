#ifndef PROGRESSDIALOG_HPP
#define PROGRESSDIALOG_HPP

#include <QDialog>
#include <QString>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <functional>

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = nullptr);
    ~ProgressDialog();

    void SetLabel(const QString& text);

private:
    Ui::ProgressDialog *ui;
};

// Runs fnDoWork on a background thread while showing a modal, indeterminate
// progress dialog with the given label. Blocks until the work completes.
//
// Note: the result is threaded through a captured local rather than
// QFuture<ResultType>::result() - under Qt5, QFuture<T>::result() requires T
// to be copyable, which move-only types like std::unique_ptr<...> aren't.
template<class ResultType>
static ResultType ExecASync(QString dialogTitle, std::function<ResultType()> fnDoWork)
{
    ProgressDialog dlg;
    dlg.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    dlg.setWindowTitle(dialogTitle);
    dlg.SetLabel(dialogTitle);

    ResultType result{};
    QFuture<void> f = QtConcurrent::run([&]() { result = fnDoWork(); });

    QFutureWatcher<void> watcher;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &dlg, &QDialog::close);
    watcher.setFuture(f);

    dlg.exec();

    return result;
}

#endif // PROGRESSDIALOG_HPP
