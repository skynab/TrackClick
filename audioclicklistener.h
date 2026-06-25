#pragma once
#ifdef HAVE_MULTIMEDIA
#include <QObject>
#include <QAudioFormat>
#include <QTimer>
#include <QtGlobal>
#include <QString>
#include <QList>

// One selectable microphone input.  `id` is the stable key persisted in
// settings; `name` is the human-readable label shown in the picker.
struct AudioInputInfo {
    QString id;
    QString name;
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSource;
#else
class QAudioInput;
#endif
class QIODevice;

// Listens to the default microphone and emits noiseDetected() whenever the
// input level crosses a loudness threshold.  This is deliberately simple: it
// reacts to *any* loud sound (a clap, a pop, a vocalisation) — it does not do
// speech or word recognition.  Used to fire the selected click as an
// alternative to the dwell timer.
class AudioClickListener : public QObject
{
    Q_OBJECT
public:
    explicit AudioClickListener(QObject* parent = nullptr);
    ~AudioClickListener() override;

    // Enumerates the available microphone inputs (empty if none / no backend).
    static QList<AudioInputInfo> availableInputs();

    // Selects which input to capture from.  Empty id = system default (with a
    // fall back to the first enumerated device).  Takes effect on the next
    // start(); call stop()/start() to switch while running.
    void setPreferredDeviceId(const QString& id) { m_preferredId = id; }

    // Begins capturing.  Returns false if there is no usable input device.
    bool start();
    void stop();
    bool isRunning() const { return m_io != nullptr; }

    // 0.0–1.0 fraction of full scale that counts as "loud".
    void setThreshold(double level01);
    // Minimum gap between two triggers so one noise fires exactly one click.
    void setCooldownMs(int ms) { m_cooldownMs = ms; }

signals:
    void noiseDetected();
    // Continuous input level (0.0–1.0 peak) for a calibration meter — emitted
    // for every captured buffer, regardless of the threshold.
    void level(double level01);

private slots:
    // Reads whatever the device has captured and processes it.  Driven by a
    // timer rather than QIODevice::readyRead, which is unreliable in pull mode
    // on some Linux audio backends (PulseAudio/PipeWire).
    void poll();

private:
    double peakLevel(const char* data, qint64 len) const;

    QTimer       m_pollTimer;
    QAudioFormat m_format;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSource* m_source = nullptr;
#else
    QAudioInput*  m_source = nullptr;
#endif
    QIODevice* m_io         = nullptr;
    QString    m_preferredId;         // empty = system default
    double     m_threshold  = 0.5;
    qint64     m_cooldownMs = 350;
    qint64     m_lastFireMs = 0;
    int        m_dbgPeaksLogged = 0;  // limits debug peak logging per capture
};
#endif // HAVE_MULTIMEDIA
