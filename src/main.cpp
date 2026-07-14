#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>


class History
{
public:

    History()
    {
        path = QDir::homePath() +
               "/.local/share/runbox_history";

        load();

        index = commands.size();
    }


    void add(QString command)
    {
        command = command.trimmed();

        if(command.isEmpty())
            return;

        commands.removeAll(command);
        commands.append(command);

        index = commands.size();

        save();
    }


    QString last()
    {
        if(commands.isEmpty())
            return "";

        return commands.last();
    }


    QString previous()
    {
        if(commands.isEmpty())
            return "";

        if(index > 0)
            index--;

        return commands[index];
    }


    QString next()
    {
        if(commands.isEmpty())
            return "";

        if(index < commands.size() - 1)
        {
            index++;
            return commands[index];
        }

        index = commands.size();

        return "";
    }


private:

    QStringList commands;
    QString path;
    int index;


    void load()
    {
        QFile file(path);

        if(file.open(QIODevice::ReadOnly |
                     QIODevice::Text))
        {
            QTextStream in(&file);

            while(!in.atEnd())
                commands.append(in.readLine());
        }
    }


    void save()
    {
        QDir().mkpath(
            QDir::homePath() +
            "/.local/share"
        );


        QFile file(path);

        if(file.open(QIODevice::WriteOnly |
                     QIODevice::Text))
        {
            QTextStream out(&file);

            for(QString cmd : commands)
                out << cmd << "\n";
        }
    }
};



bool executeCommand(QString cmd)
{
    cmd = cmd.trimmed();

    if(cmd.isEmpty())
        return false;


    if(cmd == "calc")
        cmd = "kcalc";



    // URLs
    if(cmd.startsWith("http://") ||
       cmd.startsWith("https://") ||
       (!cmd.contains(" ") && cmd.contains(".")))
    {

        if(!cmd.startsWith("http"))
            cmd = "https://" + cmd;


        return QProcess::startDetached(
            "xdg-open",
            {cmd}
        );
    }



    QFileInfo file(cmd);


    if(file.exists())
    {
        return QProcess::startDetached(
            "xdg-open",
            {file.absoluteFilePath()}
        );
    }



    QString firstWord =
        cmd.split(" ").first();


    QStringList terminalCommands =
    {
        "ping",
        "ssh",
        "top",
        "htop",
        "nano",
        "vim"
    };


    if(terminalCommands.contains(firstWord))
    {
        return QProcess::startDetached(
            "konsole",
            {
                "--hold",
                "-e",
                "bash",
                "-c",
                cmd
            }
        );
    }



    QStringList parts =
        cmd.split(QRegularExpression("\\s+"),
                  Qt::SkipEmptyParts);


    QString program =
        parts.takeFirst();



    if(QStandardPaths::findExecutable(program).isEmpty())
    {
        return false;
    }


    return QProcess::startDetached(
        program,
        parts
    );
}





class RunLineEdit : public QLineEdit
{
public:

    History *history;


    RunLineEdit(History *h)
        :
          history(h)
    {
    }


protected:

    void keyPressEvent(QKeyEvent *event) override
    {
        if(event->key() == Qt::Key_Up)
        {
            setText(history->previous());
        }
        else if(event->key() == Qt::Key_Down)
        {
            setText(history->next());
        }
        else
        {
            QLineEdit::keyPressEvent(event);
        }
    }
};





int main(int argc, char *argv[])
{
    QApplication app(argc, argv);



    QString serverName =
        "RunBoxSingleInstance";



    // Check existing instance first
    QLocalSocket socket;

    socket.connectToServer(serverName);


    if(socket.waitForConnected(100))
    {
        socket.write("show");
        socket.waitForBytesWritten(100);

        return 0;
    }



    // Create server for first instance
    QLocalServer *server =
        new QLocalServer();



    QLocalServer::removeServer(serverName);

    server->listen(serverName);




    QDialog window;


    window.setWindowFlags(
        Qt::Dialog |
        Qt::CustomizeWindowHint |
        Qt::WindowTitleHint
    );


    window.setWindowTitle("Run");


    window.setFixedSize(
        500,
        170
    );



    auto *layout =
        new QVBoxLayout(&window);



    auto *description =
        new QLabel(
        "Type the name of a program, folder, document,\n"
        "or Internet resource, and RunBox will open it."
        );



    History history;



    auto *input =
        new RunLineEdit(&history);



    input->setText(history.last());
    input->selectAll();



    auto *inputLayout =
        new QHBoxLayout();


    inputLayout->addWidget(
        new QLabel("Open:")
    );


    inputLayout->addWidget(input);



    auto *buttonLayout =
        new QHBoxLayout();



    auto *ok =
        new QPushButton("OK");


    auto *cancel =
        new QPushButton("Cancel");



    ok->setAutoDefault(false);
    cancel->setAutoDefault(false);



    buttonLayout->addStretch();

    buttonLayout->addWidget(ok);

    buttonLayout->addWidget(cancel);



    layout->addWidget(description);

    layout->addLayout(inputLayout);

    layout->addLayout(buttonLayout);



    // Second launch handler
    QObject::connect(
        server,
        &QLocalServer::newConnection,
        [&]()
        {
            QLocalSocket *client =
                server->nextPendingConnection();


            client->waitForReadyRead(100);


            QByteArray message =
                client->readAll();


            if(message == "show")
            {
                QTimer::singleShot(
                    50,
                    [&]()
                    {
                        window.showNormal();
                        window.raise();
                        window.activateWindow();

                        input->setFocus();
                        input->selectAll();
                    }
                );
            }


            client->disconnectFromServer();
        }
    );



    bool running = false;



    auto run = [&]()
    {
        if(running)
            return;


        running = true;


        QString cmd =
            input->text();



        if(executeCommand(cmd))
        {
            history.add(cmd);
            window.close();
        }
        else
        {
            running = false;

            QMessageBox::warning(
                &window,
                "RunBox",
                "Cannot find:\n\n" + cmd
            );
        }
    };



    QObject::connect(
        ok,
        &QPushButton::clicked,
        run
    );


    QObject::connect(
        input,
        &QLineEdit::returnPressed,
        run
    );



    QObject::connect(
        cancel,
        &QPushButton::clicked,
        &window,
        &QDialog::reject
    );



    input->setFocus();


    window.show();



    return app.exec();
}