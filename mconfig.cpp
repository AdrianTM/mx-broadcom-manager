//
//   Copyright (C) 2003-2010 by Warren Woodford
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

#include "mconfig.h"
#include <QFileDialog>
#include <QFont>
#include <QPainter>
#include <QProcess>
#include <QTextStream>
#include <QMenu>
#include <QClipboard>
#include <QWhatsThis>

#include <unistd.h>


MConfig::MConfig(QWidget* parent)
    : QDialog(parent) {
    setupUi(this);
    setWindowIcon(QApplication::windowIcon());

    wifiChanges = 0;

    currentTab = 0;
    tabWidget->setCurrentIndex(0);

    configurationChanges[0] = false;
    configurationChanges[1] = false;
    configurationChanges[2] = false;
    configurationChanges[3] = false;
    configurationChanges[4] = false;

    pingProc  = new QProcess(this);
    traceProc = new QProcess(this);

    connect(hwList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForHw(const QPoint &)));
    connect(linuxDrvList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForLinuxDrv(const QPoint &)));
    connect(windowsDrvList, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(showContextMenuForWindowsDrv(const QPoint &)));
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

void MConfig::questionApplyChanges() 
{
    if (!buttonApply->isEnabled())
    {
        return;
    }

    int ret = QMessageBox::information(0, tr("Configuration has been modified"),
                                       tr("The configuration options have changed. Do you want to apply the changes?"),
                                       QMessageBox::Save | QMessageBox::Discard, QMessageBox::Save);

    if (ret == QMessageBox::Save)
    {
        setCursor(QCursor(Qt::WaitCursor));
        switch (currentTab)
        {
        case 1:
            // wireless
            applyWireless();
            break;

        case 2:
            // ifaces
            applyIface();
            break;

        default:
            // dns
            applyDns();
            break;
        }

        setCursor(QCursor(Qt::ArrowCursor));

        // disable button
        buttonApply->setEnabled(false);
    }
}

/////////////////////////////////////////////////////////////////////////
// common

void MConfig::refresh() {
    int i = tabWidget->currentIndex();

    switch (i) {
    case 1:
        // wireless
        refreshWireless();
        break;

    case 2:
        // ifaces
        refreshIface();
        break;

    case 3:
        updateNdiswrapStatus();
        break;

    case 4:
        break;

    default:
        // dns
        bool changed = configurationChanges[0];
        refreshDns();
        configurationChanges[0] = changed;
        on_refreshPushButton_clicked();
        break;
    }
}

/////////////////////////////////////////////////////////////////////////
// special


bool MConfig::wirelessExists(const char *net) {
    QString val = getCmdValue("iwconfig 2>&1", net, " \t", " ");
    if (val.isEmpty()) {
        return false;
    }
    return val.localeAwareCompare("no") != 0;
}

bool MConfig::netExists(const char *net) {
    QString val = getCmdValue("cat /proc/net/dev 2>&1", net, ":", " ");
    return !val.isEmpty();
}

bool MConfig::netIsUp(const char *net) {
    char line[130];
    sprintf(line, "grep -q '%s' /etc/network/run/ifstate 2>&1", net);
    return system(line) == 0;
}

bool MConfig::netUsesDhcp(const char *net) {
    char line[130];
    sprintf(line, "grep 'iface %s' /etc/network/interfaces", net);
    QString val = getCmdValue(line, "inet", " ", " ");
    return val.localeAwareCompare("dhcp") == 0;
}

void MConfig::refreshIface() {
    QString net = ifaceComboBox->currentText();
    QString cmd = "";
    QString val = "";

    if (netExists(net.toAscii())) {
        // iface exists
        nowCheckBox->setEnabled(true);
        if (netIsUp(net.toAscii())) {
            // iface is up
            cmd = QString("ifconfig | grep -A 1 '^%1' 2>&1").arg(net);
            val = getCmdValue(cmd.toAscii(), "inet ", ":", " ");
            if (val.isEmpty()) {
                statusLabel->setText(tr("Status: Started but failed to connect"));
            } else {
                val.prepend(tr("Status: Connected with IP "));
                statusLabel->setText(val);
            }
        } else {
            // iface is not up
            statusLabel->setText(tr("Status: Was not started"));
        }
    } else {
        // iface does not exist
        statusLabel->setText(tr("Status: Does not exist"));
        nowCheckBox->setEnabled(false);
        nowCheckBox->setChecked(false);
    }

    cmd = QString("grep 'auto %1' /etc/network/interfaces").arg(net);
    val = getCmdValue(cmd.toAscii(), "#", " ", " ");
    startCheckBox->setChecked(val.localeAwareCompare(net) != 0);
    cmd = QString("grep 'allow-hotplug %1' /etc/network/interfaces").arg(net);
    val = getCmdValue(cmd.toAscii(), "#", " ", " ");
    pluggedCheckBox->setChecked(val.localeAwareCompare(net) != 0);

    if (netUsesDhcp(net.toAscii())) {
        if (!dhcpButton->isChecked()) {
            dhcpButton->toggle();
        }
    } else {
        if (!staticButton->isChecked()) {
            staticButton->toggle();
        }
    }

    cmd = QString("grep -A 4 'iface %1' /etc/network/interfaces").arg(net);
    val = getCmdValue(cmd.toAscii(), "address", " ", " ");
    ipEdit->setText(val);
    val = getCmdValue(cmd.toAscii(), "netmask", " ", " ");
    netmaskEdit->setText(val);
    val = getCmdValue(cmd.toAscii(), "broadcast", " ", " ");
    bcastEdit->setText(val);
    val = getCmdValue(cmd.toAscii(), "gateway", " ", " ");
    gatewayEdit->setText(val);

    buttonApply->setEnabled(classicRadioButton->isChecked());
}

void MConfig::addAPs(QString cardIf, QString configuredAP)
{
    int APposition, insertedElements = 0;
    QStringList apList = getCmdOuts(QString("iwlist %1 scan").arg(cardIf));
    QString curLine("");
    QString signalStrength("");
    QString apName("");
    QString aditionalInfo("ADDITIONAL INFO\n\n");
    essidEditCombo->clear();
    int i = apList.indexOf(QRegExp("(\\t|\\s)*(Cell).*"));

    if (i != -1)
    {
        int nextCell = apList.indexOf(QRegExp("(\\t|\\s)*(Cell).*"), i + 1);
        while (i < apList.size())
        {
            curLine = apList.at(i);
            if (curLine.contains("ESSID:\""))
            {
                apName = curLine.split("\"").at(1);
                aditionalInfo += tr("Access Point=") + apName + "\n";
            }
            if (curLine.contains("Quality="))
            {
                signalStrength = curLine.left(curLine.indexOf("/"));
                signalStrength = signalStrength.right((signalStrength.length() - signalStrength.indexOf("=")) - 1);
                QString translatedString = curLine.right(curLine.length() - curLine.indexOf("Signal level"));
                aditionalInfo += translatedString.replace(QRegExp("^\\s*Signal level="), tr("Signal level=")) + "\n";
            }
            if (curLine.contains("Quality:"))
            {
                signalStrength = curLine.left(curLine.indexOf("/"));
                signalStrength = signalStrength.right((signalStrength.length() - signalStrength.indexOf(":")) - 1);
                QString translatedString = curLine.right(curLine.length() - curLine.indexOf("Signal level"));
                aditionalInfo += translatedString.replace(QRegExp("^\\s*Signal level:"), tr("Signal level=")) + "\n";
            }
            if (curLine.contains("Protocol:"))
            {
                aditionalInfo += curLine.replace(QRegExp("^\\s*Protocol:"), tr("Protocol=")) + "\n";
            }
            if (curLine.contains("Mode:"))
            {
                aditionalInfo += curLine.replace(QRegExp("^\\s*Mode:"), tr("Mode=")) + "\n";
            }
            if (curLine.contains("Frequency:"))
            {
                aditionalInfo += curLine.replace(QRegExp("^\\s*Frequency:"), tr("Frequency=")) + "\n";
            }
            if (curLine.contains("Encryption key:"))
            {
                aditionalInfo += curLine.replace(QRegExp("^\\s*Encryption key:"), tr("Encryption key=")) + "\n";
            }
            if (curLine.contains("IE:"))
            {
                aditionalInfo += curLine.replace(QRegExp("^\\s*IE:"), tr("Encryption protocol=")) + "\n";
            }

            if ((i == nextCell - 1) || ((nextCell == -1) && (i == (apList.size() - 1))))
            {
                if (!signalStrength.isEmpty())
                {
                    essidEditCombo->addItem(createSignalIcon(signalStrength),
                                            QString("%1").arg(apName),
                                            aditionalInfo);
                }
                else
                {
                    essidEditCombo->addItem(
                                QString("%1").arg(apName),
                                aditionalInfo);
                }
                ++insertedElements;
                signalStrength = "";
                aditionalInfo = tr("ADDITIONAL INFO\n\n");
                if (nextCell != -1)
                {
                    nextCell = apList.indexOf(QRegExp("(\\t|\\s)*(Cell).*"), i + 2);
                }
            }
            ++i;
        }
    }
    disconnect(essidEditCombo, SIGNAL(currentIndexChanged(int)), 0, 0);
    if (insertedElements > 0)
    {

        connect(essidEditCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(showAPinfo(int)));
    }
    if ((APposition = essidEditCombo->findText(configuredAP)) == -1)
    {
        QString defaultAPTip = "";
        if (configuredAP.isEmpty())
        {
            defaultAPTip = tr("Default Access Point not configured.  Please edit this entry or scroll down to see available networks");
        }
        else
        {
            defaultAPTip = tr("Default Access Point not found.  Please edit this entry or scroll down to see available networks");
        }
        essidEditCombo->insertItem(0,configuredAP);
        essidEditCombo->setCurrentIndex(0);
        APposition = 0;
        QWhatsThis::showText(essidEditCombo->mapToGlobal(QPoint(125,5)), defaultAPTip);
    }
    else
    {
        if (insertedElements > 0)
        {
            showAPinfo(essidEditCombo->currentIndex());
        }
    }

}

void MConfig::showAPinfo(int index)
{
    QVariant vari = essidEditCombo->itemData(index);
    QString car = vari.toString();
    essidEditCombo->setToolTip(car);
}

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
    
    //pingOutputEdit->append(QString(bytes));
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
                                 tr("Plese, fill the destination host field"),
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

void MConfig::on_scanAPPushButton_clicked() 
{
    if (!wifiIf.isEmpty())
    {
        addAPs(wifiIf, essidEditCombo->currentText());
    }
}

void MConfig::refreshWireless() {
    QString essid;
    QString mode;
    QString freq;
    QString key;
    QString keymode;
    QString nwid;
    uint i = 0;
    QString val;

    wifiIf = "";

    QString w(tr("Wireless Interfaces:"));
    if (wirelessExists("eth0")) {
        w.append(" eth0");
        wifiIf = "eth0";
    }
    if (wirelessExists("eth1")) {
        w.append(" eth1");
        wifiIf = "eth1";
    }
    if (wirelessExists("eth2")) {
        w.append(" eth2");
        wifiIf = "eth2";
    }
    if (wirelessExists("ath0")) {
        w.append(" ath0");
        wifiIf = "ath0";
    }
    if (wirelessExists("wlan0")) {
        w.append(" wlan0");
        wifiIf = "wlan0";
    }
    if (wirelessExists("ra0")) {
        w.append(" ra0");
        wifiIf = "ra0";
    }
    if (wirelessExists("rausb0")) {
        w.append(" rausb0");
        wifiIf = "rausb0";
    }
    if (wirelessExists("wifi0")) {
        w.append(" wifi0");
        wifiIf = "wifi0";
    }
    wirelessFoundLabel->setText(w);
    essidEditCombo->clear();
    wifiChanges = -2;
    // frequency
    freq = getCmdValue("grep 'FREQ=' /etc/mepis-network/wireless/default", "FREQ=", "\"", "\"");
    if (freq.isEmpty() || freq.localeAwareCompare("0") == 0) {
        freqEdit->setText("");
        freqCheckBox->setChecked(false);
    } else {
        i = freq.indexOf('G');
        freq.truncate(i);
        freqEdit->setText(freq.toAscii());
        freqCheckBox->setChecked(true);
    }

    // wpa ssid
    val = getCmdValue("grep 'WPA_SSID=' /etc/mepis-network/wireless/default", "WPA_SSID=", "\"", "\"");
    if (val.isEmpty()) {
        // no wpa ssid, assume wep or none mode
        // ssid
        val = getCmdValue("grep 'ESSID=' /etc/mepis-network/wireless/default", "ESSID=", "\"", "\"");
        essidEditCombo->insertItem(0,val);
        essidEditCombo->setCurrentIndex(0);

        // wep key
        key = getCmdValue("grep 'KEY=' /etc/mepis-network/wireless/default", "KEY=", "\"", "\"");
        keymode = getCmdValue("grep 'KEYMODE=' /etc/mepis-network/wireless/default", "KEYMODE=", "\"", "\"");
        if (key.isEmpty() || key.localeAwareCompare("off") == 0 || key.localeAwareCompare("any") == 0) {
            // no wep key, must be none mode
            keyEdit->setText("");
            noneRadioButton->setChecked(true);
            restrictedCheckBox->setChecked(false);
        } else {
            // wep key found, must be wep mode
            wepRadioButton->setChecked(true);
            if (key.startsWith("s:")) {
                // ascii key
                keyEdit->setText(key.mid(2).toAscii());
            } else {
                //could be either
                keyEdit->setText(key.toAscii());
            }
            if (keymode.isEmpty() || keymode.localeAwareCompare("open") == 0) {
                restrictedCheckBox->setChecked(false);
            } else {
                restrictedCheckBox->setChecked(true);
            }
        }
        // clear wpa options
        passEdit->setText("");
        hiddenCheckBox->setChecked(false);
    } else {
        // wpa ssid found, must be wpa mode
        essidEditCombo->insertItem(0,val);
        essidEditCombo->setCurrentIndex(0);
        wpaRadioButton->setChecked(true);
        // can't display unencrypted passphrase
        passEdit->setText("");
        val = getCmdValue("grep 'WPA_AP_SCAN=' /etc/mepis-network/wireless/default", "WPA_AP_SCAN=", "\"", "\"");
        if (val.isEmpty() || val.localeAwareCompare("2") != 0) {
            // unspecified or 1
            hiddenCheckBox->setChecked(false);
        } else {
            // 2 means hidden
            hiddenCheckBox->setChecked(true);
        }
        // clear wep options
        keyEdit->setText("");
    }
    buttonApply->setEnabled(classicRadioButton->isChecked());
}

bool MConfig::isDNSstatic()
{
    QFile dhcpConfigFile(QString("/etc/dhcp3/dhclient.conf"));
    dhcpConfigFile.open(QFile::ReadOnly|QFile::Text);

    bool requestFound = false;
    bool requestDNS   = false;
    QString s;
    while (!dhcpConfigFile.atEnd() && !requestDNS)
    {
        s = dhcpConfigFile.readLine();
        if (s.contains(QRegExp("^(request)")))
        {
            requestFound = true;
        }
        if (requestFound)
        {
            if (s.contains(QRegExp("(domain-name-servers)")))
            {
                requestDNS = true;
            }
            if (s.contains(QRegExp("(;)")))
            {
                requestFound = false;
            }
        };
    }
    dhcpConfigFile.close();
    return !requestDNS;
}

void MConfig::setStaticDNS()
{
    QFile inputDHCPConf(QString("/etc/dhcp3/dhclient.conf"));
    QFile outputDHCPConf(QString("/etc/dhcp3/dhclient.conf"));;
    bool requestFound = false;
    if (!inputDHCPConf.open(QFile::ReadOnly|QFile::Text))
    {
        return;
    }

    QString s, outputString("");
    while (!inputDHCPConf.atEnd())
    {
        s = inputDHCPConf.readLine();
        if (s.contains(QRegExp("(request)")))
        {
            requestFound = true;
        }
        if (requestFound)
        {
            s = s.remove(QRegExp("^#"));
            s = s.remove(QRegExp("(domain-name-servers)"));
            s = s.replace(QRegExp(",\\s*;"),";");
            s = s.replace(QRegExp(",\\s*,"),", ");
            outputString += s;
            if (s.contains(QRegExp("(;)")))
            {
                requestFound = false;
            }
        }
        else
        {
            outputString += s;
        }
    }
    inputDHCPConf.close();
    if (!outputDHCPConf.open(QFile::WriteOnly|QFile::Text))
    {
        return;
    }
    outputDHCPConf.write(outputString.toAscii());
    outputDHCPConf.close();
}

void MConfig::setDNSWithDNS()
{
    QFile inputDHCPConf(QString("/etc/dhcp3/dhclient.conf"));
    QFile outputDHCPConf(QString("/etc/dhcp3/dhclient.conf"));;
    bool requestFound = false;
    bool requestDNSFound = false;
    if (!inputDHCPConf.open(QFile::ReadOnly|QFile::Text))
    {
        return;
    }

    QString s, outputString("");
    while (!inputDHCPConf.atEnd())
    {
        s = inputDHCPConf.readLine();
        if (s.contains(QRegExp("(request)")))
        {
            requestFound = true;
        }
        if (requestFound)
        {
            s = s.remove(QRegExp("^#")).remove(QRegExp("(domain-name-servers),?"));
            if (s.contains("domain-name-servers"))
            {
                requestDNSFound = true;;
            }
            if (s.contains(QRegExp("(;)")))
            {
                if (!requestDNSFound)
                {
                    s = s.replace(";",", domain-name-servers;");
                }
                requestFound = false;
            }
        }
        outputString += s;
    }
    inputDHCPConf.close();
    if (!outputDHCPConf.open(QFile::WriteOnly|QFile::Text))
    {
        return;
    }
    outputDHCPConf.write(outputString.toAscii());
    outputDHCPConf.close();
}

void MConfig::refreshDns() {
    QString val = getCmdValue("grep 'timeout' /etc/dhcp3/dhclient.conf", "timeout", " ", " ;");
    dhcpTimeoutSpinBox->setValue(val.toInt());

    if (!this->isDNSstatic()) {
        if (!dnsDhcpButton->isChecked()) {
            dnsDhcpButton->toggle();
        }
    } else {
        if (!dnsStaticButton->isChecked()) {
            dnsStaticButton->toggle();
        }
    }
    QStringList vals = getCmdValues("grep '^nameserver' /etc/resolv.conf", "nameserver", " ", " ");
    if (vals.empty()) {
        primaryDnsEdit->setText("");
        secondaryDnsEdit->setText("");
    } else {
        QStringList::iterator it = vals.begin();
        val = (*it);
        primaryDnsEdit->setText(val);
        ++it;
        if (it != vals.end()) {
            val = (*it);
            secondaryDnsEdit->setText(val);
        } else {
            secondaryDnsEdit->setText("");
        }
    }
    val = getCmdOut("grep '^iface eth0' /etc/network/interfaces");
    classicRadioButton->setChecked(!val.isEmpty());

    val = getCmdValue("dpkg -s network-manager-kde | grep '^Status'", "ok", " ", " ");
    if (val.compare("installed") == 0) {
        autoRadioButton->setEnabled(true);
    } else {
        autoRadioButton->setEnabled(false);
    }

    val = getCmdOut("grep '^install b43' /etc/modprobe.d/b43.conf");
    if (val.isEmpty()) {
        b43CheckBox->setChecked(true);
    } else {
        b43CheckBox->setChecked(false);
    }
    val = getCmdOut("grep '^install wl' /etc/modprobe.d/b43.conf");
    if (val.isEmpty()) {
        wlCheckBox->setChecked(true);
    } else {
        wlCheckBox->setChecked(false);
    }
    val = getCmdOut("grep '^install ndiswrapper' /etc/modprobe.d/b43.conf");
    if (val.isEmpty()) {
        ndisCheckBox->setChecked(true);
    } else {
        ndisCheckBox->setChecked(false);
    }
    val = getCmdOut("grep '^net.ipv6.conf.all.disable_ipv6=1' /etc/sysctl.conf");
    if (val.isEmpty()) {
        ipv6CheckBox->setChecked(true);
    } else {
        ipv6CheckBox->setChecked(false);
    }
    // ufw firewall enabler
    val = getCmdValue("dpkg -s ufw | grep '^Status'", "ok", " ", " ");
    if (val.compare("installed") == 0) {
        //ufw is installed
        fwCheckBox->setEnabled(true);
        val = getCmdOut("grep '^ENABLED=no' /etc/ufw/ufw.conf");
        if (val.isEmpty()) {
            fwCheckBox->setChecked(true);
        } else {
            // ufw not enabled
            fwCheckBox->setChecked(false);
        }
    } else {
        //ufw not installed
        fwCheckBox->setChecked(false);
        fwCheckBox->setEnabled(false);
    }
    buttonApply->setEnabled(true);
}

void MConfig::applyWireless() {
    QString cmd;
    // wireless
    if (wpaRadioButton->isChecked()) {
        // wpa mode
        if (!essidEditCombo->currentText().isEmpty()) {
            // has a ssid
            cmd = QString("WPA_SSID=\"%1\"").arg(essidEditCombo->currentText());
            replaceStringInFile("WPA_SSID=.*", cmd, "/etc/mepis-network/wireless/default");
        } else {
            // no specifed ssid, error
            QMessageBox::critical(0, QString::null,
                                  tr("With WPA encryption, you must specify a SSID."));
            return;
        }
        if (!passEdit->text().isEmpty()) {
            // has a passphrase
            cmd = QString("/usr/bin/wpa_passphrase %1 %2 | grep '[^#]psk'").arg(essidEditCombo->currentText()).arg(passEdit->text());
            QString val = getCmdValue(cmd, "psk", "=", " ");
            if (!val.isEmpty()) {
                cmd = QString("WPA_PSK=\"%1\"").arg(val);
                replaceStringInFile("WPA_PSK=.*", cmd, "/etc/mepis-network/wireless/default");
            } else {
                QMessageBox::critical(0, QString::null,
                                      tr("Failed to generate encryption key.  Please check the SSID and passphrase values."));
                return;
            }
        } else {
            // no specifed passphrase, error
            QMessageBox::critical(0, QString::null,
                                  tr("With WPA encryption, you must specify a passphrase."));
            return;
        }

        if (hiddenCheckBox->isChecked()) {
            // the ssid is hidden
            replaceStringInFile("WPA_AP_SCAN=.*", "WPA_AP_SCAN=\"2\"", "/etc/mepis-network/wireless/default");
        } else {
            replaceStringInFile("WPA_AP_SCAN=.*", "WPA_AP_SCAN=\"\"", "/etc/mepis-network/wireless/default");
        }
        // allow wpa 1 and wpa2
        replaceStringInFile("WPA_KEY_MGMT=.*", "WPA_KEY_MGMT=\"WPA-PSK\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_PROTO=.*", "WPA_PROTO=\"WPA RSN\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_PAIRWISE=.*", "WPA_PAIRWISE=\"TKIP CCMP\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_GROUP=.*", "WPA_GROUP=\"TKIP CCMP\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("ESSID=.*", "ESSID=\"\"", "/etc/mepis-network/wireless/default");
    } else {
        // not wpa mode
        if (!essidEditCombo->currentText().isEmpty()) {
            // has a ssid
            cmd = QString("ESSID=\"%1\"").arg(essidEditCombo->currentText());
            replaceStringInFile("ESSID=.*", cmd, "/etc/mepis-network/wireless/default");
        } else {
            // no specifed ssid
            replaceStringInFile("ESSID=.*", "ESSID=\"any\"", "/etc/mepis-network/wireless/default");
        }
        // clear wpa values
        replaceStringInFile("WPA_SSID=.*", "WPA_SSID=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_KEY_MGMT=.*", "WPA_KEY_MGMT=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_PROTO=.*", "WPA_PROTO=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_PAIRWISE=.*", "WPA_PAIRWISE=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_GROUP=.*", "WPA_GROUP=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_AP_SCAN=.*", "WPA_AP_SCAN=\"\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("WPA_PSK=.*", "WPA_PSK=\"\"", "/etc/mepis-network/wireless/default");
    }
    if (wepRadioButton->isChecked()) {
        // wep mode
        QString val = QString(keyEdit->text());
        if (val.startsWith("s:") || val.length() == 10 || val.length() == 26 || val.length() == 32 || val.length() == 56) {
            // it's a hex number or a forced ascii string
            cmd = QString("KEY=\"%1\"").arg(val);
        } else if (val.length() == 5 || val.length() == 13 || val.length() == 16 || val.length() == 29) {
            // it's ascii
            cmd = QString("KEY=\"s:%1\"").arg(val);
        } else {
            QMessageBox::critical(0, QString::null,
                                  tr("The encryption key appears to be incorrect. The size and value of the key is determined by the configuration of your Access Point. ASCII keys may be 5, 13, 16 or 29 characters long. Optionally, 's:' may be added in front of an ASCII key. Hex keys may be 10, 26, 32 or 56 hex digits."));
            return;
        }
        replaceStringInFile("KEY=.*", cmd, "/etc/mepis-network/wireless/default");
        if (restrictedCheckBox->isChecked()) {
            replaceStringInFile("KEYMODE=.*", "KEYMODE=\"restricted\"", "/etc/mepis-network/wireless/default");
        } else {
            replaceStringInFile("KEYMODE=.*", "KEYMODE=\"open\"", "/etc/mepis-network/wireless/default");
        }
    } else {
        // not wep mode
        replaceStringInFile("KEY=.*", "KEY=\"off\"", "/etc/mepis-network/wireless/default");
        replaceStringInFile("KEYMODE=.*", "KEYMODE=\"\"", "/etc/mepis-network/wireless/default");
    }
    if (freqCheckBox->isChecked()) {
        cmd = QString("FREQ=\"%1G\"").arg(freqEdit->text().section(" ", 0, 0));
        replaceStringInFile("FREQ=.*", cmd, "/etc/mepis-network/wireless/default");
    } else {
        replaceStringInFile("FREQ=.*", "FREQ=\"\"", "/etc/mepis-network/wireless/default");
    }
    QMessageBox::information(0, QString::null,
                             tr("The configuration has been updated. The changes should take effect if you restart the active Interface, but it may be necessary to reboot."));

    refreshWireless();
}

void MConfig::applyIface() {
    QString net = ifaceComboBox->currentText();
    int idx = ifaceComboBox->currentIndex();
    QString end = QString("ABOVE");
    if (idx+1 < ifaceComboBox->count()) {
        // more to go
        end = ifaceComboBox->itemText(idx+1);
    }
    QString val, val2;

    val = QString("#auto %1").arg(net);
    if (startCheckBox->isChecked()) {
        val2 = QString("auto %1").arg(net);
        replaceStringInFile(val, val2, "/etc/network/interfaces");
    } else {
        val2 = QString("^auto %1").arg(net);
        replaceStringInFile(val2, val, "/etc/network/interfaces");
    }
    val = QString("#allow-hotplug %1").arg(net);
    if (pluggedCheckBox->isChecked()) {
        val2 = QString("allow-hotplug %1").arg(net);
        replaceStringInFile(val, val2, "/etc/network/interfaces");
    } else {
        val2 = QString("^allow-hotplug %1").arg(net);
        replaceStringInFile(val2, val, "/etc/network/interfaces");
    }
    val = QString("iface %1 inet static").arg(net);
    val2 = QString("iface %1 inet dhcp").arg(net);
    if (dhcpButton->isChecked()) {
        // dhcp
        replaceStringInFile(val, val2, "/etc/network/interfaces");
    } else {
        // static
        replaceStringInFile(val2, val, "/etc/network/interfaces");

        val = QString("sed -i '/%1/,/%2/s/address .*/address %3/' /etc/network/interfaces").arg(net).arg(end).arg(ipEdit->text());
        system(val.toAscii());
        val = QString("sed -i '/%1/,/%2/s/netmask .*/netmask %3/' /etc/network/interfaces").arg(net).arg(end).arg(netmaskEdit->text());
        system(val.toAscii());
        val = QString("sed -i '/%1/,/%2/s/gateway .*/gateway %3/' /etc/network/interfaces").arg(net).arg(end).arg(gatewayEdit->text());
        system(val.toAscii());
        val = QString("sed -i '/%1/,/%2/s/broadcast .*/broadcast %3/' /etc/network/interfaces").arg(net).arg(end).arg(bcastEdit->text());
        system(val.toAscii());
    }
    if(nowCheckBox->isChecked()) {
        QMessageBox::information(0, QString::null,
                                 tr("The configuration has been updated. The Interface will be restarted automaticaly but it may be necessary that you reboot.  This may take up to a minulte."));
        val = QString("ifdown %1").arg(net);
        system(val.toAscii());
        system("skill KILL wpa_supplicant");
        val = QString("ifup %1").arg(net);
        system(val.toAscii());
    } else {
        QMessageBox::information(0, QString::null,
                                 tr("The configuration has been updated. The changes\n"
                                    "will take effect the next time you start the Interface."));
    }
    refreshIface();
}


void MConfig::applyDns() {
    QString val;

    // misc
    replaceStringInFile("^#timeout", "timeout", "/etc/dhcp3/dhclient.conf");
    QString cmd = QString("sed -i 's/^timeout .*/timeout %1;/' /etc/dhcp3/dhclient.conf").arg(dhcpTimeoutSpinBox->text().section(" ", 0, 0));
    system(cmd.toAscii());

    // dns
    if (dnsDhcpButton->isChecked() || autoRadioButton->isChecked()) {
        // dhcp
        if (isDNSstatic()) {
            // enable
            this->setDNSWithDNS();
        }
    } else {
        // static
        //val = getCmdOut("grep 'request domain-name-servers' /etc/dhcp3/dhclient.conf");
        //if (val.contains("domain-name-servers")) {
        // disable
        this->setStaticDNS();
        //replaceStringInFile("domain-name-servers,", "", "/etc/dhcp3/dhclient.conf");
        //}
        if (!primaryDnsEdit->text().isEmpty()) {
            cmd = QString("echo \"nameserver %1\" | cat > /etc/resolv.conf").arg(primaryDnsEdit->text());
            system(cmd.toAscii());
            if (!secondaryDnsEdit->text().isEmpty()) {
                cmd = QString("echo \"nameserver %1\" | cat >> /etc/resolv.conf").arg(secondaryDnsEdit->text());
                system(cmd.toAscii());
            }
        } else if (!secondaryDnsEdit->text().isEmpty()) {
            cmd = QString("echo \"nameserver %1\" | cat > /etc/resolv.conf").arg(secondaryDnsEdit->text());
            system(cmd.toAscii());
        }
    }

    if (b43CheckBox->isChecked()) {
        replaceStringInFile("^install b43.*", "#install b43 true", "/etc/modprobe.d/b43.conf");
    } else {
        replaceStringInFile("^#install b43.*", "install b43 true", "/etc/modprobe.d/b43.conf");
    }
    if (wlCheckBox->isChecked()) {
        replaceStringInFile("^install wl.*", "#install wl true", "/etc/modprobe.d/b43.conf");
    } else {
        replaceStringInFile("^#install wl.*", "install wl true", "/etc/modprobe.d/b43.conf");
    }
    if (ndisCheckBox->isChecked()) {
        replaceStringInFile("^install ndis.*", "#install ndiswrapper true", "/etc/modprobe.d/b43.conf");
    } else {
        replaceStringInFile("^#install ndis.*", "install ndiswrapper true", "/etc/modprobe.d/b43.conf");
    }
    if (ipv6CheckBox->isChecked()) {
        replaceStringInFile(".*net.ipv6.conf.all.disable_ipv6.*", "#net.ipv6.conf.all.disable_ipv6=1", "/etc/sysctl.conf");
    } else {
        replaceStringInFile(".*net.ipv6.conf.all.disable_ipv6.*", "net.ipv6.conf.all.disable_ipv6=1", "/etc/sysctl.conf");
    }

    if (fwCheckBox->isEnabled()) {
        if (fwCheckBox->isChecked()) {
            system("/usr/sbin/ufw enable");
        } else {
            system("/usr/sbin/ufw disable");
        }
    }

    QMessageBox::information(0, QString::null,
                             tr("Attempting to restart network related subsystems.  If this doesn't work, you will need to reboot! "));
    system("/etc/init.d/network-manager stop");
    system("/etc/init.d/networking stop");
    if (classicRadioButton->isChecked()) {
        // make sure data enabled in interfaces
        system("sed -i 's/#iface/iface/' /etc/network/interfaces");
        system("sed -i 's/#  a/  a/' /etc/network/interfaces");
        system("sed -i 's/#  n/  n/' /etc/network/interfaces");
        system("sed -i 's/#  b/  b/' /etc/network/interfaces");
        system("sed -i 's/#  g/  g/' /etc/network/interfaces");
        system("/etc/init.d/networking start");
    } else {
        // make sure interfaces are inactive
        system("sed -i 's/^auto e/#auto e/' /etc/network/interfaces");
        system("sed -i 's/^allow-hotplug e/#allow-hotplug e/' /etc/network/interfaces");
        system("sed -i 's/^iface e/#iface e/' /etc/network/interfaces");
        system("sed -i 's/^auto a/#auto a/' /etc/network/interfaces");
        system("sed -i 's/^allow-hotplug a/#allow-hotplug a/' /etc/network/interfaces");
        system("sed -i 's/^iface a/#iface a/' /etc/network/interfaces");
        system("sed -i 's/^auto w/#auto w/' /etc/network/interfaces");
        system("sed -i 's/^allow-hotplug w/#allow-hotplug w/' /etc/network/interfaces");
        system("sed -i 's/^iface w/#iface w/' /etc/network/interfaces");
        system("sed -i 's/^auto r/#auto r/' /etc/network/interfaces");
        system("sed -i 's/^allow-hotplug r/#allow-hotplug r/' /etc/network/interfaces");
        system("sed -i 's/^iface r/#iface r/' /etc/network/interfaces");
        system("sed -i 's/^  a/#  a/' /etc/network/interfaces");
        system("sed -i 's/^  n/#  n/' /etc/network/interfaces");
        system("sed -i 's/^  b/#  b/' /etc/network/interfaces");
        system("sed -i 's/^  g/#  g/' /etc/network/interfaces");
        system("/etc/init.d/network-manager start");
    }

    refreshDns();

}

/////////////////////////////////////////////////////////////////////////
// slots

void MConfig::show() {
    QDialog::show();
    refresh();
    // try to pre-aim iface
    if (wirelessExists("eth0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("eth0"));
    } else if (wirelessExists("eth1")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("eth1"));
    } else if (wirelessExists("eth2")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("eth2"));
    } else if (wirelessExists("ath0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("ath0"));
    } else if (wirelessExists("wlan0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("wlan0"));
    } else if (wirelessExists("ra0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("ra0"));
    } else if (wirelessExists("rausb0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("rausb0"));
    } else if (wirelessExists("wifi0")) {
        ifaceComboBox->setCurrentIndex(ifaceComboBox->findText("wifi0"));
    }
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

QPixmap MConfig::createSignalIcon(QString strength)
{
    int signalStrength = strength.toUInt() ;
    //QPixmap iconOverlay = QPixmap("/usr/share/icons/nuvola_platin_me/16x16/actions/blend.png");
    QPixmap iconOverlay(30,22);
    QPainter p(&iconOverlay);   // create a QPainter for it
    //p.rotate(90); // rotate the painter
    p.drawPixmap(0, 0, iconOverlay); // and draw your original pixmap on it
    p.setBrush(Qt::white);
    p.setPen(Qt::white);
    p.drawRect(0, 0, 30, 22);
    p.setRenderHints(QPainter::SmoothPixmapTransform |
                     QPainter::TextAntialiasing      |
                     QPainter::Antialiasing, true);

    qreal coordX = 5.0 + (signalStrength*20.0)/100.0;
    QPointF points[4] = {QPointF(0.0, 21.0),
                         QPointF(0.0, 18.0),
                         QPointF(coordX, 19.0 + ((-18.0/30.0)*coordX)),
                         QPointF(coordX, 21.0)};

    if (signalStrength >= 66)
    {
        p.setBrush(QColor(0, 255, 0, 64));
        p.setPen(QColor(0, 255, 0, 128));
    }
    else
    {
        if (signalStrength >= 33)
        {
            p.setBrush(QColor(255, 255, 0, 64));
            p.setPen(QColor(255, 255, 0, 128));
        }
        else
        {
            p.setBrush(QColor(255, 0, 0, 64));
            p.setPen(QColor(255, 0, 0, 128));
        }
    }
    //p.drawRect(0, 0, (signalStrength*30)/100 , 22);
    p.drawPolygon(points, 4);
    p.setBrush(Qt::black);
    p.setPen(Qt::black);
    QFont currentFont = p.font();
    currentFont.setPixelSize(12);
    p.setFont(currentFont);
    p.drawText(0, 15, QString("%1%").arg(strength));
    p.end();
    return iconOverlay;
}

void MConfig::on_refreshPushButton_clicked() 
{
    statusList->clear();
    internetConnection  = false;
    // Query network interface status
    QStringList interfaceList  = getCmdOuts("ifconfig -a -s");
    int i=1;
    while (i<interfaceList.size())
    {
        QString interface = interfaceList.at(i);
        interface = interface.left(interface.indexOf(" "));
        if ((interface != "lo") && !interface.contains("nr") && !interface.contains("rose") && !interface.contains("wmaster") && !interface.contains("wifi0"))
        {
            QStringList ifStatus  = getCmdOuts(QString("ifconfig %1").arg(interface));
            QString unwrappedList = ifStatus.join(" ");
            if (unwrappedList.indexOf("UP ") != -1)
            {
                if (unwrappedList.indexOf(" RUNNING ") != -1)
                {
                    QListWidgetItem* item = new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/status/user-online.png"), interface, statusList);
                    item->setToolTip(tr("Running"));
                    internetConnection  = true;
                }
                else
                {
                    QListWidgetItem* item = new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/status/user-offline.png"), interface, statusList);
                    item->setToolTip(tr("Not Running"));
                }
            }
            else
            {
                QListWidgetItem* item = new QListWidgetItem(QIcon("/usr/share/icons/default.kde4/16x16/status/user-busy.png"), interface, statusList);
                item->setToolTip(tr("Not Available"));
            }
        }
        ++i;
    }
    statusList->sortItems();
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
}

void MConfig::on_linuxDrvDiagnosePushButton_clicked() 
{
    linuxDrvList->clear();
    //QStringList queryResult = getCmdOuts("lsmod | grep -i net");
    QStringList loadedKernelModules = getCmdOuts("lsmod");
    QStringList completeKernelNetModules = getCmdOuts("/sbin/modprobe -lt drivers/net");
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

    outputBlacklist.write(QString("blacklist %1\n\n").arg(module).toAscii());
    outputBlacklist.close();
    return true;
}

void MConfig::on_linuxDrvBlacklistPushButton_clicked() 
{
    if (linuxDrvList->currentRow() != -1)
    {
        QListWidgetItem* currentDriver = linuxDrvList->currentItem();
        QString driver = currentDriver->text();
        driver = driver.left(driver.indexOf(" "));
        if (driver.compare("ndiswrapper") == 0 )
        {
            windowsDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist NDISwrapper"));
            ndiswrapBlacklisted = true;
        }
        if (blacklistModule(driver))
        {
            QMessageBox::information(0, QString::null, QApplication::tr("Module blacklisted"));
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

void MConfig::on_windowsDrvBlacklistPushButton_clicked() 
{
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
                                 QApplication::tr("NDISwrapper removed from blacklist.  You must reboot for the changes to take effect "));
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Blacklist NDISwrapper"));
        ndiswrapBlacklisted = false;
    }
    else
    {
        blacklistModule("ndiswrapper");
        QMessageBox::information(0, QApplication::tr("NDISwrapper blacklisted"),
                                 QApplication::tr("NDISwrapper blacklisted.  You must reboot for the changes to take effect "));
        windowsDrvBlacklistPushButton->setText(QApplication::tr("Unblacklist NDISwrapper"));
        ndiswrapBlacklisted = true;
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
    QString manualPackage = tr("mepis-manual");
    QString statusl = getCmdValue(QString("dpkg -s %1 | grep '^Status'").arg(manualPackage).toAscii(), "ok", " ", " ");
    if (statusl.compare("installed") != 0) {
        if (internetConnection) {
            setCursor(QCursor(Qt::WaitCursor));
            int ret = QMessageBox::information(0, tr("The Mepis manual is not installed"),
                                               tr("The Mepis manual is not installed, do you want to install it now?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (ret == QMessageBox::Yes) {
                system(QString("apt-get install -qq %1").arg(manualPackage).toAscii());
                setCursor(QCursor(Qt::ArrowCursor));
                statusl = getCmdValue(QString("dpkg -s %1 | grep '^Status'").arg(manualPackage).toAscii(), "ok", " ", " ");
                if (statusl.compare("installed") != 0) {
                    QMessageBox::information(0, tr("The Mepis manual hasn't been installed"),
                                             tr("The Mepis manual cannot be installed. This may mean you are using the LiveCD or you are unable to reach the software repository,"),
                                             QMessageBox::Ok);

                }
            }
            else {
                setCursor(QCursor(Qt::ArrowCursor));
                return;
            }
        }
        else {
            QMessageBox::information(0, tr("The Mepis manual is not installed"),
                                     tr("The Mepis manual is not installed and no Internet connection could be detected so it cannot be installed"),
                                     QMessageBox::Ok);
            return;
        }
    }
    QString page;
    switch (tabWidget->currentIndex())
    {
    case 0:
        page = tr("file:///usr/share/mepis-manual/en/index.html#s05-3-1_general-tab");
        break;
    case 1:
        page = tr("file:///usr/share/mepis-manual/en/index.html#s05-3-1_wireless-tab");
        break;
    case 2:
        page = tr("file:///usr/share/mepis-manual/en/index.html#s05-3-1_interfaces-tab");
        break;
    case 3:
        page = tr("file:///usr/share/mepis-manual/en/index.html#s05-3-1_troubleshooting-tab");
        break;
    case 4:
        page = tr("file:///usr/share/mepis-manual/en/index.html#s05-3-1_diagnosis-tab");
        break;
    default:
        page = tr("file:///usr/share/mepis-manual/en/index.html#section05-3-1");
        break;
    }
    //executeChild("/usr/bin/konqueror", page.toAscii());
    executeChild("/etc/alternatives/x-www-browser", page.toAscii());
}

void MConfig::on_stopPushButton_clicked() {
    system("/etc/init.d/network-manager stop");
    system("/etc/init.d/networking stop");
    on_refreshPushButton_clicked();
    QMessageBox::information(0, QString::null,
                             tr("The network was completely stopped. To restart the network you can click the Re/start button or reboot the system."));
}

void MConfig::on_startPushButton_clicked() {
    QMessageBox::information(0, QString::null,
                             tr("Attempting to completely restart the network.  This may take up to one minute. Please be patient."));
    system("/etc/init.d/network-manager stop");
    system("/etc/init.d/networking stop");
    if (classicRadioButton->isChecked()) {
        system("/etc/init.d/networking start");
    } else {
        system("/etc/init.d/network-manager start");
    }
    on_refreshPushButton_clicked();
}

void MConfig::on_ndisCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_b43CheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_wlCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_ipv6CheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_fwCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_startCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_pluggedCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[2] = true;
}

void MConfig::on_nowCheckBox_clicked() {
    buttonApply->setEnabled(true);
}

void MConfig::on_dhcpButton_clicked() {
    buttonApply->setEnabled(true);
}

void MConfig::on_staticButton_clicked() {
    buttonApply->setEnabled(true);
}

void MConfig::on_ipEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[2] = true;
}

void MConfig::on_netmaskEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[2] = true;
}

void MConfig::on_gatewayEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[2] = true;
}

void MConfig::on_bcastEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[2] = true;
}

void MConfig::on_essidEditCombo_editTextChanged() {
    ++wifiChanges;
    buttonApply->setEnabled(true);
}

void MConfig::on_essidEditCombo_currentIndexChanged() {
    ++wifiChanges;
    buttonApply->setEnabled(true);
}

void MConfig::on_keyEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_freqEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_noneRadioButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_wepRadioButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_wpaRadioButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_hiddenCheckBox_clicked() {
    buttonApply->setEnabled(true);
}

void MConfig::on_restrictedCheckBox_clicked() {
    buttonApply->setEnabled(true);
}

void MConfig::on_freqCheckBox_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[1] = true;
}

void MConfig::on_dnsDhcpButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_dnsStaticButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_dhcpTimeoutSpinBox_valueChanged() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_primaryDnsEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_secondaryDnsEdit_textEdited() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_autoRadioButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_classicRadioButton_clicked() {
    buttonApply->setEnabled(true);
    configurationChanges[0] = true;
}

void MConfig::on_ifaceComboBox_activated() {
    refresh();
}

void MConfig::on_tabWidget_currentChanged() 
{
    int i = tabWidget->currentIndex();
    configurationChanges[1] = configurationChanges[1] || (wifiChanges > 0);
    wifiChanges = -2;
    if (i != currentTab)
    {
        if (configurationChanges[currentTab])
        {
            questionApplyChanges();
            configurationChanges[currentTab] = false;
        }
        currentTab = i;
    }

    refresh();
}

// apply but do not close
void MConfig::on_buttonApply_clicked() {
    if (!buttonApply->isEnabled()) {
        return;
    }
    setCursor(QCursor(Qt::WaitCursor));

    int i = tabWidget->currentIndex();
    switch (i) {
    case 1:
        // wireless
        applyWireless();
        break;

    case 2:
        // ifaces
        applyIface();
        break;

    default:
        // dns
        applyDns();
        break;
    }

    setCursor(QCursor(Qt::ArrowCursor));

    // disable button
    buttonApply->setEnabled(false);
}

// close but do not apply
void MConfig::on_buttonCancel_clicked() {
    close();
}

// apply then close
void MConfig::on_buttonOk_clicked() {
    on_buttonApply_clicked();
    close();
}

// show about
void MConfig::on_buttonAbout_clicked() {
    QMessageBox::about(this, tr("About"),
                       tr("<p><b>MEPIS Network Assistant</b></p>"
                          "<p>Copyright (C) 2003-10 by MEPIS LLC.  All rights reserved.</p>"));
}



