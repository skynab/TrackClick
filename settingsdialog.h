#pragma once
#include <QComboBox>
#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QProgressBar>
#include <QTabWidget>
#include <QTimer>
#include <QTranslator>

class QKeySequenceEdit;

#ifdef HAVE_MULTIMEDIA
class AudioClickListener;
#endif

enum class ButtonLayout { Rectangle, Horizontal, Vertical, VerticalTwo };
enum class EdgeLock    { None, Left, Right };

struct HotkeySlot {
    bool    enabled     = false;
    QString label;         // button display label; falls back to key sequence text if empty
    QString keySequence;   // stored as QKeySequence::toString(PortableText)
};

struct AppSettings {
    // Dwell / AutoMouse
    int  dwellMs        = 1000;
    int  sensitivityPx  = 5;
    int  scrollRepeat   = 7;
    bool repeatOnDwell  = true;      // true = repeat every dwell period; false = fire once per arm
    bool autoSelectEnabled = false;  // start with AutoSelect on

    // Which buttons are visible on the toolbar
    bool showNoClick           = true;
    bool showLeftClick        = true;
    bool showLeftDouble       = true;
    bool showLeftDrag         = true;
    bool showRightClick       = true;
    bool showRightDouble      = false;
    bool showRightDrag        = false;
    bool showMiddleClick      = true;
    bool showMiddleDouble     = false;
    bool showScrollUp         = false;
    bool showScrollDown       = false;
    bool showScrollHoriz      = false;
    bool showModCtrl          = true;
    bool showModAlt           = true;
    bool showModShift         = true;
    bool showExitButton       = true;
    bool showQuitButton       = true;
    bool showDwellActiveBtn   = true;

    // Window
    double windowOpacity  = 1.0;
    bool   alwaysOnTop    = true;
    bool   startMinimized  = false;
    bool   xMinimizesApp   = false;
    bool   launchOnStartup = false;

    // Audio feedback
    bool audioFeedback   = false;

    // Click indicator — brief expanding ring at the cursor when a click fires
    bool showClickIndicator = true;

    // Audio click — fire the selected action on a loud sound instead of the
    // dwell timer.  Off by default.  Threshold is 1–100 (higher = louder needed).
    bool    audioClickEnabled   = false;
    int     audioClickThreshold = 50;
    QString audioInputDevice;   // microphone id; empty = system default

    // Button appearance
    bool iconsOnly           = false;
    bool largeButtons        = false;
    ButtonLayout buttonLayout = ButtonLayout::Vertical;

    // Language (ISO code: "en", "fr", "es", "zh_CN", "ja", "ko")
    QString language         = "en";

    // Edge lock / hide
    EdgeLock edgeLock = EdgeLock::None;
    bool     edgeHide = false;

    // Custom hotkey buttons (up to 3)
    HotkeySlot hotkeys[3];
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    // appTranslator — the translator currently installed by MainWindow (may be
    // nullptr for English).  The dialog removes it from qApp for its lifetime
    // so that preview translators have sole control, then restores it on close.
    explicit SettingsDialog(const AppSettings& current,
                            QTranslator* appTranslator = nullptr,
                            QWidget* parent = nullptr);
    AppSettings settings() const { return m_settings; }

protected:
    void changeEvent(QEvent* e) override;
    void done(int result) override;

private:
    void buildUi();
    void loadFrom(const AppSettings& s);
    AppSettings readUi() const;
    void retranslateUi();
    // Descriptive text for the Audio Click tab (differs when the build has no
    // audio support).  Shared by buildUi() and retranslateUi().
    QString audioClickInfoText() const;
    // Start/stop the live microphone meter used to calibrate the threshold.
    // No-ops when the build has no audio support.
    void startAudioMeter();
    void stopAudioMeter();
    void applyLanguagePreview(const QString& lang);
    void cleanupPreviewTranslator();
#ifdef Q_OS_LINUX
    // When no on-screen keyboard is installed, offer to install one through the
    // system package manager (with a graphical pkexec password prompt) rather
    // than just reporting that none was found.
    void promptInstallOnScreenKeyboard();
#endif

    AppSettings  m_settings;
    QTranslator* m_previewTranslator = nullptr;
    QTranslator* m_appTranslator     = nullptr; // borrowed from MainWindow

    // ── Tab container (tab labels need retranslation) ─────────
    QTabWidget* m_tabs = nullptr;

    // ── Section / form-row labels (need retranslation) ────────
    QLabel* m_lblVisibleButtons = nullptr;
    QLabel* m_lblDwellTime    = nullptr;
    QLabel* m_lblSensitivity  = nullptr;
    QLabel* m_lblScrollRepeat = nullptr;
    QLabel* m_lblOpacity     = nullptr;
    QLabel* m_lblBtnLayout   = nullptr;
    QLabel* m_lblLanguage    = nullptr;
#ifdef Q_OS_MAC
    QLabel*      m_lblPermissions   = nullptr;
    QPushButton* m_btnAccessibility = nullptr;
#endif

    // Dwell
    QSpinBox*    m_dwellMs;
    QSpinBox*    m_sensitivPx;
    QSpinBox*    m_scrollRepeat;
    QLabel*      m_lblRepeatMode = nullptr;
    QCheckBox*   m_chkRepeatMode;

    // Buttons visibility
    QCheckBox*   m_chkNoClick;
    QCheckBox*   m_chkLeftClick;
    QCheckBox*   m_chkLeftDouble;
    QCheckBox*   m_chkLeftDrag;
    QCheckBox*   m_chkRightClick;
    QCheckBox*   m_chkRightDouble;
    QCheckBox*   m_chkRightDrag;
    QCheckBox*   m_chkMiddleClick;
    QCheckBox*   m_chkMiddleDouble;
    QCheckBox*   m_chkScrollUp;
    QCheckBox*   m_chkScrollDown;
    QCheckBox*   m_chkScrollHoriz;
    QCheckBox*   m_chkModCtrl;
    QCheckBox*   m_chkModAlt;
    QCheckBox*   m_chkModShift;
    QCheckBox*   m_chkExitButton;
    QCheckBox*   m_chkQuitButton;
    QCheckBox*   m_chkDwellActiveBtn;

    // Window
    QSlider*     m_opacitySlider;
    QLabel*      m_opacityLabel;
    QCheckBox*   m_chkAlwaysOnTop;
    QCheckBox*   m_chkStartMinimized;
    QCheckBox*   m_chkXMinimizesApp;
    QCheckBox*   m_chkLaunchOnStartup;
    QCheckBox*   m_chkAudio;
    QCheckBox*   m_chkClickIndicator;
    QCheckBox*   m_chkIconsOnly;
    QCheckBox*   m_chkLargeButtons;
    QComboBox*   m_cmbLayout;
    QComboBox*   m_cmbLanguage;

    // Edge lock
    QLabel*    m_lblEdgeLock = nullptr;
    QComboBox* m_cmbEdgeLock = nullptr;
    QCheckBox* m_chkEdgeHide = nullptr;

    // Audio Click tab
    QCheckBox*    m_chkAudioClick     = nullptr;
    QLabel*       m_lblAudioClickInfo = nullptr;
    QLabel*       m_lblAudioDevice    = nullptr;
    QComboBox*    m_cmbAudioDevice    = nullptr;
    QLabel*       m_lblAudioThreshold = nullptr;
    QSlider*      m_audioThreshSlider = nullptr;
    QLabel*       m_audioThreshValue  = nullptr;
    QLabel*       m_lblAudioMeter     = nullptr;
    QProgressBar* m_audioMeter        = nullptr;
    QTimer        m_meterTimer;            // drives smooth meter decay
    double        m_meterTarget = 0.0;     // peak-hold level, 0.0–1.0
#ifdef HAVE_MULTIMEDIA
    AudioClickListener* m_meterListener = nullptr;
#endif

    // Custom Hotkeys section (Buttons tab)
    QLabel*           m_lblCustomHotkeys  = nullptr;
    QCheckBox*        m_chkHotkey[3]      = {};
    QLineEdit*        m_edtHotkeyLabel[3] = {};
    QKeySequenceEdit* m_kseHotkey[3]      = {};

    QDialogButtonBox* m_buttons;
    QPushButton*      m_resetBtn      = nullptr;
    QPushButton*      m_btnSensTester = nullptr;
    QPushButton*      m_btnOnScreenKbd = nullptr;
};
