#ifndef PROGRESSDIALOG_HPP
#define PROGRESSDIALOG_HPP

#include <QDialog>
#include <QString>
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

// Non-template part of ExecASync, defined in ProgressDialog.cpp so QtConcurrent is only
// parsed there.
void ExecASyncImpl(const QString& dialogTitle, const std::function<void()>& fnDoWork);

// Runs fnDoWork on a background thread while showing a modal, indeterminate
// progress dialog with the given label. Blocks until the work completes.
//
// Note: the result is threaded through a captured local rather than
// QFuture<ResultType>::result() - under Qt5, QFuture<T>::result() requires T
// to be copyable, which move-only types like std::unique_ptr<...> aren't.
template<class ResultType>
static ResultType ExecASync(QString dialogTitle, std::function<ResultType()> fnDoWork)
{
    ResultType result{};
    ExecASyncImpl(dialogTitle, [&]() { result = fnDoWork(); });
    return result;
}

#endif // PROGRESSDIALOG_HPP
