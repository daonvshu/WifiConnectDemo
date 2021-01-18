#pragma once

#include <Windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wlanapi.lib")

#include <qobject.h>

struct EntryInfo {
    QString profile;
    QString ssid;
    bool connectable;
    long signalQuality;//信号质量
    int rssi;
    bool securityEnabled;//启动网络安全
    QString authAlgorithm;//认证算法
    QString cipherAlgorithm;//加密算法
    DWORD dwFlags;
    QString status;//当前状态
    //
    DOT11_SSID dot11_ssid;
    DOT11_BSS_TYPE dot11BssType;
    DOT11_AUTH_ALGORITHM dot11DefaultAuthAlgorithm;
    DOT11_CIPHER_ALGORITHM dot11DefaultCipherAlgorithm;
};

struct WifiInfo {
    QString description;
    GUID guid;
    QString guidStr;
    QString status;
    QList<EntryInfo> entryList;
};

class WifiHelper : public QObject {
    Q_OBJECT

public:
    WifiHelper(QString profileTemplateStr, QObject* parent = nullptr);
    ~WifiHelper();

    void loadWifiInfo(); 
    void scanWifiList(int interfaceIndex);
    void reloadWifiList(int interfaceIndex);
    void connectWifi(int interfaceIndex, int entryInfoIndex, QString passwordIfNeed);
    void disconnectWifi(int interfaceIndex);

signals:
    void getNewInterfaceGuid(QString guidStr);
    void interfaceListRefreshed(const QList<WifiInfo>& wifiList);
    void wifiListRefreshed(const WifiInfo& wifiInfo);
    void printErr(QString title, QString content);

private:
    QList<WifiInfo> wifiList;
    HANDLE hClient = NULL;
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    QString profileTemplateStr;

private:
    void findActiveWireless(WifiInfo& wifiInfo);
    void connectWifi(const GUID& interfaceGuid, const EntryInfo& entryInfo, QString password);
    void disconnectWifi(const GUID& interfaceGuid);
    bool setProfile(const GUID& interfaceGuid, const EntryInfo& entryInfo, QString password);
    QString getProfileStr(const EntryInfo& entryInfo, QString password);
    QString getProfileStr(const GUID& interfaceGuid, QString profileName);

private slots:
    void reloadWifiListInMainThread(QString guidStr);
};

