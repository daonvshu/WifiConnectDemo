#pragma once

#include <QtWidgets/QWidget>
#include "ui_WifiConnectTest.h"

#include <qstandarditemmodel.h>

#include "WifiHelper.h"

class WifiConnectTest : public QWidget
{
    Q_OBJECT

public:
    WifiConnectTest(QWidget *parent = Q_NULLPTR);
    ~WifiConnectTest();

private:
    Ui::WifiConnectTestClass ui; 
    WifiHelper* wifiHelper;

    void addTableHeader(QStandardItemModel* model);

private slots:
    void loadWifiEntryList(const WifiInfo& wifiInfo);
    void loadWifiInterfaceList(const QList<WifiInfo>& wifiList);
    void showErrMessageBox(QString title, QString content);
};
