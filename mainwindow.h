#pragma once
#include <QMainWindow>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QProgressBar>
#include <QAction>
#include <QSettings>
#ifdef HAVE_MULTIMEDIA
#  include <QSoundEffect>
#endif
#include <QTranslator>
#include "clickinjector.h"
#include "dwellmanager.h"
#include "settingsdialog.h"

class ClickButton;  // forward

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QTranslator* startupTranslator = nullptr, QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*)  override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void paintEvent(QPaintEvent*)      override;
    void closeEvent(QCloseEvent*)      override;
    void changeEvent(QEvent*)          override;

private slots:
    void onClickButtonPressed(ClickType type);
    void onAutoToggled(bool on);
    void onSettingsClicked();
    void onExitClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason);
    void onDwellProgress(float frac);
    void onDwellFired(QPoint pos, ClickType type);
    void applySettings(const AppSettings& s);

private:
    void buildUi();
    void buildTray();
    void rebuildButtons();
    void setClickType(ClickType t);
    void saveWindowSettings();
    void loadWindowSettings();
    void retranslateUi();
    void installLanguage(const QString& lang);
    ClickButton* makeButton(const QString& label, const QString& tooltip, ClickType type, const QString& iconName);

    // ── Resize helpers ────────────────────────────────────────────
    enum class ResizeEdge {
        None,
        Left, Right, Top, Bottom,
        TopLeft, TopRight, BottomLeft, BottomRight
    };
    ResizeEdge edgeAt(QPoint pos) const;
    static Qt::CursorShape cursorForEdge(ResizeEdge e);
    static constexpr int RESIZE_MARGIN = 10;

    // ── UI elements ───────────────────────────────────────────
    QWidget*      m_titleBar   = nullptr;
    QLabel*       m_titleIcon  = nullptr;
    QLabel*       m_titleLabel = nullptr;
    QPushButton*  m_autoBtn    = nullptr;
    QPushButton*  m_settingsBtn= nullptr;
    QPushButton*  m_exitBtn    = nullptr;
    QWidget*      m_btnArea    = nullptr;
    QProgressBar* m_dwellBar   = nullptr;
    QLabel*       m_statusLabel= nullptr;

    // Click buttons (rebuilt on settings change)
    QList<ClickButton*> m_clickButtons;

    // Modifier toggles
    QPushButton*  m_ctrlBtn        = nullptr;
    QPushButton*  m_altBtn         = nullptr;
    QPushButton*  m_shiftBtn       = nullptr;
    QPushButton*  m_dwellActiveBtn = nullptr;

    // ── State ─────────────────────────────────────────────────
    ClickType   m_selectedType = ClickType::LeftClick;
    int         m_modifiers    = ModNone;
    bool        m_autoEnabled  = false;
    bool        m_dragging     = false;  // window drag in progress
    QPoint      m_dragOffset;            // for window dragging
    ResizeEdge  m_resizeEdge  = ResizeEdge::None;
    QPoint      m_resizeStart;           // global pos when resize began
    QRect       m_resizeGeo;             // window geometry when resize began

    // ── Hover-select timer ────────────────────────────────────
    QTimer*    m_hoverTimer  = nullptr;
    ClickType  m_hoveredType = ClickType::None;

    // ── Sub-objects ───────────────────────────────────────────
    DwellManager*     m_dwell      = nullptr;
    QSystemTrayIcon*  m_tray       = nullptr;
    QMenu*            m_trayMenu   = nullptr;
    QAction*          m_showAct    = nullptr;
    QAction*          m_quitAct    = nullptr;
    QTranslator*      m_translator = nullptr;
#ifdef HAVE_MULTIMEDIA
    QSoundEffect*     m_clickSound = nullptr;
#endif

    AppSettings   m_settings;
    QSettings     m_persist;
};

// ─── A styled click-type button ────────────────────────────────────────────
class ClickButton : public QToolButton
{
    Q_OBJECT
public:
    ClickButton(const QString& label, ClickType type, QWidget* parent = nullptr);
    ClickType clickType() const { return m_type; }
    void setSelected(bool sel);
    bool isSelected() const { return m_selected; }
    void setButtonIcon(const QString& iconName);
    void setLargeMode(bool large);

signals:
    void clickTypePressed(ClickType type);
    void clickTypeHovered(ClickType type);
    void clickTypeLeft();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* ev) override;
#else
    void enterEvent(QEvent* ev) override;
#endif
    void leaveEvent(QEvent* ev) override;

private:
    ClickType m_type;
    bool      m_selected = false;
    bool      m_large    = false;
    QString   m_iconName;
    void updateStyle();
    void updateIcon();
};
