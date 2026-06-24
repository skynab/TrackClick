#include "audioclicklistener.h"
#ifdef HAVE_MULTIMEDIA
#include <QDateTime>
#include <QDebug>
#include <QIODevice>
#include <cmath>
#include <cstring>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QAudioSource>
#  include <QAudioDevice>
#  include <QMediaDevices>
#else
#  include <QAudioInput>
#  include <QAudioDeviceInfo>
#endif

// Set TRACKCLICK_AUDIO_DEBUG=1 to log the chosen device, format and the first
// few captured peak levels — useful for diagnosing "the meter doesn't move",
// especially on Linux where the default input may be the wrong/muted source.
static bool audioDebug()
{
    static const bool on = !qEnvironmentVariableIsEmpty("TRACKCLICK_AUDIO_DEBUG");
    return on;
}

AudioClickListener::AudioClickListener(QObject* parent) : QObject(parent)
{
    // Request a small, universally-supported capture format: 16 kHz mono 16-bit.
    // If the device rejects it, start() falls back to the device's own format
    // and peakLevel() interprets whatever sample format we actually get.
    m_format.setSampleRate(16000);
    m_format.setChannelCount(1);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_format.setSampleFormat(QAudioFormat::Int16);
#else
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);
#endif

    // Poll the capture buffer ~33×/second.  Connected once here (not in start())
    // so repeated start()/stop() cycles don't stack duplicate connections.
    m_pollTimer.setInterval(30);
    connect(&m_pollTimer, &QTimer::timeout, this, &AudioClickListener::poll);
}

AudioClickListener::~AudioClickListener() { stop(); }

QList<AudioInputInfo> AudioClickListener::availableInputs()
{
    QList<AudioInputInfo> out;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QList<QAudioDevice> devs = QMediaDevices::audioInputs();
    for (const QAudioDevice& d : devs)
        out.append({ QString::fromUtf8(d.id()), d.description() });
#else
    const QList<QAudioDeviceInfo> devs =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo& d : devs)
        out.append({ d.deviceName(), d.deviceName() });
#endif
    return out;
}

void AudioClickListener::setThreshold(double level01)
{
    m_threshold = qBound(0.01, level01, 1.0);
}

bool AudioClickListener::start()
{
    if (m_io) return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Use the explicitly-chosen device if one is set; otherwise prefer the
    // system default, then fall back to the first enumerated input — on Linux
    // the "default" can be null or the wrong source even when a usable
    // microphone (e.g. a webcam mic) exists.
    QAudioDevice dev;
    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    if (!m_preferredId.isEmpty()) {
        for (const QAudioDevice& d : inputs)
            if (QString::fromUtf8(d.id()) == m_preferredId) { dev = d; break; }
    }
    if (dev.isNull()) dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull() && !inputs.isEmpty()) dev = inputs.first();
    if (dev.isNull()) {
        qWarning("TrackClick audio: no input device available");
        return false;
    }
    QAudioFormat fmt = m_format;
    if (!dev.isFormatSupported(fmt)) fmt = dev.preferredFormat();
    m_format = fmt;
    if (audioDebug())
        qWarning() << "TrackClick audio: using" << dev.description()
                   << "rate" << fmt.sampleRate() << "ch" << fmt.channelCount()
                   << "sampleFormat" << int(fmt.sampleFormat());
    m_source = new QAudioSource(dev, fmt, this);
    if (audioDebug())
        connect(m_source, &QAudioSource::stateChanged, this,
                [](QAudio::State st){ qWarning() << "TrackClick audio state:" << st; });
#else
    QAudioDeviceInfo info;
    const QList<QAudioDeviceInfo> inputs =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (!m_preferredId.isEmpty()) {
        for (const QAudioDeviceInfo& d : inputs)
            if (d.deviceName() == m_preferredId) { info = d; break; }
    }
    if (info.isNull()) info = QAudioDeviceInfo::defaultInputDevice();
    if (info.isNull() && !inputs.isEmpty()) info = inputs.first();
    if (info.isNull()) {
        qWarning("TrackClick audio: no input device available");
        return false;
    }
    QAudioFormat fmt = m_format;
    if (!info.isFormatSupported(fmt)) fmt = info.nearestFormat(fmt);
    m_format = fmt;
    if (audioDebug())
        qWarning() << "TrackClick audio: using" << info.deviceName()
                   << "rate" << fmt.sampleRate() << "ch" << fmt.channelCount()
                   << "size" << fmt.sampleSize();
    m_source = new QAudioInput(info, fmt, this);
    if (audioDebug())
        connect(m_source, &QAudioInput::stateChanged, this,
                [](QAudio::State st){ qWarning() << "TrackClick audio state:" << st; });
#endif

    m_io = m_source->start();   // QAudioSource/QAudioInput owns the QIODevice
    if (!m_io) {
        qWarning() << "TrackClick audio: failed to start capture, error"
                   << m_source->error();
        m_source->deleteLater();
        m_source = nullptr;
        return false;
    }
    m_dbgPeaksLogged = 0;
    m_pollTimer.start();
    return true;
}

void AudioClickListener::stop()
{
    m_pollTimer.stop();
    if (m_source) m_source->stop();
    if (m_io) m_io = nullptr;   // owned by m_source; invalidated by stop()
    if (m_source) { m_source->deleteLater(); m_source = nullptr; }
}

void AudioClickListener::poll()
{
    if (!m_io) return;
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty()) return;

    const double peak = peakLevel(chunk.constData(), chunk.size());
    emit level(peak);   // drives the calibration meter

    if (audioDebug() && m_dbgPeaksLogged < 10) {
        ++m_dbgPeaksLogged;
        qWarning() << "TrackClick audio: peak" << peak << "bytes" << chunk.size();
    }

    if (peak < m_threshold) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFireMs < m_cooldownMs) return;   // debounce one noise = one click
    m_lastFireMs = now;
    emit noiseDetected();
}

// Returns the peak absolute sample in the buffer, normalised to 0.0–1.0.
double AudioClickListener::peakLevel(const char* data, qint64 len) const
{
    double peak = 0.0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int bps = m_format.bytesPerSample();
    if (bps <= 0) return 0.0;
    const QAudioFormat::SampleFormat sf = m_format.sampleFormat();
    for (qint64 off = 0; off + bps <= len; off += bps) {
        const char* s = data + off;
        double v = 0.0;
        switch (sf) {
        case QAudioFormat::UInt8: v = (double(quint8(*s)) - 128.0) / 128.0; break;
        case QAudioFormat::Int16: { qint16 x; std::memcpy(&x, s, 2); v = double(x) / 32768.0; } break;
        case QAudioFormat::Int32: { qint32 x; std::memcpy(&x, s, 4); v = double(x) / 2147483648.0; } break;
        case QAudioFormat::Float: { float  x; std::memcpy(&x, s, 4); v = double(x); } break;
        default: return 0.0;
        }
        v = std::fabs(v);
        if (v > peak) peak = v;
    }
#else
    const int bytes = m_format.sampleSize() / 8;
    if (bytes <= 0) return 0.0;
    for (qint64 off = 0; off + bytes <= len; off += bytes) {
        const char* s = data + off;
        double v = 0.0;
        if (bytes == 1)      v = (double(quint8(*s)) - 128.0) / 128.0;
        else if (bytes == 2) { qint16 x; std::memcpy(&x, s, 2); v = double(x) / 32768.0; }
        else if (bytes == 4) { qint32 x; std::memcpy(&x, s, 4); v = double(x) / 2147483648.0; }
        else return 0.0;
        v = std::fabs(v);
        if (v > peak) peak = v;
    }
#endif
    return peak;
}
#endif // HAVE_MULTIMEDIA
