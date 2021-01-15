#include "WifiConnectTest.h"
#include <qfile.h>
#include <qdebug.h>
#include <qmessagebox.h>

WifiConnectTest::WifiConnectTest(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    auto model = new QStandardItemModel(this);
    ui.tableView->setModel(model);
    ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    addTableHeader(model);

    connect(ui.scan_adapter, &QPushButton::clicked, [&]() {
        wifiHelper->loadWifiInfo();
    });
    
    connect(ui.scan_wifi, &QPushButton::clicked, [&]() {
        int currentAdapterIndex = ui.interfaceDescription->currentIndex();
        if (currentAdapterIndex >= 0) {
            wifiHelper->scanWifiList(currentAdapterIndex);
        }
    });

    connect(ui.interfaceDescription, qOverload<int>(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0) {
            wifiHelper->reloadWifiList(index);
        }
    });

    connect(ui.connect_tag, &QPushButton::clicked, [&]() {
        int interfaceIndex = ui.interfaceDescription->currentIndex();
        if (interfaceIndex >= 0) {
            int row = ui.tableView->currentIndex().row();
            if (row >= 0) {
                wifiHelper->connectWifi(interfaceIndex, row,
                                        ui.tableView->model()->index(row, ui.tableView->model()->columnCount() - 1).data(Qt::DisplayRole).toString());
            }
        }
    });

    connect(ui.disconnect_tag, &QPushButton::clicked, [&]() {
        int interfaceIndex = ui.interfaceDescription->currentIndex();
        if (interfaceIndex >= 0) {
            wifiHelper->disconnectWifi(interfaceIndex);
        }
    });

    QFile file(":/WifiConnectTest/Resources/profile_template.xml");
    file.open(QIODevice::ReadOnly);
    QString templateContent = file.readAll();
    file.close();

    wifiHelper = new WifiHelper(templateContent, this);
    connect(wifiHelper, &WifiHelper::interfaceListRefreshed, this, &WifiConnectTest::loadWifiInterfaceList);
    connect(wifiHelper, &WifiHelper::wifiListRefreshed, this, &WifiConnectTest::loadWifiEntryList);
    connect(wifiHelper, &WifiHelper::printErr, this, &WifiConnectTest::showErrMessageBox);

}

WifiConnectTest::~WifiConnectTest() {
    
}

void WifiConnectTest::addTableHeader(QStandardItemModel * model) {
    QStringList titles;
    titles << "ssid" << u8"是否可连接" << u8"信号质量" << "rssi" << u8"启动网络安全" << u8"认证算法" << u8"加密算法" << u8"状态" << u8"密码";
    model->setColumnCount(titles.size());
    for (int i = 0; i < titles.size(); i++) {
        model->setHeaderData(i, Qt::Horizontal, titles[i], Qt::DisplayRole);
    }
}

void WifiConnectTest::loadWifiEntryList(const WifiInfo& wifiInfo) {
    auto tabModel = static_cast<QStandardItemModel*>(ui.tableView->model());
    tabModel->clear();
    addTableHeader(tabModel);

    ui.interfaceStatus->setText(wifiInfo.status);
    for (int i = 0; i < wifiInfo.entryList.size(); i++) {
        auto entryInfo = wifiInfo.entryList.at(i);

        QStandardItem* item;
        int col = 0;

        item = new QStandardItem(entryInfo.ssid);
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(entryInfo.connectable ? "yes" : "no");
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(QString::number(entryInfo.signalQuality));
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(QString("%1 dBm").arg(entryInfo.rssi));
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(entryInfo.securityEnabled ? "yes" : "no");
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(entryInfo.authAlgorithm);
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(entryInfo.cipherAlgorithm);
        tabModel->setItem(i, col++, item);

        item = new QStandardItem(entryInfo.status);
        tabModel->setItem(i, col++, item);

        item = new QStandardItem;
        tabModel->setItem(i, col++, item);
    }
}

void WifiConnectTest::loadWifiInterfaceList(const QList<WifiInfo>& wifiList) {
    ui.interfaceDescription->clear();
    for (const auto& i : wifiList) {
        ui.interfaceDescription->addItem(i.description);
    }
}

void WifiConnectTest::showErrMessageBox(QString title, QString content) {
    QMessageBox::warning(NULL, title, content);
}