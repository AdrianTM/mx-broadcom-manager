//
//   Copyright (C) 2003-2010 by Warren Woodford
//   Copyright (C) 2014 by Adrian adrian@mxlinux.org
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

//   With big modification made by Adrian adrian@mxlinux.org

#include "mconfig.h"
#include <QFileDialog>
#include <QWebView>
#include <QMenu>
#include <QClipboard>
#include <QDesktopWidget>

#include <unistd.h>

MConfig::MConfig(QWidget* parent)
    : QDialog(parent) {
    setupUi(this);
    setWindowIcon(QApplication::windowIcon());

    currentTab = 0;
    tabWidget->setCurrentIndex(0);

    configurationChanges[0] = false;
    configurationChanges[1] = false;

    pingProc  = new QProcess(this);
    traceProc = new QProcess(this);    
    installProc = new QProcess(this);

    installOutputEdit = new QTextEdit();

    connect(hwList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForHw(const QPoint &)));
    connect(linuxDrvList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForLinuxDrv(const QPoint &)));
    connect(windowsDrvList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForWindowsDrv(const QPoint &)));
    on_hwDiagnosePushButton_clicked();
    on_linuxDrvDiagnosePushButton_clicked();
}

MConfig::~MConfig() {
}

/////////////////////////////////////////////////////////////////////////
// util functions

QString MConfig::getCmdOut(QString cmd) {
    char line[260];
    const char* ret = "";
    FILE* fp = popen(cmd.toAscii(), "r");
    if (fp == NULL) {
        return QString (ret);
    }
    int i;
    if (fgets(line, sizeof line, fp) != NULL) {
        i = strlen(line);
        line[--i] = '\0';
        ret = line;
    }
    pclose(fp);
    return QString (ret);
}

QStringList MConfig::getCmdOuts(QString cmd) {
    char line[260];
    FILE* fp = popen(cmd.toAscii(), "r");
    QStringList results;
    if (fp == NULL) {
        return results;
    }
    int i;
    while (fgets(line, sizeof line, fp) != NULL) {
        i = strlen(line);
        line[--i] = '\0';
        results.append(line);
    }
    pclose(fp);
    return results;
}

QString MConfig::getCmdValue(QString cmd, QString key, QString keydel, QString valdel) {
    const char *ret = "";
    char line[260];

    QStringList strings = getCmdOuts(cmd);
    for (QStringList::Iterator it = strings.begin(); it != strings.end(); ++it) {
        strcpy(line, ((QString)*it).toAscii());
        char* keyptr = strstr(line, key.toAscii());
        if (keyptr != NULL) {
            // key found
            strtok(keyptr, keydel.toAscii());
            const char* val = strtok(NULL, valdel.toAscii());
            if (val != NULL) {
                ret = val;
            }
            break;
        }
    }
    return QString (ret);
}

QStringList MConfig::getCmdValues(QString cmd, QString key, QString keydel, QString valdel) {
    char line[130];
    FILE* fp = popen(cmd.toAscii(), "r");
    QStringList results;
    if (fp == NULL) {
        return results;
    }
    int i;
    while (fgets(line, sizeof line, fp) != NULL) {
        i = strlen(line);
        line[--i] = '\0';
        char* keyptr = strstr(line, key.toAscii());
        if (keyptr != NULL) {
            // key found
            strtok(keyptr, keydel.toAscii());
            const char* val = strtok(NULL, valdel.toAscii());
            if (val != NULL) {
                results.append(val);
            }
        }
    }
    pclose(fp);
    return results;
}

bool MConfig::replaceStringInFile(QString oldtext, QString newtext, QString filepath) {

    QString cmd = QString("sed -i 's/%1/%2/g' %3").arg(oldtext).arg(newtext).arg(filepath);
    if (system(cmd.toAscii()) != 0) {
        return false;
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////
// common

void MConfig::refresh() {
    int i = tabWidget->currentIndex();

    switch (i) {
    case 1:
        break;
    // Windows drivers
    case 2:
        updateNdiswrapStatus();
        break;

    default:
        bool changed = configurationChanges[0];
        configurationChanges[0] = changed;
        break;
    }
}

/////////////////////////////////////////////////////////////////////////
// special

void MConfig::on_cancelPing_clicked()
{
    if (pingProc->state() != QProcess::NotRunning)
    {
        pingProc->kill();
    }
    cancelPing->setEnabled(false);
}

void MConfig::on_cancelTrace_clicked()
{
    if (traceProc->state() != QProcess::NotRunning)
    {
        traceProc->kill();
    }
    cancelTrace->setEnabled(false);
}

void MConfig::on_clearPingOutput_clicked()
{
    pingOutputEdit->clear();
    clearPingOutput->setEnabled(false);
}

void MConfig::hwListToClipboard()
{
    if (hwList->currentRow() != -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QListWidgetItem* currentElement = hwList->currentItem();
        clipboard->setText(currentElement->text());
    }
}

void MConfig::hwListFullToClipboard()
{
    if (hwList->count() > -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QString elementList = "";
        for (int i = 0; i < hwList->count(); i++)
        {
            QListWidgetItem* currentElement = hwList->item(i);
            elementList += currentElement->text() + "\n";
        }
        clipboard->setText(elementList);
    }
}

void MConfig::linuxDrvListToClipboard()
{
    if (linuxDrvList->currentRow() != -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QListWidgetItem* currentElement = linuxDrvList->currentItem();
        clipboard->setText(currentElement->text());
    }
}

void MConfig::linuxDrvListFullToClipboard()
{
    if (hwList->count() > -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QString elementList = "";
        for (int i = 0; i < linuxDrvList->count(); i++)
        {
            QListWidgetItem* currentElement = linuxDrvList->item(i);
            elementList += currentElement->text() + "\n";
        }
        clipboard->setText(elementList);
    }
}

void MConfig::windowsDrvListToClipboard()
{
    if (linuxDrvList->currentRow() != -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QListWidgetItem* currentElement = windowsDrvList->currentItem();
        clipboard->setText(currentElement->text());
    }
}

void MConfig::windowsDrvListFullToClipboard()
{
    if (hwList->count() > -1)
    {
        QClipboard *clipboard = QApplication::clipboard();
        QString elementList = "";
        for (int i = 0; i < windowsDrvList->count(); i++)
        {
            QListWidgetItem* currentElement = windowsDrvList->item(i);
            elementList += currentElement->text() + "\n";
        }
        clipboard->setText(elementList);
    }
}

void MConfig::showContextMenuForHw(const QPoint &pos)
{
    QMenu contextMenu(this);
    QAction * copyAction = new QAction(tr("&Copy"), this);
    connect(copyAction, SIGNAL(activated()) , this, SLOT(hwListToClipboard()));
    copyAction->setShortcut(tr("Ctrl+C"));
    QAction * copyAllAction = new QAction(tr("Copy &All"), this);
    connect(copyAllAction, SIGNAL(activated()) , this, SLOT(hwListFullToClipboard()));
    copyAllAction->setShortcut(tr("Ctrl+A"));
    contextMenu.addAction(copyAction);
    contextMenu.addAction(copyAllAction);
    contextMenu.exec(hwList->mapToGlobal(pos));
}

void MConfig::showContextMenuForLinuxDrv(const QPoint &pos)
{
    QMenu contextMenu(this);
    QAction * copyAction = new QAction(tr("&Copy"), this);
    connect(copyAction, SIGNAL(activated()) , this, SLOT(linuxDrvListToClipboard()));
    copyAction->setShortcut(tr("Ctrl+C"));
    QAction * copyAllAction = new QAction(tr("Copy &All"), this);
    connect(copyAllAction, SIGNAL(activated()) , this, SLOT(linuxDrvListFullToClipboard()));
    copyAllAction->setShortcut(tr("Ctrl+A"));
    contextMenu.addAction(copyAction);
    contextMenu.addAction(copyAllAction);
    contextMenu.exec(linuxDrvList->mapToGlobal(pos));
}

void MConfig::showContextMenuForWindowsDrv(const QPoint &pos)
{
    QMenu contextMenu(this);
    QAction * copyAction = new QAction(tr("&Copy"), this);
    connect(copyAction, SIGNAL(activated()) , this, SLOT(windowsDrvListToClipboard()));
    copyAction->setShortcut(tr("Ctrl+C"));
    QAction * copyAllAction = new QAction(tr("Copy &All"), this);
    connect(copyAllAction, SIGNAL(activated()) , this, SLOT(windowsDrvListFullToClipboard()));
    copyAllAction->setShortcut(tr("Ctrl+A"));
    contextMenu.addAction(copyAction);
    contextMenu.addAction(copyAllAction);
    contextMenu.exec(windowsDrvList->mapToGlobal(pos));
}

void MConfig::on_clearTraceOutput_clicked()
{
    tracerouteOutputEdit->clear();
    clearTraceOutput->setEnabled(false);
}

void MConfig::writeTraceOutput()
{
    QByteArray bytes = traceProc->readAllStandardOutput();
    QStringList lines = QString(bytes).split("\n");
    foreach (QString line, lines) {
        if (!line.isEmpty())
        {
            tracerouteOutputEdit->append(line);
        }
    }
}

void MConfig::tracerouteFinished()
{
    cancelTrace->setEnabled(false);
}

void MConfig::on_tracerouteButton_clicked()
{
    QString statusl = getCmdValue("dpkg -s traceroute | grep '^Status'", "ok", " ", " ");
    if (statusl.compare("installed") != 0)
    {
        if (internetConnection)
        {
            setCursor(QCursor(Qt::WaitCursor));
            int ret = QMessageBox::information(0, tr("Traceroute not installed"),
                                               tr("Traceroute is not installed, do you want to install it now?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (ret == QMessageBox::Yes)
            {
                system("apt-get install -qq traceroute");
                setCursor(QCursor(Qt::ArrowCursor));
                statusl = getCmdValue("dpkg -s traceroute | grep '^Status'", "ok", " ", " ");
                if (statusl.compare("installed") != 0) {
                    QMessageBox::information(0, tr("Traceroute hasn't been installed"),
                                             tr("Traceroute cannot be installed. This may mean you are using the LiveCD or you are unable to reach the software repository,"),
                                             QMessageBox::Ok);
                }
                else
                {
                    setCursor(QCursor(Qt::ArrowCursor));
                    return;
                }
            }
        }
        else
        {
            QMessageBox::information(0, tr("Traceroute not installed"),
                                     tr("Traceroute is not installed and no Internet connection could be detected so it cannot be installed"),
                                     QMessageBox::Ok);
            return;
        }
    }
    if (tracerouteHostEdit->text().isEmpty())
    {
        QMessageBox::information(0, tr("No destination host"),
                                 tr("Please fill in the destination host field"),
                                 QMessageBox::Ok);
    }
    else
    {
        setCursor(QCursor(Qt::WaitCursor));

        QString program = "traceroute";
        QStringList arguments;
        arguments << tracerouteHostEdit->text() << QString("-m %1").arg(traceHopsNumber->value());

        if (traceProc->state() != QProcess::NotRunning)
        {
            traceProc->kill();
        }

        traceProc->start(program, arguments);
        disconnect(traceProc, SIGNAL(readyReadStandardOutput()), 0, 0);
        connect(traceProc, SIGNAL(readyReadStandardOutput()), this, SLOT(writeTraceOutput()));
        disconnect(traceProc, SIGNAL(finished(int,QProcess::ExitStatus)), 0, 0);
        connect(traceProc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(tracerouteFinished()));

        //QStringList vals = getCmdOuts(QString("traceroute %1").arg(tracerouteHostEdit->text()));
        //tracerouteOutputEdit->append(vals.join("\n"));
        clearTraceOutput->setEnabled(true);
        cancelTrace->setEnabled(true);
        setCursor(QCursor(Qt::ArrowCursor));
    }
}

void MConfig::writePingOutput()
{
    QByteArray bytes = pingProc->readAllStandardOutput();
    QStringList lines = QString(bytes).split("\n");
    foreach (QString line, lines) {
        if (!line.isEmpty())
        {
            pingOutputEdit->append(line);
        }
    }
}

void MConfig::pingFinished()
{
    cancelPing->setEnabled(false);
}

void MConfig::on_pingButton_clicked()
{
    if (pingHostEdit->text().isEmpty())
    {
        QMessageBox::information(0, tr("No destination host"),
                                 tr("Please, fill the destination host field"),
                                 QMessageBox::Ok);
    }
    else
    {
        setCursor(QCursor(Qt::WaitCursor));
        QString program = "ping";
        QStringList arguments;
        arguments << QString("-c %1").arg(pingPacketNumber->value()) << "-W 5" << pingHostEdit->text();

        if (pingProc->state() != QProcess::NotRunning)
        {
            pingProc->kill();
        }
        pingProc->start(program, arguments);
        disconnect(pingProc, SIGNAL(readyReadStandardOutput()), 0, 0);
        connect(pingProc, SIGNAL(readyReadStandardOutput()), this, SLOT(writePingOutput()));
        disconnect(pingProc, SIGNAL(finished(int,QProcess::ExitStatus)), 0, 0);
        connect(pingProc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(pingFinished()));

        //QStringList vals = getCmdOuts(QString("ping -c 4 -W 5 %1").arg(pingHostEdit->text()));
        //pingOutputEdit->append(vals.join("\n"));
        clearPingOutput->setEnabled(true);
        cancelPing->setEnabled(true);
        setCursor(QCursor(Qt::ArrowCursor));
    }
}


/////////////////////////////////////////////////////////////////////////
// slots

void MConfig::show() {
    QDialog::show();
    refresh();
}

void MConfig::executeChild(const char* cmd, const char* param)
{
    pid_t childId;
    childId = fork();
    if (childId >= 0)
    {
        if (childId == 0)
        {
            execl(cmd, cmd, param, (char *) 0);

            //system(cmd);
        }
    }
}


// Added
void MConfig::on_hwDiagnosePushButton_clicked()
{
    hwList->clear();

    // Query PCI cards
    QStringList  queryResult = getCmdOuts("lspci -nn | grep -i net");
    for (int i = 0; i < queryResult.size(); ++i)
    {
        QString currentElement = queryResult.at(i);
        if (currentElement.indexOf("Ethernet controller") != -1)
        {
            currentElement.remove(QRegExp("Ethernet controller .[\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f].:"));
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/places/network-server.png"),currentElement, hwList);
        }
        else if (currentElement.indexOf("Network controller") != -1)
        {
            currentElement.remove(QRegExp("Network controller .[\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f][\\d|a|b|c|d|e|f].:"));
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/places/network-workgroup.png"),currentElement, hwList);
        }
        else
        {
            new QListWidgetItem(currentElement, hwList);
        }
    }

    // Query USB cards
    queryResult = getCmdOuts("lsusb | grep -i network");
    for (int i = 0; i < queryResult.size(); ++i)
    {
        QString currentElement = queryResult.at(i);
        if (currentElement.indexOf("Ethernet controller:") != -1)
        {
            currentElement.remove(QString("Ethernet controller:"), Qt::CaseSensitive);
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/places/network-server.png"),currentElement, hwList);
        }
        else if (currentElement.indexOf("Network controller:") != -1)
        {
            currentElement.remove(QString("Network controller:"), Qt::CaseSensitive);
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/places/network-workgroup.png"),currentElement, hwList);
        }
        else
        {
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/places/network-server-database.png"),currentElement, hwList);
        }
    }
    /** @todo warn when no devices were found*/
}

void MConfig::on_linuxDrvList_currentRowChanged(int currentRow )
{
    linuxDrvBlacklistPushButton->setEnabled(currentRow != -1);
    linuxDrvInstall->setEnabled(currentRow != -1);
    updateDriverStatus();
}

void MConfig::on_linuxDrvDiagnosePushButton_clicked()
{
    linuxDrvList->clear();
    //QStringList queryResult = getCmdOuts("lsmod | grep -i net");
    QStringList loadedKernelModules = getCmdOuts("lsmod");
    QStringList completeKernelNetModules = getCmdOuts("find /lib/modules/$(uname -r)/kernel/drivers/net -name *.ko");
    completeKernelNetModules = completeKernelNetModules.replaceInStrings(".ko", "");
    completeKernelNetModules = completeKernelNetModules.replaceInStrings(QRegExp("[\\w|\\.|-]*/"), "");
    // Those three kernel modules are in the "misc" section we add them manually
    // To the filter list for convenience
    completeKernelNetModules << "ndiswrapper";
    completeKernelNetModules << "atl2";
    completeKernelNetModules << "wl";
    for (int i = 0; i < loadedKernelModules.size(); ++i)
    {
        QString mod = loadedKernelModules.at(i);
        if (completeKernelNetModules.contains(mod.left(mod.indexOf(' '))))
        {
            new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/apps/ksysguardd.png"), mod, linuxDrvList);
        }
    }

    QFile inputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));
    inputBlacklist.open(QFile::ReadOnly|QFile::Text);

    QString driver;

    QString s;
    while (!inputBlacklist.atEnd())
    {
        s = inputBlacklist.readLine();
        QRegExp expr("^\\s*blacklist\\s*.*");
        if (expr.exactMatch(s)) {
            QString captured = expr.cap(0);
            captured.remove("blacklist");
            driver = captured.trimmed();
            QListWidgetItem *blacklisted = new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/apps/ksysguardd.png"), driver, linuxDrvList);
            blacklisted->setForeground(Qt::red);
        }
    }
    inputBlacklist.close();
}

//apName = curLine.split("\"").at(1);
//aditionalInfo += tr("Access Point=") + apName + "\n";


void MConfig::on_windowsDrvDiagnosePushButton_clicked()
{
    windowsDrvList->clear();
    QStringList queryResult = getCmdOuts("ndiswrapper -l");
    int i = 0;
    while (i < queryResult.size())
    {
        QString currentElement = queryResult.at(i);
        QString label = currentElement.left(currentElement.indexOf(":"));
        label.append(QApplication::tr("driver installed"));
        //label = currentElement.remove(": ");
        if ((i + 1) < queryResult.size())
        {
            if (!queryResult.at(i + 1).contains(": driver installed"))
            {
                QString installInfo = queryResult.at(i + 1);
                int infoPos = installInfo.indexOf(QRegExp("[\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F]:[\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F][\\d|A|B|C|D|E|F]"));
                //device (14E4:4320) present (alternate driver: bcm43xx)
                if (infoPos != -1)
                {
                    label.append(QApplication::tr(" and in use by "));
                    label.append(installInfo.midRef(infoPos, 9));
                }
                if (installInfo.contains("alternate driver"))
                {
                    infoPos = installInfo.lastIndexOf(": ");
                    if (infoPos != -1)
                    {
                        int strLen = installInfo.length();
                        label.append(QApplication::tr(". Alternate driver: "));
                        QString s =installInfo.right(strLen - infoPos);
                        s.remove(")");
                        s.remove(" ");
                        s.remove(":");
                        label.append(s);
                    }
                }
                i++;
            }
        }
        new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/apps/krfb.png"), label, windowsDrvList);
        i++;
    }
}

bool MConfig::blacklistModule(QString module)
{   
    QFile outputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));;
    if (!outputBlacklist.open(QFile::Append|QFile::Text))
    {
        return false;
    }

    outputBlacklist.write(QString("blacklist %1\n").arg(module).toAscii());
    outputBlacklist.close();

    if (removable(module))
    {
        if (!removeModule(module))
        {
            return false;
        }
    }
    return true;
}

void MConfig::on_linuxDrvBlacklistPushButton_clicked()
{         
    if (linuxDrvList->currentRow() != -1)
    {
        QListWidgetItem* currentDriver = linuxDrvList->currentItem();
        QString driver = currentDriver->text();
        driver = driver.left(driver.indexOf(" "));
        if (driverBlacklisted)
        {
            QFile inputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));
            QFile outputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));;
            if (!inputBlacklist.open(QFile::ReadOnly|QFile::Text))
            {
                return;
            }

            QString s, outputString("");
            while (!inputBlacklist.atEnd())
            {
                s = inputBlacklist.readLine();
                QString expr = QString("^\\s*(blacklist)\\s*(%1)\\s*").arg(driver);
                if (!s.contains(QRegExp(expr)))
                {
                    outputString += s;
                }
                outputBlacklist.write(s.toAscii());
            }
            inputBlacklist.close();
            if (!outputBlacklist.open(QFile::WriteOnly|QFile::Text))
            {
                return;
            }
            outputBlacklist.write(outputString.toAscii());
            outputBlacklist.close();
            QMessageBox::information(0, QApplication::tr("Driver removed from blacklist"),
                                     QApplication::tr("Driver removed from blacklist."));
            loadModule(driver);
            linuxDrvBlacklistPushButton->setText(QApplication::tr("Blacklist Driver"));
            driverBlacklisted = false;
        }
        else if (blacklistModule(driver))
        {
            QMessageBox::information(0, QString::null, QApplication::tr("Module blacklisted"));
            linuxDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist Driver"));
            driverBlacklisted = true;
        }
    }
    on_linuxDrvDiagnosePushButton_clicked();
}

// load module
bool MConfig::loadModule(QString module)
{
    QString cmd = QString("modprobe %1").arg(module);
    if (system(cmd.toAscii()) != 0)
    {
        QString msg = QObject::tr("Count not load ");
        msg += module;
        QMessageBox::information(0, QString::null, msg);
        return false;
    }
    return true;
}

// check if the module can be removed
bool MConfig::removable(QString module)
{
    QString cmd = QString("modprobe -rn %1").arg(module);
    if (system(cmd.toAscii()) != 0)
    {
        return false;
    }
    return true;
}


// remove module
bool MConfig::removeModule(QString module)
{
    QString cmd = QString("modprobe -r %1").arg(module);
    if (system(cmd.toAscii()) != 0)
    {
        QString msg = QObject::tr("Count not unload ");
        msg += module;
        QMessageBox::information(0, QString::null, msg);
        return false;
    }
    return true;
}

// install Linux Driver
bool MConfig::installModule(QString module)
{
    if (!loadModule(module))
    {
        return false;
    }
    if (module.compare("ndiswrapper") != 0)
    {
        QFile outputModules(QString("/etc/modules"));;
        if (!outputModules.open(QFile::Append|QFile::Text))
        {
            return false;
        }
    outputModules.write(QString("%1\n").arg(module).toAscii());
    outputModules.close();
    }
    return true;    
}


void MConfig::on_linuxDrvInstall_clicked()
{
    if (linuxDrvList->currentRow() != -1)
    {
        QListWidgetItem* currentDriver = linuxDrvList->currentItem();
        QString driver = currentDriver->text();
        driver = driver.left(driver.indexOf(" "));
        if (installModule(driver))
        {
            QMessageBox::information(0, QString::null, QApplication::tr("Installation successful"));
        }
    }
}



// run apt-get update and at the end start installNDIS
void MConfig::on_installNdiswrapper_clicked()
{
    setCursor(QCursor(Qt::BusyCursor));
    // enable testing repos
    system("sed -i -r '/testrepo/ s/^#+//' /etc/apt/sources.list.d/mepis.list");

    if (installProc->state() != QProcess::NotRunning)
    {
        installProc->kill();
    }
    installProc->start("apt-get update");
    installOutputEdit->show();
    installOutputEdit->resize(800, 600);
    // center output window
    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = (screenGeometry.width()-installOutputEdit->width()) / 2;
    int y = (screenGeometry.height()-installOutputEdit->height()) / 2;
    installOutputEdit->move(x, y);
    // hide main window
    this->hide();
    installOutputEdit->raise();
    disconnect(installProc, SIGNAL(readyReadStandardOutput()), 0, 0);
    connect(installProc, SIGNAL(readyReadStandardOutput()), this, SLOT(writeInstallOutput()));
    disconnect(installProc, SIGNAL(finished(int,QProcess::ExitStatus)), 0, 0);
    connect(installProc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(aptUpdateFinished()));
}

// install NDISwrapper
void MConfig::aptUpdateFinished()
{
    if (installProc->state() != QProcess::NotRunning)
    {
        installProc->kill();
    }
    installProc->start("apt-get install -y ndiswrapper-utils-1.9 ndiswrapper-dkms");
    disconnect(installProc, SIGNAL(readyReadStandardOutput()), 0, 0);
    connect(installProc, SIGNAL(readyReadStandardOutput()), this, SLOT(writeInstallOutput()));
    disconnect(installProc, SIGNAL(finished(int,QProcess::ExitStatus)), 0, 0);
    connect(installProc, SIGNAL(finished(int)), this, SLOT(installFinished(int)));
}

// finished ndiswrapper install
void MConfig::installFinished(int errorCode)
{
    installOutputEdit->close();
    this->show();
    setCursor(QCursor(Qt::ArrowCursor));
    if (errorCode == 0)
    {
        if (installModule("ndiswrapper"))
        {
            QMessageBox::information(0, QString::null, QApplication::tr("Installation successful"));
        }
        else
        {
            QMessageBox::information(0, QString::null, QApplication::tr("Error detected, could not compile ndiswrapper driver."));
        }
    }
    else
    {
        QMessageBox::information(0, QString::null, QApplication::tr("Error detected, could not install ndiswrapper."));
    }
    // disable testing repo
    system("sed -i -r '/testrepo/ s/^([^#])/#\\1/' /etc/apt/sources.list.d/mepis.list");
}

void MConfig::writeInstallOutput()
{
    QByteArray bytes = installProc->readAllStandardOutput();
    QStringList lines = QString(bytes).split("\n");

    foreach (QString line, lines) {
        if (!line.isEmpty())
        {
            installOutputEdit->append(line);
        }
    }
}


void MConfig::updateNdiswrapStatus()
{
    ndiswrapBlacklisted = false;
    QFile inputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));
    inputBlacklist.open(QFile::ReadOnly|QFile::Text);

    QString s;
    while (!inputBlacklist.atEnd())
    {
        s = inputBlacklist.readLine();
        if (s.contains(QRegExp("^\\s*(blacklist)\\s*(ndiswrapper)\\s*")))
        {
            ndiswrapBlacklisted = true;
            break;
        }
    }
    if (ndiswrapBlacklisted)
    {
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist NDISwrapper"));
    }
    else
    {
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Blacklist NDISwrapper"));
    }
    inputBlacklist.close();
}


void MConfig::updateDriverStatus()
{
    driverBlacklisted = false;
    QFile inputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));
    inputBlacklist.open(QFile::ReadOnly|QFile::Text);

    QString driver;

    if (linuxDrvList->currentRow() != -1)
    {
        QListWidgetItem* currentDriver = linuxDrvList->currentItem();
        driver = currentDriver->text();
        driver = driver.left(driver.indexOf(" "));
    }

    QString s;
    while (!inputBlacklist.atEnd())
    {
        s = inputBlacklist.readLine();
        QString expr = QString("^\\s*(blacklist)\\s*(%1)\\s*").arg(driver);
        if (s.contains(QRegExp(expr)))
        {
            driverBlacklisted = true;
            break;
        }
    }
    if (driverBlacklisted)
    {
        linuxDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist Driver"));
    }
    else
    {
        linuxDrvBlacklistPushButton->setText(QApplication::tr("Blacklist Driver"));
    }
    inputBlacklist.close();
}


void MConfig::on_windowsDrvBlacklistPushButton_clicked()
{
    QString module = "ndiswrapper";
    if (ndiswrapBlacklisted)
    {
        QFile inputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));
        QFile outputBlacklist(QString("/etc/modprobe.d/blacklist.conf"));;
        if (!inputBlacklist.open(QFile::ReadOnly|QFile::Text))
        {
            return;
        }

        QString s, outputString("");
        while (!inputBlacklist.atEnd())
        {
            s = inputBlacklist.readLine();
            if (!s.contains(QRegExp(QRegExp("^\\s*(blacklist)\\s*(ndiswrapper)\\s*"))))
            {
                outputString += s;
            }
            outputBlacklist.write(s.toAscii());
        }
        inputBlacklist.close();
        if (!outputBlacklist.open(QFile::WriteOnly|QFile::Text))
        {
            return;
        }
        outputBlacklist.write(outputString.toAscii());
        outputBlacklist.close();
        QMessageBox::information(0, QApplication::tr("NDISwrapper removed from blacklist"),
                                 QApplication::tr("NDISwrapper removed from blacklist."));
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Blacklist NDISwrapper"));
        ndiswrapBlacklisted = false;
        loadModule("ndiswrapper");
    }
    else
    {
        blacklistModule(module);
        QMessageBox::information(0, QApplication::tr("NDISwrapper blacklisted"),
                                 QApplication::tr("NDISwrapper blacklisted."));
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist NDISwrapper"));        
        ndiswrapBlacklisted = true;
        if (removable(module))
        {
            removeModule(module);
        }
    }
}

bool MConfig::checkSysFileExists(QDir searchPath, QString fileName, Qt::CaseSensitivity cs)
{
    QStringList fileList = searchPath.entryList(QStringList() << "*.SYS");
    bool found = false;
    QStringList::Iterator it = fileList.begin();
    while (it != fileList.end()) {
        if ((*it).contains(fileName, cs)) {
            found = true;
            break;
        }
        ++it;
    }
    return found;
}

void MConfig::on_windowsDrvAddPushButton_clicked()
{
    QString infFileName = QFileDialog::getOpenFileName(this,
                                                       tr("Locate the Windows driver you want to add"), "/home", tr("Windows installation information file (*.inf)"));
    if (!infFileName.isEmpty())
    {
        // Search in the inf file the name of the .sys file
        QFile infFile(infFileName);
        infFile.open(QFile::ReadOnly|QFile::Text);

        QString s;
        bool found = false;
        bool exist = false;
        QStringList foundSysFiles;
        QDir sysDir(infFileName);
        sysDir.cdUp();
        while ((!infFile.atEnd())) {
            s = infFile.readLine();
            if (s.contains("ServiceBinary",Qt::CaseInsensitive)) {
                s = s.right(s.length() - s.lastIndexOf('\\'));
                s = s.remove('\\');
                s = s.remove('\n');
                found = true;
                if (this->checkSysFileExists(sysDir, s, Qt::CaseInsensitive)) {
                    exist = true;
                }
                else {
                    foundSysFiles << s;
                }
            }
        }
        infFile.close();

        if (found) {
            if (!exist) {
                QMessageBox::warning(0, QString(tr("*.sys file not found")), tr("The *.sys files must be in the same location as the *.inf file. %1 cannot be found").arg(foundSysFiles.join(", ")));
            }
            else {
                QString cmd = QString("ndiswrapper -i %1").arg(infFileName);
                system(cmd.toAscii());
                cmd = QString("ndiswrapper -ma");
                system(cmd.toAscii());
                on_windowsDrvDiagnosePushButton_clicked();
            }
        }
        else {
            QMessageBox::critical(0, QString(tr("sys file reference not found")).arg(infFile.fileName()), tr("The sys file for the given driver cannot be determined after parsing the inf file"));
        }
    }
}

void MConfig::on_windowsDrvList_currentRowChanged(int row)
{
    windowsDrvRemovePushButton->setEnabled(row != -1);
}


void MConfig::on_windowsDrvRemovePushButton_clicked()
{
    if (windowsDrvList->currentRow() != -1)
    {
        QListWidgetItem* currentDriver = windowsDrvList->currentItem();
        QString driver = currentDriver->text();
        QString cmd = QString("ndiswrapper -r %1").arg(driver.left(driver.indexOf(" ")));
        system(cmd.toAscii());
        QMessageBox::information(0, QString::null, tr("Ndiswrapper driver removed."));
        on_windowsDrvDiagnosePushButton_clicked();
    }
}

void MConfig::on_generalHelpPushButton_clicked()
{
    displaySite("file:///usr/local/share/doc/mxapps.html#network");
}

void MConfig::on_tabWidget_currentChanged()
{
    int i = tabWidget->currentIndex();
    if (i != currentTab)
    {
        if (configurationChanges[currentTab])
        {
            configurationChanges[currentTab] = false;
        }
        currentTab = i;
    }

    refresh();
}


// close but do not apply
void MConfig::on_buttonCancel_clicked()
{
    close();
}


// About button clicked
void MConfig::on_buttonAbout_clicked()
{
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Broadcom Manager"), "<p align=\"center\"><b><h2>" +
                       tr("MX Network Assistant") + "</h2></b></p><p align=\"center\">MX14+git20140803</p><p align=\"center\"><h3>" +
                       tr("Program for troubleshooting and configuring network for antiX MX") + "</h3></p><p align=\"center\"><a href=\"http://www.mepiscommunity.org/mx\">http://www.mepiscommunity.org/mx</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MEPIS LLC and antiX") + "<br /><br /></p>", 0, this);
    msgBox.addButton(tr("License"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);
    if (msgBox.exec() == QMessageBox::AcceptRole)
        displaySite("file:///usr/share/doc/mx-network/license.html");
}


// pop up a window and display website
void MConfig::displaySite(QString site)
{
    QWidget *window = new QWidget(this, Qt::Dialog);
    window->setWindowTitle(this->windowTitle());
    window->resize(800, 500);
    QWebView *webview = new QWebView(window);
    webview->load(QUrl(site));
    webview->show();
    window->show();
}


