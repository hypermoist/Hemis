// Copyright (c) 2019-2022 The Hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/Hemis/gmmodel.h"

#include "coincontrol.h"
#include "gamemaster.h"
#include "gamemasterman.h"
#include "net.h" // for validateGamemasterIP
#include "primitives/transaction.h"
#include "qt/bitcoinunits.h"
#include "qt/optionsmodel.h"
#include "qt/Hemis/guitransactionsutils.h"
#include "qt/walletmodel.h"
#include "qt/walletmodeltransaction.h"
#include "tiertwo/tiertwo_sync_state.h"
#include "uint256.h"

#include <QFile>
#include <QHostAddress>

GMModel::GMModel(QObject *parent) : QAbstractTableModel(parent) {}

void GMModel::init()
{
    updateGMList();
}

void GMModel::updateGMList()
{
    int gmMinConf = getGamemasterCollateralMinConf();
    int end = nodes.size();
    nodes.clear();
    collateralTxAccepted.clear();
    for (const CGamemasterConfig::CGamemasterEntry& gme : gamemasterConfig.getEntries()) {
        int nIndex;
        if (!gme.castOutputIndex(nIndex))
            continue;
        const uint256& txHash = uint256S(gme.getTxHash());
        CTxIn txIn(txHash, uint32_t(nIndex));
        CGamemaster* pgm = gamemasterman.Find(txIn.prevout);
        if (!pgm) {
            pgm = new CGamemaster();
            pgm->vin = txIn;
        }
        nodes.insert(QString::fromStdString(gme.getAlias()), std::make_pair(QString::fromStdString(gme.getIp()), pgm));
        if (walletModel) {
            collateralTxAccepted.insert(gme.getTxHash(), walletModel->getWalletTxDepth(txHash) >= gmMinConf);
        }
    }
    Q_EMIT dataChanged(index(0, 0, QModelIndex()), index(end, 5, QModelIndex()) );
}

int GMModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return nodes.size();
}

int GMModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 6;
}


QVariant GMModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
            return QVariant();

    // rec could be null, always verify it.
    CGamemaster* rec = static_cast<CGamemaster*>(index.internalPointer());
    bool isAvailable = rec;
    int row = index.row();
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ALIAS:
                return nodes.uniqueKeys().value(row);
            case ADDRESS:
                return nodes.values().value(row).first;
            case PUB_KEY:
                return (isAvailable) ? QString::fromStdString(nodes.values().value(row).second->pubKeyGamemaster.GetHash().GetHex()) : "Not available";
            case COLLATERAL_ID:
                return (isAvailable) ? QString::fromStdString(rec->vin.prevout.hash.GetHex()) : "Not available";
            case COLLATERAL_OUT_INDEX:
                return (isAvailable) ? QString::number(rec->vin.prevout.n) : "Not available";
            case STATUS: {
                std::pair<QString, CGamemaster*> pair = nodes.values().value(row);
                std::string status = "MISSING";
                if (pair.second) {
                    status = pair.second->Status();
                    // Quick workaround to the current Gamemaster status types.
                    // If the status is REMOVE and there is no pubkey associated to the Gamemaster
                    // means that the GM is not in the network list and was created in
                    // updateGMList(). Which.. denotes a not started gamemaster.
                    // This will change in the future with the GamemasterWrapper introduction.
                    if (status == "REMOVE" && !pair.second->pubKeyCollateralAddress.IsValid()) {
                        return "MISSING";
                    }
                }
                return QString::fromStdString(status);
            }
            case PRIV_KEY: {
                if (isAvailable) {
                    for (CGamemasterConfig::CGamemasterEntry gme : gamemasterConfig.getEntries()) {
                        if (gme.getTxHash().compare(rec->vin.prevout.hash.GetHex()) == 0) {
                            return QString::fromStdString(gme.getPrivKey());
                        }
                    }
                }
                return "Not available";
            }
            case WAS_COLLATERAL_ACCEPTED:{
                return isAvailable && collateralTxAccepted.value(rec->vin.prevout.hash.GetHex());
            }
        }
    }
    return QVariant();
}

QModelIndex GMModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    std::pair<QString, CGamemaster*> pair = nodes.values().value(row);
    CGamemaster* data = pair.second;
    if (data) {
        return createIndex(row, column, data);
    } else if (!pair.first.isEmpty()) {
        return createIndex(row, column, nullptr);
    } else {
        return QModelIndex();
    }
}


bool GMModel::removeGm(const QModelIndex& modelIndex)
{
    QString alias = modelIndex.data(Qt::DisplayRole).toString();
    int idx = modelIndex.row();
    beginRemoveRows(QModelIndex(), idx, idx);
    nodes.take(alias);
    endRemoveRows();
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, 5, QModelIndex()) );
    return true;
}

bool GMModel::addGm(CGamemasterConfig::CGamemasterEntry* gme)
{
    beginInsertRows(QModelIndex(), nodes.size(), nodes.size());
    int nIndex;
    if (!gme->castOutputIndex(nIndex))
        return false;

    CGamemaster* pgm = gamemasterman.Find(COutPoint(uint256S(gme->getTxHash()), uint32_t(nIndex)));
    nodes.insert(QString::fromStdString(gme->getAlias()), std::make_pair(QString::fromStdString(gme->getIp()), pgm));
    endInsertRows();
    return true;
}

int GMModel::getGMState(const QString& gmAlias)
{
    QMap<QString, std::pair<QString, CGamemaster*>>::const_iterator it = nodes.find(gmAlias);
    if (it != nodes.end()) return it.value().second->GetActiveState();
    throw std::runtime_error(std::string("Gamemaster alias not found"));
}

bool GMModel::isGMInactive(const QString& gmAlias)
{
    int activeState = getGMState(gmAlias);
    return activeState == CGamemaster::GAMEMASTER_EXPIRED || activeState == CGamemaster::GAMEMASTER_REMOVE;
}

bool GMModel::isGMActive(const QString& gmAlias)
{
    int activeState = getGMState(gmAlias);
    return activeState == CGamemaster::GAMEMASTER_PRE_ENABLED || activeState == CGamemaster::GAMEMASTER_ENABLED;
}

bool GMModel::isGMCollateralMature(const QString& gmAlias)
{
    QMap<QString, std::pair<QString, CGamemaster*>>::const_iterator it = nodes.find(gmAlias);
    if (it != nodes.end()) return collateralTxAccepted.value(it.value().second->vin.prevout.hash.GetHex());
    throw std::runtime_error(std::string("Gamemaster alias not found"));
}

bool GMModel::isGMsNetworkSynced()
{
    return g_tiertwo_sync_state.IsSynced();
}

bool GMModel::validateGMIP(const QString& addrStr)
{
    return validateGamemasterIP(addrStr.toStdString());
}

CAmount GMModel::getGMCollateralRequiredAmount()
{
    return Params().GetConsensus().nGMCollateralAmt;
}

int GMModel::getGamemasterCollateralMinConf()
{
    return Params().GetConsensus().GamemasterCollateralMinConf();
}

bool GMModel::createGMCollateral(
        const QString& alias,
        const QString& addr,
        COutPoint& ret_outpoint,
        QString& ret_error)
{
    SendCoinsRecipient sendCoinsRecipient(addr, alias, getGMCollateralRequiredAmount(), "");

    // Send the 10 tx to one of your address
    QList<SendCoinsRecipient> recipients;
    recipients.append(sendCoinsRecipient);
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    // no P2CS delegations
    prepareStatus = walletModel->prepareTransaction(&currentTransaction, coinControl, false);
    QString returnMsg = tr("Unknown error");
    // process prepareStatus and on error generate message shown to user
    CClientUIInterface::MessageBoxFlags informType;
    returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(
            prepareStatus,
            walletModel,
            informType, // this flag is not needed
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        ret_error = tr("Prepare master node failed.\n\n%1\n").arg(returnMsg);
        return false;
    }

    WalletModel::SendCoinsReturn sendStatus = walletModel->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    returnMsg = GuiTransactionsUtils::ProcessSendCoinsReturn(sendStatus, walletModel, informType);

    if (sendStatus.status != WalletModel::OK) {
        ret_error = tr("Cannot send collateral transaction.\n\n%1").arg(returnMsg);
        return false;
    }

    // look for the tx index of the collateral
    CTransactionRef walletTx = currentTransaction.getTransaction();
    std::string txID = walletTx->GetHash().GetHex();
    int indexOut = -1;
    for (int i=0; i < (int)walletTx->vout.size(); i++) {
        const CTxOut& out = walletTx->vout[i];
        if (out.nValue == getGMCollateralRequiredAmount()) {
            indexOut = i;
            break;
        }
    }
    if (indexOut == -1) {
        ret_error = tr("Invalid collateral output index");
        return false;
    }
    // save the collateral outpoint
    ret_outpoint = COutPoint(walletTx->GetHash(), indexOut);
    return true;
}

bool GMModel::startLegacyGM(const CGamemasterConfig::CGamemasterEntry& gme, int chainHeight, std::string& strError)
{
    CGamemasterBroadcast gmb;
    if (!CGamemasterBroadcast::Create(gme.getIp(), gme.getPrivKey(), gme.getTxHash(), gme.getOutputIndex(), strError, gmb, false, chainHeight))
        return false;

    gamemasterman.UpdateGamemasterList(gmb);
    if (activeGamemaster.pubKeyGamemaster == gmb.GetPubKey()) {
        activeGamemaster.EnableHotColdGameMaster(gmb.vin, gmb.addr);
    }
    gmb.Relay();
    return true;
}

void GMModel::startAllLegacyGMs(bool onlyMissing, int& amountOfGmFailed, int& amountOfGmStarted,
                                std::string* aliasFilter, std::string* error_ret)
{
    for (const auto& gme : gamemasterConfig.getEntries()) {
        if (!aliasFilter) {
            // Check for missing only
            QString gmAlias = QString::fromStdString(gme.getAlias());
            if (onlyMissing && !isGMInactive(gmAlias)) {
                if (!isGMActive(gmAlias))
                    amountOfGmFailed++;
                continue;
            }

            if (!isGMCollateralMature(gmAlias)) {
                amountOfGmFailed++;
                continue;
            }
        } else if (*aliasFilter != gme.getAlias()){
            continue;
        }

        std::string ret_str;
        if (!startLegacyGM(gme, walletModel->getLastBlockProcessedNum(), ret_str)) {
            amountOfGmFailed++;
            if (error_ret) *error_ret = ret_str;
        } else {
            amountOfGmStarted++;
        }
    }
}

// Future: remove after v6.0
CGamemasterConfig::CGamemasterEntry* GMModel::createLegacyGM(COutPoint& collateralOut,
                             const std::string& alias,
                             std::string& serviceAddr,
                             const std::string& port,
                             const std::string& gmKeyString,
                             QString& ret_error)
{
    // Update the conf file
    QString strConfFileQt(Hemis_GAMEMASTER_CONF_FILENAME);
    std::string strConfFile = strConfFileQt.toStdString();
    std::string strDataDir = GetDataDir().string();
    fs::path conf_file_path(strConfFile);
    if (strConfFile != conf_file_path.filename().string()) {
        throw std::runtime_error(strprintf(_("%s %s resides outside data directory %s"), strConfFile, strConfFile, strDataDir));
    }

    fs::path pathBootstrap = GetDataDir() / strConfFile;
    if (!fs::exists(pathBootstrap)) {
        ret_error = tr("%1 file doesn't exists").arg(strConfFileQt);
        return nullptr;
    }

    fs::path pathGamemasterConfigFile = GetGamemasterConfigFile();
    fsbridge::ifstream streamConfig(pathGamemasterConfigFile);

    if (!streamConfig.good()) {
        ret_error = tr("Invalid %1 file").arg(strConfFileQt);
        return nullptr;
    }

    int linenumber = 1;
    std::string lineCopy;
    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                streamConfig.close();
                ret_error = tr("Error parsing %1 file").arg(strConfFileQt);
                return nullptr;
            }
        }
        lineCopy += line + "\n";
    }

    if (lineCopy.empty()) {
        lineCopy = "# Gamemaster config file\n"
                   "# Format: alias IP:port gamemasterprivkey collateral_output_txid collateral_output_index\n"
                   "# Example: gm1 127.0.0.2:49165 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0"
                   "#";
    }
    lineCopy += "\n";

    streamConfig.close();

    std::string txID = collateralOut.hash.ToString();
    std::string indexOutStr = std::to_string(collateralOut.n);

    // Check IP address type
    QHostAddress hostAddress(QString::fromStdString(serviceAddr));
    QAbstractSocket::NetworkLayerProtocol layerProtocol = hostAddress.protocol();
    if (layerProtocol == QAbstractSocket::IPv6Protocol) {
        serviceAddr = "["+serviceAddr+"]";
    }

    fs::path pathConfigFile = AbsPathForConfigVal(fs::path("gamemaster_temp.conf"));
    FILE* configFile = fopen(pathConfigFile.string().c_str(), "w");
    lineCopy += alias+" "+serviceAddr+":"+port+" "+gmKeyString+" "+txID+" "+indexOutStr+"\n";
    fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
    fclose(configFile);

    fs::path pathOldConfFile = AbsPathForConfigVal(fs::path("old_gamemaster.conf"));
    if (fs::exists(pathOldConfFile)) {
        fs::remove(pathOldConfFile);
    }
    rename(pathGamemasterConfigFile, pathOldConfFile);

    fs::path pathNewConfFile = AbsPathForConfigVal(fs::path(strConfFile));
    rename(pathConfigFile, pathNewConfFile);

    auto ret_gm_entry = gamemasterConfig.add(alias, serviceAddr+":"+port, gmKeyString, txID, indexOutStr);

    // Lock collateral output
    walletModel->lockCoin(collateralOut.hash, collateralOut.n);
    return ret_gm_entry;
}

// Future: remove after v6.0
bool GMModel::removeLegacyGM(const std::string& alias_to_remove, const std::string& tx_id, unsigned int out_index, QString& ret_error)
{
    QString strConfFileQt(Hemis_GAMEMASTER_CONF_FILENAME);
    std::string strConfFile = strConfFileQt.toStdString();
    std::string strDataDir = GetDataDir().string();
    fs::path conf_file_path(strConfFile);
    if (strConfFile != conf_file_path.filename().string()) {
        throw std::runtime_error(strprintf(_("%s %s resides outside data directory %s"), strConfFile, strConfFile, strDataDir));
    }

    fs::path pathBootstrap = GetDataDir() / strConfFile;
    if (!fs::exists(pathBootstrap)) {
        ret_error = tr("%1 file doesn't exists").arg(strConfFileQt);
        return false;
    }

    fs::path pathGamemasterConfigFile = GetGamemasterConfigFile();
    fsbridge::ifstream streamConfig(pathGamemasterConfigFile);

    if (!streamConfig.good()) {
        ret_error = tr("Invalid %1 file").arg(strConfFileQt);
        return false;
    }

    int lineNumToRemove = -1;
    int linenumber = 1;
    std::string lineCopy;
    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                streamConfig.close();
                ret_error = tr("Error parsing %1 file").arg(strConfFileQt);
                return false;
            }
        }

        if (alias_to_remove == alias) {
            lineNumToRemove = linenumber;
        } else
            lineCopy += line + "\n";

    }

    if (lineCopy.empty()) {
        lineCopy = "# Gamemaster config file\n"
                   "# Format: alias IP:port gamemasterprivkey collateral_output_txid collateral_output_index\n"
                   "# Example: gm1 127.0.0.2:49165 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0\n";
    }

    streamConfig.close();

    if (lineNumToRemove == -1) {
        ret_error = tr("GM alias %1 not found in %2 file").arg(QString::fromStdString(alias_to_remove)).arg(strConfFileQt);
        return false;
    }

    // Update file
    fs::path pathConfigFile = AbsPathForConfigVal(fs::path("gamemaster_temp.conf"));
    FILE* configFile = fsbridge::fopen(pathConfigFile, "w");
    fwrite(lineCopy.c_str(), std::strlen(lineCopy.c_str()), 1, configFile);
    fclose(configFile);

    fs::path pathOldConfFile = AbsPathForConfigVal(fs::path("old_gamemaster.conf"));
    if (fs::exists(pathOldConfFile)) {
        fs::remove(pathOldConfFile);
    }
    rename(pathGamemasterConfigFile, pathOldConfFile);

    fs::path pathNewConfFile = AbsPathForConfigVal(fs::path(strConfFile));
    rename(pathConfigFile, pathNewConfFile);

    // Unlock collateral
    walletModel->unlockCoin(uint256S(tx_id), out_index);
    // Remove alias
    gamemasterConfig.remove(alias_to_remove);
    return true;
}

void GMModel::setCoinControl(CCoinControl* coinControl)
{
    this->coinControl = coinControl;
}

void GMModel::resetCoinControl()
{
    coinControl = nullptr;
}
