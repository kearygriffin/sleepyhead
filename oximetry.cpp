#include <QApplication>
#include <QDebug>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QLocale>
#include <QTimer>
#include <QCalendarWidget>
#include <QTextCharFormat>

#include "oximetry.h"
#include "ui_oximetry.h"
#include "common_gui.h"
#include "qextserialport/qextserialenumerator.h"
#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/event.h"
#include "SleepLib/calcs.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gSummaryChart.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gLineOverlay.h"

extern QLabel * qstatus2;
#include "mainwindow.h"
extern MainWindow * mainwin;

int lastpulse;
SerialOximeter::SerialOximeter(QObject * parent,QString oxiname, QString portname, BaudRateType baud, FlowType flow, ParityType parity, DataBitsType databits, StopBitsType stopbits) :
    QObject(parent),
    session(NULL),pulse(NULL),spo2(NULL),plethy(NULL),m_port(NULL),
    m_opened(false),
    m_oxiname(oxiname),
    m_portname(portname),
    m_baud(baud),
    m_flow(flow),
    m_parity(parity),
    m_databits(databits),
    m_stopbits(stopbits)
{
    machine=PROFILE.GetMachine(MT_OXIMETER);
    if (!machine) {
        // Create generic Serial Oximeter object..
        CMS50Loader *l=dynamic_cast<CMS50Loader *>(GetLoader("CMS50"));
        if (l) {
            machine=l->CreateMachine(p_profile);
        }
        qDebug() << "Create Oximeter device";
    }
    timer=new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(Timeout()));
    import_mode=false;
    m_mode=SO_WAIT;

}

SerialOximeter::~SerialOximeter()
{
    if (m_opened) {
        if (m_port) m_port->close();
    }
    disconnect(timer,SIGNAL(timeout()),this,SLOT(Timeout()));
    delete timer;
}

void SerialOximeter::Timeout()
{
    qDebug() << "Timeout!";
    if (!import_mode) emit(liveStopped(session));
}

bool SerialOximeter::Open(QextSerialPort::QueryMode mode)
{
    if (m_portname.isEmpty()) {
        qDebug() << "Tried to open with empty portname";
        return false;
    }

    qDebug() << "Opening serial port" << m_portname << "in mode" << mode;

    if (m_opened) { // Open already?
        // Just close it
        if (m_port) m_port->close();
    }

    m_portmode=mode;
    m_callbacks=0;

    m_port=new QextSerialPort(m_portname,m_portmode);
    m_port->setBaudRate(m_baud);
    m_port->setFlowControl(m_flow);
    m_port->setParity(m_parity);
    m_port->setDataBits(m_databits);
    m_port->setStopBits(m_stopbits);

    if (m_port->open(QIODevice::ReadWrite) == true) {
       // if (m_mode==QextSerialPort::EventDriven)
            connect(m_port, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
        //connect(port, SIGNAL(dsrChanged(bool)), this, SLOT(DsrChanged(bool)));
        if (!(m_port->lineStatus() & LS_DSR))
            qDebug() << "check device is turned on";
        qDebug() << "listening for data on" << m_port->portName();
        return m_opened=true;
    } else {
        qDebug() << "device failed to open:" << m_port->errorString();
        return m_opened=false;
    }
}

void SerialOximeter::Close()
{
    qDebug() << "Closing serial port" << m_portname;
    if (!m_opened) return;

    if (m_port) m_port->close();
    //if (m_portmode==QextSerialPort::EventDriven)
        disconnect(m_port, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
    m_mode=SO_OFF;
    m_opened=false;
}

void SerialOximeter::setPortName(QString portname)
{
    if (m_opened) {
        qDebug() << "Can't change serial PortName settings while port is open!";
        return;
    }
    m_portname=portname;
}

void SerialOximeter::setBaudRate(BaudRateType baud)
{
    if (m_opened) {
        qDebug() << "Can't change serial BaudRate settings while port is open!";
        return;
    }
    m_baud=baud;
}

void SerialOximeter::setFlowControl(FlowType flow)
{
    if (m_opened) {
        qDebug() << "Can't change serial FlowControl settings while port is open!";
        return;
    }
    m_flow=flow;
}

void SerialOximeter::setParity(ParityType parity)
{
    if (m_opened) {
        qDebug() << "Can't change serial Parity settings while port is open!";
        return;
    }
    m_parity=parity;
}

void SerialOximeter::setDataBits(DataBitsType databits)
{
    if (m_opened) {
        qDebug() << "Can't change serial DataBit settings while port is open!";
        return;
    }
    m_databits=databits;
}

void SerialOximeter::setStopBits(StopBitsType stopbits)
{
    if (m_opened) {
        qDebug() << "Can't change serial StopBit settings while port is open!";
        return;
    }
    m_stopbits=stopbits;
}

void SerialOximeter::addPulse(qint64 time, EventDataType pr)
{
    //EventDataType min=0,max=0;
    if (pr>0) {
        if (lastpr==0) {
            if (pulse->count()==0) {
               pulse->setFirst(time);
               if (session->eventlist[OXI_Pulse].size()<=1) {
                   session->setFirst(OXI_Pulse,time);
                   if (session->first()==0)
                       session->set_first(time);
               }

            } else {
                qDebug() << "Shouldn't happen in addPulse()";
            }
        }
        pulse->AddEvent(time,pr);
        session->setCount(OXI_Pulse,session->count(OXI_Pulse)+1);
        session->setLast(OXI_Pulse,time);
        session->set_last(time);
    } else {
        if (lastpr!=0) {
            if (pulse->count() > 0) {
                pulse->AddEvent(time,lastpr);
                this->compactToEvent(pulse);
                session->setLast(OXI_Pulse,time);
                pulse=session->AddEventList(OXI_Pulse,EVL_Event);
            }
        }
    }
    lastpr=pr;
    emit(updatePulse(pr));
}

void SerialOximeter::addSpO2(qint64 time, EventDataType o2)
{
    //EventDataType min=0,max=0;
    if (o2>0) {
        if (lasto2==0) {
            if (spo2->count()==0) {
                spo2->setFirst(time);
                if (session->eventlist[OXI_SPO2].size()<=1) {
                    session->setFirst(OXI_SPO2,time);
                    if (session->first()==0)
                        session->set_first(time);
                }
            } else {
                qDebug() << "Shouldn't happen in addSpO2()";
            }
        }

        spo2->AddEvent(time,o2);
        session->setCount(OXI_SPO2,session->count(OXI_SPO2)+1);
        session->setLast(OXI_SPO2,time);
        session->set_last(time);
    } else {
        if (lasto2!=0) {
            if (spo2->count() > 0) {
                spo2->AddEvent(time,lasto2);
                this->compactToEvent(spo2);
                session->setLast(OXI_SPO2,time);
                spo2=session->AddEventList(OXI_SPO2,EVL_Event);
            }
        }
    }

    lasto2=o2;
    emit(updateSpO2(o2));
}

void SerialOximeter::addPlethy(qint64 time, EventDataType pleth)
{
    if (!plethy) {
        plethy=new EventList(EVL_Event);
        session->eventlist[OXI_Plethy].push_back(plethy);
        session->setFirst(OXI_Plethy,lasttime);
        plethy->setFirst(lasttime);
    }
    plethy->AddEvent(time,pleth);
    session->setCount(OXI_Plethy,plethy->count()); // update the cache
    session->setMin(OXI_Plethy,plethy->Min());
    session->setMax(OXI_Plethy,plethy->Max());
    session->setLast(OXI_Plethy,time);
    session->set_last(time);
    plethy->setLast(time);
}
void SerialOximeter::compactToWaveform(EventList *el)
{
    double rate=double(el->duration())/double(el->count());
    el->setType(EVL_Waveform);
    el->setRate(rate);
    el->getTime().clear();
}
void SerialOximeter::compactToEvent(EventList *el)
{
    if (el->count()<2) return;
    EventList nel(EVL_Waveform);
    EventDataType t=0,lastt=0; //el->data(0);
    qint64 ti=0;//=el->time(0);
    //nel.AddEvent(ti,lastt);
    bool f=false;
    qint64 lasttime=0;
    EventDataType min=999,max=0;
    for (quint32 i=0;i<el->count();i++) {
        t=el->data(i);
        ti=el->time(i);
        f=false;
        if (t!=0) {
            if (t!=lastt) {
                if (!lasttime) {
                    nel.setFirst(ti);
                }
                nel.AddEvent(ti,t);
                if (t < min) min=t;
                if (t > max) max=t;
                lasttime=ti;
                f=true;
            }
        } else {
            if (lastt!=0) {
                nel.AddEvent(ti,lastt);
                lasttime=ti;
                f=true;
            }
        }
        lastt=t;
    }
    if (!f) {
        if (t!=0) {
            nel.AddEvent(ti,t);
            lasttime=ti;
        }
    }
    el->setFirst(nel.first());
    el->setLast(nel.last());
    el->setMin(min);
    el->setMax(max);

    el->getData().clear();
    el->getTime().clear();
    el->setCount(nel.count());

    el->getData()=nel.getData();
    el->getTime()=nel.getTime();
}

void SerialOximeter::compactAll()
{
    if (!session) return;
    QHash<ChannelID,QVector<EventList *> >::iterator i;

    qint64 tminx=0,tmaxx=0,minx,maxx;
    EventDataType min,max;
    for (i=session->eventlist.begin();i!=session->eventlist.end();i++) {
        const ChannelID & code=i.key();
        min=999,max=0;
        minx=maxx=0;
        for (int j=0;j<i.value().size();j++) {
            EventList *e=i.value()[j];
            if ((code==OXI_SPO2) || (code==OXI_Pulse)) {
                compactToEvent(e);
            } else if (code==OXI_Plethy) {
                compactToWaveform(e);
            }
            if (min > e->Min())
                min=e->Min();
            if (max < e->Max())
                max=e->Max();
            if (!minx || (minx > e->first()))
                minx=e->first();
            if (!maxx || (maxx < e->last()))
                maxx=e->last();
        }
        if ((code==OXI_SPO2) || (code==OXI_Pulse) || (code==OXI_Plethy)) {
            session->setMin(code,min);
            session->setMax(code,max);
            if (minx!=0) {
                session->setFirst(code,minx);
                if (!tminx || tminx > minx) tminx=minx;
            }
            if (maxx!=0){
                session->setLast(code,maxx);
                if (!tmaxx || tmaxx < max) tmaxx=maxx;
            }
        }
    }

    if (tminx>0) session->really_set_first(tminx);
    if (tmaxx>0) session->really_set_last(tmaxx);
}

Session *SerialOximeter::createSession(QDateTime date)
{
    if (session) {
         delete session;
    }
    int sid=date.toTime_t();
    lasttime=qint64(sid)*1000L;
    lasto2=lastpr=0;

    session=new Session(machine,sid);
    session->SetChanged(true);

    session->set_first(lasttime);
    pulse=new EventList(EVL_Event);
    spo2=new EventList(EVL_Event);
    plethy=NULL;
    session->eventlist[OXI_Pulse].push_back(pulse);
    session->eventlist[OXI_SPO2].push_back(spo2);

    session->setFirst(OXI_Pulse,lasttime);
    session->setFirst(OXI_SPO2,lasttime);

    pulse->setFirst(lasttime);
    spo2->setFirst(lasttime);

    m_callbacks=0;

    emit(sessionCreated(session));
    return session;
}

bool SerialOximeter::startLive()
{
    import_mode=false;
    m_mode=SO_LIVE;
    lastpr=lasto2=0;

    if (!m_opened && !Open(QextSerialPort::EventDriven)) return false;
    createSession();

    return true;
}

void SerialOximeter::stopLive()
{
    if (timer->isActive()) timer->stop();
    m_mode=SO_WAIT;
    if (session) {
        compactAll();
        calcSPO2Drop(session);
        calcPulseChange(session);
    }
}

CMS50Serial::CMS50Serial(QObject * parent, QString portname="") :
 SerialOximeter(parent,"CMS50", portname, BAUD19200, FLOW_OFF, PAR_ODD, DATA_8, STOP_1)
{
}

CMS50Serial::~CMS50Serial()
{
}

void CMS50Serial::import_process()
{
    if (!session) {
        qDebug() << "User pushing import too many times in a row?";
        return;
    }
    qDebug() << "CMS50 import complete. Processing" << data.size() << "bytes";
    unsigned short a,pl,o2,lastpl=0,lasto2=0;
    int i=0;
    int size=data.size();


    EventList * pulse=(session->eventlist[OXI_Pulse][0]);
    EventList * spo2=(session->eventlist[OXI_SPO2][0]);
    lasttime=f2time[0].toTime_t();
    session->SetSessionID(lasttime);
    lasttime*=1000;

    //spo2->setFirst(lasttime);

    EventDataType plmin=999,plmax=0;
    EventDataType o2min=100,o2max=0;
    int plcnt=0,o2cnt=0;
    qint64 lastpltime=0,lasto2time=0;
    bool first=true;
    while (i<(size-3)) {
        a=data.at(i++); // low bits are supposedly the high bits of the heart rate
        pl=((data.at(i++) & 0x7f) | ((a & 1) << 7)) & 0xff;
        o2=data.at(i++) & 0x7f;
        if (pl!=0) {
            if (lastpl!=pl) {
                if (lastpl==0 || !pulse) {
                    if (first) {
                        session->set_first(lasttime);
                        first=false;
                    }
                    if (plcnt==0)
                        session->setFirst(OXI_Pulse,lasttime);
                    if (pulse && pulse->count()==0)  {

                    } else {
                        pulse=new EventList(EVL_Event);
                        session->eventlist[OXI_Pulse].push_back(pulse);
                    }
                }
                lastpltime=lasttime;
                pulse->AddEvent(lasttime,pl);
                if (pl < plmin) plmin=pl;
                if (pl > plmax) plmax=pl;
                plcnt++;
            }
        } else {
            if (lastpl!=0) {
                pulse->AddEvent(lasttime,pl);
                lastpltime=lasttime;
                plcnt++;
            }
        }
        if (o2!=0) {
            if (lasto2!=o2) {
                if (lasto2==0  || !spo2) {
                    if (first) {
                        session->set_first(lasttime);
                        first=false;
                    }
                    if (o2cnt==0)
                        session->setFirst(OXI_SPO2,lasttime);
                    if (spo2 && spo2->count()==0) {
                    } else {
                        spo2=new EventList(EVL_Event);
                        session->eventlist[OXI_SPO2].push_back(spo2);
                    }
                }
                lasto2time=lasttime;
                spo2->AddEvent(lasttime,o2);
                if (o2 < o2min) o2min=o2;
                if (o2 > o2max) o2max=o2;
                o2cnt++;
            }
        } else {
            if (lasto2!=0) {
                spo2->AddEvent(lasttime,o2);
                lasto2time=lasttime;
                o2cnt++;
            }
        }

        lasttime+=1000;
        //emit(updateProgress(float(i)/float(size)));

        lastpl=pl;
        lasto2=o2;
    }
    /*if (pulse && (lastpltime!=lasttime) && (pl!=0)) {
        // lastpl==pl
        pulse->AddEvent(lasttime,pl);
        lastpltime=lastpltime;
        plcnt++;
    }
    if (spo2 && (lasto2time!=lasttime) && (o2!=0)) {
        spo2->AddEvent(lasttime,o2);
        lasto2time=lasttime;
        o2cnt++;
    }*/
    qint64 rlasttime=qMax(lastpltime,lasto2time);
    session->set_last(rlasttime);
    session->setLast(OXI_Pulse,lastpltime);
    session->setLast(OXI_SPO2,lasto2time);
    session->setMin(OXI_Pulse,plmin);
    session->setMax(OXI_Pulse,plmax);
    session->setMin(OXI_SPO2,o2min);
    session->setMax(OXI_SPO2,o2max);
    session->setCount(OXI_Pulse,plcnt);
    session->setCount(OXI_SPO2,o2cnt);
    session->UpdateSummaries();
    emit(importComplete(session));
    disconnect(this,SIGNAL(importProcess()),this,SLOT(import_process()));
}

void CMS50Serial::ReadyRead()
{
    QByteArray bytes;
    int a = m_port->bytesAvailable();
    bytes.resize(a);
    m_port->read(bytes.data(), bytes.size());
    m_callbacks++;

    if (m_mode==SO_WAIT) return;

    int i=0;

    // Was going out of sync previously.. To fix this unfortunately requires 4.7

#if QT_VERSION >= QT_VERSION_CHECK(4,7,0)
    qint64 current=QDateTime::currentMSecsSinceEpoch(); //double(QDateTime::currentDateTime().toTime_t())*1000L;
    //qint64 since=current-lasttime;
    //if (since>25)
    lasttime=current;
#endif
    // else (don't bother - we can work some magic at the end of recording.)

    static int lastbytesize=0;
    int size=bytes.size();
    // Process all incoming serial data packets
    unsigned short c;
    unsigned short pl,o2;

    if (!import_mode) {
        QString data="Read: ";
#ifdef SERIAL_DEBUG
        for (int i=0;i<bytes.size();i++) {
            c=bytes[i];
            data+=QString().sprintf("%02X,",c);
        }
        qDebug() << data;
#endif
        if (bytes.size()==1) { // transmits a single 0 when switching off.
            if (lastbytesize!=1) {
                if (timer->isActive()) {
                    timer->stop();
                }
                timer->setSingleShot(true);
                timer->setInterval(10000);
                timer->start();
            }
            return;
            //qDebug() << "Oximeter switched of.. wait for timeout?";
        } else {
            if (timer->isActive()) {
                timer->stop();
            }
        }
    }
    lastbytesize=size;
    bool fixtime=false;

    while (i<bytes.size()) {
        if (import_mode) {

            // why can't this stay in a waitf6 loop for 30 or so seconds?

            if (waitf6) { //ack sequence from f6 command.
                if ((unsigned char)bytes.at(i++)==0xf2) {
                    c=bytes.at(i);
                    if (c & 0x80) {
                        int h=(c & 0x1f);
                        int m=(bytes.at(i+1) % 60);
                        if (!((h==0) && (m==0))) { // CMS50E's have a realtime clock (apparently)
                            QDateTime d(PROFILE.LastDay(),QTime(h,m,0));
                            f2time.push_back(d);
                            qDebug() << "Session start (according to CMS50)" << d << h << m;
                        } else {
                            // otherwise pick the first session of the last days data..
                            Day *day=PROFILE.GetDay(PROFILE.LastDay(),MT_CPAP);
                            QDateTime d;
                            fixtime=true;
                            if (day) {
                                int ti=day->first()/1000L;

                                d=QDateTime::fromTime_t(ti);
                                qDebug() << "Guessing session starting from CPAP data" << d;
                            } else {
                                qDebug() << "Can't guess start time, defaulting to ending at 7:30am this morning" << d;
                                d=QDateTime::currentDateTime();
                                d.setTime(QTime(7,30,0));
                                //d.addDays(-1);
                            }
                            f2time.push_back(d);
                        }
                        i+=2;
                        cntf6++;
                    } else continue;
                } else {
                    if (cntf6>0) {
                        qDebug() << "Got Acknowledge Sequence" << cntf6;
                        i--;
                        if ((i+3)<size) {
                            c=bytes.at(i);

                            datasize=(((unsigned char)bytes.at(i) & 0x3f) << 14) | (((unsigned char)bytes.at(i+1)&0x7f) << 7) | ((unsigned char)bytes.at(i+2) & 0x7f);
                            received_bytes=0;
                            qDebug() << "Data Size=" << datasize << "??";
                            if (fixtime) {
                                QDateTime time;
                                for (int i=0;i<f2time.size();i++) {
                                    time=f2time.at(i);
                                    time.addSecs(-(datasize/3));
                                    break;
                                }
                                f2time.clear();
                                f2time.push_back(time);
                            }
                            done_import=false;

                            i+=3;
                        }
                        int z;
                        for (z=i;z<size;z++) {
                            c=bytes.at(z);
                            if (c & 0x80) break;
                        }
                        mainwin->getOximetry()->graphView()->setEmptyText("Please Wait, Importing...");
                        mainwin->getOximetry()->graphView()->redraw();

                        data.clear();
                        for (z=i;z<size;z++) {
                            data.push_back(bytes.at(z));
                            received_bytes++;
                        }
                        if (z>=datasize)
                            done_import=true;
                        waitf6=false;
                        break;
                    }
                }
            } else {
                qDebug() << "Recieving Block" << size << "(" << received_bytes << "of" << datasize <<")";
                for (int z=i;z<size;z++) {
                    data.push_back(bytes.at(z));
                    received_bytes++;
                }
                emit(updateProgress(float(received_bytes)/float(datasize)));
                if ((received_bytes>=datasize) || (((received_bytes/datasize)>0.7) && (size<250))) {
                    done_import=true;
                }
                break;
                //read data blocks..
            }
        } else {
            static unsigned short hb=0;

            if (bytes[i]&0x80) { // 0x80 == sync bit
                EventDataType d=bytes[i+1] & 0x7f;
                hb=(bytes[i+2] & 0x40) << 1;

                addPlethy(lasttime,d);
                lasttime+=20;
                i+=3;
            } else {
                pl=(bytes[i] & 0x7f) | hb;
                o2=bytes[i+1] & 0x7f;
                addPulse(lasttime,pl);
                addSpO2(lasttime,o2);
                i+=2;
            }
        }
    }
    if (import_mode && waitf6 && (cntf6==0)) {
        int i=imptime.elapsed();

        //mainwin->getOximetry()->graphView()->setEmptyText("fun");
        //mainwin->getOximetry()->graphView()->redraw();

        if (i>1000) {
            //mainwin->getOximetry()->graphView()->setEmptyText("fun");
            //mainwin->getOximetry()->graphView()->redraw();
            imptime.start();
            failcnt++;
            QString a;

            if (failcnt>15) { // Give up after ~15 seconds
                m_mode=SO_WAIT;
                emit(importAborted());
                mainwin->getOximetry()->graphView()->setEmptyText("Import Failed");
                mainwin->getOximetry()->graphView()->redraw();
                return;
            } else {
                a="Waiting";
                for (int i=0;i<failcnt;i++) a+=".";
                mainwin->getOximetry()->graphView()->setEmptyText(a);
                mainwin->getOximetry()->graphView()->redraw();
                requestData(); // retransmit the data request code
            }
        }
    }
    if (!import_mode)
        emit(dataChanged());
    else if (done_import) {
        qDebug() << "End";
        resetDevice();
        m_mode=SO_WAIT;
        emit(importProcess());

    }
}
void CMS50Serial::resetDevice()
{
    m_port->flush();
    static unsigned char b1[3]={0xf6,0xf6,0xf6};
    if (m_port->write((char *)b1,3)==-1) {
        qDebug() << "Couldn't write closing bytes to CMS50";
    }
}

void CMS50Serial::requestData()
{
    static unsigned char b1[2]={0xf5,0xf5};

    if (m_port->write((char *)b1,2)==-1) {
        qDebug() << "Couldn't write data request bytes to CMS50";
    }
}

bool CMS50Serial::startImport()
{
    m_mode=SO_WAIT;

    if (!m_opened && !Open(QextSerialPort::EventDriven))
        return false;

    imptime.start();

    m_callbacks=0;
    import_fails=0;

    QTimer::singleShot(5000,this,SLOT(startImportTimeout()));
    //make sure there is a data stream first..
    createSession();

    return true;
}

void CMS50Serial::startImportTimeout()
{
    if (m_callbacks>0) {
        connect(this,SIGNAL(importProcess()),this,SLOT(import_process()));
        import_mode=true;
        waitf6=true;
        done_import=false;
        cntf6=0;
        failcnt=0;
        m_mode=SO_IMPORT;
        requestData();
    } else {
        import_fails++;
        if (import_fails<5) {
           // resetDevice();
            QTimer::singleShot(250,this,SLOT(startImportTimeout()));
        } else {
            qDebug() << "No oximeter signal!!!!!!!!!!!!!!!!";
            emit(importAborted());
        }
    }
}


Oximetry::Oximetry(QWidget *parent,gGraphView * shared) :
    QWidget(parent),
    ui(new Ui::Oximetry)
{
    m_shared=shared;
    ui->setupUi(this);

    port=NULL;
    portname="";

    oximeter=new CMS50Serial(this);

    day=new Day(oximeter->getMachine());

    layout=new QHBoxLayout(ui->graphArea);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setContentsMargins(0,0,0,0);
    ui->graphArea->setLayout(layout);
    ui->graphArea->setAutoFillBackground(false);

    GraphView=new gGraphView(ui->graphArea,m_shared);
    GraphView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    scrollbar=new MyScrollBar(ui->graphArea);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);

    GraphView->setScrollBar(scrollbar);
    layout->addWidget(GraphView,1);
    layout->addWidget(scrollbar,0);

    layout->layout();

    PLETHY=new gGraph(GraphView,schema::channel[OXI_Plethy].label(),schema::channel[OXI_Plethy].units(),120);
    CONTROL=new gGraph(GraphView,tr("Control"),"",75);
    PULSE=new gGraph(GraphView,schema::channel[OXI_Pulse].label(),schema::channel[OXI_Pulse].units(),120);
    SPO2=new gGraph(GraphView,schema::channel[OXI_SPO2].label(),schema::channel[OXI_SPO2].units(),120);
    foobar=new gShadowArea();
    CONTROL->AddLayer(foobar);
    Layer *cl=new gLineChart(OXI_Plethy);
    CONTROL->AddLayer(cl);
    cl->SetDay(day);
    CONTROL->setBlockZoom(true);

    for (int i=0;i<GraphView->size();i++) {
        gGraph *g=(*GraphView)[i];
        g->AddLayer(new gXAxis(),LayerBottom,0,gXAxis::Margin);
        g->AddLayer(new gYAxis(),LayerLeft,gYAxis::Margin);
        g->AddLayer(new gXGrid());
    }

    plethy=new gLineChart(OXI_Plethy,Qt::black,false,true);
    plethy->SetDay(day);

    CONTROL->AddLayer(plethy); //new gLineChart(OXI_Plethysomogram));


    pulse=new gLineChart(OXI_Pulse,Qt::red,true);
    //pulse->SetDay(day);

    spo2=new gLineChart(OXI_SPO2,Qt::blue,true);
    //spo2->SetDay(day);

    PLETHY->AddLayer(plethy);

    PULSE->AddLayer(lo1=new gLineOverlayBar(OXI_PulseChange,QColor("light gray"),"PD",FT_Span));
    SPO2->AddLayer(lo2=new gLineOverlayBar(OXI_SPO2Drop,QColor("light blue"),"O2",FT_Span));
    PULSE->AddLayer(pulse);
    SPO2->AddLayer(spo2);
    PULSE->setDay(day);
    SPO2->setDay(day);

    lo1->SetDay(day);
    lo2->SetDay(day);
    //go->SetDay(day);

    GraphView->setEmptyText(tr("No Oximetry Data"));
    GraphView->redraw();

    on_RefreshPortsButton_clicked();
    ui->RunButton->setChecked(false);

    ui->saveButton->setEnabled(false);
    GraphView->LoadSettings("Oximetry");
    GraphView->setCubeImage(images["oximeter"]);

    QLocale locale=QLocale::system();
    QString shortformat=locale.dateFormat(QLocale::ShortFormat);
    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy","yyyy");
    }
    ui->dateEdit->setDisplayFormat(shortformat+" HH:mm:ss");
    //Qt::DayOfWeek dow=firstDayOfWeekFromLocale();

    //ui->dateEdit->calendarWidget()->setFirstDayOfWeek(dow);


    // Stop both calendar drop downs highlighting weekends in red
    //QTextCharFormat format = ui->dateEdit->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    //format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    //ui->dateEdit->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    //ui->dateEdit->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    dont_update_date=false;
}

Oximetry::~Oximetry()
{
    delete oximeter;
    GraphView->SaveSettings("Oximetry");
    delete ui;
}

void Oximetry::on_RefreshPortsButton_clicked()
{
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();

    ui->SerialPortsCombo->clear();
    int z=0;
    QString firstport;
    bool current_found=false;

    // Windows build mixes these up
#if defined(Q_WS_WIN32) || defined(Q_WS_MAC)
#define qesPORTNAME portName
#else
#define qesPORTNAME physName
#endif
    for (int i = 0; i < ports.size(); i++) {
        if (!ports.at(i).friendName.isEmpty()) {
            if (ports.at(i).friendName.toUpper().contains("USB")) {
                if (firstport.isEmpty()) firstport=ports.at(i). qesPORTNAME;
                if (!portname.isEmpty() && ports.at(i).qesPORTNAME==portname) current_found=true;
                ui->SerialPortsCombo->addItem(ports.at(i).qesPORTNAME);
                z++;
            }
        } else { // Mac stuff.
            if (ports.at(i).portName.toUpper().contains("USB") || ports.at(i).portName.toUpper().contains("SPO2")) {
                if (firstport.isEmpty()) firstport=ports.at(i).portName;
                if (!portname.isEmpty() && ports.at(i).portName==portname) current_found=true;
                ui->SerialPortsCombo->addItem(ports.at(i).portName);
                z++;
            }
        }
        qDebug() << "Serial Port:" << ports.at(i).qesPORTNAME << ports.at(i).friendName;
        qDebug() << "port name:" << ports.at(i).portName;
        qDebug() << "phys name:" << ports.at(i).physName;
        qDebug() << "friendly name:" << ports.at(i).friendName;
        qDebug() << "enumerator name:" << ports.at(i).enumName;
    }
    if (z>0) {
        ui->RunButton->setEnabled(true);
        ui->ImportButton->setEnabled(true);
        if (!current_found) portname=firstport;
    } else {
        ui->RunButton->setEnabled(false);
        ui->ImportButton->setEnabled(false);
        portname="";
    }
    oximeter->setPortName(portname);
}
void Oximetry::RedrawGraphs()
{
    GraphView->redraw();
}
void Oximetry::on_SerialPortsCombo_activated(const QString &arg1)
{
    portname=arg1;
    oximeter->setPortName(arg1);
}

void Oximetry::live_stopped(Session * session)
{
    Q_UNUSED(session);
    mainwin->Notify(tr("Oximetry live recording has been terminated due to timeout."));
    //qDebug () << "Live Stopped";
    on_RunButton_toggled(false);
}

void Oximetry::on_RunButton_toggled(bool checked)
{
    if (!checked) {
            oximeter->stopLive();
            ui->RunButton->setText(tr("&Start"));
            ui->SerialPortsCombo->setEnabled(true);
            disconnect(oximeter,SIGNAL(dataChanged()),this,SLOT(data_changed()));
            disconnect(oximeter,SIGNAL(updatePulse(float)),this,SLOT(pulse_changed(float)));
            disconnect(oximeter,SIGNAL(updateSpO2(float)),this,SLOT(spo2_changed(float)));
            disconnect(oximeter,SIGNAL(liveStopped(Session*)),this,SLOT(live_stopped(Session *)));
            ui->saveButton->setEnabled(true);
            ui->ImportButton->setEnabled(true);
            lo2->SetDay(day);
            lo1->SetDay(day);
            if (oximeter->getSession())
                saved_starttime=oximeter->getSession()->first();



            //CONTROL->setVisible(true);
    } else {
        if (oximeter->getSession() && oximeter->getSession()->IsChanged()) {
            int res=QMessageBox::question(this,tr("Save Session?"),tr("Creating a new oximetry session will destroy the old one.\nWould you like to save it first?"),tr("Save"),tr("Destroy It"),tr("Cancel"),0,2);
            if (res==0) {
                ui->RunButton->setChecked(false);
                on_saveButton_clicked();
                return;
            } else if (res==2) {
                ui->RunButton->setChecked(false);
                return;
            }
        } // else it's already saved.
        GraphView->setEmptyText(tr("Please Wait"));
        GraphView->redraw();

        PLETHY->setVisible(true);
        SPO2->setVisible(true);
        PULSE->setVisible(true);

        PLETHY->setRecMinY(0);
        PLETHY->setRecMaxY(128);
        PULSE->setRecMinY(60);
        PULSE->setRecMaxY(100);
        SPO2->setRecMinY(90);
        SPO2->setRecMaxY(100);

        day->getSessions().clear();
        //QTimer::singleShot(10000,this,SLOT(oximeter_running_check()));
        if (!oximeter->startLive()) {
            mainwin->Notify(tr("Oximetry Error!\n\nSomething is wrong with the device connection."));
            return;
        }
        ui->saveButton->setEnabled(false);
        day->AddSession(oximeter->getSession());

        firstPulseUpdate=true;
        firstSPO2Update=true;
        secondPulseUpdate=true;
        secondSPO2Update=true;

        qint64 f=oximeter->getSession()->first();
        //day->setFirst(f);
        plethy->setMinX(f);
        pulse->setMinX(f);
        spo2->setMinX(f);
        PLETHY->SetMinX(f);
        CONTROL->SetMinX(f);
        PULSE->SetMinX(f);
        SPO2->SetMinX(f);

        //graphView()->updateScale();
        /*PLETHY->setForceMinY(0);
        PLETHY->setForceMaxY(128);
        PULSE->setForceMinY(30);
        PULSE->setForceMaxY(180);
        SPO2->setForceMinY(50);
        SPO2->setForceMaxY(100); */

        connect(oximeter,SIGNAL(dataChanged()),this,SLOT(data_changed()));
        connect(oximeter,SIGNAL(updatePulse(float)),this,SLOT(pulse_changed(float)));
        connect(oximeter,SIGNAL(updateSpO2(float)),this,SLOT(spo2_changed(float)));
        connect(oximeter,SIGNAL(liveStopped(Session*)),this,SLOT(live_stopped(Session *)));

        CONTROL->setVisible(false);
        // connect.
        ui->RunButton->setText(tr("&Stop"));
        ui->SerialPortsCombo->setEnabled(false);
        ui->ImportButton->setEnabled(false);
    }

}
void Oximetry::data_changed()
{

    qint64 last=oximeter->lastTime();
    qint64 first=last-30000L;
    //day->setLast(last);

    plethy->setMinX(first);
    plethy->setMaxX(last);
    pulse->setMinX(first);
    pulse->setMaxX(last);
    spo2->setMinX(first);
    spo2->setMaxX(last);

    plethy->setMinY((oximeter->Plethy())->Min());
    plethy->setMaxY((oximeter->Plethy())->Max());
    pulse->setMinY((oximeter->Pulse())->Min());
    pulse->setMaxY((oximeter->Pulse())->Max());
    spo2->setMinY((oximeter->Spo2())->Min());
    spo2->setMaxY((oximeter->Spo2())->Max());

    PLETHY->SetMinY((oximeter->Plethy())->Min());
    PLETHY->SetMaxY((oximeter->Plethy())->Max());
    PULSE->SetMinY((oximeter->Pulse())->Min());
    PULSE->SetMaxY((oximeter->Pulse())->Max());
    SPO2->SetMinY((oximeter->Spo2())->Min());
    SPO2->SetMaxY((oximeter->Spo2())->Max());

    /*PLETHY->MinY();
    PLETHY->MaxY();
    PULSE->MinY();
    PULSE->MaxY();
    SPO2->MinY();
    SPO2->MaxY(); */

    PLETHY->SetMaxX(last);
    PULSE->SetMaxX(last);
    CONTROL->SetMaxX(last);
    SPO2->SetMaxX(last);

    for (int i=0;i<GraphView->size();i++) {
        (*GraphView)[i]->SetXBounds(first,last);
    }

    {
        int len=(last-first)/1000L;
        int h=len/3600;
        int m=(len /60) % 60;
        int s=(len % 60);
        if (qstatus2) qstatus2->setText(QString().sprintf("Rec %02i:%02i:%02i",h,m,s)); // translation fix?
    }

    GraphView->updateScale();
    GraphView->redraw();
}


extern QProgressBar *qprogress;
extern QLabel *qstatus;


void DumpBytes(int blocks, unsigned char * b,int len)
{
    QString a=QString::number(blocks,16)+": Bytes "+QString::number(len,16)+": ";
    for (int i=0;i<len;i++) {
        a.append(QString::number(b[i],16)+" ");
    }
    qDebug() << a;
}
void Oximetry::oximeter_running_check()
{
    if (!oximeter->isOpen()) {
        if (oximeter->callbacks()==0) {
            qDebug() << "Not sure how oximeter_running_check gets called with a closed oximeter.. Restarting import process";
            //mainwin->Notify(tr("Oximeter Error\n\nThe device has not responded.. Make sure it's switched on2"));
            on_ImportButton_clicked();
            return;
        }
    }
    if (oximeter->callbacks()==0) {
        mainwin->Notify(tr("Oximeter Error\n\nThe device has not responded.. Make sure it's switched on."));
        if (oximeter->mode()==SO_IMPORT) oximeter->stopImport();
        if (oximeter->mode()==SO_LIVE) oximeter->stopLive();

        oximeter->destroySession();
        day->getSessions().clear();
        ui->SerialPortsCombo->setEnabled(true);
        qstatus->setText(tr("Ready"));
        ui->ImportButton->setEnabled(true);
        ui->RunButton->setChecked(false);
        ui->saveButton->setEnabled(false);
        GraphView->setEmptyText(tr("Check Oximeter is Ready"));
        GraphView->redraw();

    }
}

// Move this code to CMS50 Importer??
void Oximetry::on_ImportButton_clicked()
{
    connect(oximeter,SIGNAL(importComplete(Session*)),this,SLOT(import_complete(Session*)));
    connect(oximeter,SIGNAL(importAborted()),this,SLOT(import_aborted()));
    connect(oximeter,SIGNAL(updateProgress(float)),this,SLOT(update_progress(float)));

    PLETHY->setVisible(false);
    day->getSessions().clear();
    GraphView->setDay(day);
    GraphView->setEmptyText("Make Sure Oximeter Is Ready");
    GraphView->redraw();

    if (!oximeter->startImport()) {
        mainwin->Notify(tr("Oximeter Error\n\nThe device did not respond.. Make sure it's switched on."));
        disconnect(oximeter,SIGNAL(importComplete(Session*)),this,SLOT(import_complete(Session*)));
        disconnect(oximeter,SIGNAL(importAborted()),this,SLOT(import_aborted()));
        disconnect(oximeter,SIGNAL(updateProgress(float)),this,SLOT(update_progress(float)));
        //qDebug() << "Error starting oximetry serial import process";
        return;
    }
    //QTimer::singleShot(1000,this,SLOT(oximeter_running_check()));

    if (qprogress) {
        qprogress->setValue(0);
        qprogress->setMaximum(100);
        qprogress->show();
    }
    ui->ImportButton->setDisabled(true);
    ui->SerialPortsCombo->setEnabled(false);
    ui->RunButton->setText(tr("&Start"));
    ui->RunButton->setChecked(false);
}

void Oximetry::import_finished()
{
    disconnect(oximeter,SIGNAL(importComplete(Session*)),this,SLOT(import_complete(Session*)));
    disconnect(oximeter,SIGNAL(importAborted()),this,SLOT(import_aborted()));
    disconnect(oximeter,SIGNAL(updateProgress(float)),this,SLOT(update_progress(float)));

    ui->SerialPortsCombo->setEnabled(true);
    qstatus->setText(tr("Ready"));
    ui->ImportButton->setDisabled(false);
    ui->saveButton->setEnabled(true);

    if (qprogress) {
        qprogress->setValue(100);
        qprogress->hide();
    }
}

void Oximetry::import_aborted()
{
    oximeter->disconnect(oximeter,SIGNAL(importProcess()),0,0);
    day->getSessions().clear();
    //QMessageBox::warning(mainwin,tr("Oximeter Error"),tr("Please make sure your oximeter is switched on, and able to transmit data.\n(You may need to enter the oximeters Settings screen for it to be able to transmit.)"),QMessageBox::Ok);
    mainwin->Notify(tr("Please make sure your oximeter is switched on, and in the right mode to transmit data."),tr("Oximeter Error!"),5000);
    //qDebug() << "Oximetry import failed";
    import_finished();

}
void Oximetry::import_complete(Session * session)
{
    qDebug() << "Oximetry import complete";
    import_finished();
    day->AddSession(oximeter->getSession());

    if (!session) {
        qDebug() << "Shouldn't happen";
        return;
    }

    //calcSPO2Drop(session);
    //calcPulseChange(session);
    //PLETHY->setVisible(false);

    CONTROL->setVisible(false);

    saved_starttime=session->first();
    dont_update_date=true;
    ui->dateEdit->setDateTime(QDateTime::fromTime_t(saved_starttime/1000L));
    dont_update_date=false;


    updateGraphs();
}

void Oximetry::pulse_changed(float p)
{
    ui->pulseLCD->display(p);
    return;
    if (firstPulseUpdate) {
        if (secondPulseUpdate) {
            secondPulseUpdate=false;
        } else {
            firstPulseUpdate=false;
            GraphView->updateScale();
        }
    }
}

// Only really need to do this once..
void Oximetry::spo2_changed(float o2)
{
    ui->spo2LCD->display(o2);
    return;
    if (firstSPO2Update) {
        if (secondSPO2Update) {
            secondSPO2Update=false;
        } else {
            firstSPO2Update=false;
            GraphView->updateScale();
        }
    }
}

void Oximetry::on_saveButton_clicked()
{
    if (QMessageBox::question(this,"Keep This Recording?","Would you like to save this oximetery session?",QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes) {
        Session *session=oximeter->getSession();

        // Process???
        //session->UpdateSummaries();
        PROFILE.RemoveSession(session);
        Machine *m=oximeter->getMachine();
        if (m->SessionExists(session->session())) {
            m->sessionlist.erase(m->sessionlist.find(session->session()));
        }
        QString path=PROFILE.Get(m->properties[STR_PROP_Path])+QString().sprintf("%08lx",session->session());
        QString f1=path+".000";
        QString f2=path+".001";
        QFile::remove(f1);
        QFile::remove(f2);
        // Forgetting to reset the session ID sucks, as it will delete sessions you don't want to delete..
        session->SetSessionID(qint64(session->first())/1000L);

        m->AddSession(session,p_profile);

        oximeter->getMachine()->Save();
        day->getSessions().clear();

        mainwin->getDaily()->LoadDate(mainwin->getDaily()->getDate());
        mainwin->getOverview()->ReloadGraphs();
        GraphView->setEmptyText("No Oximetry Data");
        GraphView->redraw();
    }
}
void Oximetry::update_progress(float f)
{
    if (qprogress) {
        qprogress->setValue(f*100.0);
        QApplication::processEvents();
    }
}

bool Oximetry::openSPOFile(QString filename)
{
    QFile f(filename);
    if (!f.open(QFile::ReadOnly)) return false;

    QByteArray data;

    data=f.readAll();
    long size=data.size();

    int pos=((unsigned char)data.at(1) << 8) | (unsigned char)data.at(0);
    char dchr[20];
    int j=0;
    for (int i=0;i<18*2;i+=2) {
        dchr[j++]=data.at(8+i);
    }
    dchr[j]=0;
    QString dstr(dchr);
    QDateTime date=QDateTime::fromString(dstr,"MM/dd/yy HH:mm:ss");
    if (date.date().year()<2000) date=date.addYears(100);
    //ui->dateEdit->setDateTime(date);

    day->getSessions().clear();
    oximeter->createSession(date);
    Session *session=oximeter->getSession();
    day->AddSession(session);
    session->set_first(0);

    firstPulseUpdate=true;
    firstSPO2Update=true;
    secondPulseUpdate=true;
    secondSPO2Update=true;

    unsigned char o2,pr;
    //quint16 pl;
    qint64 tt=qint64(date.toTime_t())*1000L;

    for (int i=pos;i<size-5;) {
        o2=(unsigned char)(data.at(i+4));
        pr=(unsigned char)(data.at(i+3));
        //oximeter->setLastTime(tt);
        oximeter->addPulse(tt,pr);
        oximeter->addSpO2(tt,o2);
        //pl=(unsigned char)(data.at(i+1));


        //oximeter->addPlethy(tt,pl);
        //pl=(unsigned char)(data.at(i+1));
        //oximeter->addPlethy(tt,pl);
        //pl=(unsigned char)(data.at(i+2));
        //oximeter->addPlethy(tt,pl);
        i+=5;
        //data_changed();
        tt+=1000;
    }
    qint64 t1=session->first(OXI_Pulse);
    qint64 t2=session->first(OXI_SPO2);
    qint64 t3=qMin(t1,t2);
    session->set_first(t3);
    //day->setFirst(t3);
    int zi=t3/1000L;
    session->SetSessionID(zi);
    date.fromTime_t(zi);
    dont_update_date=true;
    ui->dateEdit->setDateTime(date);
    dont_update_date=false;

    t1=session->last(OXI_Pulse);
    t2=session->last(OXI_SPO2);
    t3=qMax(t1,t2);
    session->set_last(t3);
    //day->setLast(t3);
    CONTROL->setVisible(false);

    updateGraphs();

    f.close();
    ui->saveButton->setEnabled(true);
    return true;
}

bool Oximetry::openSPORFile(QString filename)
{
    //GraphView->setEmptyText("Please Wait");
    //GraphView->redraw();
    QFile f(filename);
    if (!f.open(QFile::ReadOnly)) return false;

    QByteArray data;

    data=f.readAll();
    long size=data.size();

    int pos=((unsigned char)data.at(1) << 8) | (unsigned char)data.at(0);
    char dchr[20];
    int j=0;
    for (int i=0;i<18*2;i+=2) {
        dchr[j++]=data.at(8+i);
    }
    dchr[j]=0;
    QString dstr(dchr);
    QDateTime date=QDateTime::fromString(dstr,"MM/dd/yy HH:mm:ss");
    if (date.date().year()<2000) date=date.addYears(100);

    day->getSessions().clear();
    oximeter->createSession(date);
    Session *session=oximeter->getSession();
    day->AddSession(session);
    session->set_first(0);

    firstPulseUpdate=true;
    firstSPO2Update=true;
    secondPulseUpdate=true;
    secondSPO2Update=true;

    unsigned char o2,pr;
    //quint16 pl;
    qint64 tt=qint64(date.toTime_t())*1000L;

    for (int i=pos;i<size-2;) {
        o2=(unsigned char)(data.at(i+1));
        pr=(unsigned char)(data.at(i+0));
        oximeter->addPulse(tt,pr);
        oximeter->addSpO2(tt,o2);
        //pl=(unsigned char)(data.at(i+1));
        i+=2;
        tt+=1000;
    }
    qint64 t1=session->first(OXI_Pulse);
    qint64 t2=session->first(OXI_SPO2);
    qint64 t3=qMin(t1,t2);
    session->set_first(t3);
    //day->setFirst(t3);
    int zi=t3/1000L;
    session->SetSessionID(zi);
    date.fromTime_t(zi);
    dont_update_date=true;
    ui->dateEdit->setDateTime(date);
    dont_update_date=false;


    t1=session->last(OXI_Pulse);
    t2=session->last(OXI_SPO2);
    t3=qMax(t1,t2);
    session->set_last(t3);
    //day->setLast(t3);


    //PLETHY->setVisible(false);
    CONTROL->setVisible(false);

    updateGraphs();
    f.close();
    ui->saveButton->setEnabled(true);
    return true;
}

void Oximetry::on_openButton_clicked()
{
    if (oximeter->getSession() && oximeter->getSession()->IsChanged()) {
        int res=QMessageBox::question(this,"Save Session?","Opening this oximetry file will destroy the current session.\nWould you like to keep it?","Save","Destroy It","Cancel",0,2);
        if (res==0) {
            on_saveButton_clicked();
            return;
        } else if (res==2) {
            return;
        }
    } // else it's already saved.

    QString dir="";
    QFileDialog fd(this,"Select an oximetry file",dir,"Oximetry Files (*.spo *.spoR)");
    fd.setAcceptMode(QFileDialog::AcceptOpen);
    fd.setFileMode(QFileDialog::ExistingFile);
    if (fd.exec()!=QFileDialog::Accepted) return;
    QStringList filenames=fd.selectedFiles();
    if (filenames.size()>1) {
        qDebug() << "Can only open one oximetry file at a time";
    }
    QString filename=filenames[0];
    bool r=false;
    if (filename.toLower().endsWith(".spo")) r=openSPOFile(filename);
    else if (filename.toLower().endsWith(".spor")) r=openSPORFile(filename);

    if (!r) {
        mainwin->Notify("Couldn't open oximetry file \""+filename+"\".");
    }
    qDebug() << "opening" << filename;
}

void Oximetry::on_dateEdit_dateTimeChanged(const QDateTime &date)
{
    Session *session=oximeter->getSession();
    if (!session)
        return;
    if (dont_update_date)
        return;

    qint64 first=session->first();
    //qint64 last=session->last();
    qint64 tt=qint64(date.toTime_t())*1000L;
    qint64 offset=tt-first;

    if (offset!=0) {
        session->SetChanged(true);
        ui->saveButton->setEnabled(true);
    }

    session->offsetSession(offset);

    updateGraphs();
}

void Oximetry::openSession(Session * session)
{
    if (oximeter->getSession() && oximeter->getSession()->IsChanged()) {
        int res=QMessageBox::question(this,"Save Session?","Opening this oximetry session will destroy the unsavedsession in the oximetry tab.\nWould you like to store it first?","Save","Destroy It","Cancel",0,2);
        if (res==0) {
            on_saveButton_clicked();
            return;
        } else if (res==2) {
            return;
        }
    } // else it's already saved.

    day->getSessions().clear();
    day->AddSession(session);

    oximeter->setSession(session);

    saved_starttime=session->first();
    oximeter->compactAll();

    QDateTime date=QDateTime::fromTime_t(saved_starttime/1000L);
    dont_update_date=true;
    ui->dateEdit->setDateTime(date);
    dont_update_date=false;
    updateGraphs();

}
void Oximetry::updateGraphs()
{
    Session * session=oximeter->getSession();
    if (!session) return;

    qint64 first=session->first();
    qint64 last=session->last();

    ui->pulseLCD->display(session->Min(OXI_Pulse));
    ui->spo2LCD->display(session->Min(OXI_SPO2));

    pulse->setMinY(session->Min(OXI_Pulse));
    pulse->setMaxY(session->Max(OXI_Pulse));
    spo2->setMinY(session->Min(OXI_SPO2));
    spo2->setMaxY(session->Max(OXI_SPO2));

    PULSE->setRecMinY(60);
    PULSE->setRecMaxY(100);
    SPO2->setRecMinY(90);
    SPO2->setRecMaxY(100);

    //day->setFirst(first);
    //day->setLast(last);
    pulse->setMinY(session->Min(OXI_Pulse));
    pulse->setMaxY(session->Max(OXI_Pulse));
    spo2->setMinY(session->Min(OXI_SPO2));
    spo2->setMaxY(session->Max(OXI_SPO2));

    PULSE->setRecMinY(60);
    PULSE->setRecMaxY(100);
    SPO2->setRecMinY(90);
    SPO2->setRecMaxY(100);

    plethy->setMinX(first);
    pulse->setMinX(first);
    spo2->setMinX(first);
    PLETHY->SetMinX(first);
    CONTROL->SetMinX(first);
    PULSE->SetMinX(first);
    SPO2->SetMinX(first);

    plethy->setMaxX(last);
    pulse->setMaxX(last);
    spo2->setMaxX(last);
    PLETHY->SetMaxX(last);
    CONTROL->SetMaxX(last);
    PULSE->SetMaxX(last);
    SPO2->SetMaxX(last);

    PULSE->setDay(day);
    SPO2->setDay(day);
    for (int i=0;i<GraphView->size();i++) {
       (*GraphView)[i]->SetXBounds(first,last);
    }
    {
        int len=(last-first)/1000L;
        int h=len/3600;
        int m=(len /60) % 60;
        int s=(len % 60);
        if (qstatus2) qstatus2->setText(QString().sprintf("%02i:%02i:%02i",h,m,s));
    }
    GraphView->updateScale();
    GraphView->redraw();
}

void Oximetry::on_resetTimeButton_clicked() //revert to original session time
{
    if (oximeter->getSession()) {

        //qint64 tt=ui->dateEdit->dateTime().toTime_t()*1000;
        //qint64 offset=saved_starttime-tt;
        //oximeter->getSession()->offsetSession(offset);
        dont_update_date=false;
        //ui->dateEdit->setDateTime(QDateTime::fromTime_t(saved_starttime/1000L));
        ui->dateEdit->setDateTime(QDateTime::fromTime_t(saved_starttime/1000L));
        dont_update_date=false;
    }
}
