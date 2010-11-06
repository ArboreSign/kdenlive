/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "recmonitor.h"
#include "gentime.h"
#include "kdenlivesettings.h"
#include "managecapturesdialog.h"

#include <KDebug>
#include <KLocale>
#include <QPainter>
#include <KStandardDirs>
#include <KComboBox>
#include <KIO/NetAccess>
#include <KFileItem>
#include <KMessageBox>
#include <KApplication>

#if KDE_IS_VERSION(4,2,0)
#include <KDiskFreeSpaceInfo>
#endif

#include <QMouseEvent>
#include <QMenu>
#include <QToolButton>
#include <QFile>
#include <QDir>


RecMonitor::RecMonitor(QString name, QWidget *parent) :
    QWidget(parent),
    m_name(name),
    m_isActive(false),
    m_isCapturing(false),
    m_didCapture(false),
    m_isPlaying(false),
    m_bmCapture(NULL),
    m_blackmagicCapturing(false)
{
    setupUi(this);

    video_frame->setAttribute(Qt::WA_PaintOnScreen);
    device_selector->setCurrentIndex(KdenliveSettings::defaultcapture());
    connect(device_selector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoDeviceChanged(int)));

    QToolBar *toolbar = new QToolBar(this);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");

    m_discAction = toolbar->addAction(KIcon("network-connect"), i18n("Connect"));
    connect(m_discAction, SIGNAL(triggered()), this, SLOT(slotDisconnect()));

    m_rewAction = toolbar->addAction(KIcon("media-seek-backward"), i18n("Rewind"));
    connect(m_rewAction, SIGNAL(triggered()), this, SLOT(slotRewind()));

    m_playAction = toolbar->addAction(m_playIcon, i18n("Play"));
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotStartCapture()));

    m_stopAction = toolbar->addAction(KIcon("media-playback-stop"), i18n("Stop"));
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(slotStopCapture()));
    m_stopAction->setEnabled(false);
    m_fwdAction = toolbar->addAction(KIcon("media-seek-forward"), i18n("Forward"));
    connect(m_fwdAction, SIGNAL(triggered()), this, SLOT(slotForward()));

    m_recAction = toolbar->addAction(KIcon("media-record"), i18n("Record"));
    connect(m_recAction, SIGNAL(triggered()), this, SLOT(slotRecord()));
    m_recAction->setCheckable(true);

    toolbar->addSeparator();

    QAction *configAction = toolbar->addAction(KIcon("configure"), i18n("Configure"));
    connect(configAction, SIGNAL(triggered()), this, SLOT(slotConfigure()));
    configAction->setCheckable(false);

    layout->addWidget(toolbar);
    layout->addWidget(&m_logger);
    layout->addWidget(&m_dvinfo);
    m_logger.setMaxCount(10);
    m_logger.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logger.setFrame(false);
    //m_logger.setInsertPolicy(QComboBox::InsertAtTop);

#if KDE_IS_VERSION(4,2,0)
    m_freeSpace = new KCapacityBar(KCapacityBar::DrawTextInline, this);
    m_freeSpace->setMaximumWidth(150);
    QFontMetricsF fontMetrics(font());
    m_freeSpace->setMaximumHeight(fontMetrics.height() * 1.2);
    slotUpdateFreeSpace();
    layout->addWidget(m_freeSpace);
    connect(&m_spaceTimer, SIGNAL(timeout()), this, SLOT(slotUpdateFreeSpace()));
    m_spaceTimer.setInterval(30000);
    m_spaceTimer.setSingleShot(false);
#endif

    control_frame_firewire->setLayout(layout);

    slotVideoDeviceChanged(device_selector->currentIndex());
    m_displayProcess = new QProcess;
    m_captureProcess = new QProcess;

    connect(m_captureProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotProcessStatus(QProcess::ProcessState)));
    connect(m_captureProcess, SIGNAL(readyReadStandardError()), this, SLOT(slotReadDvgrabInfo()));

    QStringList env = QProcess::systemEnvironment();
    env << "SDL_WINDOWID=" + QString::number(video_frame->winId());

    QString videoDriver = KdenliveSettings::videodrivername();
    if (!videoDriver.isEmpty()) {
        if (videoDriver == "x11_noaccel") {
            env << "SDL_VIDEO_YUV_HWACCEL=0";
            env << "SDL_VIDEODRIVER=x11";
        } else env << "SDL_VIDEODRIVER=" + videoDriver;
    }
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);

    m_displayProcess->setEnvironment(env);

    if (KdenliveSettings::video4capture().isEmpty()) {
        QString captureCommand;
        if (!KdenliveSettings::video4adevice().isEmpty()) captureCommand = "-f " + KdenliveSettings::video4aformat() + " -i " + KdenliveSettings::video4adevice() + " -acodec " + KdenliveSettings::video4acodec();

        captureCommand +=  " -f " + KdenliveSettings::video4vformat() + " -s " + KdenliveSettings::video4size() + " -r " + QString::number(KdenliveSettings::video4rate()) + " -i " + KdenliveSettings::video4vdevice() + " -vcodec " + KdenliveSettings::video4vcodec();;
        KdenliveSettings::setVideo4capture(captureCommand);
    }

    kDebug() << "/////// BUILDING MONITOR, ID: " << video_frame->winId();
}

RecMonitor::~RecMonitor()
{
#if KDE_IS_VERSION(4,2,0)
    m_spaceTimer.stop();
#endif
    delete m_captureProcess;
    delete m_displayProcess;
}

QString RecMonitor::name() const
{
    return m_name;
}

void RecMonitor::slotConfigure()
{
    emit showConfigDialog(4, device_selector->currentIndex());
}

void RecMonitor::slotUpdateCaptureFolder(const QString currentProjectFolder)
{
    if (KdenliveSettings::capturetoprojectfolder()) m_capturePath = currentProjectFolder;
    else m_capturePath = KdenliveSettings::capturefolder();
    if (m_captureProcess) m_captureProcess->setWorkingDirectory(m_capturePath);
    if (m_captureProcess->state() != QProcess::NotRunning) {
        if (device_selector->currentIndex() == FIREWIRE)
            KMessageBox::information(this, i18n("You need to disconnect and reconnect in the capture monitor to apply your changes"), i18n("Capturing"));
        else KMessageBox::information(this, i18n("You need to stop capture before your changes can be applied"), i18n("Capturing"));
    } else slotVideoDeviceChanged(device_selector->currentIndex());

#if KDE_IS_VERSION(4,2,0)
    // update free space info
    slotUpdateFreeSpace();
#endif
}

void RecMonitor::slotVideoDeviceChanged(int ix)
{
    QString capturefile;
    QString capturename;
    video_capture->setHidden(true);
    video_frame->setHidden(false);
    m_fwdAction->setVisible(ix != BLACKMAGIC);
    m_discAction->setVisible(ix != BLACKMAGIC);
    m_rewAction->setVisible(ix != BLACKMAGIC);
    m_logger.setVisible(ix == BLACKMAGIC);
    switch (ix) {
    case SCREENGRAB:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        if (KdenliveSettings::rmd_path().isEmpty()) {
            QString rmdpath = KStandardDirs::findExe("recordmydesktop");
            if (rmdpath.isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("dialog-warning").pixmap(QSize(50, 50)), i18n("Recordmydesktop utility not found,\n please install it for screen grabs")));
            else KdenliveSettings::setRmd_path(rmdpath);
        }
        if (!KdenliveSettings::rmd_path().isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("video-display").pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
        //video_frame->setText(i18n("Press record button\nto start screen capture"));
        break;
    case VIDEO4LINUX:
        m_discAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);
        checkDeviceAvailability();
        break;
    case BLACKMAGIC:
        createBlackmagicDevice();
        m_recAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(true);

        capturefile = m_capturePath;
        if (!capturefile.endsWith("/")) capturefile.append("/");
        capturename = KdenliveSettings::hdmifilename();
        capturename.append("xxx.raw");
        capturefile.append(capturename);
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-photo").pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress play button\nto start preview.\nFiles will be saved in:\n%1", capturefile)));
        break;
    default: // FIREWIRE
        m_discAction->setEnabled(true);
        m_recAction->setEnabled(false);
        m_stopAction->setEnabled(false);
        m_playAction->setEnabled(false);
        m_rewAction->setEnabled(false);
        m_fwdAction->setEnabled(false);

        // Check that dvgab is available
        if (KdenliveSettings::dvgrab_path().isEmpty()) {
            QString dvgrabpath = KStandardDirs::findExe("dvgrab");
            if (dvgrabpath.isEmpty()) video_frame->setPixmap(mergeSideBySide(KIcon("dialog-warning").pixmap(QSize(50, 50)), i18n("dvgrab utility not found,\n please install it for firewire capture")));
            else KdenliveSettings::setDvgrab_path(dvgrabpath);
        } else {
            // Show capture info
            capturefile = m_capturePath;
            if (!capturefile.endsWith("/")) capturefile.append("/");
            capturename = KdenliveSettings::dvgrabfilename();
            if (capturename.isEmpty()) capturename = "capture";
            QString extension;
            switch (KdenliveSettings::firewireformat()) {
            case 0:
                extension = ".dv";
                break;
            case 1:
            case 2:
                extension = ".avi";
                break;
            case 3:
                extension = ".m2t";
                break;
            }
            capturename.append("xxx" + extension);
            capturefile.append(capturename);
            video_frame->setPixmap(mergeSideBySide(KIcon("network-connect").pixmap(QSize(50, 50)), i18n("Plug your camcorder and\npress connect button\nto initialize connection\nFiles will be saved in:\n%1", capturefile)));
        }
        break;
    }
}

void RecMonitor::createBlackmagicDevice()
{
    //video_capture->setVisible(true);
    if (m_bmCapture == NULL) {
        QVBoxLayout *lay = new QVBoxLayout;
        m_bmCapture = new BmdCaptureHandler(lay);
        connect(m_bmCapture, SIGNAL(gotTimeCode(ulong)), this, SLOT(slotGotBlackMagicFrameNumber(ulong)));
        connect(m_bmCapture, SIGNAL(gotMessage(const QString &)), this, SLOT(slotGotBlackmagicMessage(const QString &)));
        video_capture->setLayout(lay);
    }
}

void RecMonitor::slotGotBlackmagicFrameNumber(ulong ix)
{
    m_dvinfo.setText(QString::number(ix));
}

void RecMonitor::slotGotBlackmagicMessage(const QString &message)
{
    m_logger.insertItem(0, message);
}

QPixmap RecMonitor::mergeSideBySide(const QPixmap& pix, const QString txt)
{
    QPainter p;
    QRect r = QApplication::fontMetrics().boundingRect(QRect(0, 0, video_frame->width(), video_frame->height()), Qt::AlignLeft, txt);
    int strWidth = r.width();
    int strHeight = r.height();
    int pixWidth = pix.width();
    int pixHeight = pix.height();
    QPixmap res(strWidth + 8 + pixWidth, qMax(strHeight, pixHeight));
    res.fill(Qt::transparent);
    p.begin(&res);
    p.drawPixmap(0, 0, pix);
    p.setPen(kapp->palette().text().color());
    p.drawText(QRect(pixWidth + 8, 0, strWidth, strHeight), 0, txt);
    p.end();
    return res;
}


void RecMonitor::checkDeviceAvailability()
{
    if (!KIO::NetAccess::exists(KUrl(KdenliveSettings::video4vdevice()), KIO::NetAccess::SourceSide , this)) {
        m_playAction->setEnabled(false);
        m_recAction->setEnabled(false);
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice())));
        //video_frame->setText(i18n("Cannot read from device %1\nPlease check drivers and access rights.", KdenliveSettings::video4vdevice()));
    } else //video_frame->setText(i18n("Press play or record button\nto start video capture"));
        video_frame->setPixmap(mergeSideBySide(KIcon("camera-web").pixmap(QSize(50, 50)), i18n("Press play or record button\nto start video capture\nFiles will be saved in:\n%1", m_capturePath)));
}

void RecMonitor::slotDisconnect()
{
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_captureTime = KDateTime::currentLocalDateTime();
        kDebug() << "CURRENT TIME: " << m_captureTime.toString();
        m_didCapture = false;
        slotStartCapture(false);
        m_discAction->setIcon(KIcon("network-disconnect"));
        m_discAction->setText(i18n("Disconnect"));
        m_recAction->setEnabled(true);
        m_stopAction->setEnabled(true);
        m_playAction->setEnabled(true);
        m_rewAction->setEnabled(true);
        m_fwdAction->setEnabled(true);
    } else {
        m_captureProcess->write("q", 1);
        QTimer::singleShot(1000, m_captureProcess, SLOT(kill()));
        if (m_didCapture) manageCapturedFiles();
        m_didCapture = false;
    }
}

void RecMonitor::slotRewind()
{
    m_captureProcess->write("a", 1);
}

void RecMonitor::slotForward()
{
    m_captureProcess->write("z", 1);
}

void RecMonitor::slotStopCapture()
{
    // stop capture
    video_capture->setHidden(true);
    video_frame->setHidden(false);
    switch (device_selector->currentIndex()) {
    case FIREWIRE:
        m_captureProcess->write("\e", 2);
        m_playAction->setIcon(m_playIcon);
        m_isPlaying = false;
        break;
    case VIDEO4LINUX:
    case SCREENGRAB:
        m_captureProcess->write("q\n", 3);
        QTimer::singleShot(1000, m_captureProcess, SLOT(kill()));
        break;
    case BLACKMAGIC:
        m_bmCapture->stopPreview();
        m_playAction->setEnabled(true);
        m_stopAction->setEnabled(false);
        m_recAction->setEnabled(false);
        break;
    default:
        break;
    }
}

void RecMonitor::slotStartCapture(bool play)
{
    if (m_captureProcess->state() != QProcess::NotRunning) {
        if (device_selector->currentIndex() == FIREWIRE) {
            if (m_isPlaying) {
                m_captureProcess->write("k", 1);
                //captureProcess->write("\e", 2);
                m_playAction->setIcon(m_playIcon);
                m_isPlaying = false;
            } else {
                m_captureProcess->write("p", 1);
                m_playAction->setIcon(m_pauseIcon);
                m_isPlaying = true;
            }
        }
        return;
    }
    m_captureArgs.clear();
    m_displayArgs.clear();
    m_isPlaying = false;
    QString capturename = KdenliveSettings::dvgrabfilename();
    QStringList dvargs = KdenliveSettings::dvgrabextra().simplified().split(" ", QString::SkipEmptyParts);
    video_capture->setVisible(device_selector->currentIndex() == BLACKMAGIC);
    video_frame->setHidden(device_selector->currentIndex() == BLACKMAGIC);

    switch (device_selector->currentIndex()) {
    case FIREWIRE:
        switch (KdenliveSettings::firewireformat()) {
        case 0:
            // RAW DV CAPTURE
            m_captureArgs << "--format" << "raw";
            m_displayArgs << "-f" << "dv";
            break;
        case 1:
            // DV type 1
            m_captureArgs << "--format" << "dv1";
            m_displayArgs << "-f" << "dv";
            break;
        case 2:
            // DV type 2
            m_captureArgs << "--format" << "dv2";
            m_displayArgs << "-f" << "dv";
            break;
        case 3:
            // HDV CAPTURE
            m_captureArgs << "--format" << "hdv";
            m_displayArgs << "-f" << KdenliveSettings::video4container();
            break;
        }
        if (KdenliveSettings::firewireautosplit()) m_captureArgs << "--autosplit";
        if (KdenliveSettings::firewiretimestamp()) m_captureArgs << "--timestamp";
        if (!dvargs.isEmpty()) {
            m_captureArgs << dvargs;
        }
        m_captureArgs << "-i";
        if (capturename.isEmpty()) capturename = "capture";
        m_captureArgs << capturename << "-";

        m_displayArgs << "-x" << QString::number(video_frame->width()) << "-y" << QString::number(video_frame->height()) << "-";

        m_captureProcess->setStandardOutputProcess(m_displayProcess);
        m_captureProcess->setWorkingDirectory(m_capturePath);
        kDebug() << "Capture: Running dvgrab " << m_captureArgs.join(" ");

        m_captureProcess->start(KdenliveSettings::dvgrab_path(), m_captureArgs);
        if (play) m_captureProcess->write(" ", 1);
        m_discAction->setEnabled(true);
        break;
    case VIDEO4LINUX:
        m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << KdenliveSettings::video4encoding().simplified().split(' ') << "-f" << KdenliveSettings::video4container() << "-";
        m_displayArgs << "-f" << KdenliveSettings::video4container() << "-x" << QString::number(video_frame->width()) << "-y" << QString::number(video_frame->height()) << "-";
        m_captureProcess->setStandardOutputProcess(m_displayProcess);
        kDebug() << "Capture: Running ffmpeg " << m_captureArgs.join(" ");
        m_captureProcess->start("ffmpeg", m_captureArgs);
        break;
    case BLACKMAGIC:
        m_bmCapture->startPreview(KdenliveSettings::hdmi_capturedevice(), KdenliveSettings::hdmi_capturemode());
        m_playAction->setEnabled(false);
        m_stopAction->setEnabled(true);
        m_recAction->setEnabled(true);
        break;
    default:
        break;
    }

    if (device_selector->currentIndex() == FIREWIRE || device_selector->currentIndex() == VIDEO4LINUX) {
        kDebug() << "Capture: Running ffplay " << m_displayArgs.join(" ");
        m_displayProcess->start("ffplay", m_displayArgs);
        video_frame->setText(i18n("Initialising..."));
    } else {
        // do something when starting screen grab
    }
}

void RecMonitor::slotRecord()
{
    if (device_selector->currentIndex() == BLACKMAGIC) {
        if (m_blackmagicCapturing) {
            // We are capturing, stop it
            m_bmCapture->stopCapture();
            m_blackmagicCapturing = false;
        } else {
            // Start capture, get capture filename first
            QString path = m_capturePath;
            if (!path.endsWith("/")) path.append("/");
            path.append(KdenliveSettings::hdmifilename());
            m_bmCapture->startCapture(path);
            m_blackmagicCapturing = true;
        }
        return;
    }

    if (m_captureProcess->state() == QProcess::NotRunning && device_selector->currentIndex() == FIREWIRE) {
        slotStartCapture();
    }
    if (m_isCapturing) {
        switch (device_selector->currentIndex()) {
        case FIREWIRE:
            m_captureProcess->write("\e", 2);
            m_playAction->setIcon(m_playIcon);
            m_isCapturing = false;
            m_isPlaying = false;
            m_recAction->setChecked(false);
            break;
        case VIDEO4LINUX:
            m_captureProcess->terminate();
            slotStopCapture();
            //m_isCapturing = false;
            QTimer::singleShot(1000, this, SLOT(slotStartCapture()));
            break;
        case SCREENGRAB:
            //captureProcess->write("q\n", 3);
            m_captureProcess->terminate();
            video_frame->setText(i18n("Encoding captured video..."));
            // in case ffmpeg doesn't exit with the 'q' command, kill it one second later
            //QTimer::singleShot(1000, captureProcess, SLOT(kill()));
            break;
        }
        return;
    } else if (device_selector->currentIndex() == FIREWIRE) {
        m_isCapturing = true;
        m_didCapture = true;
        m_captureProcess->write("c\n", 3);
#if KDE_IS_VERSION(4,2,0)
        m_spaceTimer.start();
#endif
        return;
    }
    if (m_captureProcess->state() == QProcess::NotRunning) {
        m_recAction->setChecked(true);
        QString extension = "mp4";
        if (device_selector->currentIndex() == SCREENGRAB) extension = "ogv"; //KdenliveSettings::screengrabextension();
        else if (device_selector->currentIndex() == VIDEO4LINUX) extension = KdenliveSettings::video4extension();
        QString path = m_capturePath + "/capture0000." + extension;
        int i = 1;
        while (QFile::exists(path)) {
            QString num = QString::number(i).rightJustified(4, '0', false);
            path = m_capturePath + "/capture" + num + '.' + extension;
            i++;
        }
        m_captureFile = KUrl(path);

        m_captureArgs.clear();
        m_displayArgs.clear();
        QString args;
        QString capturename = KdenliveSettings::dvgrabfilename();
        if (capturename.isEmpty()) capturename = "capture";

        switch (device_selector->currentIndex()) {
        case VIDEO4LINUX:
            m_captureArgs << KdenliveSettings::video4capture().simplified().split(' ') << KdenliveSettings::video4encoding().simplified().split(' ') << "-y" << m_captureFile.path() << "-f" << KdenliveSettings::video4container() << "-acodec" << KdenliveSettings::video4acodec() << "-vcodec" << KdenliveSettings::video4vcodec() << "-";
            m_displayArgs << "-f" << KdenliveSettings::video4container() << "-x" << QString::number(video_frame->width()) << "-y" << QString::number(video_frame->height()) << "-";
            m_captureProcess->setStandardOutputProcess(m_displayProcess);
            kDebug() << "Capture: Running ffmpeg " << m_captureArgs.join(" ");
            m_captureProcess->start("ffmpeg", m_captureArgs);
            break;
        case SCREENGRAB:
            switch (KdenliveSettings::rmd_capture_type()) {
            case 0:
                // Full screen capture, nothing special to do
                break;
            default:
                // Region capture
                m_captureArgs << "--width" << QString::number(KdenliveSettings::rmd_width()) << "--height" << QString::number(KdenliveSettings::rmd_height());
                if (!KdenliveSettings::rmd_follow_mouse()) {
                    m_captureArgs << "-x" << QString::number(KdenliveSettings::rmd_offsetx()) << "-y" << QString::number(KdenliveSettings::rmd_offsety());
                } else {
                    m_captureArgs << "--follow-mouse";
                    if (KdenliveSettings::rmd_hide_frame()) m_captureArgs << "--no-frame";
                }
                break;
            }
            if (KdenliveSettings::rmd_hide_mouse()) m_captureArgs << "--no-cursor";
            m_isCapturing = true;
            if (KdenliveSettings::rmd_capture_audio()) {
                m_captureArgs << "--freq" << KdenliveSettings::rmd_freq();
                m_captureArgs << "--channels" << QString::number(KdenliveSettings::rmd_audio_channels());
                if (KdenliveSettings::rmd_use_jack()) {
                    m_captureArgs << "--use-jack";
                    QStringList ports = KdenliveSettings::rmd_jackports().split(" ", QString::SkipEmptyParts);
                    for (int i = 0; i < ports.count(); ++i) {
                        m_captureArgs << ports.at(i);
                    }
                    if (KdenliveSettings::rmd_jack_buffer() > 0.0)
                        m_captureArgs << "--ring-buffer-size" << QString::number(KdenliveSettings::rmd_jack_buffer());
                } else {
                    if (!KdenliveSettings::rmd_alsadevicename().isEmpty())
                        m_captureArgs << "--device" << KdenliveSettings::rmd_alsadevicename();
                    if (KdenliveSettings::rmd_alsa_buffer() > 0)
                        m_captureArgs << "--buffer-size" << QString::number(KdenliveSettings::rmd_alsa_buffer());
                }
            } else m_captureArgs << "--no-sound";

            if (KdenliveSettings::rmd_fullshots()) m_captureArgs << "--full-shots";
            m_captureArgs << "--v_bitrate" << QString::number(KdenliveSettings::rmd_bitrate());
            m_captureArgs << "--v_quality" << QString::number(KdenliveSettings::rmd_quality());
            m_captureArgs << "--workdir" << KdenliveSettings::currenttmpfolder();
            m_captureArgs << "--fps" << QString::number(KdenliveSettings::rmd_fps()) << "-o" << m_captureFile.path();
            m_captureProcess->start(KdenliveSettings::rmd_path(), m_captureArgs);
            kDebug() << "// RecordMyDesktop params: " << m_captureArgs;
            break;
        default:
            break;
        }


        if (device_selector->currentIndex() != SCREENGRAB) {
            m_isCapturing = true;
            kDebug() << "Capture: Running ffplay " << m_displayArgs.join(" ");
            m_displayProcess->start("ffplay", m_displayArgs);
            video_frame->setText(i18n("Initialising..."));
        }
    } else {
        // stop capture
        m_displayProcess->kill();
        //captureProcess->kill();
        QTimer::singleShot(1000, this, SLOT(slotRecord()));
    }
}

/*
void RecMonitor::slotStartGrab(const QRect &rect) {
    rgnGrab->deleteLater();
    QApplication::restoreOverrideCursor();
    show();
    if (rect.isNull()) return;
    int width = rect.width();
    int height = rect.height();
    if (width % 2 != 0) width--;
    if (height % 2 != 0) height--;
    QString args = KdenliveSettings::screengrabcapture().replace("%size", QString::number(width) + "x" + QString::number(height)).replace("%offset", "+" + QString::number(rect.x()) + "," + QString::number(rect.y()));
    if (KdenliveSettings::screengrabenableaudio()) {
        // also capture audio
        if (KdenliveSettings::useosscapture()) m_captureArgs << KdenliveSettings::screengrabosscapture().simplified().split(' ');
        else m_captureArgs << KdenliveSettings::screengrabalsacapture2().simplified().split(' ');
    }
    m_captureArgs << args.simplified().split(' ') << KdenliveSettings::screengrabencoding().simplified().split(' ') << m_captureFile.path();
    m_isCapturing = true;
    video_frame->setText(i18n("Capturing..."));
    if (KdenliveSettings::screengrabenableaudio() && !KdenliveSettings::useosscapture()) {
        QStringList alsaArgs = KdenliveSettings::screengrabalsacapture().simplified().split(' ');
        alsaProcess->setStandardOutputProcess(captureProcess);
        kDebug() << "Capture: Running arecord " << alsaArgs.join(" ");
        alsaProcess->start("arecord", alsaArgs);
    }
    kDebug() << "Capture: Running ffmpeg " << m_captureArgs.join(" ");
    captureProcess->start("ffmpeg", m_captureArgs);
}*/

void RecMonitor::slotProcessStatus(QProcess::ProcessState status)
{
    if (status == QProcess::NotRunning) {
        m_displayProcess->kill();
        if (m_isCapturing && device_selector->currentIndex() != FIREWIRE)
            if (autoaddbox->isChecked() && QFile::exists(m_captureFile.path())) emit addProjectClip(m_captureFile);
        if (device_selector->currentIndex() == FIREWIRE) {
            m_discAction->setIcon(KIcon("network-connect"));
            m_discAction->setText(i18n("Connect"));
            m_playAction->setEnabled(false);
            m_rewAction->setEnabled(false);
            m_fwdAction->setEnabled(false);
            m_recAction->setEnabled(false);
        }
        m_isPlaying = false;
        m_playAction->setIcon(m_playIcon);
        m_recAction->setChecked(false);
        m_stopAction->setEnabled(false);
        device_selector->setEnabled(true);
        if (m_captureProcess && m_captureProcess->exitStatus() == QProcess::CrashExit) {
            video_frame->setText(i18n("Capture crashed, please check your parameters"));
        } else {
            if (device_selector->currentIndex() != SCREENGRAB) {
                video_frame->setText(i18n("Not connected"));
            } else {
                if (m_captureProcess->exitCode() != 0) {
                    video_frame->setText(i18n("Capture crashed, please check your parameters\nRecordMyDesktop exit code: %1", QString::number(m_captureProcess->exitCode())));
                } else {
                    video_frame->setPixmap(mergeSideBySide(KIcon("video-display").pixmap(QSize(50, 50)), i18n("Press record button\nto start screen capture\nFiles will be saved in:\n%1", m_capturePath)));
                }
            }
        }
        m_isCapturing = false;

#if KDE_IS_VERSION(4,2,0)
        m_spaceTimer.stop();
        // update free space info
        slotUpdateFreeSpace();
#endif

    } else {
        if (device_selector->currentIndex() != SCREENGRAB) m_stopAction->setEnabled(true);
        device_selector->setEnabled(false);
    }
}

void RecMonitor::manageCapturedFiles()
{
    QString extension;
    switch (KdenliveSettings::firewireformat()) {
    case 0:
        extension = ".dv";
        break;
    case 1:
    case 2:
        extension = ".avi";
        break;
    case 3:
        extension = ".m2t";
        break;
    }
    QDir dir(m_capturePath);
    QStringList filters;
    QString capturename = KdenliveSettings::dvgrabfilename();
    if (capturename.isEmpty()) capturename = "capture";
    filters << capturename + "*" + extension;
    const QStringList result = dir.entryList(filters, QDir::Files, QDir::Time);
    KUrl::List capturedFiles;
    foreach(const QString & name, result) {
        KUrl url = KUrl(dir.filePath(name));
        if (KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, this)) {
            KFileItem file(KFileItem::Unknown, KFileItem::Unknown, url, true);
            if (file.time(KFileItem::ModificationTime) > m_captureTime) capturedFiles.append(url);
        }
    }
    kDebug() << "Found : " << capturedFiles.count() << " new capture files";
    kDebug() << capturedFiles;

    if (capturedFiles.count() > 0) {
        ManageCapturesDialog *d = new ManageCapturesDialog(capturedFiles, this);
        if (d->exec() == QDialog::Accepted) {
            capturedFiles = d->importFiles();
            foreach(const KUrl & url, capturedFiles) {
                emit addProjectClip(url);
            }
        }
        delete d;
    }
}

// virtual
void RecMonitor::mousePressEvent(QMouseEvent * /*event*/)
{
#if KDE_IS_VERSION(4,2,0)
    if (m_freeSpace->underMouse()) slotUpdateFreeSpace();
#endif
}

void RecMonitor::slotUpdateFreeSpace()
{
#if KDE_IS_VERSION(4,2,0)
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(m_capturePath);
    if (info.isValid()) {
        m_freeSpace->setValue(100 * info.used() / info.size());
        m_freeSpace->setText(i18n("Free space: %1", KIO::convertSize(info.available())));
        m_freeSpace->update();
    }
#endif
}

void RecMonitor::activateRecMonitor()
{
    //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);
}

void RecMonitor::stop()
{
    m_isActive = false;

}

void RecMonitor::start()
{
    m_isActive = true;

}

void RecMonitor::refreshRecMonitor(bool visible)
{
    if (visible) {
        //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);

    }
}

void RecMonitor::slotPlay()
{

    //if (!m_isActive) m_monitorManager->activateRecMonitor(m_name);

}

void RecMonitor::slotReadDvgrabInfo()
{
    QString data = m_captureProcess->readAllStandardError().simplified();
    data = data.section('"', 2, 2).simplified();
    m_dvinfo.setText(data.left(11));
    m_dvinfo.updateGeometry();
}

#include "recmonitor.moc"

