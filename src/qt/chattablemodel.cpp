#include "chattablemodel.h"
#include "guiutil.h"
#include "walletmodel.h"

#include "wallet.h"
#include "base58.h"

#include <QFont>
#include <QColor>

const QString ChatTableModel::Send = "S";
const QString ChatTableModel::Receive = "R";

struct ChatTableEntry
{
    enum Type {
        Sending,
        Receiving
    };

    Type type;
    QString label;
    QString address;

    ChatTableEntry() {}
    ChatTableEntry(Type type, const QString &label, const QString &address):
        type(type), label(label), address(address) {}
};

struct ChatTableEntryLessThan
{
    bool operator()(const ChatTableEntry &a, const ChatTableEntry &b) const
    {
        return a.address < b.address;
    }
    bool operator()(const ChatTableEntry &a, const QString &b) const
    {
        return a.address < b;
    }
    bool operator()(const QString &a, const ChatTableEntry &b) const
    {
        return a < b.address;
    }
};

// Private implementation
class ChatTablePriv
{
public:
    CWallet *wallet;
    QList<ChatTableEntry> cachedChatTable;
    ChatTableModel *parent;

    ChatTablePriv(CWallet *wallet, ChatTableModel *parent):
        wallet(wallet), parent(parent) {}

    void refreshChatTable()
    {
        cachedChatTable.clear();
        {
            LOCK(wallet->cs_wallet);
            BOOST_FOREACH(const PAIRTYPE(CTxDestination, std::string)& item, wallet->mapAddressBook)
            {
                const CBitcoinAddress& address = item.first;
                const std::string& strName = item.second;
                bool fMine = IsMine(*wallet, address.Get());
                cachedChatTable.append(ChatTableEntry(fMine ? ChatTableEntry::Receiving : ChatTableEntry::Sending,
                                  QString::fromStdString(strName),
                                  QString::fromStdString(address.ToString())));
            }
        }
    }

    void updateEntry(const QString &address, const QString &label, bool isMine, int status)
    {
        // Find address / label in model
        QList<ChatTableEntry>::iterator lower = qLowerBound(
            cachedChatTable.begin(), cachedChatTable.end(), address, ChatTableEntryLessThan());
        QList<ChatTableEntry>::iterator upper = qUpperBound(
            cachedChatTable.begin(), cachedChatTable.end(), address, ChatTableEntryLessThan());
        int lowerIndex = (lower - cachedChatTable.begin());
        int upperIndex = (upper - cachedChatTable.begin());
        bool inModel = (lower != upper);
        ChatTableEntry::Type newEntryType = isMine ? ChatTableEntry::Receiving : ChatTableEntry::Sending;

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                OutputDebugStringF("Warning: ChatTablePriv::updateEntry: Got CT_NOW, but entry is already in model\n");
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedChatTable.insert(lowerIndex, ChatTableEntry(newEntryType, label, address));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                OutputDebugStringF("Warning: ChatTablePriv::updateEntry: Got CT_UPDATED, but entry is not in model\n");
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                OutputDebugStringF("Warning: ChatTablePriv::updateEntry: Got CT_DELETED, but entry is not in model\n");
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedChatTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedChatTable.size();
    }

    ChatTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedChatTable.size())
        {
            return &cachedChatTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

ChatTableModel::ChatTableModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent),walletModel(parent),wallet(wallet),priv(0)
{
    columns << tr("Label") << tr("Address");
    priv = new ChatTablePriv(wallet, this);
    priv->refreshChatTable();
}

ChatTableModel::~ChatTableModel()
{
    delete priv;
}

int ChatTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int ChatTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant ChatTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    ChatTableEntry *rec = static_cast<ChatTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Label:
            if(rec->label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->label;
            }
        case Address:
            return rec->address;
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::bitcoinAddressFont();
        }
        return font;
    }
    else if (role == TypeRole)
    {
        switch(rec->type)
        {
        case ChatTableEntry::Sending:
            return Send;
        case ChatTableEntry::Receiving:
            return Receive;
        default: break;
        }
    }
    return QVariant();
}

bool ChatTableModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if(!index.isValid())
        return false;
    ChatTableEntry *rec = static_cast<ChatTableEntry*>(index.internalPointer());

    editStatus = OK;

    if(role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Label:
            wallet->SetAddressBookName(CBitcoinAddress(rec->address.toStdString()).Get(), value.toString().toStdString());
            rec->label = value.toString();
            break;
        case Address:
            // Refuse to set invalid address, set error status and return false
            if(!walletModel->validateAddress(value.toString()))
            {
                editStatus = INVALID_ADDRESS;
                return false;
            }
            // Double-check that we're not overwriting a receiving address
            if(rec->type == ChatTableEntry::Sending)
            {
                {
                    LOCK(wallet->cs_wallet);
                    // Remove old entry
                    wallet->DelAddressBookName(CBitcoinAddress(rec->address.toStdString()).Get());
                    // Add new entry with new address
                    wallet->SetAddressBookName(CBitcoinAddress(value.toString().toStdString()).Get(), rec->label.toStdString());
                }
            }
            break;
        }

        return true;
    }
    return false;
}

QVariant ChatTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags ChatTableModel::flags(const QModelIndex & index) const
{
    if(!index.isValid())
        return 0;
    ChatTableEntry *rec = static_cast<ChatTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // Can edit address and label for sending addresses,
    // and only label for receiving addresses.
    if(rec->type == ChatTableEntry::Sending ||
      (rec->type == ChatTableEntry::Receiving && index.column()==Label))
    {
        retval |= Qt::ItemIsEditable;
    }
    return retval;
}

QModelIndex ChatTableModel::index(int row, int column, const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    ChatTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void ChatTableModel::updateEntry(const QString &address, const QString &label, bool isMine, int status)
{
    // Update address book model from Bitcoin core
    priv->updateEntry(address, label, isMine, status);
}

QString ChatTableModel::addRow(const QString &type, const QString &label, const QString &address)
{
    std::string strLabel = label.toStdString();
    std::string strAddress = address.toStdString();

    editStatus = OK;

    if(type == Send)
    {
        if(!walletModel->validateAddress(address))
        {
            editStatus = INVALID_ADDRESS;
            return QString();
        }
        // Check for duplicate addresses
        {
            LOCK(wallet->cs_wallet);
            if(wallet->mapAddressBook.count(CBitcoinAddress(strAddress).Get()))
            {
                editStatus = DUPLICATE_ADDRESS;
                return QString();
            }
        }
    }
    else if(type == Receive)
    {
        // Generate a new address to associate with given label
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if(!ctx.isValid())
        {
            // Unlock wallet failed or was cancelled
            editStatus = WALLET_UNLOCK_FAILURE;
            return QString();
        }
        CPubKey newKey;
        if(!wallet->GetKeyFromPool(newKey, true))
        {
            editStatus = KEY_GENERATION_FAILURE;
            return QString();
        }
        strAddress = CBitcoinAddress(newKey.GetID()).ToString();
    }
    else
    {
        return QString();
    }
    // Add entry
    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBookName(CBitcoinAddress(strAddress).Get(), strLabel);
    }
    return QString::fromStdString(strAddress);
}

bool ChatTableModel::removeRows(int row, int count, const QModelIndex & parent)
{
    Q_UNUSED(parent);
    ChatTableEntry *rec = priv->index(row);
    if(count != 1 || !rec || rec->type == ChatTableEntry::Receiving)
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove receiving addresses.
        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        wallet->DelAddressBookName(CBitcoinAddress(rec->address.toStdString()).Get());
    }
    return true;
}

/* Look up label for address in address book, if not found return empty string.
 */
QString ChatTableModel::labelForAddress(const QString &address) const
{
    {
        LOCK(wallet->cs_wallet);
        CBitcoinAddress address_parsed(address.toStdString());
        std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(address_parsed.Get());
        if (mi != wallet->mapAddressBook.end())
        {
            return QString::fromStdString(mi->second);
        }
    }
    return QString();
}

int ChatTableModel::lookupAddress(const QString &address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void ChatTableModel::emitDataChanged(int idx)
{
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}
