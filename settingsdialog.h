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
#include <QTranslator>

enum class ButtonLayout { Rectangle, Horizontal, Vertical, VerticalTwo };

struct AppSettings {
    // Dwell / AutoMouse
    int  dwellMs        = 1000;
    int  sensitivityPx  = 5;
    bool autoSelectEnabled = false;  // start with AutoSelect on

    // Which buttons are visible on the toolbar
    bool showLeftClick        = true;
    bool showLeftDouble       = true;
    bool showLeftDrag         = true;
    bool showRightClick       = true;
    bool showRightDouble      = true;
    bool showRightDrag        = true;
    bool showMiddleClick      = true;
    bool showMiddleDouble     = false;
    bool showScrollUp         = true;
    bool showScrollDown       = true;
    bool showScrollHoriz      = false;
    bool showModCtrl          = true;
    bool showModAlt           = true;
    bool showModShift         = true;
    bool showExitButton       = true;

    // Window
    double windowOpacity = 1.0;
    bool   alwaysOnTop   = true;
    bool   startMinimized = false;

    // Audio feedback
    bool audioFeedback   = false;

    // Button appearance
    bool iconsOnly           = false;
    bool largeButtons        = false;
    ButtonLayout buttonLayout = ButtonLayout::Rectangle;

    // Language (ISO code: "en", "fr", "es", "zh_CN", "ja", "ko")
    QString language         = "en";
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

    AppSettings  m_settings;
    QTranslator* m_previewTranslator = nullptr;
    QTranslator* m_appTranslator     = nullptr; // borrowed from MainWindow

    // ── Group boxes (titles need retranslation) ───────────────
    QGroupBox* m_grpDwell = nullptr;
    QGroupBox* m_grpBtns  = nullptr;
    QGroupBox* m_grpWin   = nullptr;

    // ── Form-row labels (need retranslation) ──────────────────
    QLabel* m_lblDwellTime   = nullptr;
    QLabel* m_lblSensitivity = nullptr;
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

    // Buttons visibility
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

    // Window
    QSlider*     m_opacitySlider;
    QLabel*      m_opacityLabel;
    QCheckBox*   m_chkAlwaysOnTop;
    QCheckBox*   m_chkStartMinimized;
    QCheckBox*   m_chkAudio;
    QCheckBox*   m_chkIconsOnly;
    QCheckBox*   m_chkLargeButtons;
    QComboBox*   m_cmbLayout;
    QComboBox*   m_cmbLanguage;

    QDialogButtonBox* m_buttons;
};
