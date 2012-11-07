/***************************************************************************
*   Copyright (C) 2012  Michael Carpenter (malcom2073)                     *
*                                                                          *
*   This file is a part of EMStudio                                        *
*                                                                          *
*   EMStudio is free software: you can redistribute it and/or modify       *
*   it under the terms of the GNU General Public License version 2 as      *
*   published by the Free Software Foundation.                             *
*                                                                          *
*   EMStudio is distributed in the hope that it will be useful,            *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*   GNU General Public License for more details.                           *
									   *
*   You should have received a copy of the GNU General Public License      *
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
****************************************************************************/

#include "mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include "datafield.h"
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSettings>
#include <tableview2d.h>
#include <qjson/parser.h>
#define define2string_p(x) #x
#define define2string(x) define2string_p(x)
#define TABLE_3D_PAYLOAD_SIZE 1024
#define TABLE_2D_PAYLOAD_SIZE 64
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	qRegisterMetaType<MemoryLocationInfo>("MemoryLocationInfo");
	qDebug() << "EMStudio commit:" << define2string(GIT_COMMIT);
	qDebug() << "Full hash:" << define2string(GIT_HASH);
	progressView=0;
	m_interrogationInProgress = false;

	emsData = new EmsData();
	m_settingsFile = "settings.ini";
	//TODO: Figure out proper directory names
#ifdef Q_OS_WIN32
	QString appDataDir = getenv("%AppData%");
	appDataDir = appDataDir.replace("\\","/");
	if (!QDir(appDataDir).exists("EMStudio"))
	{
		QDir(appDataDir).mkpath("EMStudio");
	}
	m_defaultsDir = QString(getenv("%ProgramFiles%")).replace("\\","/") + "/EMStudio";
	m_settingsDir = appDataDir + "/" + "EMStudio";
	//m_settingsFile = appDataDir + "/" + "EMStudio/EMStudio-config.ini";
//#elif Q_OS_MAC <- Does not exist. Need OSX checking capabilities somewhere...
	//Oh wait, it does not exist since I'm developing on a *nix box.
//#elif Q_OS_LINUX
#else
	QString appDataDir = getenv("HOME");
	if (!QDir(appDataDir).exists(".EMStudio"))
	{
		QDir(appDataDir).mkpath(".EMStudio");
	}
	m_defaultsDir = "/usr/share/EMStudio";
	m_settingsDir = appDataDir + "/" + ".EMStudio";
	//m_settingsFile = appDataDir + "/" + ".EMStudio/EMStudio-config.ini";
#endif

	//Settings, then defaults, then fallback to local
	if (QFile::exists(m_settingsDir + "/EMStudio-config.ini"))
	{
		m_settingsFile = m_settingsDir + "/EMStudio-config.ini";
	}
	else if (QFile::exists(m_defaultsDir + "/EMStudio-config.ini"))
	{
		m_settingsFile = m_defaultsDir + "/EMStudio-config.ini";
	}
	else if (QFile::exists("EMStudio-config.ini"))
	{
		m_settingsFile = "EMStudio-config.ini";
	}

	//Settings, then defaults, then fallback to local
	QString filestr = "";
	if (QFile::exists(m_settingsDir + "/" + "definitions/freeems.config.json"))
	{
		filestr = m_settingsDir + "/" + "definitions/freeems.config.json";
	}
	else if (QFile::exists(m_defaultsDir + "/definitions/freeems.config.json"))
	{
		filestr = m_defaultsDir + "/definitions/freeems.config.json";
	}
	else if (QFile::exists("freeems.config.json"))
	{
		filestr = "freeems.config.json";
	}
	else
	{
		QMessageBox::information(0,"Error","Error: No freeems.config.json file found!");
	}
	m_memoryMetaData.loadMetaDataFromFile(filestr);

	QFile decoderfile("decodersettings.json");
	decoderfile.open(QIODevice::ReadOnly);
	QByteArray decoderfilebytes = decoderfile.readAll();
	decoderfile.close();

	QJson::Parser decoderparser;
	QVariant decodertop = decoderparser.parse(decoderfilebytes);
	if (!decodertop.isValid())
	{
		QString errormsg = QString("Error parsing JSON from config file on line number: ") + QString::number(decoderparser.errorLine()) + " error text: " + decoderparser.errorString();
		QMessageBox::information(0,"Error",errormsg);
		qDebug() << "Error parsing JSON";
		qDebug() << "Line number:" << decoderparser.errorLine() << "error text:" << decoderparser.errorString();
		return;
	}
	QVariantMap decodertopmap = decodertop.toMap();
	QVariantMap decoderlocationmap = decodertopmap["locations"].toMap();
	QString str = decoderlocationmap["locationid"].toString();
	bool ok = false;
	unsigned short locid = str.toInt(&ok,16);
	QVariantList decodervalueslist = decoderlocationmap["values"].toList();
	QList<ConfigBlock> blocklist;
	for (int i=0;i<decodervalueslist.size();i++)
	{
		QVariantMap tmpmap = decodervalueslist[i].toMap();
		ConfigBlock block;
		block.setName(tmpmap["name"].toString());
		block.setType(tmpmap["type"].toString());
		block.setElementSize(tmpmap["sizeofelement"].toInt());
		block.setSize(tmpmap["size"].toInt());
		block.setOffset(tmpmap["offset"].toInt());
		QList<QPair<QString, double> > calclist;
		QVariantList calcliststr = tmpmap["calc"].toList();
		for (int j=0;j<calcliststr.size();j++)
		{
			qDebug() << "XCalc:" << calcliststr[j].toMap()["type"].toString() << calcliststr[j].toMap()["value"].toDouble();
			calclist.append(QPair<QString,double>(calcliststr[j].toMap()["type"].toString(),calcliststr[j].toMap()["value"].toDouble()));
		}
		block.setCalc(calclist);
		blocklist.append(block);
	}
	m_configBlockMap[locid] = blocklist;
/*

		anglesOfTDC: {ANGLE(0), ANGLE(90), ANGLE(180), ANGLE(270), ANGLE(360), ANGLE(450), ANGLE(540), ANGLE(630),ANGLE(0),ANGLE(360)},
		outputEventPinNumbers:       {0,0,0,0,0,0,0,0,2,4}, // LTCC e-dizzy, semi-sequential injection 1/6, 8/5, 4/7, 3/2, and repeat
		schedulingConfigurationBits: {0,0,0,0,0,0,0,0,1,1}, // See below two lines
		decoderEngineOffset:               ANGLE(0.00), // Dist is at 0 degrees.
		numberOfConfiguredOutputEvents:             10, // First half ignition, second half injection
		numberOfInjectionsPerEngineCycle:            4  // Full sync semi-sequential
*/

	qDebug() << m_memoryMetaData.errorMap().keys().size() << "Error Keys Loaded";
	qDebug() << m_memoryMetaData.table3DMetaData().size() << "3D Tables Loaded";
	qDebug() << m_memoryMetaData.table2DMetaData().size() << "2D Tables Loaded";
	//return;
	m_currentRamLocationId=0;
	//populateDataFields();
	m_localRamDirty = false;
	m_deviceFlashDirty = false;
	m_waitingForRamWriteConfirmation = false;
	m_waitingForFlashWriteConfirmation = false;
	ui.setupUi(this);
	connect(ui.actionSave_Offline_Data,SIGNAL(triggered()),this,SLOT(menu_file_saveOfflineDataClicked()));
	connect(ui.actionEMS_Status,SIGNAL(triggered()),this,SLOT(menu_windows_EmsStatusClicked()));
	connect(ui.actionLoad_Offline_Data,SIGNAL(triggered()),this,SLOT(menu_file_loadOfflineDataClicked()));
	this->setWindowTitle(QString("EMStudio ") + QString(define2string(GIT_COMMIT)));
	emsinfo.emstudioCommit = define2string(GIT_COMMIT);
	emsinfo.emstudioHash = define2string(GIT_HASH);
	ui.actionDisconnect->setEnabled(false);
	connect(ui.actionSettings,SIGNAL(triggered()),this,SLOT(menu_settingsClicked()));
	connect(ui.actionConnect,SIGNAL(triggered()),this,SLOT(menu_connectClicked()));
	connect(ui.actionDisconnect,SIGNAL(triggered()),this,SLOT(menu_disconnectClicked()));
	connect(ui.actionEMS_Info,SIGNAL(triggered()),this,SLOT(menu_windows_EmsInfoClicked()));
	connect(ui.actionGauges,SIGNAL(triggered()),this,SLOT(menu_windows_GaugesClicked()));
	connect(ui.actionTables,SIGNAL(triggered()),this,SLOT(menu_windows_TablesClicked()));
	connect(ui.actionFlags,SIGNAL(triggered()),this,SLOT(menu_windows_FlagsClicked()));
	connect(ui.actionExit_3,SIGNAL(triggered()),this,SLOT(close()));
	connect(ui.actionPacket_Status,SIGNAL(triggered()),this,SLOT(menu_windows_PacketStatusClicked()));
	connect(ui.actionAbout,SIGNAL(triggered()),this,SLOT(menu_aboutClicked()));

	emsInfo=0;
	dataTables=0;
	dataFlags=0;
	dataGauges=0;

	dataPacketDecoder = new DataPacketDecoder(this);
	connect(dataPacketDecoder,SIGNAL(payloadDecoded(QVariantMap)),this,SLOT(dataLogDecoded(QVariantMap)));

	emsComms = new FreeEmsComms(this);
	m_logFileName = QDateTime::currentDateTime().toString("yyyy.MM.dd-hh.mm.ss");
	emsComms->setLogFileName(m_logFileName);
	connect(emsComms,SIGNAL(error(QString)),this,SLOT(error(QString)));
	connect(emsComms,SIGNAL(commandTimedOut(int)),this,SLOT(commandTimedOut(int)));
	connect(emsComms,SIGNAL(connected()),this,SLOT(emsCommsConnected()));
	connect(emsComms,SIGNAL(disconnected()),this,SLOT(emsCommsDisconnected()));
	connect(emsComms,SIGNAL(dataLogPayloadReceived(QByteArray,QByteArray)),this,SLOT(logPayloadReceived(QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(firmwareVersion(QString)),this,SLOT(firmwareVersion(QString)));
	connect(emsComms,SIGNAL(compilerVersion(QString)),this,SLOT(emsCompilerVersion(QString)));
	connect(emsComms,SIGNAL(interfaceVersion(QString)),this,SLOT(interfaceVersion(QString)));
	connect(emsComms,SIGNAL(locationIdList(QList<unsigned short>)),this,SLOT(locationIdList(QList<unsigned short>)));
	connect(emsComms,SIGNAL(unknownPacket(QByteArray,QByteArray)),this,SLOT(unknownPacket(QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(commandSuccessful(int)),this,SLOT(commandSuccessful(int)));
	connect(emsComms,SIGNAL(commandFailed(int,unsigned short)),this,SLOT(commandFailed(int,unsigned short)));
	connect(emsComms,SIGNAL(locationIdInfo(unsigned short,unsigned short,QList<FreeEmsComms::LocationIdFlags>,unsigned short,unsigned char,unsigned char,unsigned short,unsigned short,unsigned short)),this,SLOT(locationIdInfo(unsigned short,unsigned short,QList<FreeEmsComms::LocationIdFlags>,unsigned short,unsigned char,unsigned char,unsigned short,unsigned short,unsigned short)));
	connect(emsComms,SIGNAL(locationIdInfo(unsigned short,MemoryLocationInfo)),this,SLOT(locationIdInfo(unsigned short,MemoryLocationInfo)));
	connect(emsComms,SIGNAL(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(interrogateRamBlockRetrieved(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(interrogateFlashBlockRetrieved(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(decoderName(QString)),this,SLOT(emsDecoderName(QString)));
	connect(emsComms,SIGNAL(operatingSystem(QString)),this,SLOT(emsOperatingSystem(QString)));
	connect(emsComms,SIGNAL(firmwareBuild(QString)),this,SLOT(emsFirmwareBuildDate(QString)));

	emsInfo = new EmsInfoView();
	emsInfo->setFirmwareVersion(m_firmwareVersion);
	emsInfo->setInterfaceVersion(m_interfaceVersion);
	connect(emsInfo,SIGNAL(displayLocationId(int,bool,int)),this,SLOT(emsInfoDisplayLocationId(int,bool,int)));

	emsMdiWindow = ui.mdiArea->addSubWindow(emsInfo);
	emsMdiWindow->setGeometry(emsInfo->geometry());
	emsMdiWindow->hide();
	emsMdiWindow->setWindowTitle(emsInfo->windowTitle());

	aboutView = new AboutView();
	aboutView->setHash(define2string(GIT_HASH));
	aboutView->setCommit(define2string(GIT_COMMIT));
	aboutMdiWindow = ui.mdiArea->addSubWindow(aboutView);
	aboutMdiWindow->setGeometry(aboutView->geometry());
	aboutMdiWindow->hide();
	aboutMdiWindow->setWindowTitle(aboutView->windowTitle());

	dataGauges = new GaugeView();
	//connect(dataGauges,SIGNAL(destroyed()),this,SLOT(dataGaugesDestroyed()));
	gaugesMdiWindow = ui.mdiArea->addSubWindow(dataGauges);
	gaugesMdiWindow->setGeometry(dataGauges->geometry());
	gaugesMdiWindow->hide();
	gaugesMdiWindow->setWindowTitle(dataGauges->windowTitle());

	if (QFile::exists(m_defaultsDir + "/" + "dashboards/gauges.qml"))
	{
		//qml file is in the program files directory, or in /usr/share
		dataGauges->setFile(m_defaultsDir + "/" + "dashboards/gauges.qml");
	}
	else if (QFile::exists("src/gauges.qml"))
	{
		//We're operating out of the src directory
		dataGauges->setFile("src/gauges.qml");
	}
	else
	{
		//Running with no install, but not src?? Still handle it.
		dataGauges->setFile("gauges.qml");
	}

	dataTables = new TableView();
	//connect(dataTables,SIGNAL(destroyed()),this,SLOT(dataTablesDestroyed()));
	dataTables->passDecoder(dataPacketDecoder);
	tablesMdiWindow = ui.mdiArea->addSubWindow(dataTables);
	tablesMdiWindow->setGeometry(dataTables->geometry());
	tablesMdiWindow->hide();
	tablesMdiWindow->setWindowTitle(dataTables->windowTitle());

	statusView = new EmsStatus(this);
	this->addDockWidget(Qt::RightDockWidgetArea,statusView);
	connect(statusView,SIGNAL(hardResetRequest()),this,SLOT(emsStatusHardResetRequested()));
	connect(statusView,SIGNAL(softResetRequest()),this,SLOT(emsStatusSoftResetRequested()));

	dataFlags = new FlagView();
	dataFlags->passDecoder(dataPacketDecoder);
	flagsMdiWindow = ui.mdiArea->addSubWindow(dataFlags);
	flagsMdiWindow->setGeometry(dataFlags->geometry());
	flagsMdiWindow->hide();
	flagsMdiWindow->setWindowTitle(dataFlags->windowTitle());

	packetStatus = new PacketStatusView();
	connect(emsComms,SIGNAL(packetSent(unsigned short,QByteArray,QByteArray)),packetStatus,SLOT(passPacketSent(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(packetAcked(unsigned short,QByteArray,QByteArray)),packetStatus,SLOT(passPacketAck(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(packetNaked(unsigned short,QByteArray,QByteArray,unsigned short)),packetStatus,SLOT(passPacketNak(unsigned short,QByteArray,QByteArray,unsigned short)));
	connect(emsComms,SIGNAL(decoderFailure(QByteArray)),packetStatus,SLOT(passDecoderFailure(QByteArray)));
	packetStatusMdiWindow = ui.mdiArea->addSubWindow(packetStatus);
	packetStatusMdiWindow->setGeometry(packetStatus->geometry());
	packetStatusMdiWindow->hide();
	packetStatusMdiWindow->setWindowTitle(packetStatus->windowTitle());

	//Load settings
	qDebug() << "Local settings file is:" << m_settingsFile;
	QSettings settings(m_settingsFile,QSettings::IniFormat);
	settings.beginGroup("comms");
	m_comPort = settings.value("port","/dev/ttyUSB0").toString();
	m_comBaud = settings.value("baud",115200).toInt();
	m_comInterByte = settings.value("interbytedelay",0).toInt();
	m_saveLogs = settings.value("savelogs",true).toBool();
	m_clearLogs = settings.value("clearlogs",false).toBool();
	m_logsToKeep = settings.value("logstokeep",0).toInt();
	m_logDirectory = settings.value("logdir",".").toString();
	settings.endGroup();

	emsComms->setBaud(m_comBaud);
	emsComms->setPort(m_comPort);
	emsComms->setLogDirectory(m_logDirectory);
	emsComms->setLogsEnabled(m_saveLogs);
	emsComms->setInterByteSendDelay(m_comInterByte);

	pidcount = 0;

	timer = new QTimer(this);
	connect(timer,SIGNAL(timeout()),this,SLOT(timerTick()));
	timer->start(1000);

	guiUpdateTimer = new QTimer(this);
	connect(guiUpdateTimer,SIGNAL(timeout()),this,SLOT(guiUpdateTimerTick()));
	guiUpdateTimer->start(250);

	statusBar()->addWidget(ui.ppsLabel);
	statusBar()->addWidget(ui.statusLabel);


	logfile = new QFile("myoutput.log");
	logfile->open(QIODevice::ReadWrite | QIODevice::Truncate);
}
void MainWindow::menu_file_saveOfflineDataClicked()
{
	QString filename = QFileDialog::getSaveFileName(this,"Save Offline File",".","Offline JSON Files (*.json)");
	if (filename == "")
	{
		return;
	}
	QVariantMap top;
	QVariantMap flashMap;
	QVariantMap metaMap;
	QVariantMap ramMap;
	QMap<unsigned short,MemoryLocationInfo>::const_iterator i = m_memoryInfoMap.constBegin();
	while (i != m_memoryInfoMap.constEnd())
	{
		QVariantMap id;
		id["id"] = QString::number((unsigned short)i.key(),16).toUpper();
		id["flags"] = QString::number((unsigned short)i.value().rawflags,16).toUpper();
		id["rampage"] = QString::number((unsigned char)i.value().rampage,16).toUpper();
		id["flashpage"] = QString::number((unsigned char)i.value().flashpage,16).toUpper();
		id["ramaddress"] = QString::number((unsigned short)i.value().ramaddress,16).toUpper();
		id["flashaddress"] = QString::number((unsigned short)i.value().flashaddress,16).toUpper();
		id["parent"] = QString::number((unsigned short)i.value().parent,16).toUpper();
		id["size"] = QString::number((unsigned short)i.value().size,16).toUpper();
		metaMap[QString::number((unsigned short)i.key(),16).toUpper()] = id;
		if (!i.value().flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			if (!i.value().flags.contains(FreeEmsComms::BLOCK_IS_READ_ONLY))
			{
				if (i.value().flags.contains(FreeEmsComms::BLOCK_IS_FLASH))
				{
					QString val = "";

					QByteArray block = emsData->getDeviceFlashBlock(i.key());
					for (int j=0;j<block.size();j++)
					{
						val += QString::number((unsigned char)block[j],16).toUpper() + ",";
					}
					val = val.mid(0,val.length()-1);
					flashMap[QString::number((unsigned short)i.key(),16).toUpper()] = val;
				}
				if (i.value().flags.contains(FreeEmsComms::BLOCK_IS_RAM))
				{
					QString val = "";
					QByteArray block = emsData->getDeviceRamBlock(i.key());
					for (int j=0;j<block.size();j++)
					{
						val += QString::number((unsigned char)block[j],16).toUpper() + ",";
					}
					val = val.mid(0,val.length()-1);
					ramMap[QString::number((unsigned short)i.key(),16).toUpper()] = val;
				}
			}
		}
		i++;
	}
	top["ram"] = ramMap;
	top["flash"] = flashMap;
	top["meta"] = metaMap;
	QJson::Serializer serializer;
	QByteArray out = serializer.serialize(top);
	QFile outfile(filename);
	outfile.open(QIODevice::ReadWrite | QIODevice::Truncate);
	outfile.write(out);
	outfile.flush();
	outfile.close();
}

void MainWindow::menu_file_loadOfflineDataClicked()
{
	QString filename = QFileDialog::getOpenFileName(this,"Load Offline File",".","Offline JSON Files (*.json)");
	if (filename == "")
	{
		return;
	}
	//QByteArray out = serializer.serialize(top);
	QFile outfile(filename);
	outfile.open(QIODevice::ReadOnly);
	QByteArray out = outfile.readAll();
	outfile.close();

	QJson::Parser parser;
	bool ok = false;
	QVariant outputvar = parser.parse(out,&ok);
	if (!ok)
	{
		qDebug() << "Error parsing json:" << parser.errorString();
		return;
	}
	QVariantMap top = outputvar.toMap();
	QVariantMap ramMap = top["ram"].toMap();
	QVariantMap flashMap = top["flash"].toMap();
	QVariantMap metaMap = top["meta"].toMap();
	QVariantMap::const_iterator i = metaMap.constBegin();
	while (i != metaMap.constEnd())
	{
		bool ok;
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = i.key().toInt();
		m_tempMemoryList.append(loc);
		unsigned short flags =i.value().toMap()["flags"].toString().toInt(&ok,16);
		unsigned char rampage = i.value().toMap()["rampage"].toString().toInt(&ok,16);
		unsigned short ramaddress = i.value().toMap()["ramaddress"].toString().toInt(&ok,16);
		unsigned char flashpage = i.value().toMap()["flashpage"].toString().toInt(&ok,16);
		unsigned short flashaddress = i.value().toMap()["flags"].toString().toInt(&ok,16);
		unsigned short parent = i.value().toMap()["parent"].toString().toInt(&ok,16);
		unsigned short size = i.value().toMap()["size"].toString().toInt(&ok,16);
		unsigned short id = i.key().toInt(&ok,16);
		QList<FreeEmsComms::LocationIdFlags> m_blockFlagList;
		m_blockFlagList.append(FreeEmsComms::BLOCK_HAS_PARENT);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_RAM);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_FLASH);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_INDEXABLE);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_READ_ONLY);
		m_blockFlagList.append(FreeEmsComms::BLOCK_GETS_VERIFIED);
		m_blockFlagList.append(FreeEmsComms::BLOCK_FOR_BACKUP_RESTORE);
		m_blockFlagList.append(FreeEmsComms::BLOCK_SPARE_7);
		m_blockFlagList.append(FreeEmsComms::BLOCK_SPARE_8);
		m_blockFlagList.append(FreeEmsComms::BLOCK_SPARE_9);
		m_blockFlagList.append(FreeEmsComms::BLOCK_SPARE_10);
		m_blockFlagList.append(FreeEmsComms::BLOCK_SPARE_11);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_2D_TABLE);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_MAIN_TABLE);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_LOOKUP_DATA);
		m_blockFlagList.append(FreeEmsComms::BLOCK_IS_CONFIGURATION);
		QList<FreeEmsComms::LocationIdFlags> nflags;
		for (int j=0;j<m_blockFlagList.size();j++)
		{
			if (flags & m_blockFlagList[j])
			{
				nflags.append(m_blockFlagList[j]);
			}
		}
		locationIdInfo(id,flags,nflags,parent,rampage,flashpage,ramaddress,flashaddress,size);
		i++;
	}
	populateParentLists();

	i = ramMap.constBegin();
	while (i != ramMap.constEnd())
	{
		bool ok = false;
		//ramBlockRetrieved(unsigned short locationid,QByteArray header,QByteArray payload)
		QString val = i.value().toString();
		QStringList valsplit = val.split(",");
		QByteArray bytes;
		for (int j=0;j<valsplit.size();j++)
		{
			bytes.append(valsplit[j].toInt(&ok,16));
		}
		ramBlockRetrieved(i.key().toInt(&ok,16),QByteArray(),bytes);
		i++;
	}

	i = flashMap.constBegin();
	while (i != flashMap.constEnd())
	{
		bool ok = false;
		//qDebug() << "Flash location" << i.key().toInt(&ok,16);
		QString val = i.value().toString();
		QStringList valsplit = val.split(",");
		QByteArray bytes;
		for (int j=0;j<valsplit.size();j++)
		{
			bytes.append(valsplit[j].toInt(&ok,16));
		}
		flashBlockRetrieved(i.key().toInt(&ok,16),QByteArray(),bytes);
		i++;
	}
	checkRamFlashSync();
	emsMdiWindow->show();
}

void MainWindow::emsCommsDisconnected()
{
	ui.actionConnect->setEnabled(true);
	ui.actionDisconnect->setEnabled(false);
	emsComms = 0;

	//Need to reset everything here.
	emsComms = new FreeEmsComms(this);
	emsComms->setLogFileName(m_logFileName);
	connect(emsComms,SIGNAL(connected()),this,SLOT(emsCommsConnected()));
	connect(emsComms,SIGNAL(error(QString)),this,SLOT(error(QString)));
	connect(emsComms,SIGNAL(disconnected()),this,SLOT(emsCommsDisconnected()));
	connect(emsComms,SIGNAL(dataLogPayloadReceived(QByteArray,QByteArray)),this,SLOT(logPayloadReceived(QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(firmwareVersion(QString)),this,SLOT(firmwareVersion(QString)));
	connect(emsComms,SIGNAL(decoderName(QString)),this,SLOT(emsDecoderName(QString)));
	connect(emsComms,SIGNAL(compilerVersion(QString)),this,SLOT(emsCompilerVersion(QString)));
	connect(emsComms,SIGNAL(interfaceVersion(QString)),this,SLOT(interfaceVersion(QString)));
	connect(emsComms,SIGNAL(operatingSystem(QString)),this,SLOT(emsOperatingSystem(QString)));
	connect(emsComms,SIGNAL(locationIdList(QList<unsigned short>)),this,SLOT(locationIdList(QList<unsigned short>)));
	connect(emsComms,SIGNAL(firmwareBuild(QString)),this,SLOT(emsFirmwareBuildDate(QString)));
	connect(emsComms,SIGNAL(unknownPacket(QByteArray,QByteArray)),this,SLOT(unknownPacket(QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(commandSuccessful(int)),this,SLOT(commandSuccessful(int)));
	connect(emsComms,SIGNAL(commandTimedOut(int)),this,SLOT(commandTimedOut(int)));
	connect(emsComms,SIGNAL(commandFailed(int,unsigned short)),this,SLOT(commandFailed(int,unsigned short)));
	connect(emsComms,SIGNAL(locationIdInfo(unsigned short,unsigned short,QList<FreeEmsComms::LocationIdFlags>,unsigned short,unsigned char,unsigned char,unsigned short,unsigned short,unsigned short)),this,SLOT(locationIdInfo(unsigned short,unsigned short,QList<FreeEmsComms::LocationIdFlags>,unsigned short,unsigned char,unsigned char,unsigned short,unsigned short,unsigned short)));
	connect(emsComms,SIGNAL(locationIdInfo(unsigned short,MemoryLocationInfo)),this,SLOT(locationIdInfo(unsigned short,MemoryLocationInfo)));
	connect(emsComms,SIGNAL(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(interrogateRamBlockRetrieved(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(interrogateFlashBlockRetrieved(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(packetSent(unsigned short,QByteArray,QByteArray)),packetStatus,SLOT(passPacketSent(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(packetAcked(unsigned short,QByteArray,QByteArray)),packetStatus,SLOT(passPacketAck(unsigned short,QByteArray,QByteArray)));
	connect(emsComms,SIGNAL(packetNaked(unsigned short,QByteArray,QByteArray,unsigned short)),packetStatus,SLOT(passPacketNak(unsigned short,QByteArray,QByteArray,unsigned short)));
	connect(emsComms,SIGNAL(decoderFailure(QByteArray)),packetStatus,SLOT(passDecoderFailure(QByteArray)));
	emsComms->setBaud(m_comBaud);
	emsComms->setPort(m_comPort);
	emsComms->setLogDirectory(m_logDirectory);
	emsComms->setLogsEnabled(m_saveLogs);
	emsComms->setInterByteSendDelay(m_comInterByte);

	ui.actionConnect->setEnabled(true);

}

void MainWindow::setDevice(QString dev)
{
	m_comPort = dev;
	emsComms->setPort(dev);
}

void MainWindow::connectToEms()
{
	menu_connectClicked();
}
void MainWindow::tableview3d_reloadTableData(unsigned short locationid,bool ram)
{
	if (ram)
	{
		emsComms->retrieveBlockFromRam(locationid,0,0);
	}
	else
	{
		if (emsData->hasLocalFlashBlock(locationid))
		{
			TableView3D *table = qobject_cast<TableView3D*>(sender());
			if (table)
			{
				table->setData(locationid,emsData->getLocalFlashBlock(locationid));
				emsData->setLocalRamBlock(locationid,emsData->getLocalFlashBlock(locationid));
				emsComms->updateBlockInRam(locationid,0,emsData->getLocalFlashBlock(locationid).size(),emsData->getLocalFlashBlock(locationid));
			}
		}
	}
}
void MainWindow::reloadDataFromDevice(unsigned short locationid,bool isram)
{
	if (isram)
	{
		emsComms->retrieveBlockFromRam(locationid,0,0);
	}
	else
	{
		DataView *view = qobject_cast<DataView*>(sender());
		if (!view)
		{
			qDebug() << "Error, unable to cast sender of reloadDataFromDevice() to DataView*";
			return;
		}
		if (emsData->hasLocalRamBlock(locationid))
		{
			view->setData(locationid,emsData->getLocalFlashBlock(locationid));
			emsComms->updateBlockInRam(locationid,0,emsData->getLocalFlashBlock(locationid).size(),emsData->getLocalFlashBlock(locationid));
			emsComms->retrieveBlockFromFlash(locationid,0,0);
			emsData->setLocalRamBlock(locationid,emsData->getLocalFlashBlock(locationid));
		}
		else if (emsData->hasLocalFlashBlock(locationid))
		{
			emsComms->retrieveBlockFromFlash(locationid,0,0);
		}
		else
		{
			qDebug() << "Local flash block does not exist! This should never happen";
			return;
		}
	}
}

void MainWindow::tableview2d_reloadTableData(unsigned short locationid,bool isram)
{
	if (isram)
	{
		emsComms->retrieveBlockFromRam(locationid,0,0);
	}
	else
	{
		if (emsData->hasLocalFlashBlock(locationid))
		{
			TableView2D *table = qobject_cast<TableView2D*>(sender());
			if (table)
			{
				table->setData(locationid,emsData->getLocalFlashBlock(locationid));
				emsComms->updateBlockInRam(locationid,0,emsData->getLocalFlashBlock(locationid).size(),emsData->getLocalFlashBlock(locationid));
				emsData->setLocalRamBlock(locationid,emsData->getLocalFlashBlock(locationid));
			}
		}
	}
}

void MainWindow::dataViewSaveLocation(unsigned short locationid,QByteArray data,int physicallocation)
{
	if (physicallocation == 0)
	{
		//RAM
		emsComms->updateBlockInRam(locationid,0,data.size(),data);
		emsData->setLocalRamBlock(locationid,data);
		/*for (int i=0;i<m_ramMemoryList.size();i++)
		{
			if (m_ramMemoryList[i]->locationid == locationid)
			{
				m_ramMemoryList[i]->setData(data);
			}
		}*/
	}
	else if (physicallocation == 1)
	{
		//FLASH
		emsComms->updateBlockInFlash(locationid,0,data.size(),data);
		emsData->setLocalFlashBlock(locationid,data);
	}
}
void MainWindow::menu_aboutClicked()
{
	aboutMdiWindow->show();
	QApplication::postEvent(aboutMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(aboutMdiWindow, new QEvent(QEvent::WindowActivate));
}
void MainWindow::menu_windows_PacketStatusClicked()
{
	packetStatusMdiWindow->show();
	QApplication::postEvent(packetStatusMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(packetStatusMdiWindow, new QEvent(QEvent::WindowActivate));
}
void MainWindow::updateView(unsigned short locid,QObject *view,QByteArray data,int type)
{
	Q_UNUSED(type)
	DataView *dview = qobject_cast<DataView*>(view);
	dview->setData(locid,data);
	m_rawDataView[locid]->show();
	m_rawDataView[locid]->raise();
	QApplication::postEvent(m_rawDataView[locid], new QEvent(QEvent::Show));
	QApplication::postEvent(m_rawDataView[locid], new QEvent(QEvent::WindowActivate));

}
void MainWindow::createView(unsigned short locid,QByteArray data,int type,bool isram, bool isflash)
{
	if (type == 1)
	{
		qDebug() << "Creating new table view for location: 0x" << QString::number(locid,16).toUpper();
		TableView2D *view = new TableView2D(isram,isflash);
		QString title;
		Table2DMetaData metadata = m_memoryMetaData.get2DMetaData(locid);
		if (metadata.valid)
		{
			if (!view->setData(locid,data,metadata))
			{
				view->deleteLater();
				QMessageBox::information(0,"Error","Table view contains invalid data! Please check your firmware");
				return;
			}
			title = metadata.tableTitle;
		}
		else
		{
			if (!view->setData(locid,data))
			{
				QMessageBox::information(0,"Error","Table view contains invalid data! Please check your firmware");
				view->deleteLater();
				return;
			}
		}
		connect(view,SIGNAL(destroyed(QObject*)),this,SLOT(rawDataViewDestroyed(QObject*)));
		//connect(view,SIGNAL(saveData(unsigned short,QByteArray,int)),this,SLOT(rawViewSaveData(unsigned short,QByteArray,int)));
		connect(view,SIGNAL(saveSingleData(unsigned short,QByteArray,unsigned short,unsigned short)),this,SLOT(saveSingleData(unsigned short,QByteArray,unsigned short,unsigned short)));
		connect(view,SIGNAL(saveToFlash(unsigned short)),this,SLOT(saveFlashLocationId(unsigned short)));
		connect(view,SIGNAL(reloadTableData(unsigned short,bool)),this,SLOT(tableview2d_reloadTableData(unsigned short,bool)));
		QMdiSubWindow *win = ui.mdiArea->addSubWindow(view);
		win->setWindowTitle("Ram Location 0x" + QString::number(locid,16).toUpper() + " " + title);
		win->setGeometry(view->geometry());
		m_rawDataView[locid] = view;
		win->show();
		QApplication::postEvent(win, new QEvent(QEvent::Show));
		QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
	}
	else if (type == 3)
	{
		TableView3D *view = new TableView3D(isram,isflash);
		connect(view,SIGNAL(show3DTable(unsigned short,Table3DData*)),this,SLOT(tableview3d_show3DTable(unsigned short,Table3DData*)));
		QString title;
		Table3DMetaData metadata = m_memoryMetaData.get3DMetaData(locid);
		if (metadata.valid)
		{
			if (!view->setData(locid,data,metadata))
			{
				QMessageBox::information(0,"Error","Table view contains invalid data! Please check your firmware");
				view->deleteLater();
				return;
			}
			title = metadata.tableTitle;

		}
		else
		{
			if (!view->setData(locid,data))
			{
				QMessageBox::information(0,"Error","Table view contains invalid data! Please check your firmware");
				view->deleteLater();
				return;
			}
		}
		connect(view,SIGNAL(destroyed(QObject*)),this,SLOT(rawDataViewDestroyed(QObject*)));
		//connect(view,SIGNAL(saveData(unsigned short,QByteArray,int)),this,SLOT(rawViewSaveData(unsigned short,QByteArray,int)));
		connect(view,SIGNAL(saveSingleData(unsigned short,QByteArray,unsigned short,unsigned short)),this,SLOT(saveSingleData(unsigned short,QByteArray,unsigned short,unsigned short)));
		connect(view,SIGNAL(saveToFlash(unsigned short)),this,SLOT(saveFlashLocationId(unsigned short)));
		connect(view,SIGNAL(reloadTableData(unsigned short,bool)),this,SLOT(tableview3d_reloadTableData(unsigned short,bool)));
		QMdiSubWindow *win = ui.mdiArea->addSubWindow(view);
		win->setWindowTitle("Ram Location 0x" + QString::number(locid,16).toUpper() + " " + title);
		win->setGeometry(view->geometry());
		m_rawDataView[locid] = view;
		win->show();
		QApplication::postEvent(win, new QEvent(QEvent::Show));
		QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
	}
	else
	{
		/*if (m_readOnlyMetaDataMap.contains(locid))
		{
			int length=0;
			for (int j=0;j<m_readOnlyMetaDataMap[locid].m_ramData.size();j++)
			{
				length += m_readOnlyMetaDataMap[locid].m_ramData[j].size;
			}
			if (data.size() != length)
			{
				//Wrong size!
				qDebug() << "Invalid meta data size for location id:" << "0x" + QString::number(locid,16).toUpper();
				qDebug() << "Expected:" << length << "Got:" << data.size();
				QMessageBox::information(this,"Error",QString("Meta data indicates this location ID should be ") + QString::number(length) + " however it is " + QString::number(data.size()) + ". Unable to load memory location. Please fix your config.json file");
				return;
			}
			//m_readOnlyMetaDataMap[locid]
			ReadOnlyRamView *view = new ReadOnlyRamView();
			view->passData(locid,data,m_readOnlyMetaDataMap[locid].m_ramData);
			connect(view,SIGNAL(readRamLocation(unsigned short)),this,SLOT(reloadLocationId(unsigned short)));
			connect(view,SIGNAL(destroyed(QObject*)),this,SLOT(rawDataViewDestroyed(QObject*)));
			QMdiSubWindow *win = ui.mdiArea->addSubWindow(view);
			win->setWindowTitle("Ram Location: 0x" + QString::number(locid,16).toUpper());
			win->setGeometry(view->geometry());
			m_rawDataView[locid] = view;
			win->show();
			QApplication::postEvent(win, new QEvent(QEvent::Show));
			QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
		}
		else
		{*/
			RawDataView *view = new RawDataView(isram,isflash);
			view->setData(locid,data);
			connect(view,SIGNAL(saveData(unsigned short,QByteArray,int)),this,SLOT(rawViewSaveData(unsigned short,QByteArray,int)));
			connect(view,SIGNAL(destroyed(QObject*)),this,SLOT(rawDataViewDestroyed(QObject*)));
			connect(view,SIGNAL(reloadData(unsigned short,bool)),this,SLOT(reloadDataFromDevice(unsigned short,bool)));
			QMdiSubWindow *win = ui.mdiArea->addSubWindow(view);
			win->setWindowTitle("Ram Location: 0x" + QString::number(locid,16).toUpper());
			win->setGeometry(view->geometry());
			m_rawDataView[locid] = view;
			win->show();
			QApplication::postEvent(win, new QEvent(QEvent::Show));
			QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
		//}
	}
}

void MainWindow::emsInfoDisplayLocationId(int locid,bool isram,int type)
{
	Q_UNUSED(type)
	Q_UNUSED(isram)
	if (emsData->hasLocalRamBlock(locid))
	{
		if (m_rawDataView.contains(locid))
		{
			updateView(locid,m_rawDataView[locid],emsData->getLocalRamBlock(locid),type);
		}
		else
		{
			createView(locid,emsData->getLocalRamBlock(locid),type,true,emsData->hasLocalFlashBlock(locid));
		}
	}
	/*else if (m_configBlockMap.contains(locid))
	{
		ConfigView *view = new ConfigView();
		QMdiSubWindow *win = ui.mdiArea->addSubWindow(view);
		win->setWindowTitle("Ram Location 0x" + QString::number(locid,16).toUpper());
		win->setGeometry(view->geometry());
		m_rawDataView[locid] = view;
		view->passConfig(m_configBlockMap[locid],emsData->getLocalFlashBlock(locid));
		win->show();
		QApplication::postEvent(win, new QEvent(QEvent::Show));
		QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
	}*/
	else if (emsData->hasLocalFlashBlock(locid))
	{
		if (m_rawDataView.contains(locid))
		{
			updateView(locid,m_rawDataView[locid],emsData->getLocalFlashBlock(locid),type);
		}
		else
		{
			createView(locid,emsData->getLocalFlashBlock(locid),type,emsData->hasLocalRamBlock(locid),true);
		}
	}
}
void MainWindow::rawViewSaveData(unsigned short locationid,QByteArray data,int physicallocation)
{
	Q_UNUSED(physicallocation)
	markRamDirty();
	if (physicallocation==0)
	{
		if (emsData->hasLocalRamBlock(locationid))
		{
			if (emsData->getLocalRamBlock(locationid) == data)
			{
				qDebug() << "Data in application memory unchanged, no reason to send write";
				return;
			}
			emsData->setLocalRamBlock(locationid,data);

		}
		else
		{
			qDebug() << "Attempted to save data for location id:" << "0x" + QString::number(locationid,16) << "but no valid location found in Ram list.";
		}
		qDebug() << "Requesting to update ram location:" << "0x" + QString::number(locationid,16).toUpper() << "data size:" << data.size();
		m_currentRamLocationId = locationid;
		m_waitingForRamWriteConfirmation=true;
		emsComms->updateBlockInRam(locationid,0,data.size(),data);
	}
	else if (physicallocation == 1)
	{
		if (emsData->hasLocalFlashBlock(locationid))
		{
			if (emsData->getLocalFlashBlock(locationid) == data)
			{
				qDebug() << "Data in application memory unchanged, no reason to send write";
				return;
			}
			emsData->setLocalFlashBlock(locationid,data);
		}
		else
		{
			qDebug() << "Attempted to save data for location id:" << "0x" + QString::number(locationid,16) << "but no valid location found in Flash list.";
		}
		qDebug() << "Requesting to update flash location:" << "0x" + QString::number(locationid,16).toUpper() << "data size:" << data.size();
		m_currentFlashLocationId = locationid;
		m_waitingForFlashWriteConfirmation=true;
		emsComms->updateBlockInFlash(locationid,0,data.size(),data);
	}


}
void MainWindow::interrogateProgressViewDestroyed(QObject *object)
{
	Q_UNUSED(object);
	progressView = 0;
	if (m_interrogationInProgress)
	{
		m_interrogationInProgress = false;
		interrogateProgressViewCancelClicked();
	}
	QMdiSubWindow *win = qobject_cast<QMdiSubWindow*>(object->parent());
	if (!win)
	{
		//qDebug() << "Error "
		return;
	}
	win->hide();
	ui.mdiArea->removeSubWindow(win);
	win->deleteLater();


}

void MainWindow::rawDataViewDestroyed(QObject *object)
{
	QMap<unsigned short,QWidget*>::const_iterator i = m_rawDataView.constBegin();
	while( i != m_rawDataView.constEnd())
	{
		if (i.value() == object)
		{
			//This is the one that needs to be removed.
			m_rawDataView.remove(i.key());
			QMdiSubWindow *win = qobject_cast<QMdiSubWindow*>(object->parent());
			if (!win)
			{
				//qDebug() << "Raw Data View without a QMdiSubWindow parent!!";
				return;
			}
			win->hide();
			ui.mdiArea->removeSubWindow(win);
			return;
		}
		i++;
	}
}
void MainWindow::markRamDirty()
{
	m_localRamDirty = true;
	emsInfo->setLocalRam(true);
}
void MainWindow::markDeviceFlashDirty()
{
	m_deviceFlashDirty = true;
	emsInfo->setDeviceFlash(true);
}
void MainWindow::markRamClean()
{
	m_localRamDirty = false;
	emsInfo->setLocalRam(false);
}
void MainWindow::markDeviceFlashClean()
{
	m_deviceFlashDirty = false;
	emsInfo->setDeviceFlash(false);
}

void MainWindow::interrogateRamBlockRetrieved(unsigned short locationid,QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	qDebug() << "Interrogation Ram block" << "0x" + QString::number(locationid,16).toUpper();
	if (emsData->hasDeviceRamBlock(locationid))
	{
		if (emsData->getDeviceRamBlock(locationid).isEmpty())
		{
			//Initial retrieval.
			emsData->setDeviceRamBlock(locationid,payload);
			return;
		}
		else
		{
			//This should not happen during interrogation.
			qDebug() << "Ram block retrieved during interrogation, but it already had a payload!" << "0x" + QString::number(locationid,16).toUpper();
			emsData->setDeviceRamBlock(locationid,payload);
			return;
		}
	}
	qDebug() << "Ram block retrieved that should not exist!!" << "0x" + QString::number(locationid,16).toUpper() << "with a size of:" << payload.size();
}
void MainWindow::interrogateFlashBlockRetrieved(unsigned short locationid,QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	qDebug() << "Interrogation Flash block" << "0x" + QString::number(locationid,16).toUpper();
	if (emsData->hasDeviceFlashBlock(locationid))
	{
		if (emsData->getDeviceFlashBlock(locationid).isEmpty())
		{
			emsData->setDeviceFlashBlock(locationid,payload);
			return;
		}
		else
		{
			qDebug() << "Flash block retrieved during interrogation, but it already had a payload!" << "0x" + QString::number(locationid,16).toUpper();
			emsData->setDeviceFlashBlock(locationid,payload);
			return;
		}
	}
	qDebug() << "Flash block retrieved that should not exist!!" << "0x" + QString::number(locationid,16).toUpper() << "with a size of:" << payload.size();
}

//Used to verify that a block of memory matches what meta data thinks it should be.
bool MainWindow::verifyMemoryBlock(unsigned short locationid,QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	if (m_memoryMetaData.has2DMetaData(locationid))
	{
		if (payload.size() != TABLE_2D_PAYLOAD_SIZE)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	if (m_memoryMetaData.has3DMetaData(locationid))
	{
		if (payload.size() != TABLE_3D_PAYLOAD_SIZE)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	//If we get here, the table does not exist in meta data
	return true;

}

void MainWindow::ramBlockRetrieved(unsigned short locationid,QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	qDebug() << "Ram Block retrieved:" << "0x" + QString::number(locationid,16).toUpper();
	if (!emsData->hasDeviceRamBlock(locationid))
	{
		//This should not happen
		/*RawDataBlock *block = new RawDataBlock();
		block->locationid = locationid;
		block->header = header;
		block->data = payload;
		//m_flashRawBlockList.append(block);
		m_deviceRamRawBlockList.append(block);*/
	}
	else
	{
		//Check to see if it's supposed to be a table, and if so, check size
		if (!verifyMemoryBlock(locationid,header,payload))
		{
			QMessageBox::information(this,"Error","RAM Location ID 0x" + QString::number(locationid,16).toUpper() + " should be 1024 sized, but it is " + QString::number(payload.size()) + ". This should never happen");
			return;
		}
		if (emsData->getDeviceRamBlock(locationid).isEmpty())
		{
			//This should not happen
			qDebug() << "Ram block on device while ram block on tuner is empty! This should not happen" << "0x" + QString::number(locationid,16).toUpper();
			qDebug() << "Current block size:" << emsData->getDeviceRamBlock(locationid).size();
			emsData->setDeviceRamBlock(locationid,payload);
		}
		else
		{
			if (emsData->getDeviceRamBlock(locationid) != payload)
			{
				qDebug() << "Ram block on device does not match ram block on tuner! This should ONLY happen during a manual update!";
				qDebug() << "Tuner ram size:" << emsData->getDeviceRamBlock(locationid).size();
				emsData->setDeviceRamBlock(locationid,payload);
				emsData->setLocalRamBlock(locationid,payload);
			}
		}
		updateDataWindows(locationid);
	}
	return;
}


void MainWindow::flashBlockRetrieved(unsigned short locationid,QByteArray header,QByteArray payload)
{
	qDebug() << "Flash Block retrieved:" << "0x" + QString::number(locationid,16).toUpper();
	Q_UNUSED(header)
	if (!verifyMemoryBlock(locationid,header,payload))
	{
		interrogateProgressViewCancelClicked();
		QMessageBox::information(this,"Error","Flash Location ID 0x" + QString::number(locationid,16).toUpper() + " should be 1024 sized, but it is " + QString::number(payload.size()) + ". This should never happen");
		return;
	}
	if (emsData->hasDeviceFlashBlock(locationid))
	{
			if (emsData->getDeviceFlashBlock(locationid).isEmpty())
			{
				emsData->setDeviceFlashBlock(locationid,payload);
				return;
			}
			else
			{
				if (emsData->getDeviceFlashBlock(locationid) != payload)
				{

					qDebug() << "Flash block in memory does not match flash block on tuner! This should not happen!";
					qDebug() << "Flash size:" << emsData->getDeviceFlashBlock(locationid).size();
					qDebug() << "Flash ID:" << "0x" + QString::number(locationid,16).toUpper();
					emsData->setDeviceFlashBlock(locationid,payload);
				}
			}
	}
	updateDataWindows(locationid);
	return;
}

void MainWindow::ui_saveDataButtonClicked()
{

}

void MainWindow::menu_settingsClicked()
{
	ComSettings *settings = new ComSettings();
	settings->setComPort(m_comPort);
	settings->setBaud(m_comBaud);
	settings->setSaveDataLogs(m_saveLogs);
	settings->setClearDataLogs(m_clearLogs);
	settings->setNumLogsToSave(m_logsToKeep);
	settings->setDataLogDir(m_logDirectory);
	settings->setInterByteDelay(m_comInterByte);
	//m_saveLogs = settings.value("savelogs",true).toBool();
	//m_clearLogs = settings.value("clearlogs",false).toBool();
	//m_logsToKeep = settings.value("logstokeep",0).toInt();
	//m_logDirectory = settings.value("logdir",".").toString();
	connect(settings,SIGNAL(saveClicked()),this,SLOT(settingsSaveClicked()));
	connect(settings,SIGNAL(cancelClicked()),this,SLOT(settingsCancelClicked()));
	QMdiSubWindow *win = ui.mdiArea->addSubWindow(settings);
	win->setWindowTitle(settings->windowTitle());
	win->setGeometry(settings->geometry());
	win->show();
	settings->show();
}

void MainWindow::menu_connectClicked()
{
	ui.actionConnect->setEnabled(false);
	ui.actionDisconnect->setEnabled(true);
	m_interrogationInProgress = true;
	emsData->clearAllMemory();
	m_tempMemoryList.clear();
	interrogationSequenceList.clear();
	m_locIdMsgList.clear();
	m_locIdInfoMsgList.clear();
	emsMdiWindow->hide();
	for (QMap<unsigned short,QWidget*>::const_iterator i= m_rawDataView.constBegin();i != m_rawDataView.constEnd();i++)
	{
		QMdiSubWindow *win = qobject_cast<QMdiSubWindow*>((*i)->parent());
		ui.mdiArea->removeSubWindow(win);
		delete win;
		//delete (*i);
	}
	m_rawDataView.clear();
	emsInfo->clear();
	emsComms->start();
	emsComms->connectSerial(m_comPort,m_comBaud);
}

void MainWindow::menu_disconnectClicked()
{
	emsComms->disconnectSerial();
	ui.actionConnect->setEnabled(true);
	ui.actionDisconnect->setEnabled(false);
}

void MainWindow::timerTick()
{
	ui.ppsLabel->setText("PPS: " + QString::number(pidcount));
	pidcount = 0;
}
void MainWindow::settingsSaveClicked()
{
	ComSettings *comSettingsWidget = qobject_cast<ComSettings*>(sender());
	m_comBaud = comSettingsWidget->getBaud();
	m_comPort = comSettingsWidget->getComPort();
	m_comInterByte = comSettingsWidget->getInterByteDelay();
	m_saveLogs = comSettingsWidget->getSaveDataLogs();
	m_clearLogs = comSettingsWidget->getClearDataLogs();
	m_logsToKeep = comSettingsWidget->getNumLogsToSave();
	m_logDirectory = comSettingsWidget->getDataLogDir();
	/*if (!subwin)
	{
		subwin->deleteLater();
	}*/
	comSettingsWidget->hide();
	QSettings settings(m_settingsFile,QSettings::IniFormat);
	settings.beginGroup("comms");
	settings.setValue("port",m_comPort);
	settings.setValue("baud",m_comBaud);
	settings.setValue("interbytedelay",m_comInterByte);
	settings.setValue("savelogs",m_saveLogs);
	settings.setValue("clearlogs",m_clearLogs);
	settings.setValue("logstokeep",m_logsToKeep);
	settings.setValue("logdir",m_logDirectory);
	settings.endGroup();
	QMdiSubWindow *subwin = qobject_cast<QMdiSubWindow*>(comSettingsWidget->parent());
	ui.mdiArea->removeSubWindow(subwin);
	comSettingsWidget->deleteLater();

}
void MainWindow::locationIdInfo(unsigned short locationid,MemoryLocationInfo info)
{
	if (m_memoryInfoMap.contains(locationid))
	{
		qDebug() << "Duplicate location ID recieved from ECU:" << "0x" + QString::number(locationid,16).toUpper();
	}
	m_memoryInfoMap[locationid] = info;
	QString title = "";
	if (m_memoryMetaData.has2DMetaData(locationid))
	{
		title = m_memoryMetaData.get2DMetaData(locationid).tableTitle;
		if (m_memoryMetaData.get2DMetaData(locationid).size != info.size)
		{
			interrogateProgressViewCancelClicked();
			QMessageBox::information(0,"Interrogate Error","Error: Meta data for table location 0x" + QString::number(locationid,16).toUpper() + " is not valid for actual table. Size: " + QString::number(info.size) + " expected: " + QString::number(m_memoryMetaData.get2DMetaData(locationid).size));
		}
	}
	if (m_memoryMetaData.has3DMetaData(locationid))
	{
		title = m_memoryMetaData.get3DMetaData(locationid).tableTitle;
		if (m_memoryMetaData.get3DMetaData(locationid).size != info.size)
		{
			interrogateProgressViewCancelClicked();
			QMessageBox::information(0,"Interrogate Error","Error: Meta data for table location 0x" + QString::number(locationid,16).toUpper() + " is not valid for actual table. Size: " + QString::number(info.size) + " expected: " + QString::number(m_memoryMetaData.get3DMetaData(locationid).size));
		}
	}
	if (m_memoryMetaData.hasRORMetaData(locationid))
	{
		title = m_memoryMetaData.getRORMetaData(locationid).dataTitle;
		//m_readOnlyMetaDataMap[locationid]
	}
	//emsInfo->locationIdInfo(locationid,title,rawFlags,flags,parent,rampage,flashpage,ramaddress,flashaddress,size);
	emsInfo->locationIdInfo(locationid,title,info);
	/*if (flags.contains(FreeEmsComms::BLOCK_IS_RAM) && flags.contains((FreeEmsComms::BLOCK_IS_FLASH)))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isRam = true;
		loc->isFlash = true;
		loc->ramAddress = ramaddress;
		loc->ramPage = rampage;
		loc->flashAddress = flashaddress;
		loc->flashPage = flashpage;
		//m_deviceRamMemoryList.append(loc);
		emsData->addDeviceRamBlock(loc);
		emsData->addDeviceFlashBlock(new MemoryLocation(*loc));
		//m_flashMemoryList.append(new MemoryLocation(*loc));
		//m_deviceFlashMemoryList.append(new MemoryLocation(*loc));

	}
	else if (flags.contains(FreeEmsComms::BLOCK_IS_FLASH))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isFlash = true;
		loc->isRam = false;
		loc->flashAddress = flashaddress;
		loc->flashPage = flashpage;
		//m_deviceFlashMemoryList.append(loc);
		emsData->addDeviceFlashBlock(loc);
	}
	else if (flags.contains(FreeEmsComms::BLOCK_IS_RAM))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isRam = true;
		loc->isFlash = false;
		loc->ramAddress = ramaddress;
		loc->ramPage = rampage;
		//m_deviceRamMemoryList.append(loc);
		emsData->addDeviceRamBlock(loc);
	}*/
}

void MainWindow::locationIdInfo(unsigned short locationid,unsigned short rawFlags,QList<FreeEmsComms::LocationIdFlags> flags,unsigned short parent, unsigned char rampage,unsigned char flashpage,unsigned short ramaddress,unsigned short flashaddress,unsigned short size)
{
	Q_UNUSED(size)
	Q_UNUSED(rawFlags)
	Q_UNUSED(parent)
	Q_UNUSED(rampage)
	Q_UNUSED(flashpage)
	Q_UNUSED(ramaddress)
	Q_UNUSED(flashaddress)
	QString title;
	if (m_memoryInfoMap.contains(locationid))
	{
		//Duplication location id info
		qDebug() << "Duplicate location ID recieved from ECU:" << "0x" + QString::number(locationid,16).toUpper();
	}
	m_memoryInfoMap[locationid] = MemoryLocationInfo();
	m_memoryInfoMap[locationid].locationid = locationid;
	m_memoryInfoMap[locationid].rawflags = rawFlags;
	m_memoryInfoMap[locationid].flags = QList<unsigned short>();
	for (int i=0;i<flags.size();i++)
	{
		m_memoryInfoMap[locationid].flags.append(flags[i]);
	}
	m_memoryInfoMap[locationid].parent = parent;
	m_memoryInfoMap[locationid].flashpage = flashpage;
	m_memoryInfoMap[locationid].rampage = rampage;
	m_memoryInfoMap[locationid].flashaddress = flashaddress;
	m_memoryInfoMap[locationid].ramaddress = ramaddress;
	m_memoryInfoMap[locationid].size = size;
	if (m_memoryMetaData.has2DMetaData(locationid))
	{
		title = m_memoryMetaData.get2DMetaData(locationid).tableTitle;
		if (m_memoryMetaData.get2DMetaData(locationid).size != size)
		{
			interrogateProgressViewCancelClicked();
			QMessageBox::information(0,"Interrogate Error","Error: Meta data for table location 0x" + QString::number(locationid,16).toUpper() + " is not valid for actual table. Size: " + QString::number(size) + " expected: " + QString::number(m_memoryMetaData.get2DMetaData(locationid).size));
		}
	}
	if (m_memoryMetaData.has3DMetaData(locationid))
	{
		title = m_memoryMetaData.get3DMetaData(locationid).tableTitle;
		if (m_memoryMetaData.get3DMetaData(locationid).size != size)
		{
			interrogateProgressViewCancelClicked();
			QMessageBox::information(0,"Interrogate Error","Error: Meta data for table location 0x" + QString::number(locationid,16).toUpper() + " is not valid for actual table. Size: " + QString::number(size) + " expected: " + QString::number(m_memoryMetaData.get3DMetaData(locationid).size));
		}
	}
	if (m_memoryMetaData.hasRORMetaData(locationid))
	{
		title = m_memoryMetaData.getRORMetaData(locationid).dataTitle;
		//m_readOnlyMetaDataMap[locationid]
	}
	emsInfo->locationIdInfo(locationid,title,rawFlags,flags,parent,rampage,flashpage,ramaddress,flashaddress,size);
	if (flags.contains(FreeEmsComms::BLOCK_IS_RAM) && flags.contains((FreeEmsComms::BLOCK_IS_FLASH)))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isRam = true;
		loc->isFlash = true;
		loc->ramAddress = ramaddress;
		loc->ramPage = rampage;
		loc->flashAddress = flashaddress;
		loc->flashPage = flashpage;
		//m_deviceRamMemoryList.append(loc);
		emsData->addDeviceRamBlock(loc);
		emsData->addDeviceFlashBlock(new MemoryLocation(*loc));
		//m_flashMemoryList.append(new MemoryLocation(*loc));
		//m_deviceFlashMemoryList.append(new MemoryLocation(*loc));

	}
	else if (flags.contains(FreeEmsComms::BLOCK_IS_FLASH))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isFlash = true;
		loc->isRam = false;
		loc->flashAddress = flashaddress;
		loc->flashPage = flashpage;
		//m_deviceFlashMemoryList.append(loc);
		emsData->addDeviceFlashBlock(loc);
	}
	else if (flags.contains(FreeEmsComms::BLOCK_IS_RAM))
	{
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = locationid;
		loc->size = size;
		if (flags.contains(FreeEmsComms::BLOCK_HAS_PARENT))
		{
			loc->parent = parent;
			loc->hasParent = true;
		}
		loc->isRam = true;
		loc->isFlash = false;
		loc->ramAddress = ramaddress;
		loc->ramPage = rampage;
		//m_deviceRamMemoryList.append(loc);
		emsData->addDeviceRamBlock(loc);
	}
}

void MainWindow::settingsCancelClicked()
{
	//comSettings->hide();
	ComSettings *comSettingsWidget = qobject_cast<ComSettings*>(sender());
	comSettingsWidget->hide();
	QMdiSubWindow *subwin = qobject_cast<QMdiSubWindow*>(comSettingsWidget->parent());
	ui.mdiArea->removeSubWindow(subwin);
	comSettingsWidget->deleteLater();
}
void MainWindow::menu_windows_EmsStatusClicked()
{
	statusView->show();
}

void MainWindow::menu_windows_GaugesClicked()
{
	gaugesMdiWindow->show();
	QApplication::postEvent(gaugesMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(gaugesMdiWindow, new QEvent(QEvent::WindowActivate));
}

void MainWindow::menu_windows_EmsInfoClicked()
{
	emsMdiWindow->show();
	QApplication::postEvent(emsMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(emsMdiWindow, new QEvent(QEvent::WindowActivate));
}

void MainWindow::menu_windows_TablesClicked()
{
	tablesMdiWindow->show();
	QApplication::postEvent(tablesMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(tablesMdiWindow, new QEvent(QEvent::WindowActivate));
}
void MainWindow::menu_windows_FlagsClicked()
{
	flagsMdiWindow->show();
	QApplication::postEvent(flagsMdiWindow, new QEvent(QEvent::Show));
	QApplication::postEvent(flagsMdiWindow, new QEvent(QEvent::WindowActivate));
}

void MainWindow::unknownPacket(QByteArray header,QByteArray payload)
{
	QString result = "";
	for (int i=0;i<header.size();i++)
	{
		result += (((unsigned char)header[i] < (char)0xF) ? "0" : "") + QString::number((unsigned char)header[i],16).toUpper();
	}
	for (int i=0;i<payload.size();i++)
	{
		result += (((unsigned char)payload[i] < (char)0xF) ? "0" : "") + QString::number((unsigned char)payload[i],16).toUpper();
	}
}

void MainWindow::loadLogButtonClicked()
{
	QFileDialog file;
	if (file.exec())
	{
		if (file.selectedFiles().size() > 0)
		{
			QString filename = file.selectedFiles()[0];
			ui.statusLabel->setText("Status: File loaded and not playing");
			//logLoader->loadFile(filename);
			emsComms->loadLog(filename);

		}
	}
}
void MainWindow::interByteDelayChanged(int num)
{
	emsComms->setInterByteSendDelay(num);
}

void MainWindow::logFinished()
{
	ui.statusLabel->setText("Status: File loaded and log finished");
}

void MainWindow::playLogButtonClicked()
{
	//logLoader->start();
	emsComms->playLog();
	ui.statusLabel->setText("Status: File loaded and playing");
}
void MainWindow::locationIdList(QList<unsigned short> idlist)
{
	for (int i=0;i<idlist.size();i++)
	{
		//ui/listWidget->addItem(QString::number(idlist[i]));
		MemoryLocation *loc = new MemoryLocation();
		loc->locationid = idlist[i];
		m_tempMemoryList.append(loc);
		int seq = emsComms->getLocationIdInfo(idlist[i]);
		if (progressView) progressView->setMaximum(progressView->maximum()+1);
		if (progressView) progressView->addTask("Getting Location ID Info for 0x" + QString::number(idlist[i],16).toUpper(),seq,1);
		m_locIdMsgList.append(seq);
		interrogationSequenceList.append(seq);
	}
}
void MainWindow::blockRetrieved(int sequencenumber,QByteArray header,QByteArray payload)
{
	Q_UNUSED(sequencenumber)
	Q_UNUSED(header)
	Q_UNUSED(payload)
}
void MainWindow::dataLogPayloadReceived(QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	Q_UNUSED(payload)
}
void MainWindow::interfaceVersion(QString version)
{
	//ui.interfaceVersionLineEdit->setText(version);
	m_interfaceVersion = version;
	if (emsInfo)
	{
		emsInfo->setInterfaceVersion(version);

	}
	emsinfo.interfaceVersion = version;
}
void MainWindow::firmwareVersion(QString version)
{
	//ui.firmwareVersionLineEdit->setText(version);
	m_firmwareVersion = version;
	this->setWindowTitle(QString("EMStudio ") + QString(define2string(GIT_COMMIT)) + " Firmware: " + version);
	if (emsInfo)
	{
		emsInfo->setFirmwareVersion(version);
	}
	emsinfo.firmwareVersion = version;
}
void MainWindow::error(QString msg)
{
	//Q_UNUSED(msg)
	QMessageBox::information(0,"Error",msg);
}
void MainWindow::interrogateProgressViewCancelClicked()
{
	emsComms->disconnectSerial();
	emsComms->wait(1000);
	emsComms->terminate();
	emsComms->wait(1000);
}
void MainWindow::emsCompilerVersion(QString version)
{
	emsinfo.compilerVersion = version;
}

void MainWindow::emsFirmwareBuildDate(QString date)
{
	emsinfo.firmwareBuildDate = date;
}

void MainWindow::emsDecoderName(QString name)
{
	emsinfo.decoderName = name;
}

void MainWindow::emsOperatingSystem(QString os)
{
	emsinfo.operatingSystem = os;
}

void MainWindow::emsCommsConnected()
{
	//New log and settings file here.

	if (progressView)
	{
		QMdiSubWindow *win = qobject_cast<QMdiSubWindow*>(progressView->parent());
		ui.mdiArea->removeSubWindow(win);
		delete progressView;
	}
	progressView = new InterrogateProgressView();
	connect(progressView,SIGNAL(destroyed(QObject*)),this,SLOT(interrogateProgressViewDestroyed(QObject*)));
	interrogateProgressMdiWindow = ui.mdiArea->addSubWindow(progressView);
	interrogateProgressMdiWindow->setGeometry(progressView->geometry());
	connect(progressView,SIGNAL(cancelClicked()),this,SLOT(interrogateProgressViewCancelClicked()));
	progressView->setMaximum(0);
	progressView->show();
	interrogateProgressMdiWindow->show();
	progressView->addOutput("Connected to EMS");
	//this->setEnabled(false);
	int seq = emsComms->getFirmwareVersion();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Firmware Version",seq,0);

	seq = emsComms->getInterfaceVersion();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Interface Version",seq,0);

	seq = emsComms->getLocationIdList(0x00,0x00);
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Location ID List",seq,0);

	seq = emsComms->getCompilerVersion();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Compiler Version",seq,0);

	seq = emsComms->getDecoderName();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Decoder Name",seq,0);

	seq = emsComms->getFirmwareBuildDate();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Firmware Build Date",seq,0);

	seq = emsComms->getMaxPacketSize();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Max Packet Size",seq,0);

	seq = emsComms->getOperatingSystem();
	interrogationSequenceList.append(seq);
	progressView->addTask("Get Operating System",seq,0);


	progressView->setMaximum(8);
	//progressView->setMax(progressView->max()+1);
}
void MainWindow::checkSyncRequest()
{
	emsComms->getLocationIdList(0,0);
}
void MainWindow::tableview3d_show3DTable(unsigned short locationid,Table3DData *data)
{
	if (m_table3DMapViewMap.contains(locationid))
	{
		m_table3DMapViewMap[locationid]->show();
		QApplication::postEvent(m_table3DMapViewMap[locationid], new QEvent(QEvent::Show));
		QApplication::postEvent(m_table3DMapViewMap[locationid], new QEvent(QEvent::WindowActivate));
		return;
	}

	TableMap3D *m_tableMap = new TableMap3D();

	m_tableMap->passData(data);
	QMdiSubWindow *win = ui.mdiArea->addSubWindow(m_tableMap);
	connect(win,SIGNAL(destroyed(QObject*)),this,SLOT(tableMap3DDestroyed(QObject*)));
	win->setGeometry(m_tableMap->geometry());
	win->setWindowTitle("0x" + QString::number(locationid,16).toUpper());
	win->show();
	QApplication::postEvent(win, new QEvent(QEvent::Show));
	QApplication::postEvent(win, new QEvent(QEvent::WindowActivate));
	m_table3DMapViewMap[locationid] = win;

}
void MainWindow::tableMap3DDestroyed(QObject *object)
{
	Q_UNUSED(object)
	for (QMap<unsigned short,QMdiSubWindow*>::const_iterator i = m_table3DMapViewMap.constBegin(); i != m_table3DMapViewMap.constEnd();i++)
	{
		if (i.value() == sender())
		{
			m_table3DMapViewMap.remove(i.key());
			return;
		}
	}
}

void MainWindow::emsStatusHardResetRequested()
{
	if (QMessageBox::information(0,"Warning","Hard resetting the ECU will erase all changes currently in RAM, but not saved to FLASH, and restart the ECU. Are you sure you want to do this?",QMessageBox::Yes,QMessageBox::No) == QMessageBox::Yes)
	{
		qDebug() << "Attempting hard reset:" << emsComms->hardReset();
	}
}

void MainWindow::emsStatusSoftResetRequested()
{
	if (QMessageBox::information(0,"Warning","Soft resetting the ECU will erase all changes currently in RAM, but not saved to FLASH, and restart the ECU. Are you sure you want to do this?",QMessageBox::Yes,QMessageBox::No) == QMessageBox::Yes)
	{
		qDebug() << "Attempting soft reset:" << emsComms->softReset();
	}
	//ui.mdiArea->removeSubWindow(emsStatusMdiWindow);
	//this->addDockWidget(Qt::RightDockWidgetArea,statusView);
}

void MainWindow::updateRamLocation(unsigned short locationid)
{
	bool hasparent = false;
	unsigned short tempRamParentId=0;
	bool isparent = false;
	QList<unsigned short> childlist;

	childlist = emsData->getChildrenOfLocalRamLocation(locationid);
	if (childlist.size() > 0)
	{
		isparent = true;
	}
	if (emsData->getParentOfLocalRamLocation(locationid).size() > 0)
	{
		tempRamParentId = emsData->getParentOfLocalRamLocation(locationid)[0];
		hasparent = true;
	}
	emsData->setDeviceRamBlock(locationid,emsData->getLocalRamBlock(locationid));
	//Find all windows that use that location id
	if (hasparent && isparent)
	{
		//This should never happen.
		qDebug() << "Found a memory location that is parent AND child!!! This should not happen.";
		qDebug() << "Parent:" << "0x" + QString::number(tempRamParentId);
		qDebug() << "Current:" << "0x" + QString::number(locationid);
		QString children;
		for (int i=0;i<childlist.size();i++)
		{
			children += "0x" + QString::number(childlist[i],16).toUpper() + " ";
		}
		qDebug() << "Children" << children;
	}
	else if (hasparent)
	{
		//qDebug() << "No children, is a child for:" << "0x" + QString::number(m_currentRamLocationId,16).toUpper();
		updateDataWindows(tempRamParentId);
	}
	else if (isparent)
	{
		for (int i=0;i<childlist.size();i++)
		{
			updateDataWindows(childlist[i]);
		}

	}
	//qDebug() << "No children for:" << "0x" + QString::number(m_currentRamLocationId,16).toUpper();
	updateDataWindows(locationid);
}
void MainWindow::commandTimedOut(int sequencenumber)
{
	qDebug() << "Command timed out:" << QString::number(sequencenumber);
	if (m_waitingForRamWriteConfirmation)
	{
		m_waitingForRamWriteConfirmation = false;
		m_currentRamLocationId=0;
		return;
	}
	if (m_waitingForFlashWriteConfirmation)
	{
		m_waitingForFlashWriteConfirmation = false;
		m_currentFlashLocationId=0;
		return;
	}
	if (m_interrogationInProgress)
	{
		progressView->taskFail(sequencenumber);
		//If interrogation is in progress, we need to stop, since something has gone
		//horribly wrong.
		interrogateProgressViewCancelClicked();
		QMessageBox::information(0,"Error","Something has gone serious wrong, one of the commands timed out during interrogation. This should be properly investigated before continuing");
	}

}
void MainWindow::commandSuccessful(int sequencenumber)
{
	//qDebug() << "Command succesful:" << QString::number(sequencenumber);
	if (m_interrogationInProgress)
	{
		if (progressView) progressView->taskSucceed(sequencenumber);
	}
	if (m_waitingForRamWriteConfirmation)
	{
		m_waitingForRamWriteConfirmation = false;
		updateRamLocation(m_currentRamLocationId);
		checkRamFlashSync();
		m_currentRamLocationId=0;
		return;
	}
	if (m_waitingForFlashWriteConfirmation)
	{
		m_waitingForFlashWriteConfirmation = false;
		m_currentFlashLocationId=0;
		return;
	}
	checkMessageCounters(sequencenumber);

}
void MainWindow::checkMessageCounters(int sequencenumber)
{
	if (m_locIdInfoMsgList.contains(sequencenumber))
	{
		m_locIdInfoMsgList.removeOne(sequencenumber);
		if (m_locIdInfoMsgList.size() == 0)
		{
			qDebug() << "All Ram and Flash locations updated";
			//End of the location ID information messages.
			checkRamFlashSync();
		}
	}
	if (m_locIdMsgList.contains(sequencenumber))
	{
		m_locIdMsgList.removeOne(sequencenumber);
		if (m_locIdMsgList.size() == 0)
		{
			qDebug() << "All ID information recieved. Requesting Ram and Flash updates";
			populateParentLists();
			QList<unsigned short>  memorylist = emsData->getTopLevelDeviceFlashLocations();
			for (int i=0;i<memorylist.size();i++)
			{
				int seq = emsComms->retrieveBlockFromFlash(memorylist[i],0,0);
				if (progressView) progressView->addTask("Getting Location ID 0x" + QString::number(memorylist[i],16).toUpper(),seq,2);
				m_locIdInfoMsgList.append(seq);
				if (progressView) progressView->setMaximum(progressView->maximum()+1);
				interrogationSequenceList.append(seq);
			}
			memorylist = emsData->getTopLevelDeviceRamLocations();
			for (int i=0;i<memorylist.size();i++)
			{
				int seq = emsComms->retrieveBlockFromRam(memorylist[i],0,0);
				if (progressView) progressView->addTask("Getting Location ID 0x" + QString::number(memorylist[i],16).toUpper(),seq,2);
				m_locIdInfoMsgList.append(seq);
				if (progressView) progressView->setMaximum(progressView->maximum()+1);
				interrogationSequenceList.append(seq);
			}
		}
	}
	if (interrogationSequenceList.contains(sequencenumber))
	{
		if (progressView)
		{
			progressView->setProgress(progressView->progress()+1);
		}
		interrogationSequenceList.removeOne(sequencenumber);
		if (interrogationSequenceList.size() == 0)
		{
			m_interrogationInProgress = false;
			if (progressView) progressView->hide();
			if (progressView) progressView->deleteLater();
			if (progressView) progressView=0;
			//this->setEnabled(true);
			qDebug() << "Interrogation complete";

			//Disconnect from the interrogation slots, and connect to the primary slots.
			//disconnect(emsComms,SIGNAL(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)));
			emsComms->disconnect(SIGNAL(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)));
			emsComms->disconnect(SIGNAL(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)));
			//disconnect(emsComms,SIGNAL(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)));
			connect(emsComms,SIGNAL(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(ramBlockRetrieved(unsigned short,QByteArray,QByteArray)));
			connect(emsComms,SIGNAL(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)),this,SLOT(flashBlockRetrieved(unsigned short,QByteArray,QByteArray)));


			//emsInfo->show();
			emsMdiWindow->show();
			//Write everything to the settings.
			QString json = "";
			json += "{";
			QJson::Serializer jsonSerializer;
			QVariantMap top;
			top["firmwareversion"] = emsinfo.firmwareVersion;
			top["interfaceversion"] = emsinfo.interfaceVersion;
			top["compilerversion"] = emsinfo.compilerVersion;
			top["firmwarebuilddate"] = emsinfo.firmwareBuildDate;
			top["decodername"] = emsinfo.decoderName;
			top["operatingsystem"] = emsinfo.operatingSystem;
			top["emstudiohash"] = emsinfo.emstudioHash;
			top["emstudiocommit"] = emsinfo.emstudioCommit;
			/*QVariantMap memorylocations;
			for (int i=0;i<m_deviceRamMemoryList.size();i++)
			{
				QVariantMap tmp;
				tmp["flashaddress"] =  m_deviceRamMemoryList[i]->flashAddress;
				tmp["flashpage"] = m_deviceRamMemoryList[i]->flashPage;
				tmp["rampage"] = m_deviceRamMemoryList[i]->ramPage;
				tmp["ramaddress"] = m_deviceRamMemoryList[i]->ramAddress;
				tmp["hasparent"] = (m_deviceRamMemoryList[i]->hasParent) ? "true" : "false";
				tmp["size"] = m_deviceRamMemoryList[i]->size;
				QString memory = "";
				for (int j=0;j<m_deviceRamMemoryList[i]->data().size();j++)
				{
					memory += QString("0x") + (((unsigned char)m_deviceRamMemoryList[i]->data()[j] <= 0xF) ? "0" : "") + QString::number((unsigned char)m_deviceRamMemoryList[i]->data()[j],16).toUpper() + ",";
				}
				memory = memory.mid(0,memory.length()-1);
				tmp["data"] = memory;
				//memorylocations["0x" + QString::number(m_deviceRamMemoryList[i]->locationid,16).toUpper()] = tmp;

			}
			for (int i=0;i<m_deviceFlashMemoryList.size();i++)
			{
				QVariantMap tmp;
				tmp["flashaddress"] =  m_deviceFlashMemoryList[i]->flashAddress;
				tmp["flashpage"] = m_deviceFlashMemoryList[i]->flashPage;
				tmp["rampage"] = m_deviceFlashMemoryList[i]->ramPage;
				tmp["ramaddress"] = m_deviceFlashMemoryList[i]->ramAddress;
				tmp["hasparent"] = (m_deviceFlashMemoryList[i]->hasParent) ? "true" : "false";
				tmp["size"] = m_deviceFlashMemoryList[i]->size;
				QString memory = "";
				for (int j=0;j<m_deviceFlashMemoryList[i]->data().size();j++)
				{
					memory += QString("0x") + (((unsigned char)m_deviceFlashMemoryList[i]->data()[j] <= 0xF) ? "0" : "") + QString::number((unsigned char)m_deviceFlashMemoryList[i]->data()[j],16).toUpper() + ",";
				}
				memory = memory.mid(0,memory.length()-1);
				tmp["data"] = memory;
				//memorylocations["0x" + QString::number(m_deviceFlashMemoryList[i]->locationid,16).toUpper()] = tmp;
			}*/
			//top["memory"] = memorylocations;
			if (m_saveLogs)
			{
				QFile *settingsFile = new QFile(m_logDirectory + "/" + m_logFileName + ".meta.json");
				settingsFile->open(QIODevice::ReadWrite);
				settingsFile->write(jsonSerializer.serialize(top));
				settingsFile->close();
			}
		}
		else
		{
			qDebug() << interrogationSequenceList.size() << "messages left to go. First one:" << interrogationSequenceList[0];
		}
	}
}

void MainWindow::retrieveFlashLocationId(unsigned short locationid)
{
	emsComms->retrieveBlockFromFlash(locationid,0,0);
}

void MainWindow::retrieveRamLocationId(unsigned short locationid)
{
	emsComms->retrieveBlockFromRam(locationid,0,0);
}

void MainWindow::updateDataWindows(unsigned short locationid)
{
	if (m_rawDataView.contains(locationid))
	{
		DataView *dview = qobject_cast<DataView*>(m_rawDataView[locationid]);
		if (dview)
		{
			if (emsData->hasLocalRamBlock(locationid))
			{
				dview->setData(locationid,emsData->getLocalRamBlock(locationid));
			}
			else if (emsData->hasLocalFlashBlock(locationid))
			{
				dview->setData(locationid,emsData->getLocalFlashBlock(locationid));
			}
			else
			{
				qDebug() << "updateDataWindows called for location id" << "0x" + QString::number(locationid,16).toUpper() << "but no local ram or flash block exists!";
			}
			return;
		}
	}
	else
	{
		qDebug() << "Attempted to update a window that does not exist!" << "0x" + QString::number(locationid,16).toUpper();
	}
}

void MainWindow::checkRamFlashSync()
{
	emsData->populateLocalRamAndFlash();


}

void MainWindow::commandFailed(int sequencenumber,unsigned short errornum)
{
	qDebug() << "Command failed:" << QString::number(sequencenumber) << "0x" + QString::number(errornum,16);
	if (!m_interrogationInProgress)
	{
		QMessageBox::information(0,"Command Failed","Command failed with error: " + m_memoryMetaData.getErrorString(errornum));
	}
	else
	{
		if (progressView) progressView->taskFail(sequencenumber);
	}
	//bool found = false;
	if (m_waitingForRamWriteConfirmation)
	{
		m_waitingForRamWriteConfirmation = false;
		if (emsData->hasLocalRamBlock(m_currentRamLocationId))
		{
			if (emsData->hasDeviceRamBlock(m_currentRamLocationId))
			{
				qDebug() << "Data reverting for location id 0x" + QString::number(m_currentRamLocationId,16);
				if (emsData->getLocalRamBlock(m_currentRamLocationId) == emsData->getDeviceRamBlock(m_currentRamLocationId))
				{
					qDebug() << "Data valid. No need for a revert.";
				}
				else
				{
					qDebug() << "Invalid data, reverting...";
					emsData->setLocalRamBlock(m_currentRamLocationId,emsData->getDeviceRamBlock(m_currentRamLocationId));
					/*if (m_ramMemoryList[i]->data() != m_deviceRamMemoryList[j]->data())
					{
						qDebug() << "Failed to revert!!!";
					}*/
					updateRamLocation(m_currentRamLocationId);
				}
			}
		}
		else
		{
			qDebug() << "Unable to find memory location " << QString::number(m_currentRamLocationId,16) << "in local or device memory!";
		}
		//Find all windows that use that location id
		m_currentRamLocationId = 0;
		//checkRamFlashSync();
	}
	else
	{
		//qDebug() << "Error reverting! " << QString::number(m_currentRamLocationId,16) << "Location not found!";
	}
	if (m_waitingForFlashWriteConfirmation)
	{
		m_waitingForFlashWriteConfirmation = false;
		if (emsData->hasLocalFlashBlock(m_currentFlashLocationId))
		{
			if (emsData->hasDeviceFlashBlock(m_currentFlashLocationId))
			{
				qDebug() << "Data reverting for location id 0x" + QString::number(m_currentFlashLocationId,16);
				if (emsData->getLocalFlashBlock(m_currentFlashLocationId) == emsData->getDeviceFlashBlock(m_currentFlashLocationId))
				{
					qDebug() << "Data valid. No need for a revert.";
				}
				else
				{
					qDebug() << "Invalid data, reverting...";
					//m_flashMemoryList[i]->setData(m_deviceFlashMemoryList[j]->data());
					emsData->setLocalFlashBlock(m_currentFlashLocationId,emsData->getDeviceFlashBlock(m_currentFlashLocationId));
					/*if (m_flashMemoryList[i]->data() != m_deviceFlashMemoryList[j]->data())
					{
						qDebug() << "Failed to revert!!!";
					}*/
					updateRamLocation(m_currentFlashLocationId);
				}
			}
		}
		else
		{
			qDebug() << "Unable to find memory location " << QString::number(m_currentFlashLocationId,16) << "in local or device memory!";
		}
		//Find all windows that use that location id
		m_currentFlashLocationId = 0;
		//checkRamFlashSync();
		return;
	}
	else
	{
		//qDebug() << "Error reverting! " << QString::number(m_currentFlashLocationId,16) << "Location not found!";
	}
	checkMessageCounters(sequencenumber);

}
void MainWindow::populateParentLists()
{
	//Need to get a list of all IDs here now.
	qDebug() << "Populating internal memory parent list.";
	/*qDebug() << m_deviceFlashMemoryList.size() << "Device flash locations";
	qDebug() << m_flashMemoryList.size() << "Application flash locations";
	qDebug() << m_deviceRamMemoryList.size() << "Device Ram locations";
	qDebug() << m_ramMemoryList.size() << "Application Ram locations";*/
	emsData->populateDeviceRamAndFlashParents();
}

void MainWindow::pauseLogButtonClicked()
{

}
void MainWindow::saveFlashLocationIdBlock(unsigned short locationid,QByteArray data)
{
	qDebug() << "Burning flash block:" << "0x" + QString::number(locationid,16).toUpper();
	if (emsData->hasLocalFlashBlock(locationid))
	{
		emsData->setLocalFlashBlock(locationid,data);
	}
	emsComms->updateBlockInFlash(locationid,0,data.size(),data);
}

void MainWindow::saveFlashLocationId(unsigned short locationid)
{
	qDebug() << "Burning block from ram to flash for locationid:" << "0x"+QString::number(locationid,16).toUpper();
	emsComms->burnBlockFromRamToFlash(locationid,0,0);
	if (emsData->hasLocalFlashBlock(locationid))
	{
		if(emsData->hasDeviceRamBlock(locationid))
		{
			emsData->setLocalFlashBlock(locationid,emsData->getDeviceRamBlock(locationid));
			//getLocalFlashBlock(locationid);
			//getDeviceRamBlock(locationid);
		}
	}
}

void MainWindow::saveSingleData(unsigned short locationid,QByteArray data, unsigned short offset, unsigned short size)
{
	if (emsData->hasLocalRamBlock(locationid))
	{
		if (emsData->getLocalRamBlock(locationid).mid(offset,size) == data)
		{
			qDebug() << "Data in application memory unchanged, no reason to send write for single value";
			return;
		}
		emsData->setLocalRamBlock(locationid,emsData->getLocalRamBlock(locationid).replace(offset,size,data));
	}
	else
	{
		qDebug() << "Attempted to save data for single value at location id:" << "0x" + QString::number(locationid,16) << "but no valid location found in Ram list.";
	}
	qDebug() << "Requesting to update single value at ram location:" << "0x" + QString::number(locationid,16).toUpper() << "data size:" << data.size();
	qDebug() << "Offset:" << offset << "Size:" << size  <<  "Data:" << data;
	m_currentRamLocationId = locationid;
	m_waitingForRamWriteConfirmation = true;
	emsComms->updateBlockInRam(locationid,offset,size,data);
}

void MainWindow::stopLogButtonClicked()
{

}
void MainWindow::connectButtonClicked()
{
	emsComms->connectSerial(m_comPort,m_comBaud);
}

void MainWindow::logProgress(qlonglong current,qlonglong total)
{
	Q_UNUSED(current)
	Q_UNUSED(total)
	//setWindowTitle(QString::number(current) + "/" + QString::number(total) + " - " + QString::number((float)current/(float)total));
}
void MainWindow::guiUpdateTimerTick()
{
}
void MainWindow::dataLogDecoded(QVariantMap data)
{
	//m_valueMap = data;
	if (dataTables)
	{
		dataTables->passData(data);
	}
	if (dataGauges)
	{
		dataGauges->passData(data);
	}
	if (dataFlags)
	{
		dataFlags->passData(data);
	}
	if (statusView)
	{
		statusView->passData(data);
	}
	for (QMap<unsigned short,QWidget*>::const_iterator i=m_rawDataView.constBegin();i!=m_rawDataView.constEnd();i++)
	{
		DataView *dview = qobject_cast<DataView*>(i.value());
		if (dview)
		{
			dview->passDatalog(data);
		}
	}
}

void MainWindow::logPayloadReceived(QByteArray header,QByteArray payload)
{
	Q_UNUSED(header)
	pidcount++;
	if (payload.length() != 96)
	{
		//Wrong sized payload!
		//We should do something here or something...
		//return;
	}
	dataPacketDecoder->decodePayload(payload);
	//guiUpdateTimerTick();

}

MainWindow::~MainWindow()
{
	emsComms->stop();
	emsComms->wait(1000);
	delete emsComms;
}
