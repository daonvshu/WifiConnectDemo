#include "WifiHelper.h"
#include <qdebug.h>

void WlanNotificationCallback(PWLAN_NOTIFICATION_DATA Arg1, PVOID Arg2) {
    if (Arg1 != NULL) {
        switch (Arg1->NotificationSource) {
            case WLAN_NOTIFICATION_SOURCE_ACM:
                const auto wifi_list_refresh = [&] {
                    auto obj = static_cast<WifiHelper*>(Arg2);
                    if (obj != nullptr) {
                        WCHAR GuidString[39] = { 0 };
                        int iRet = StringFromGUID2(Arg1->InterfaceGuid, (LPOLESTR)&GuidString, sizeof(GuidString) / sizeof(*GuidString));
                        obj->getNewInterfaceGuid(QString::fromStdWString(GuidString));
                    }
                };
                switch (Arg1->NotificationCode) {
                    case wlan_notification_acm_connection_complete:
                        {
                            if (Arg1->dwDataSize < sizeof(WLAN_CONNECTION_NOTIFICATION_DATA)) {
                                break;
                            }
                            auto data = (PWLAN_CONNECTION_NOTIFICATION_DATA)Arg1->pData;
                            QString ssid = QByteArray((char*)data->dot11Ssid.ucSSID, data->dot11Ssid.uSSIDLength);
                            if (data->wlanReasonCode == WLAN_REASON_CODE_SUCCESS) {
                                qDebug() << QString("%1 connection successed!").arg(ssid);
                                wifi_list_refresh();
                            } else {
                                wchar_t reasonCodeStr[1024] = { 0 };
                                WlanReasonCodeToString(data->wlanReasonCode, 1024, reasonCodeStr, NULL);
                                auto obj = static_cast<WifiHelper*>(Arg2);
                                if (obj != nullptr) {
                                    obj->printErr(QString("%1 connection failed!").arg(ssid), QString::fromWCharArray(reasonCodeStr));
                                }
                            }
                        }
                        break;
                    case wlan_notification_acm_scan_complete:
                    case wlan_notification_acm_disconnected:
                        wifi_list_refresh();
                        break;
                    default:
                        break;
                }
                break;
        }
    }
    qDebug() << "notifycode = " << Arg1->NotificationCode;
}

WifiHelper::WifiHelper(QString profileTemplateStr, QObject* parent)
    : QObject(parent) 
    , profileTemplateStr(profileTemplateStr)
    , pIfList(NULL)
{
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    Q_ASSERT(dwResult == ERROR_SUCCESS);

    DWORD dwNotifSourcePre;
    dwResult = WlanRegisterNotification(hClient, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, (WLAN_NOTIFICATION_CALLBACK)WlanNotificationCallback, this, NULL, &dwNotifSourcePre);

    connect(this, &WifiHelper::getNewInterfaceGuid, this, &WifiHelper::reloadWifiListInMainThread);
}


WifiHelper::~WifiHelper() {
    if (pIfList != NULL) {
        WlanFreeMemory(pIfList);
        pIfList = NULL;
    }
    WlanCloseHandle(hClient, NULL);
}

void WifiHelper::loadWifiInfo() {
    wifiList.clear();

    if (pIfList != NULL) {
        WlanFreeMemory(pIfList);
        pIfList = NULL;
    }

    DWORD dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    Q_ASSERT(dwResult == ERROR_SUCCESS);

    for (int i = 0; i < (int)pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[i];

        WifiInfo wifiInfo;
        wifiInfo.description = QString::fromStdWString(pIfInfo->strInterfaceDescription);

        WCHAR GuidString[39] = { 0 };
        int iRet = StringFromGUID2(pIfInfo->InterfaceGuid, (LPOLESTR)&GuidString, sizeof(GuidString) / sizeof(*GuidString));
        wifiInfo.guid = pIfInfo->InterfaceGuid;
        wifiInfo.guidStr = iRet == 0 ? "StringFromGUID2 failed" : QString::fromStdWString(GuidString);

        switch (pIfInfo->isState) {
            case wlan_interface_state_not_ready:
                wifiInfo.status = "Not ready";
                break;
            case wlan_interface_state_connected:
                wifiInfo.status = "Connected";
                break;
            case wlan_interface_state_ad_hoc_network_formed:
                wifiInfo.status = "First node in a ad hoc network";
                break;
            case wlan_interface_state_disconnecting:
                wifiInfo.status = "Disconnecting";
                break;
            case wlan_interface_state_disconnected:
                wifiInfo.status = "Not connected";
                break;
            case wlan_interface_state_associating:
                wifiInfo.status = "Attempting to associate with a network";
                break;
            case wlan_interface_state_discovering:
                wifiInfo.status = "Auto configuration is discovering settings for the network";
                break;
            case wlan_interface_state_authenticating:
                wifiInfo.status = "In process of authenticating";
                break;
            default:
                wifiInfo.status = "Unknown state " + QString::number(pIfInfo->isState);
                break;
        }
        wifiList << wifiInfo;
    }

    findActiveWireless(wifiList[0]);
    wifiListRefreshed(wifiList.at(0));
    interfaceListRefreshed(wifiList);
}

void WifiHelper::scanWifiList(int interfaceIndex) {
    WLAN_RAW_DATA wlanRawData = { 0 };
    DWORD dwResult = WlanScan(hClient, &wifiList[interfaceIndex].guid, NULL, &wlanRawData, NULL);
    Q_ASSERT(dwResult == ERROR_SUCCESS);
}

void WifiHelper::reloadWifiList(int interfaceIndex) {
    findActiveWireless(wifiList[interfaceIndex]);
    wifiListRefreshed(wifiList.at(interfaceIndex));
}

void WifiHelper::connectWifi(int interfaceIndex, int entryInfoIndex, QString passwordIfNeed) {
    connectWifi(wifiList.at(interfaceIndex).guid,
                wifiList.at(interfaceIndex).entryList.at(entryInfoIndex),
                passwordIfNeed
    );
}

void WifiHelper::disconnectWifi(int interfaceIndex) {
    disconnectWifi(wifiList.at(interfaceIndex).guid);
}

void WifiHelper::reloadWifiListInMainThread(QString guidStr) {
    for (int i = 0; i < wifiList.size(); i++) {
        if (wifiList.at(i).guidStr == guidStr) {
            findActiveWireless(wifiList[i]);
            wifiListRefreshed(wifiList.at(i));
            break;
        }
    }
}

void WifiHelper::connectWifi(const GUID& interfaceGuid, const EntryInfo& entryInfo, QString password) {
    if (entryInfo.dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
        return;
    }

    disconnectWifi(interfaceGuid);

    wchar_t profile[WLAN_MAX_NAME_LENGTH] = {};
    if (!(entryInfo.dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE)) {
        setProfile(interfaceGuid, entryInfo, password);
    }
    entryInfo.ssid.toWCharArray(profile);
    auto fstr = getProfileStr(interfaceGuid, entryInfo.ssid);

    WLAN_CONNECTION_PARAMETERS parameter;
    parameter.strProfile = profile;
    parameter.pDot11Ssid = NULL;
    parameter.pDesiredBssidList = NULL;
    parameter.wlanConnectionMode = wlan_connection_mode_profile;
    parameter.dot11BssType = entryInfo.dot11BssType;
    parameter.dwFlags = WLAN_CONNECTION_HIDDEN_NETWORK;

    DWORD dwResult = WlanConnect(hClient, &interfaceGuid, &parameter, NULL);
    if (dwResult != ERROR_SUCCESS) {
        if (dwResult == ERROR_INVALID_PARAMETER) {
            printErr(u8"无法连接", u8"不支持的连接！修改profile模板！");
        } else {
            Q_ASSERT(dwResult == ERROR_SUCCESS);
        }
    }
}

void WifiHelper::disconnectWifi(const GUID & interfaceGuid) {
    DWORD dwResult = WlanDisconnect(hClient, &interfaceGuid, NULL);
    Q_ASSERT(dwResult == ERROR_SUCCESS);
}

bool WifiHelper::setProfile(const GUID& interfaceGuid, const EntryInfo& entryInfo, QString password) {
    wchar_t profile[2048] = { 0 };
    QString profileStr = getProfileStr(entryInfo, password);
    profileStr.toWCharArray(profile);
    DWORD dwReasonCode;
    wchar_t reasonCodeStr[1024] = { 0 };
    DWORD dwResult = WlanSetProfile(hClient, &interfaceGuid, 0, profile, NULL, TRUE, NULL, &dwReasonCode);
    if (dwResult != ERROR_SUCCESS) {
        WlanReasonCodeToString(dwReasonCode, 1024, reasonCodeStr, NULL);
    }
    return dwResult == ERROR_SUCCESS;
}

QString WifiHelper::getProfileStr(const EntryInfo& entryInfo, QString password) {
    QString templateContent = profileTemplateStr;
    templateContent.replace("{ssid}", entryInfo.ssid);
    templateContent.replace("{password}", password);
    switch (entryInfo.dot11DefaultAuthAlgorithm) {
        case DOT11_AUTH_ALGO_80211_OPEN:
            templateContent.replace("{authentication}", "open");
            break;
        case DOT11_AUTH_ALGO_80211_SHARED_KEY:
            templateContent.replace("{authentication}", "shared");
            break;
        case DOT11_AUTH_ALGO_WPA:
            templateContent.replace("{authentication}", "WPA");
            break;
        case DOT11_AUTH_ALGO_WPA_PSK:
            templateContent.replace("{authentication}", "WPAPSK");
            break;
        case DOT11_AUTH_ALGO_WPA_NONE:
            templateContent.replace("{authentication}", "none");
            break;
        case DOT11_AUTH_ALGO_RSNA:
            templateContent.replace("{authentication}", "WPA2");
            break;
        case DOT11_AUTH_ALGO_RSNA_PSK:
            templateContent.replace("{authentication}", "WPA2PSK");
            break;
        default:
            break;
    }
    switch (entryInfo.dot11DefaultCipherAlgorithm) {
        case DOT11_CIPHER_ALGO_NONE:
            templateContent.replace("{encryption}", "none");
            break;
        case DOT11_CIPHER_ALGO_WEP40:
            templateContent.replace("{encryption}", "WEP");
            break;
        case DOT11_CIPHER_ALGO_TKIP:
            templateContent.replace("{encryption}", "TKIP");
            break;
        case DOT11_CIPHER_ALGO_CCMP:
            templateContent.replace("{encryption}", "AES");
            break;
        case DOT11_CIPHER_ALGO_WEP104:
            templateContent.replace("{encryption}", "WEP");
            break;
        case DOT11_CIPHER_ALGO_WEP:
            templateContent.replace("{encryption}", "WEP");
            break;
        default:
            break;
    }
    return templateContent;
}

QString WifiHelper::getProfileStr(const GUID & interfaceGuid, QString profileName) {
    wchar_t profile[WLAN_MAX_NAME_LENGTH] = {};
    profileName.toWCharArray(profile);
    DWORD dwFlags, dwGrantedAccess;
    LPWSTR pProfileXml = NULL;
    DWORD dwResult = WlanGetProfile(hClient, &interfaceGuid, profile, NULL, &pProfileXml, &dwFlags, &dwGrantedAccess);
    if (dwResult == ERROR_SUCCESS) {
        return QString::fromWCharArray(pProfileXml);
    }
    return QString();
}

void WifiHelper::findActiveWireless(WifiInfo& wifiInfo) {

    PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;
    PWLAN_AVAILABLE_NETWORK pBssEntry = NULL;

    wifiInfo.entryList.clear();
    DWORD dwResult = WlanGetAvailableNetworkList(hClient, &wifiInfo.guid, 0, NULL, &pBssList);
    if (dwResult == ERROR_SUCCESS) {
        QHash<QString, EntryInfo> entryList;
        for (int j = 0; j < pBssList->dwNumberOfItems; j++) {
            pBssEntry = (WLAN_AVAILABLE_NETWORK *)& pBssList->Network[j];

            EntryInfo entryInfo;
            entryInfo.profile = QString::fromWCharArray(pBssEntry->strProfileName);
            entryInfo.dot11_ssid = pBssEntry->dot11Ssid;
            entryInfo.ssid = QByteArray((char*)pBssEntry->dot11Ssid.ucSSID, pBssEntry->dot11Ssid.uSSIDLength);
            entryInfo.connectable = pBssEntry->bNetworkConnectable;
            entryInfo.signalQuality = pBssEntry->wlanSignalQuality;

            int iRSSI = 0;
            if (pBssEntry->wlanSignalQuality == 0)
                iRSSI = -100;
            else if (pBssEntry->wlanSignalQuality == 100)
                iRSSI = -50;
            else
                iRSSI = -100 + (pBssEntry->wlanSignalQuality / 2);

            entryInfo.rssi = iRSSI;
            entryInfo.dot11BssType = pBssEntry->dot11BssType;
            entryInfo.securityEnabled = pBssEntry->bSecurityEnabled;

            entryInfo.dot11DefaultAuthAlgorithm = pBssEntry->dot11DefaultAuthAlgorithm;
            switch (pBssEntry->dot11DefaultAuthAlgorithm) {
                case DOT11_AUTH_ALGO_80211_OPEN:
                    entryInfo.authAlgorithm = "802.11 Open";
                    break;
                case DOT11_AUTH_ALGO_80211_SHARED_KEY:
                    entryInfo.authAlgorithm = "802.11 Shared";
                    break;
                case DOT11_AUTH_ALGO_WPA:
                    entryInfo.authAlgorithm = "WPA";
                    break;
                case DOT11_AUTH_ALGO_WPA_PSK:
                    entryInfo.authAlgorithm = "WPA-PSK";
                    break;
                case DOT11_AUTH_ALGO_WPA_NONE:
                    entryInfo.authAlgorithm = "WPA-None";
                    break;
                case DOT11_AUTH_ALGO_RSNA:
                    entryInfo.authAlgorithm = "RSNA";
                    break;
                case DOT11_AUTH_ALGO_RSNA_PSK:
                    entryInfo.authAlgorithm = "RSNA with PSK";
                    break;
                default:
                    entryInfo.authAlgorithm = "Other";
                    break;
            }

            entryInfo.dot11DefaultCipherAlgorithm = pBssEntry->dot11DefaultCipherAlgorithm;
            switch (pBssEntry->dot11DefaultCipherAlgorithm) {
                case DOT11_CIPHER_ALGO_NONE:
                    entryInfo.cipherAlgorithm = "None";
                    break;
                case DOT11_CIPHER_ALGO_WEP40:
                    entryInfo.cipherAlgorithm = "WEP-40";
                    break;
                case DOT11_CIPHER_ALGO_TKIP:
                    entryInfo.cipherAlgorithm = "TKIP";
                    break;
                case DOT11_CIPHER_ALGO_CCMP:
                    entryInfo.cipherAlgorithm = "CCMP";
                    break;
                case DOT11_CIPHER_ALGO_WEP104:
                    entryInfo.cipherAlgorithm = "WEP-104";
                    break;
                case DOT11_CIPHER_ALGO_WEP:
                    entryInfo.cipherAlgorithm = "WEP";
                    break;
                default:
                    entryInfo.cipherAlgorithm = "Other";
                    break;
            }
            entryInfo.dwFlags = pBssEntry->dwFlags;
            if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
                entryInfo.status = u8"已连接";
            } else if (pBssEntry->dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE) {
                entryInfo.status = u8"已保存";
            } else {
                entryInfo.status = u8"未连接";
                if (entryList.contains(entryInfo.ssid)) {
                    continue;
                }
            }
            entryList.insert(entryInfo.ssid, entryInfo);
        }
        for (EntryInfo info : entryList.values()) {
            wifiInfo.entryList << info;
        }

    } else {
        qDebug() << "WlanGetAvailableNetworkList failed with error: " << dwResult;
    }

    if (pBssList != NULL) {
        WlanFreeMemory(pBssList);
        pBssList = NULL;
    }
}

