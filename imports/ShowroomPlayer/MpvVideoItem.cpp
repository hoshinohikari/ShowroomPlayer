#include "MpvVideoItem.h"
#include "ShowroomLog.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QTimer>
#include <QVariant>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <clocale>
#include <cstring>

namespace {

void onMpvWakeup(void *ctx)
{
    const auto item = static_cast<MpvVideoItem *>(ctx);
    QMetaObject::invokeMethod(item, "handleMpvEvents", Qt::QueuedConnection);
}

void *getProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx)

    QOpenGLContext *const glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return nullptr;

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

} // namespace

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvVideoItem *item)
        : m_item(item)
    {
        mpv_set_wakeup_callback(m_item->m_mpv, onMpvWakeup, m_item);
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        if (m_item->m_mpv && !m_item->m_mpvGl) {
            mpv_opengl_init_params glInitParams;
            glInitParams.get_proc_address = getProcAddress;
            glInitParams.get_proc_address_ctx = nullptr;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
                {MPV_RENDER_PARAM_INVALID, nullptr},
            };

            if (mpv_render_context_create(&m_item->m_mpvGl, m_item->m_mpv, params) < 0) {
                qCCritical(lcShowroomPlayer) << "Failed to initialize mpv OpenGL context";
                QMetaObject::invokeMethod(
                    m_item,
                    [item = m_item]() {
                        emit item->playbackError(
                            QObject::tr("Failed to initialize mpv OpenGL context"));
                    },
                    Qt::QueuedConnection);
            } else {
                qCInfo(lcShowroomPlayer) << "mpv OpenGL render context created";
                mpv_render_context_set_update_callback(
                    m_item->m_mpvGl, MpvVideoItem::onMpvUpdate, m_item);
            }
        }

        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        if (!m_item->m_mpvGl)
            return;

        QQuickOpenGLUtils::resetOpenGLState();

        QOpenGLFramebufferObject *const fbo = framebufferObject();
        mpv_opengl_fbo mpfbo;
        mpfbo.fbo = static_cast<int>(fbo->handle());
        mpfbo.w = fbo->width();
        mpfbo.h = fbo->height();
        mpfbo.internal_format = 0;
        int flipY = 0;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        mpv_render_context_render(m_item->m_mpvGl, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    MpvVideoItem *m_item;
};

void MpvVideoItem::onMpvUpdate(void *ctx)
{
    auto *const self = static_cast<MpvVideoItem *>(ctx);
    emit self->mpvUpdate();
}

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    qCDebug(lcShowroomPlayer) << "Creating MpvVideoItem";
    initializeMpv();
    connect(this, &MpvVideoItem::mpvUpdate, this, &MpvVideoItem::scheduleUpdate,
            Qt::QueuedConnection);
}

void MpvVideoItem::reportInitError(const QString &message)
{
    qCCritical(lcShowroomPlayer) << message;
    QTimer::singleShot(0, this, [this, message]() { emit playbackError(message); });
}

MpvVideoItem::~MpvVideoItem()
{
    qCDebug(lcShowroomPlayer) << "Destroying MpvVideoItem";
    if (m_mpvGl)
        mpv_render_context_free(m_mpvGl);

    if (m_mpv)
        mpv_terminate_destroy(m_mpv);
}

void MpvVideoItem::initializeMpv()
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        reportInitError(tr("Could not create mpv context"));
        return;
    }

    mpv_set_option_string(m_mpv, "terminal", "yes");
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "hwdec", "auto");
    mpv_set_option_string(m_mpv, "cache", "yes");
    mpv_set_option_string(m_mpv, "ytdl", "no");
    mpv_set_option_string(m_mpv, "demuxer-lavf-o", "live_start_index=-1");

    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        reportInitError(tr("Could not initialize mpv context"));
        return;
    }

    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    qCInfo(lcShowroomPlayer) << "mpv initialized";
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvVideoItem *>(this));
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

    mpv_set_property_string(m_mpv, "demuxer-lavf-o", "live_start_index=-1");

    const QByteArray bytes = trimmed.toUtf8();
    const char *args[] = {"loadfile", bytes.constData(), nullptr};
    if (mpv_command(m_mpv, args) < 0) {
        qCCritical(lcShowroomPlayer) << "mpv loadfile command failed";
        emit playbackError(tr("Failed to load URL"));
        return;
    }

    m_behindLive = false;
    setAtLiveEdge(true);
    setPlaying(true);
}

void MpvVideoItem::stopPlayback()
{
    if (!m_mpv || !m_playing)
        return;

    qCInfo(lcShowroomPlayer) << "Stopping playback";

    const char *args[] = {"stop", nullptr};
    if (mpv_command(m_mpv, args) < 0) {
        qCWarning(lcShowroomPlayer) << "mpv stop command failed";
        emit playbackError(tr("Failed to stop playback"));
        return;
    }

    m_currentUrl.clear();
    if (m_paused) {
        m_paused = false;
        emit pausedChanged();
    }
    m_behindLive = false;
    setAtLiveEdge(true);
    setPlaying(false);
    update();
}

void MpvVideoItem::togglePause()
{
    if (!m_mpv)
        return;

    int paused = m_paused ? 0 : 1;
    qCDebug(lcShowroomPlayer) << "Toggle pause:" << (paused != 0);
    if (mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused) < 0) {
        qCWarning(lcShowroomPlayer) << "Failed to toggle pause";
        emit playbackError(tr("Failed to toggle playback"));
    }
}

void MpvVideoItem::catchUpToLive()
{
    if (!m_mpv || m_currentUrl.isEmpty()) {
        qCWarning(lcShowroomPlayer) << "Reject catchUpToLive: player not ready";
        return;
    }

    qCInfo(lcShowroomPlayer) << "Catching up to live edge";

    mpv_set_property_string(m_mpv, "demuxer-lavf-o", "live_start_index=-1");

    const QByteArray bytes = m_currentUrl.toUtf8();
    const char *args[] = {"loadfile", bytes.constData(), "replace", nullptr};
    if (mpv_command(m_mpv, args) < 0) {
        qCWarning(lcShowroomPlayer) << "mpv reload for live catch-up failed";
        emit playbackError(tr("Failed to catch up to live"));
        return;
    }

    int paused = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused);

    m_behindLive = false;
    setAtLiveEdge(true);
}

void MpvVideoItem::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    emit playingChanged();
}

void MpvVideoItem::setAtLiveEdge(bool atLiveEdge)
{
    if (m_atLiveEdge == atLiveEdge)
        return;

    m_atLiveEdge = atLiveEdge;
    qCDebug(lcShowroomPlayer) << "atLiveEdge:" << atLiveEdge;
    emit atLiveEdgeChanged();
}

void MpvVideoItem::handleMpvEvents()
{
    if (!m_mpv)
        return;

    while (true) {
        mpv_event *const event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto *endFile = static_cast<mpv_event_end_file *>(event->data);
            if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
                qCCritical(lcShowroomPlayer) << "Playback error:"
                                             << mpv_error_string(endFile->error);
                emit playbackError(tr("Playback error: %1").arg(mpv_error_string(endFile->error)));
            } else {
                qCInfo(lcShowroomPlayer) << "Playback ended, reason:" << endFile->reason;
                m_currentUrl.clear();
                setPlaying(false);
            }
        } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
            qCInfo(lcShowroomPlayer) << "Stream loaded";
            m_behindLive = false;
            setAtLiveEdge(true);
        } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *prop = static_cast<mpv_event_property *>(event->data);
            if (std::strcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG) {
                const bool paused = *static_cast<int *>(prop->data) != 0;
                if (m_paused != paused) {
                    m_paused = paused;
                    emit pausedChanged();
                }
                if (paused) {
                    m_behindLive = true;
                    setAtLiveEdge(false);
                } else {
                    setAtLiveEdge(!m_behindLive);
                }
            }
        }
    }
}

void MpvVideoItem::scheduleUpdate()
{
    update();
}
