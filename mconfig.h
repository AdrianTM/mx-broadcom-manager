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

#ifndef MCONFIG_H
#define MCONFIG_H

#include "ui_meconfig.h"
#include <QTreeView>
#include <QCheckBox>
#include <QMessageBox>
#include <QComboBox>
#include <QCursor>
#include <QRegExp>
#include <QFile>
#include <QLineEdit>
#include <QIcon>
#include <QHeaderView>
#include <QProcess>
#include <QDir>

class MConfig : public QDialog, public Ui::MEConfig {
    Q_OBJECT

public:
    MConfig(QWidget* parent = 0);
    ~MConfig();
    // helpers
    static QString getCmdOut(QString cmd);
    static QStringList getCmdOuts(QString cmd);
    static QString getCmdValue(QString cmd, QString key, QString keydel, QString valdel);
    static QStringList getCmdValues(QString cmd, QString key, QString keydel, QString valdel);
    static bool replaceStringInFile(QString oldtext, QString newtext, QString filepath);
    // common
    void refresh();
    // special
    bool wirelessExists(const char* net);
    bool netExists(const char* net);
    bool netIsUp(const char* net);
    bool netUsesDhcp(const char *net);
    void refreshIface();
    void refreshWireless();
    void refreshDns();
    void refreshStatus();
    void applyWireless();
    void applyIface();
    void applyDns();
    void questionApplyChanges();
    bool checkSysFileExists(QDir searchPath, QString fileName, Qt::CaseSensitivity cs);
public slots:
    virtual void show();

    virtual void on_ndisCheckBox_clicked();
    virtual void on_b43CheckBox_clicked();
    virtual void on_wlCheckBox_clicked();
    virtual void on_ipv6CheckBox_clicked();
    virtual void on_fwCheckBox_clicked();

    virtual void on_stopPushButton_clicked();
    virtual void on_startPushButton_clicked();

    virtual void on_startCheckBox_clicked();
    virtual void on_pluggedCheckBox_clicked();
    virtual void on_nowCheckBox_clicked();
    virtual void on_dhcpButton_clicked();
    virtual void on_staticButton_clicked();
    virtual void on_ipEdit_textEdited();
    virtual void on_netmaskEdit_textEdited();
    virtual void on_gatewayEdit_textEdited();
    virtual void on_bcastEdit_textEdited();

    //virtual void on_essidEditCombo_textEdited();
    virtual void on_essidEditCombo_editTextChanged();
    virtual void on_essidEditCombo_currentIndexChanged();

    virtual void on_keyEdit_textEdited();
    virtual void on_freqEdit_textEdited();
    virtual void on_noneRadioButton_clicked();
    virtual void on_wepRadioButton_clicked();
    virtual void on_wpaRadioButton_clicked();
    virtual void on_hiddenCheckBox_clicked();
    virtual void on_restrictedCheckBox_clicked();
    virtual void on_freqCheckBox_clicked();

    virtual void on_dnsDhcpButton_clicked();
    virtual void on_dnsStaticButton_clicked();
    virtual void on_dhcpTimeoutSpinBox_valueChanged();
    virtual void on_primaryDnsEdit_textEdited();
    virtual void on_secondaryDnsEdit_textEdited();

    virtual void on_autoRadioButton_clicked();
    virtual void on_classicRadioButton_clicked();
    virtual void on_ifaceComboBox_activated();
    virtual void on_tabWidget_currentChanged();
    virtual void on_buttonApply_clicked();
    virtual void on_buttonCancel_clicked();
    virtual void on_buttonOk_clicked();
    virtual void on_buttonAbout_clicked();
    
    virtual void on_generalHelpPushButton_clicked();
    virtual void on_hwDiagnosePushButton_clicked();
    virtual void on_linuxDrvDiagnosePushButton_clicked();
    virtual void on_windowsDrvDiagnosePushButton_clicked();
    virtual void on_linuxDrvList_currentRowChanged(int currentRow );
    virtual void on_linuxDrvBlacklistPushButton_clicked();
    virtual void on_windowsDrvBlacklistPushButton_clicked();
    virtual void on_windowsDrvAddPushButton_clicked() ;
    virtual void on_windowsDrvRemovePushButton_clicked();
    virtual void on_refreshPushButton_clicked();
    virtual void on_scanAPPushButton_clicked();
    virtual void on_clearPingOutput_clicked();
    virtual void on_clearTraceOutput_clicked();
    virtual void on_tracerouteButton_clicked();
    virtual void on_pingButton_clicked();
    virtual void on_cancelPing_clicked();
    virtual void on_cancelTrace_clicked();
    virtual void writePingOutput();
    virtual void writeTraceOutput();
    virtual void pingFinished();
    virtual void tracerouteFinished();
    virtual void showAPinfo(int index);
    virtual void on_windowsDrvList_currentRowChanged(int row);

    virtual void hwListToClipboard();
    virtual void hwListFullToClipboard();

    virtual void linuxDrvListToClipboard();
    virtual void linuxDrvListFullToClipboard();

    virtual void windowsDrvListToClipboard();
    virtual void windowsDrvListFullToClipboard();

    virtual void showContextMenuForHw(const QPoint &pos);
    virtual void showContextMenuForLinuxDrv(const QPoint &pos);
    virtual void showContextMenuForWindowsDrv(const QPoint &pos);

protected:
    /*$PROTECTED_FUNCTIONS$*/
    void executeChild(const char* cmd, const char* param);
    void updateNdiswrapStatus();
    void addAPs(QString cardIf, QString configuredAP);
    bool configurationChanges[5];
    int currentTab;
    bool blacklistModule(QString module);
    QPixmap createSignalIcon(QString strength);
    QString wifiIf;
    bool internetConnection;
    bool ndiswrapBlacklisted;
    int wifiChanges;
    QProcess *pingProc;
    QProcess *traceProc;
    static bool isDNSstatic();
    static void setStaticDNS();
    static void setDNSWithDNS();

protected slots:
    /*$PROTECTED_SLOTS$*/

};

#endif

