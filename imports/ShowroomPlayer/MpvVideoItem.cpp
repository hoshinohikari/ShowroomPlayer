#include "MpvVideoItem.h"
#include "ShowroomLog.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QPlaybackOptions>
#include <QTimer>
#include <QUrl>

#include <QtMultimediaQuick/private/qquickvideooutput_p.h>

namespace {

void ensureFfmpegBackend()
{
    if (!qEnvironmentVariableIsSet("QT_MEDIA_BACKEND"))
        qputenv("QT_MEDIA_BACKEND", "ffmpeg");
}

} // namespace

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    qCDebug(lcShowroomPlayer) << "Creating MpvVideoItem (Qt Multimedia)";
    ensureFfmpegBackend();

    m_videoOutput = new QQuickVideoOutput(this);
    m_videoOutput->setFillMode(QQuickVideoOutput::PreserveAspectFit);
    m_videoOutput->setEndOfStreamPolicy(QQuickVideoOutput::KeepLastFrame);

    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoOutput);
    configurePlayer();

    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            &MpvVideoItem::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MpvVideoItem::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &MpvVideoItem::onErrorOccurred);

    if (width() > 0 && height() > 0) {
        m_videoOutput->setSize(QSizeF(width(), height()).toSize());
    }
}

MpvVideoItem::~MpvVideoItem()
{
    qCDebug(lcShowroomPlayer) << "Destroying MpvVideoItem";
}

void MpvVideoItem::configurePlayer()
{
    QPlaybackOptions options;
    options.setPlaybackIntent(QPlaybackOptions::PlaybackIntent::LowLatencyStreaming);
    options.setNetworkTimeout(std::chrono::seconds(15));
    options.setProbeSize(256 * 1024);
    m_player->setPlaybackOptions(options);
}

void MpvVideoItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (!m_videoOutput || newGeometry.size() == oldGeometry.size())
        return;

    m_videoOutput->setSize(newGeometry.size().toSize());
}

void MpvVideoItem::reportError(const QString &message)
{
    qCCritical(lcShowroomPlayer) << message;
    QTimer::singleShot(0, this, [this, message]() { emit playbackError(message); });
}

void MpvVideoItem::loadUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        qCWarning(lcShowroomPlayer) << "Reject loadUrl: empty URL";
        return;
    }

    qCInfo(lcShowroomPlayer) << "Loading stream:" << trimmed;
    m_currentUrl = trimmed;
    m_behindLive = false;
    setAtLiveEdge(true);
    m_player->setSource(QUrl(trimmed));
    m_player->play();
}

void MpvVideoItem::stopPlayback()
{
    if (!m_player || !m_playing)
        return;

    qCInfo(lcShowroomPlayer) << "Stopping playback";
    m_player->stop();
    m_player->setSource(QUrl());

    m_currentUrl.clear();
    if (m_paused) {
        m_paused = false;
        emit pausedChanged();
    }
    m_behindLive = false;
    setAtLiveEdge(true);
    setPlaying(false);
}

void MpvVideoItem::togglePause()
{
    if (!m_player || m_currentUrl.isEmpty())
        return;

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        qCDebug(lcShowroomPlayer) << "Toggle pause: true";
        m_player->pause();
        return;
    }

    qCDebug(lcShowroomPlayer) << "Toggle pause: false";
    m_player->play();
}

void MpvVideoItem::catchUpToLive()
{
    if (!m_player || m_currentUrl.isEmpty()) {
        qCWarning(lcShowroomPlayer) << "Reject catchUpToLive: player not ready";
        return;
    }

    qCInfo(lcShowroomPlayer) << "Catching up to live edge";
    reloadCurrentUrl(true);
}

void MpvVideoItem::reloadCurrentUrl(const bool resumePlayback)
{
    const QUrl source(m_currentUrl);
    m_player->stop();
    m_player->setSource(source);
    if (resumePlayback)
        m_player->play();

    m_behindLive = false;
    setAtLiveEdge(true);
}

void MpvVideoItem::setPlaying(const bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    emit playingChanged();
}

void MpvVideoItem::setAtLiveEdge(const bool atLiveEdge)
{
    if (m_atLiveEdge == atLiveEdge)
        return;

    m_atLiveEdge = atLiveEdge;
    qCDebug(lcShowroomPlayer) << "atLiveEdge:" << atLiveEdge;
    emit atLiveEdgeChanged();
}

void MpvVideoItem::onPlaybackStateChanged()
{
    const auto state = m_player->playbackState();
    const bool paused = state == QMediaPlayer::PausedState;
    if (m_paused != paused) {
        m_paused = paused;
        emit pausedChanged();
    }

    if (paused) {
        m_behindLive = true;
        setAtLiveEdge(false);
    } else if (state == QMediaPlayer::PlayingState) {
        setAtLiveEdge(!m_behindLive);
    }

    const bool active = state == QMediaPlayer::PlayingState
                        || state == QMediaPlayer::PausedState;
    setPlaying(active && !m_currentUrl.isEmpty());
}

void MpvVideoItem::onMediaStatusChanged()
{
    const auto status = m_player->mediaStatus();
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
        qCInfo(lcShowroomPlayer) << "Stream loaded";
        m_behindLive = false;
        setAtLiveEdge(m_player->playbackState() != QMediaPlayer::PausedState);
        setPlaying(true);
    } else if (status == QMediaPlayer::EndOfMedia) {
        qCInfo(lcShowroomPlayer) << "Playback ended (EOF)";
        m_currentUrl.clear();
        setPlaying(false);
    } else if (status == QMediaPlayer::InvalidMedia) {
        reportError(tr("Playback error: invalid media"));
        m_currentUrl.clear();
        setPlaying(false);
    }
}

void MpvVideoItem::onErrorOccurred(const QMediaPlayer::Error error, const QString &errorString)
{
    if (error == QMediaPlayer::NoError)
        return;

    qCCritical(lcShowroomPlayer) << "Playback error:" << errorString;
    reportError(tr("Playback error: %1").arg(errorString));
    m_currentUrl.clear();
    setPlaying(false);
}
