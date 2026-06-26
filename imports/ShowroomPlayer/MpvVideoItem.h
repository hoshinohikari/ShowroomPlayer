#pragma once

#include <QQuickFramebufferObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;
struct mpv_render_context;

class MpvRenderer;

class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MpvVideoItem)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool atLiveEdge READ atLiveEdge NOTIFY atLiveEdgeChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpvGl = nullptr;
    bool m_paused = false;
    bool m_atLiveEdge = true;
    bool m_behindLive = false;
    bool m_playing = false;
    QString m_currentUrl;

    friend class MpvRenderer;

public:
    static void onMpvUpdate(void *ctx);

    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    bool paused() const { return m_paused; }
    bool atLiveEdge() const { return m_atLiveEdge; }
    bool playing() const { return m_playing; }

    Renderer *createRenderer() const override;

public slots:
    void loadUrl(const QString &url);
    void stopPlayback();
    void togglePause();
    void catchUpToLive();
    void handleMpvEvents();

signals:
    void mpvUpdate();
    void pausedChanged();
    void atLiveEdgeChanged();
    void playingChanged();
    void playbackError(const QString &message);

private slots:
    void scheduleUpdate();

private:
    void initializeMpv();
    void reportInitError(const QString &message);
    void setPlaying(bool playing);
    void setAtLiveEdge(bool atLiveEdge);
};
