#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QProcess>
#include <QComboBox>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class AvrDudeProcess : public QProcess
{
    Q_OBJECT
public:
    QByteArray stdOutData;
    QByteArray stdErrData;

    AvrDudeProcess(QObject *parent = nullptr);

    int execute(const QString &command);

private slots:
    void slotReadStdOut();
    void slotReadStdErr();

};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    QString lastProgrammer;
    QString lastDevice;
    QString lastPort;

    QString lastHfuse;
    QString lastLfuse;
    QString lastEfuse;
    QString lastPath;

    bool hasHfuse;
    bool hasEfuse;
    bool fillCombobox(const QString& prog, QComboBox *box, const QString &matchingD);

    void fromInterface();
    void toInterface();

    void writeFuse(const QString& path);
    void eraseDevice();

    void programFlash(QString fileName, bool verifyAfter, bool eraseBefore);

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void slotStart();
    void slotRead();

    void slotHfuse(bool);
    void slotEfuse(bool);

    void showFlashFileBrowse();
    void showEepromFileBrowse();

    void on_pushButtonErase_clicked();
    void on_pushButtonKalk_clicked();

    void on_pushButtonWriteFuse_clicked();
    void on_pushButtonReadFuse_clicked();

    void on_pushButtonClearAvrDudeOut_clicked();

    void on_pushButtonProgramFlash_clicked();
    void on_pushButtonReadFlash_clicked();
    void on_pushButtonVerifyFlash_clicked();

    void on_pushButtonProgramEeprom_clicked();
    void on_pushButtonReadEeprom_clicked();
    void on_pushButtonVerifyEeprom_clicked();


    void on_pushButtonHelp_clicked();
    void on_pushButtonExecute_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
