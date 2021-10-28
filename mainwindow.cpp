#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDebug>

#include <QTimer>
#include <QMimeData>
#include <QSettings>

#include <QSerialPort>
#include <QSerialPortInfo>


AvrDudeProcess::AvrDudeProcess(QObject *parent)
    : QProcess(parent)
{
    connect(this, SIGNAL(readyReadStandardOutput()), SLOT(slotReadStdOut()));
    connect(this, SIGNAL(readyReadStandardError()), SLOT(slotReadStdErr()));
}

void AvrDudeProcess::slotReadStdOut()
{
    stdOutData.append(readAllStandardOutput());
}

void AvrDudeProcess::slotReadStdErr()
{
    stdErrData.append(readAllStandardError());
}

int AvrDudeProcess::execute(const QString &command)
{
    start(command, QProcess::ReadOnly);
    if (!waitForStarted())
    {
        return -1;
    }
    if(!waitForFinished())
    {
        return -2;
    }
    return exitCode();
}

bool extract_asset(const QString& pathRead, const QString& pathWrite)
{
    if (QFile::exists(pathWrite))
    {
        return true;
    }

    QFile fileRead (pathRead);
    QFile fileWrite (pathWrite);
    bool ret = false;

    if (fileRead.open(QIODevice::ReadOnly))
    {
        if (fileWrite.open(QIODevice::WriteOnly))
        {
            fileWrite.write(fileRead.readAll());
            fileWrite.close();
            ret = true;
        }

        fileRead.close();
    }

    return ret;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(QIcon(":/avrdude/qtIcon.png"));
    ui->statusBar->addPermanentWidget(ui->scrollArea, 1);

    connect(ui->pushButtonSearchFlash, SIGNAL(clicked()), this, SLOT(showFlashFileBrowse()));
    connect(ui->pushButtonSearchEeprom, SIGNAL(clicked()), this, SLOT(showEepromFileBrowse()));

    connect(ui->checkBoxExFuse, SIGNAL(toggled(bool)), SLOT(slotEfuse(bool)));
    connect(ui->checkBoxHighFuse, SIGNAL(toggled(bool)), SLOT(slotHfuse(bool)));

    QTimer::singleShot(10, this, SLOT(slotStart()));

    //odczyt dostepnych portów w PC
    foreach (const QSerialPortInfo &Port, QSerialPortInfo::availablePorts()){
        ui->comboBoxPort->addItem(Port.portName());
    }
}

void MainWindow::writeFuse(const QString &path)
{
    QString fuses;
    QString update;
    fromInterface();

    fuses = QString(" -U lfuse:w:0x%1:m").arg(lastLfuse);

    if (hasHfuse)
    {
        fuses += QString(" -U hfuse:w:0x%1:m").arg(lastHfuse);
    }
    if (hasEfuse)
    {
        fuses += QString(" -U efuse:w:0x%1:m").arg(lastEfuse);
    }

    if (!path.isEmpty())
    {
        update = QString(" -U flash:w:\"%1\":i").arg(path);
    }

    QString writeFuse = QString("avrdude -s -C avrdude.conf -c %1 -P %2 -p %3")
            .arg(lastProgrammer, lastPort, lastDevice)
            + fuses
            + update;

    ui->lineEditCommand->setText(writeFuse);
}

void MainWindow::slotHfuse(bool checked)
{
    ui->lineEditHighFuse->setEnabled(checked);
}

void MainWindow::slotEfuse(bool checked)
{
    ui->lineEditExFuse->setEnabled(checked);
}

#define SAVE_STR(x) conf.setValue(#x, x)
#define SAVE_BOOL(x) conf.setValue(#x,x)

MainWindow::~MainWindow()
{
    fromInterface();
    QSettings conf("Qavardude.ini", QSettings::IniFormat);
    SAVE_STR(lastProgrammer);
    SAVE_STR(lastDevice);
    SAVE_STR(lastPort);
    SAVE_STR(lastEfuse);
    SAVE_STR(lastHfuse);
    SAVE_STR(lastLfuse);
    SAVE_BOOL(hasEfuse);
    SAVE_BOOL(hasHfuse);

    conf.sync();

    delete ui;
}

void MainWindow::fromInterface()
{
    lastProgrammer = ui->comboBoxProgrammer->currentText();
    lastDevice = ui->comboBoxDevice->currentText();
    lastPort = ui->comboBoxPort->currentText();

    lastHfuse = ui->lineEditHighFuse->text().toLower();
    lastEfuse = ui->lineEditExFuse->text().toLower();
    lastLfuse = ui->lineEditLowFuse->text().toLower();

    hasHfuse = ui->checkBoxHighFuse->isChecked();
    hasEfuse = ui->checkBoxExFuse->isChecked();
}

void MainWindow::toInterface()
{
    ui->lineEditLowFuse->setText(lastLfuse);
    ui->lineEditHighFuse->setText(lastHfuse);
    ui->lineEditExFuse->setText(lastEfuse);

    ui->checkBoxExFuse->setChecked(hasEfuse);
    ui->lineEditExFuse->setEnabled(hasEfuse);

    ui->checkBoxHighFuse->setChecked(hasHfuse);
    ui->lineEditHighFuse->setEnabled(hasHfuse);
}

bool MainWindow::fillCombobox(const QString &prog, QComboBox *box, const QString &matchingD)
{
    AvrDudeProcess proces;
    if (proces.execute(prog) < 0)
    {
        return false;
    }

    QList<QByteArray> lines = proces.stdErrData.split('\n');
    QString str, key, value;
    int i = 0;
    int found = -1;
    int n;

    foreach (const QByteArray& line, lines)
    {
        str = QString::fromLocal8Bit(line);
        if ((n = str.indexOf(" = ")) == -1)
        {
            continue;
        }

        key = str.left(n).simplified();
        value = str.mid(n+3).simplified();

        if (box == ui->comboBoxProgrammer){
            box->addItem(QString("%1").arg(key), key);
            if (found == -1 && key == matchingD)
            {
                found = i;
                box->setCurrentIndex(i);
            }
        }
        else if (box == ui->comboBoxDevice) {
            box->addItem(QString("%2").arg(value).toLower(), value);
            if (found == -1 && value == matchingD)
            {
                found = i;
                box->setCurrentIndex(i);
            }
        }
        ++i;
    }
    return true;
}

#define READ_STR(x) x = conf.value(#x).toString()
#define READ_BOOL(x) x = conf.value(#x).toBool()

void MainWindow::slotStart()
{

#ifdef Q_OS_WIN32
    if (!extract_asset(":/avrdude/avrdude.exe", "avrdude.exe")
        || !extract_asset(":/avrdude/avrdude.conf", "avrdude.conf"))
    {
        QMessageBox::warning(this, tr("Błąd"), tr("biblioteka avrdude nie działa!"),
                QMessageBox::Ok);
        qApp->quit();
        return;
    }
#endif

    QSettings conf("Qavrdude.ini", QSettings::IniFormat);
    READ_STR(lastProgrammer);
    READ_STR(lastDevice);
    READ_STR(lastPort);
    READ_STR(lastHfuse);
    READ_STR(lastLfuse);
    READ_STR(lastEfuse);
    READ_BOOL(hasEfuse);


    if (!fillCombobox("avrdude -c?", ui->comboBoxProgrammer, lastProgrammer))
    {
        QMessageBox::warning(this, tr("Błąd"),
                             tr("AvrDude nie jest zainstalowane w systemie!"),
                             QMessageBox::Ok);
        qApp->quit();
        return;
    }

    fillCombobox("avrdude -p?", ui->comboBoxDevice, lastDevice);
    toInterface();
}

void MainWindow::slotRead()
{
    AvrDudeProcess process;
    fromInterface();

    ui->lineEditCommand->setText("avrdude -s -C avrdude.conf -c "+lastProgrammer+ " -P " +lastPort+ " -p " +lastDevice);

    QString str = QString::fromLocal8Bit(process.stdErrData);
    int n;
    QRegExp expr("Fuses OK \\(E:[0-9A-F][0-9A-F], H:[0-9A-F][0-9A-F], L:[0-9A-F][0-9A-F]\\)");
    if ((n = str.indexOf(expr)) != -1)
    {
        ui->lineEditExFuse->setText(str.mid(n + 12, 2));
        ui->lineEditHighFuse->setText(str.mid(n + 18, 2));
        ui->lineEditLowFuse->setText(str.mid(n + 24, 2));
    }
}

void MainWindow::eraseDevice()
{
    AvrDudeProcess process;
    fromInterface();

    process.execute(QString("avrdude -s -C avrdude.conf -c %1 -P %2 -p %3 -e")
                    .arg(lastProgrammer, lastPort, lastDevice));

    ui->textEditAvrDudeOut->append(process.stdOutData);
    ui->textEditAvrDudeOut->append("---");
    ui->textEditAvrDudeOut->append(process.stdErrData);
}

void MainWindow::programFlash(QString fileName, bool verifyAfter, bool eraseBefore)
{
    fromInterface();
    fileName = ui->lineEditFlash->text();
    QString writeFlash;

    QChar flashType = 'i';
    if (fileName.toLower().endsWith(".s")){
        flashType = 's';
    }
    else if (fileName.toLower().endsWith(".bin") || fileName.toLower().endsWith(".raw")){
        flashType = 'r';
    }

    writeFlash = ("avrdude -c %1 -P %2 -p %3 -U flash:w:"+fileName+":"+flashType)
            .arg(lastProgrammer, lastPort, lastDevice);

    if(!verifyAfter)
    {
        writeFlash = ("avrdude -c %1 -P %2 -p %3 -V -U flash:w:"+fileName+":"+flashType)
                .arg(lastProgrammer, lastPort, lastDevice);
    }
    if(!eraseBefore)
    {
        writeFlash = ("avrdude -c %1 -P %2 -p %3 -D -U flash:w:"+fileName+":"+flashType)
                .arg(lastProgrammer, lastPort, lastDevice);
    }

    ui->lineEditCommand->setText(writeFlash);
}

//okienka do szukania plików
void MainWindow::showFlashFileBrowse(){
    QString path;
    QString fn = QFileDialog::getOpenFileName(this,
                                              tr("Wybierz plik Flash ..."),
                                              path,
                                              tr("HEX files (*.hex);;RAW image (*.bin *.raw);;Motorla S Records (*.s)"));
    if (fn != ""){
        ui->lineEditFlash->setText(fn);
    }
}

void MainWindow::showEepromFileBrowse(){
    QString path;
    QString fn = QFileDialog::getOpenFileName(this, tr("Wybierz plik EEPROM ..."), path, tr("HEX files (*.hex)"));
    ui->lineEditEeprom->setText(fn);
}

void MainWindow::on_pushButtonErase_clicked()
{
    QMessageBox question(QMessageBox::Question,
                        "Jesteś pewny?",
                        "Czy na pewno chcesz wyczyścić pamięci\n"
                        "Flash i EEPROM urządzenia?",
                        (QMessageBox::Yes | QMessageBox::No));
    if (question.exec() == QMessageBox::Yes) {

        QMessageBox::warning(this, tr("Uwaga!"), tr("Czyszczenie urządzenia"), QMessageBox::Ok);
        eraseDevice();
    }
}

void MainWindow::on_pushButtonProgramFlash_clicked()
{
    if (QFile::exists(ui->lineEditFlash->text())){
        programFlash(ui->lineEditFlash->text(),
                     ui->checkBoxVerifyAfter->isChecked(),
                     ui->checkBoxClearBefore->isChecked());
    }
    else {
        ui->textEditAvrDudeOut->setText("Nie wybrałeś pliku pamięci Flash!");
    }
}

void MainWindow::on_pushButtonReadFlash_clicked()
{
    fromInterface();
    QString readFlash;
    QString fileName = ui->lineEditFlash->text();

    if (QFile::exists(ui->lineEditFlash->text())) {
        QMessageBox exists(QMessageBox::Warning	,
                           tr("Plik już istnieje!"),
                           tr("Wyjściowy plik istnieje.\n Czy napisać plik?"),
                            (QMessageBox::Yes | QMessageBox::No));
        if (exists.exec() == QMessageBox::Yes) {
            ui->textEditAvrDudeOut->setText("Czytanie pamięci Flash");
            readFlash = ("avrdude -c %1 -P %2 -p %3 -U flash:r:"+fileName+":i").arg(lastProgrammer, lastPort, lastDevice);
            ui->lineEditCommand->setText(readFlash);
        }
    } else {
        ui->textEditAvrDudeOut->setText("Czytanie pamięci Flash");
        readFlash = ("avrdude -c %1 -P %2 -p %3 -U flash:r:"+fileName+":i").arg(lastProgrammer, lastPort, lastDevice);
        ui->lineEditCommand->setText(readFlash);
    }
}

void MainWindow::on_pushButtonVerifyFlash_clicked()
{
    fromInterface();
    QString verifyFlash;
    QString fileName = ui->lineEditFlash->text();

    if (QFile::exists(ui->lineEditFlash->text())){
        verifyFlash = ("avrdude -c %1 -P %2 -p %3 -U flash:v:\""+fileName+"\":i").arg(lastProgrammer, lastPort, lastDevice);
        ui->lineEditCommand->setText(verifyFlash);
    } else {
        ui->textEditAvrDudeOut->setText("Nie można zweryfikować ponieważ plik pamięci Flash nie istnieje.");
    }
}

void MainWindow::on_pushButtonProgramEeprom_clicked()
{
    fromInterface();

    QString programEeprom;
    QString fileName = ui->lineEditEeprom->text();

    if (QFile::exists(ui->lineEditEeprom->text())){
        programEeprom = ("avrdude -c %1 -P %2 -p %3 -U eeprom:w:"+fileName+":i").arg(lastProgrammer,lastPort, lastDevice);
        ui->lineEditCommand->setText(programEeprom);
    }
    else {
        ui->textEditAvrDudeOut->setText("Nie wybrałeś pliku pamięci Eeprom!");
    }
}

void MainWindow::on_pushButtonReadEeprom_clicked()
{
    fromInterface();
    QString readEeprom;
    QString fileName = ui->lineEditEeprom->text();

    if (QFile::exists(ui->lineEditEeprom->text())) {
        QMessageBox exists(QMessageBox::Warning	,
                           tr("Plik już istnieje!"),
                           tr("Wyjściowy plik istnieje.\n Czy napisać plik?"),
                            (QMessageBox::Yes | QMessageBox::No));
        if (exists.exec() == QMessageBox::Yes) {
            ui->textEditAvrDudeOut->setText("Czytanie pamięci Eeprom.");
            readEeprom = ("avrdude -c %1 -P %2 -p %3 -U eeprom:r:\""+fileName+"\":i").arg(lastProgrammer, lastPort, lastDevice);
            ui->lineEditCommand->setText(readEeprom);
        }
    } else {
        ui->textEditAvrDudeOut->setText("Czytanie pamięci Eeprom.");
        readEeprom = ("avrdude -c %1 -P %2 -p %3 -U eeprom:r:\""+fileName+"\":i").arg(lastProgrammer, lastPort, lastDevice);
        ui->lineEditCommand->setText(readEeprom);
    }
}

void MainWindow::on_pushButtonVerifyEeprom_clicked()
{
    fromInterface();
    QString verifyEeprom;
    QString fileName = ui->lineEditEeprom->text();

    if (QFile::exists(ui->lineEditEeprom->text())){
        verifyEeprom = ("avrdude -c %1 -P %2 -p %3 -U eeprom:v:\""+fileName+"\":i").arg(lastProgrammer, lastPort, lastDevice);
        ui->lineEditCommand->setText(verifyEeprom);
    } else {
        ui->textEditAvrDudeOut->setText("Nie można zweryfikować ponieważ plik pamięci Eeprom nie istnieje.");
    }
}

void MainWindow::on_pushButtonWriteFuse_clicked()
{
    writeFuse(lastPath);
}

void MainWindow::on_pushButtonReadFuse_clicked()
{
    slotRead();
}

void MainWindow::on_pushButtonKalk_clicked()
{
   QDesktopServices::openUrl(QUrl("http://www.engbedded.com/fusecalc/"));
}

void MainWindow::on_pushButtonHelp_clicked()
{
    QString help;
    help = ("avrdude -h");

    ui->lineEditCommand->setText(help);
}

void MainWindow::on_pushButtonExecute_clicked()
{
    AvrDudeProcess process;
    QString command;

    process.execute(ui->lineEditCommand->text());
    ui->lineEditCommand->clear();
    ui->textEditAvrDudeOut->append(process.stdOutData);
    ui->textEditAvrDudeOut->append("---");
    ui->textEditAvrDudeOut->append(process.stdErrData);

}

void MainWindow::on_pushButtonClearAvrDudeOut_clicked()
{
    ui->textEditAvrDudeOut->clear();
}
