//
// C++ Implementation: sshprocess
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
#include "sshprocess.h"
#include <QDir>
#include "x2gologdebug.h"
#include <QMessageBox>
#include <QTemporaryFile>
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
sshProcess::sshProcess ( QObject* parent,const QString& user,
                         const QString& host,const QString& pt,
                         const QString& cmd,const QString& pass,
                         const QString& key, bool acc )
		: QProcess ( parent )
{
	sudoErr=false;
	QString root=QDir::homePath() +"/.x2go";
	isTunnel=false;
	isCopy=false;
	fwX=false;
	sshPort=pt;
	localSocket=0l;
	serverSocket=0l;
	askpass=root+"/ssh";
	QDir dr ( askpass );
	if ( !dr.exists() )
		if ( !dr.mkpath ( askpass ) )
		{
			QString message=tr ( "Unable to create: " );
			message+=askpass;
			throw message;
		}
	askpass+="/socaskpass";
	QTemporaryFile fl ( askpass );
	fl.open();
	askpass=fl.fileName();
	fl.setAutoRemove ( false );
	fl.close();
	QFile::remove ( askpass );
	this->user=user;
	this->host=host;
	command=cmd;
	this->pass=pass;
	this->key=key;
	autoAccept=acc;
	env = QProcess::systemEnvironment();
	cleanEnv();
#ifdef Q_OS_DARWIN
	//run x2goclient from bundle
	QDir dir ( QApplication::applicationDirPath() );
	dir.cdUp();
	dir.cd ( "MacOS" );
	QString askpass_var="SSH_ASKPASS=";
	askpass_var+=dir.absolutePath() +"/x2goclient";
	env.insert ( 0, askpass_var);
#else
	env.insert ( 0, "SSH_ASKPASS=x2goclient" );
#endif
	env.insert ( 0, "X2GO_PSOCKET="+askpass );
#ifdef WINDOWS
	env.insert ( 0, "DISPLAY=localhost:0" );
	//don't care if real display is not 0
	//we need it only to start winaskpass
	//which is not X application
#endif
	setEnvironment ( env );
}


sshProcess::~sshProcess()
{
	QFile::remove
	( askpass );
	QFile::remove
	( askpass+".log" );
	if ( state() ==QProcess::Running )
		kill();
	if ( serverSocket )
	{
		serverSocket->close();
		delete serverSocket;
	}

}


void sshProcess::slot_error ( QProcess::ProcessError )
{}


void sshProcess::slot_finished ( int exitCode, QProcess::ExitStatus status )
{
	hidePass();
// 	QString resout ( readAllStandardOutput() );
	x2goDebug<<outputString<<endl;
	x2goDebug<<errorString<<endl;
	x2goDebug<<"exitCode: "<<exitCode<<" status:"<<status<<endl;
	if ( ( exitCode!=0&&exitCode!=1 ) || status !=0 )
	{
		QString resp=getResponce();
		if ( errorString.indexOf (
		            "Host key verification failed" ) !=-1 )
		{
			int res;
			if ( !autoAccept )
				res=QMessageBox::warning ( 0l,errorString,resp,
				                           tr ( "Yes" ),
				                           tr ( "No" ) );
			else
				res=0;
			if ( res==0 )
			{
				if ( isTunnel )
					startTunnel ( tunnelHost,localPort,
					              remotePort,reverse,true );
				else if ( isCopy )
					start_cp ( source,destination,true );
				else
					startNormal ( true );
			}
			else
				emit sshFinished ( false,host+":\n"+
				                   errorString,this );
		}
		else
			emit sshFinished ( false,host+":\n"+errorString,this );
	}
	else
		emit sshFinished ( true,outputString,this );
}


void sshProcess::slot_stderr()
{
	QString reserr ( readAllStandardError() );
	errorString+=reserr;
// 	x2goDebug<<reserr<<endl;
	if ( reserr.indexOf (
	            "Permission denied (publickey,keyboard-interactive)" ) !=-1
	   )
	{
		needPass=true;
		kill();
	}
	if ( reserr.indexOf ( "Authentication succeeded" ) !=-1 )
	{
		hidePass();
		emit sshTunnelOk();
	}
	if ( ( reserr.indexOf ( "Password:" ) !=-1 ) ||
	        ( reserr.indexOf ( "[sudo] password for" ) !=-1 ) )
	{
		sudoErr=true;
		emit sudoConfigError ( errorString, this );
	}
}


void sshProcess::slot_stdout()
{
	if ( isTunnel )
	{
		QString resout ( readAllStandardOutput() );
	}
	else
	{
		QString reserr ( readAllStandardOutput() );
		outputString+=reserr;
// 		x2goDebug<<reserr<<endl;

	}
}

void sshProcess::startNormal ( bool accept )
{
	x2goDebug<<"normal"<<endl;
	x2goDebug<<command<<endl;
	x2goDebug<<"host:"<<host<<endl;
	errorString="";
	needPass=false;
	disconnect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	             SLOT ( slot_error ( QProcess::ProcessError ) ) );
	disconnect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	             SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	disconnect ( this,SIGNAL ( readyReadStandardError() ),this,
	             SLOT ( slot_stderr() ) );
	disconnect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	             SLOT ( slot_stdout() ) );

	connect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	          SLOT ( slot_error ( QProcess::ProcessError ) ) );
	connect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	          SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	connect ( this,SIGNAL ( readyReadStandardError() ),this,
	          SLOT ( slot_stderr() ) );
	connect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	          SLOT ( slot_stdout() ) );

	QString cmX;
	if ( fwX )
	{
		cmX=" -X ";
	}
	if ( key!=QString::null && key!="" )
	{
		printKey ( accept );
#ifndef  WINDOWS

		start ( setsid() +" ssh "+cmX+"-i \""+key+"\" -p "+sshPort+" "+
		        user+"@"+host+" \""+command+"\"" );
#else
		start ( "ssh "+cmX+"-i \""+key+"\" -p "+sshPort+" "+user+"@"+
		        host+" \""+command+"\"" );
#endif
	}
	else
	{
		printPass ( accept );
#ifndef  WINDOWS

		start ( setsid() +" ssh" +cmX+" -p "+sshPort+" "+user+"@"+host+
		        " \""+command+"\"" );
#else

		start ( "ssh "+cmX+"-p "+sshPort+" "+user+"@"+host+" \""+
		        command+"\"" );
#endif

	}
}


void sshProcess::printPass ( bool accept )
{
	if ( serverSocket )
		delete serverSocket;
	serverSocket=new QLocalServer();
	if ( serverSocket->listen ( askpass ) )
	{
		QFile fl ( askpass );
		fl.setPermissions (
		    QFile::ReadOwner|QFile::WriteOwner );
		cleanEnv();
		passcookie=cookie();
		env.insert ( 0, "X2GO_PCOOKIE="+passcookie );
		if ( accept )
			env.insert ( 0, "X2GO_PACCEPT=yes" );
		else
			env.insert ( 0, "X2GO_PACCEPT=no" );
		setEnvironment ( env );
		connect ( serverSocket,SIGNAL ( newConnection() ),
		          this,SLOT ( slot_pass_connection() ) );
	}
}

void sshProcess::printKey ( bool accept )
{
	cleanEnv();
	env.insert ( 0, "X2GO_PCOOKIE=X2GO_RSA_DSA_KEY_USED" );
	if ( accept )
		env.insert ( 0, "X2GO_PACCEPT=yes" );
	else
		env.insert ( 0, "X2GO_PACCEPT=no" );
	setEnvironment ( env );
}

QString sshProcess::getResponce()
{
	QFile fl ( askpass+".log" );
	if ( !fl.open ( QIODevice::ReadOnly | QIODevice::Text ) )
	{
		return QString::null;
	}
	QTextStream in ( &fl );
	return in.readAll();
}

void sshProcess::hidePass()
{
	if ( serverSocket )
	{
		serverSocket->close();
		delete serverSocket;
		serverSocket=0l;
	}
	if ( QFile::exists ( askpass ) )
		QFile::remove ( askpass );
}

void sshProcess::startTunnel ( QString h,QString lp,
                               QString rp,bool rev,bool accept )
{
	x2goDebug<<"tunnel"<<endl;
	isTunnel=true;
	errorString="";
	needPass=false;

	tunnelHost=h;
	localPort=lp;
	remotePort=rp;
	reverse=rev;

	disconnect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	             SLOT ( slot_error ( QProcess::ProcessError ) ) );
	disconnect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	             SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	disconnect ( this,SIGNAL ( readyReadStandardError() ),this,
	             SLOT ( slot_stderr() ) );
	disconnect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	             SLOT ( slot_stdout() ) );

	connect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	          SLOT ( slot_error ( QProcess::ProcessError ) ) );
	connect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	          SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	connect ( this,SIGNAL ( readyReadStandardError() ),this,
	          SLOT ( slot_stderr() ) );
	connect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	          SLOT ( slot_stdout() ) );

	QString params=" -N ";
	if ( reverse )
		params+=" -R ";
	else
		params+=" -L ";
	params+=localPort+":"+tunnelHost+":"+remotePort;
	if ( key!=QString::null && key!="" )
	{
		printKey ( accept );
#ifndef  WINDOWS

		start ( setsid() +" ssh -c blowfish -v -i \""+key+"\" -p "+
		        sshPort+" "+user+"@"+host+params );
#else

		start ( "ssh -c blowfish -v -i \""+key+"\" -p "+sshPort+" "+
		        user+"@"+host+params );
#endif

	}
	else
	{
		printPass ( accept );
#ifndef  WINDOWS

		start ( setsid() +" ssh -c blowfish -v "+user+"@"+host+params+
		        " -p "+sshPort );
#else

		start ( "ssh -c blowfish -v "+user+"@"+host+params+" -p "+
		        sshPort );
#endif

	}
}

void sshProcess::start_cp ( QString src, QString dst, bool accept )
{
	x2goDebug<<"copy"<<endl;
	isCopy=true;
	errorString="";
	needPass=false;
	source=src;
	destination=dst;
	disconnect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	             SLOT ( slot_error ( QProcess::ProcessError ) ) );
	disconnect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	             SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	disconnect ( this,SIGNAL ( readyReadStandardError() ),this,
	             SLOT ( slot_stderr() ) );
	disconnect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	             SLOT ( slot_stdout() ) );

	connect ( this,SIGNAL ( error ( QProcess::ProcessError ) ),this,
	          SLOT ( slot_error ( QProcess::ProcessError ) ) );
	connect ( this,SIGNAL ( finished ( int,QProcess::ExitStatus ) ),this,
	          SLOT ( slot_finished ( int,QProcess::ExitStatus ) ) );
	connect ( this,SIGNAL ( readyReadStandardError() ),this,
	          SLOT ( slot_stderr() ) );
	connect ( this,SIGNAL ( readyReadStandardOutput() ),this,
	          SLOT ( slot_stdout() ) );

	if ( key!=QString::null && key!="" )
	{
		printKey ( accept );
#ifndef  WINDOWS

		start ( setsid() +" scp -i \""+key+"\" -P "+sshPort+" "+" \""+
		        src+"\" "+user+"@"+host+":"+dst );
#else

		start ( "scp -i \""+key+"\" -P "+sshPort+" "+" \""+src+"\" "+
		        user+"@"+host+":"+dst );

#endif

	}
	else
	{
		printPass ( accept );
#ifndef  WINDOWS

		start ( setsid() +" scp -P "+sshPort+" \""+src+"\" "+user+"@"+
		        host+":"+dst );
#else

		start ( "scp -P "+sshPort+" \""+src+"\" "+user+"@"+host+
		        ":"+dst );
#endif
	}
}


QString sshProcess::setsid()
{
#ifdef Q_OS_DARWIN
	//run setsid from bundle
	QDir dir ( QApplication::applicationDirPath() );
	dir.cdUp();
	dir.cd ( "exe" );
	return "\""+dir.absolutePath() +"/setsid\"";
#else
#ifdef Q_WS_HILDON
	return "/usr/lib/x2go/setsid";
#endif
	return "setsid";
#endif
}

void sshProcess::setErrorString ( const QString& str )
{
	if ( sudoErr )
		errorString=str;
}

QString sshProcess::cookie()
{
	QString res;
	for ( uint i=0;i<16;++i )
	{
		QString hex;
		hex.sprintf ( "%02X",qrand() );
		res+=hex;
	}
	return res;
}


void sshProcess::cleanEnv()
{
	for ( int i=env.count()-1;i>=0;--i )
	{
		if ( ( env[i].indexOf ( "X2GO_PCOOKIE" ) !=-1 ) ||
		        ( env[i].indexOf ( "X2GO_PACCEPT" ) !=-1 ) ||
		        ( env[i].indexOf ( "GPG_AGENT_INFO" ) !=-1 ) ||
		        ( env[i].indexOf ( "SSH_AUTH_SOCK" ) !=-1 ) ||
		        ( env[i].indexOf ( "SSH_AGENT_PID" ) !=-1 ) )
		{
			env.removeAt ( i );
		}
	}
}

void sshProcess::slot_pass_connection()
{
	if ( localSocket )
		delete localSocket;
	localSocket=serverSocket->nextPendingConnection ();
	if ( localSocket )
	{
		connect ( localSocket,SIGNAL ( readyRead() ),this,
		          SLOT ( slot_read_cookie_from_socket() ) );
	}
}

void sshProcess::slot_read_cookie_from_socket()
{
	char buffer[140];
	int read=localSocket->read ( buffer,139 );
	if ( read<=0 )
	{
		QString message="Cannot get cookie from askpass program";
		throw message;
	}
	buffer[read]=0;
	QString cookie ( buffer );
	if ( cookie!=passcookie )
	{
		QString message="Got wrong cookie from askpass program";
		throw message;
	}
	localSocket->write ( pass.toAscii().data(),pass.toAscii().length() );
	QTimer::singleShot ( 100, this, SLOT ( hidePass() ) );
}
