#pragma once

#include <QString>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <QMediaPlayer>

class QAudioOutput;
class QQuickVideoOutput;

class MpvVideoItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MpvVideoItem)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool atLiveEdge READ atLiveEdge NOTIFY atLiveEdgeChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QQuickVideoOutput *m_videoOutput = nullptr;
    bool m_paused = false;
    bool m_atLiveEdge = true;
    bool m_behindLive = false;
    bool m_playing = false;
    QString m_currentUrl;

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    bool paused() const { return m_paused; }
    bool atLiveEdge() const { return m_atLiveEdge; }
    bool playing() const { return m_playing; }

public slots:
    void loadUrl(const QString &url);
    void stopPlayback();
    void togglePause();
    void catchUpToLive();

signals:
    void pausedChanged();
    void atLiveEdgeChanged();
    void playingChanged();
    void playbackError(const QString &message);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onPlaybackStateChanged();
    void onMediaStatusChanged();
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
    void configurePlayer();
    void reportError(const QString &message);
    void setPlaying(bool playing);
    void setAtLiveEdge(bool atLiveEdge);
    void reloadCurrentUrl(bool resumePlayback);
};
