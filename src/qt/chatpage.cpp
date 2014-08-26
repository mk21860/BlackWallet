#include "chatpage.h"


#include "chattablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>


#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

ChatPage::ChatPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChatPage),
    model(0),
    optionsModel(0)
{
    setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddressButton->setIcon(QIcon());
    ui->copyToClipboard->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());
#endif

    lineEdit->setFocusPolicy(Qt::StrongFocus);
    textEdit->setFocusPolicy(Qt::NoFocus);
    textEdit->setReadOnly(true);
    listWidget->setFocusPolicy(Qt::NoFocus);

    connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
    connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
    connect(&client, SIGNAL(newMessage(QString,QString)),
             this, SLOT(appendMessage(QString,QString)));
    connect(&client, SIGNAL(newParticipant(QString)),
             this, SLOT(newParticipant(QString)));
    connect(&client, SIGNAL(participantLeft(QString)),
             this, SLOT(participantLeft(QString)));

    myNickName = client.nickName();
    newParticipant(myNickName);
    tableFormat.setBorder(0);
    QTimer::singleShot(10 * 1000, this, SLOT(showInformation()));

}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::setModel(ChatTableModel *model)
{
    this->model = model;
    if(!model)
        return;

}

void ChatPage::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

void ChatPage::appendMessage(const QString &from, const QString &message)
 {
     if (from.isEmpty() || message.isEmpty())
         return;

     QTextCursor cursor(textEdit->textCursor());
     cursor.movePosition(QTextCursor::End);
     QTextTable *table = cursor.insertTable(1, 2, tableFormat);
     table->cellAt(0, 0).firstCursorPosition().insertText('<' + from + "> ");
     table->cellAt(0, 1).firstCursorPosition().insertText(message);
     QScrollBar *bar = textEdit->verticalScrollBar();
     bar->setValue(bar->maximum());
 }
 
void ChatPage::returnPressed()
 {
     QString text = lineEdit->text();
     if (text.isEmpty())
         return;

     if (text.startsWith(QChar('/'))) {
         QColor color = textEdit->textColor();
         textEdit->setTextColor(Qt::red);
         textEdit->append(tr("! Unknown command: %1")
                          .arg(text.left(text.indexOf(' '))));
         textEdit->setTextColor(color);
     } else {
         client.sendMessage(text);
         appendMessage(myNickName, text);
     }

     lineEdit->clear();
 }

void ChatPage::newParticipant(const QString &nick)
 {
     if (nick.isEmpty())
         return;

     QColor color = textEdit->textColor();
     textEdit->setTextColor(Qt::gray);
     textEdit->append(tr("* %1 has joined").arg(nick));
     textEdit->setTextColor(color);
     listWidget->addItem(nick);
 }
 
void ChatPage::participantLeft(const QString &nick)
 {
     if (nick.isEmpty())
         return;

     QList<QListWidgetItem *> items = listWidget->findItems(nick,
                                                            Qt::MatchExactly);
     if (items.isEmpty())
         return;

     delete items.at(0);
     QColor color = textEdit->textColor();
     textEdit->setTextColor(Qt::gray);
     textEdit->append(tr("* %1 has left").arg(nick));
     textEdit->setTextColor(color);
 }
 
void ChatPage::showInformation()
 {
 //disable info for now
 /*
     if (listWidget->count() == 1) {
         QMessageBox::information(this, tr("Chat"),
                                  tr("Launch several instances of this "
                                     "program on your local network and "
                                     "start chatting!"));
     }
*/	 
 }