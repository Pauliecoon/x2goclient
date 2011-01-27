//
// C++ Interface: sshprocess
//
// Description:
//
//
// Author: Oleksandr Shneyder <oleksandr.shneyder@obviously-nice.de>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "x2goclientconfig.h"
#ifndef SSHPROCESS_H
#define SSHPROCESS_H

#include <QProcess>

/**
	@author Oleksandr Shneyder <oleksandr.shneyder@obviously-nice.de>
*/
class QLocalServer;
class QLocalSocket;
class sshProcess : public QProcess
{
		Q_OBJECT
	public:
		sshProcess ( QObject* parent, const QString& user,
		             const QString& host,const QString& pt,
		             const QString& cmd, const QString& pass,
		             const QString& key=QString::null,
		             bool acc=false );
		virtual ~sshProcess();
		void startNormal ( bool accept=false );
		QString getResponce();
		void startTunnel ( QString host,QString localPort,QString remotePort,
		                   bool reverse=false,bool accept=false );
		void start_cp ( QString src, QString dst, bool accept=false );
		QString getSource() {return source;}
		QString setsid();
		void setErrorString ( const QString& str );
		void setFwX ( bool s ) {fwX=s;}
    virtual void setEnvironment(QStringList newEnv);
	private:
		QString askpass;
		QString user;
		QString host;
		QString command;
		QString pass;
		QString key;
		QString errorString;
		QString outputString;
		QString passcookie;
		QLocalServer* serverSocket;
		QLocalSocket* localSocket;
		bool needPass;
		bool autoAccept;
		bool isTunnel;
		bool isCopy;
		QString sshPort;
		QString tunnelHost;
		QString localPort;
		QString source;
		QString destination;
		QString remotePort;
		QStringList env;
		bool reverse;
		bool fwX;
		bool sudoErr;


	private slots:
		void slot_error ( QProcess::ProcessError );
		void slot_finished ( int, QProcess::ExitStatus );
		void slot_stderr();
		void slot_stdout();
		void hidePass();
		void slot_pass_connection();
		void slot_read_cookie_from_socket();
	private:
		void printPass ( bool accept=false );
		void printKey ( bool accept=false );
		QString cookie();
		void cleanEnv(bool all=false);
	signals:
		void sshFinished ( bool,QString,sshProcess* );
		void sudoConfigError ( QString,sshProcess* );
		void sshTunnelOk();
};

#endif
