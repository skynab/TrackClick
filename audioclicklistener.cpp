#include "audioclicklistener.h"
#ifdef HAVE_MULTIMEDIA
#include <QDateTime>
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
}

AudioClickListener::~AudioClickListener() { stop(); }

void AudioClickListener::setThreshold(double level01)
{
    m_threshold = qBound(0.01, level01, 1.0);
}

bool AudioClickListener::start()
{
    if (m_io) return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) return false;
    QAudioFormat fmt = m_format;
    if (!dev.isFormatSupported(fmt)) fmt = dev.preferredFormat();
    m_format = fmt;
    m_source = new QAudioSource(dev, fmt, this);
#else
    const QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (info.isNull()) return false;
    QAudioFormat fmt = m_format;
    if (!info.isFormatSupported(fmt)) fmt = info.nearestFormat(fmt);
    m_format = fmt;
    m_source = new QAudioInput(info, fmt, this);
#endif

    m_io = m_source->start();   // QAudioSource/QAudioInput owns the QIODevice
    if (!m_io) {
        m_source->deleteLater();
        m_source = nullptr;
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioClickListener::onReadyRead);
    return true;
}

void AudioClickListener::stop()
{
    if (m_source) m_source->stop();
    if (m_io) { m_io->disconnect(this); m_io = nullptr; }
    if (m_source) { m_source->deleteLater(); m_source = nullptr; }
}

void AudioClickListener::onReadyRead()
{
    if (!m_io) return;
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty()) return;

    const double peak = peakLevel(chunk.constData(), chunk.size());
    emit level(peak);   // drives the calibration meter

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
