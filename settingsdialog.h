#pragma once
#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSlider>
#include <QLabel>

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
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings& current, QWidget* parent = nullptr);
    AppSettings settings() const { return m_settings; }

private:
    void buildUi();
    void loadFrom(const AppSettings& s);
    AppSettings readUi() const;

    AppSettings m_settings;

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

    QDialogButtonBox* m_buttons;
};
