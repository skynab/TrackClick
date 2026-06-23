#pragma once
#include <QComboBox>
#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTranslator>

enum class ButtonLayout { Rectangle, Horizontal, Vertical, VerticalTwo };
enum class EdgeLock    { None, Left, Right };

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

    // Button appearance
    bool iconsOnly           = false;
    bool largeButtons        = false;
    ButtonLayout buttonLayout = ButtonLayout::Vertical;

    // Language (ISO code: "en", "fr", "es", "zh_CN", "ja", "ko")
    QString language         = "en";

    // Edge lock / hide
    EdgeLock edgeLock = EdgeLock::None;
    bool     edgeHide = false;
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
    QCheckBox*   m_chkIconsOnly;
    QCheckBox*   m_chkLargeButtons;
    QComboBox*   m_cmbLayout;
    QComboBox*   m_cmbLanguage;

    // Edge lock
    QLabel*    m_lblEdgeLock = nullptr;
    QComboBox* m_cmbEdgeLock = nullptr;
    QCheckBox* m_chkEdgeHide = nullptr;

    QDialogButtonBox* m_buttons;
    QPushButton*      m_resetBtn      = nullptr;
    QPushButton*      m_btnSensTester = nullptr;
    QPushButton*      m_btnOnScreenKbd = nullptr;
};
