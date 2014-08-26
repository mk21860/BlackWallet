#ifndef CHATPAGE_H
#define CHATPAGE_H
#include "client.h"
#include "ui_chatpage.h"
#include <QDialog>
#include <QtGui>

namespace Ui {
    class ChatPage;
}

class ChatTableModel;
class OptionsModel;

QT_BEGIN_NAMESPACE
//class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class ChatPage : public QDialog, private Ui::ChatPage
{
    Q_OBJECT

public:

    ChatPage(QWidget *parent = 0);
    ~ChatPage();

    void setModel(ChatTableModel *model);
    void setOptionsModel(OptionsModel *optionsModel);

public slots:
	void appendMessage(const QString &from, const QString &message);

private:
    Ui::ChatPage *ui;
    ChatTableModel *model;
    OptionsModel *optionsModel;
	Client client;
    QString myNickName;
    QTextTableFormat tableFormat;

private slots:
    void returnPressed();
    void newParticipant(const QString &nick);
    void participantLeft(const QString &nick);
    void showInformation();
    
signals:

};

#endif // ADDRESSBOOKDIALOG_H