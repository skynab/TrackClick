#include "settingsdialog.h"
#include "translations/tsparser.h"
#include "audioclicklistener.h"
#include "clickinjector.h"
#include <QApplication>
#include <QKeySequenceEdit>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QAbstractButton>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QSysInfo>
#include <cmath>
#ifdef Q_OS_MAC
#  include <QDesktopServices>
#  include <QUrl>
#endif

// ── TrackIR palette ──────────────────────────────────────────
// Settings-window font zoom: the base font size (at 100%) plus the bounds and
// step of the "+/-" zoom control that sits next to the language selector.  The
// stylesheet font-size is regenerated from this base whenever the user zooms.
static constexpr int kBaseFontPx  = 12;
static constexpr int kZoomMinPct  = 80;
static constexpr int kZoomMaxPct  = 200;
static constexpr int kZoomStepPct = 10;

// Build the dialog stylesheet with every text element sized at fontPx, so the
// zoom control can scale the whole settings window just by re-applying it.
// The @FS@ token is substituted with the requested pixel size below.
static QString buildStyle(int fontPx)
{
    return QString(R"(
QDialog {
    background: #2D2D2D;
    color: #E6E6E6;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: @FS@px;
}
QGroupBox {
    color: #FFA600;
    border: 1px solid #979797;
    border-radius: 4px;
    margin-top: 10px;
    padding-top: 6px;
    font-size: @FS@px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}
QLabel  { color: #E6E6E6; font-size: @FS@px; }
QCheckBox { color: #E6E6E6; spacing: 6px; font-size: @FS@px; }
QCheckBox::indicator {
    width: 25px; height: 11px;
    border: none;
    border-radius: 0;
    background: transparent;
    image: url(:/icons/toggle_off.svg);
}
QCheckBox::indicator:checked {
    image: url(:/icons/toggle_on.svg);
}
QListWidget {
    background: #1A1A1A;
    color: #E6E6E6;
    border: 1px solid #555;
    border-radius: 3px;
    font-size: @FS@px;
    outline: none;
}
QListWidget::item { padding: 3px 4px; }
QListWidget::item:selected { background: #3D3D3D; color: #FFA600; }
QListWidget::indicator {
    width: 25px; height: 11px;
    background: transparent;
    image: url(:/icons/toggle_off.svg);
}
QListWidget::indicator:checked {
    image: url(:/icons/toggle_on.svg);
}
QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit, QKeySequenceEdit {
    background: #1A1A1A;
    color: #E6E6E6;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 2px 4px;
    font-size: @FS@px;
}
QLineEdit:focus, QKeySequenceEdit:focus {
    border: 1px solid #FFA600;
}
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: #2D2D2D;
    color: #E6E6E6;
    border: 1px solid #555;
    selection-background-color: #FFA600;
    selection-color: #1A1A1A;
    font-size: @FS@px;
}
QSlider::groove:horizontal {
    height: 4px;
    background: #555;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 14px; height: 14px;
    background: #FFA600;
    border-radius: 7px;
    margin: -5px 0;
}
QSlider::sub-page:horizontal { background: #FFA600; border-radius: 2px; }
QPushButton {
    background: #FFA600;
    color: #1A1A1A;
    border: none;
    border-radius: 4px;
    padding: 6px 18px;
    font-weight: bold;
    font-size: @FS@px;
}
QPushButton:hover  { background: #FFB833; }
QPushButton:pressed{ background: #CC8400; }
QPushButton[flat=true] {
    background: #3D3D3D;
    color: #FFFFFF;
}
QPushButton[flat=true]:hover { background: #4D4D4D; }
QTabWidget::pane {
    border: 1px solid #555;
    border-radius: 4px;
    top: -1px;
    background: #2D2D2D;
}
QTabBar::tab {
    background: #1A1A1A;
    color: #E6E6E6;
    padding: 6px 14px;
    border: 1px solid #555;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
    font-size: @FS@px;
}
QTabBar::tab:selected { background: #2D2D2D; color: #FFA600; }
QTabBar::tab:hover    { color: #FFB833; }
)").replace(QLatin1String("@FS@"), QString::number(fontPx));
}

// ── Reorderable click buttons ────────────────────────────────────────────────
// Canonical/default toolbar order.  Right Double / Right Drag are included so
// the order model is complete and MainWindow can still lay them out if ever
// re-enabled in code, but they are read-only placeholders (kept off) and are
// not shown in the settings reorder list.
const QVector<ClickButtonDesc>& clickButtonDescs()
{
    static const QVector<ClickButtonDesc> v = {
        { "no_click",      &AppSettings::showNoClick     },
        { "left_click",    &AppSettings::showLeftClick   },
        { "left_double",   &AppSettings::showLeftDouble  },
        { "left_drag",     &AppSettings::showLeftDrag    },
        { "right_click",   &AppSettings::showRightClick  },
        { "right_double",  &AppSettings::showRightDouble },
        { "right_drag",    &AppSettings::showRightDrag   },
        { "middle_click",  &AppSettings::showMiddleClick },
        { "middle_double", &AppSettings::showMiddleDouble},
        { "scroll_up",     &AppSettings::showScrollUp       },
        { "scroll_down",   &AppSettings::showScrollDown     },
        { "scroll_horiz",  &AppSettings::showScrollHoriz    },
        { "ctrl",          &AppSettings::showModCtrl        },
        { "alt",           &AppSettings::showModAlt         },
        { "shift",         &AppSettings::showModShift       },
        { "dwell_active",  &AppSettings::showDwellActiveBtn },
        { "quit",          &AppSettings::showQuitButton     },
    };
    return v;
}

QStringList orderedClickButtonIds(const AppSettings& s)
{
    QStringList result;
    auto isKnown = [](const QString& id) {
        for (const auto& d : clickButtonDescs())
            if (id == QLatin1String(d.id)) return true;
        return false;
    };
    // Saved order first (recognised ids, no duplicates).
    for (const QString& id : s.buttonOrder)
        if (isKnown(id) && !result.contains(id))
            result << id;
    // Then any canonical ids the saved order didn't cover (new/unsaved buttons).
    for (const auto& d : clickButtonDescs()) {
        const QString id = QLatin1String(d.id);
        if (!result.contains(id))
            result << id;
    }
    return result;
}

// The AppSettings visibility flag controlled by a given id (nullptr if unknown).
static bool AppSettings::* showMemberForId(const QString& id)
{
    for (const auto& d : clickButtonDescs())
        if (id == QLatin1String(d.id))
            return d.show;
    return nullptr;
}

// ── Sensitivity Tester ────────────────────────────────────────────────────────

class CrosshairWidget : public QWidget
{
public:
    explicit CrosshairWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(120, 120);
    }

    void setHighlighted(bool h) { if (m_hl != h) { m_hl = h; update(); } }

    QPoint centerInScreen() const
    {
        return mapToGlobal(QPoint(width() / 2, height() / 2));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(0x1A, 0x1A, 0x1A));

        p.setPen(QPen(QColor("#555555"), 1));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        int cx = width() / 2, cy = height() / 2;
        QColor col = m_hl ? QColor("#FFD000") : QColor("#FFA600");

        p.setPen(QPen(col, 1));
        p.drawLine(0, cy, width() - 1, cy);
        p.drawLine(cx, 0, cx, height() - 1);

        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), 8, 8);

        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPoint(cx, cy), 2, 2);
    }

private:
    bool m_hl = false;
};

class SensitivityTesterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SensitivityTesterDialog(QWidget* parent = nullptr);

signals:
    void sensitivityChosen(int px);

private slots:
    void onStart();
    void onPoll();

private:
    void finishMeasurement();

    enum class Phase { Idle, Waiting, Measuring, Done };

    CrosshairWidget* m_crosshair;
    QLabel*          m_infoLbl;
    QProgressBar*    m_bar;
    QLabel*          m_resultLbl;
    QPushButton*     m_startBtn;
    QTimer           m_timer;

    Phase  m_phase     = Phase::Idle;
    QPoint m_lastPos;
    double m_sumDelta  = 0.0;
    int    m_samples   = 0;
    int    m_elapsedMs = 0;

    static constexpr int k_durationMs = 8000;
    static constexpr int k_hoverPx   = 30;
    static constexpr int k_pollMs    = 50;
};

SensitivityTesterDialog::SensitivityTesterDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sensitivity Tester"));
    setModal(true);
    setStyleSheet(buildStyle(kBaseFontPx));
    setFixedWidth(280);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    m_infoLbl = new QLabel(
        tr("Click Start, then move your mouse over\nthe crosshairs and keep it still."));
    m_infoLbl->setAlignment(Qt::AlignCenter);
    m_infoLbl->setWordWrap(true);
    root->addWidget(m_infoLbl);

    auto* crossRow = new QHBoxLayout;
    m_crosshair = new CrosshairWidget;
    crossRow->addStretch();
    crossRow->addWidget(m_crosshair);
    crossRow->addStretch();
    root->addLayout(crossRow);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(8);
    m_bar->setStyleSheet(
        "QProgressBar{background:#1A1A1A;border:1px solid #555;border-radius:3px;}"
        "QProgressBar::chunk{background:#FFA600;border-radius:2px;}");
    root->addWidget(m_bar);

    m_resultLbl = new QLabel;
    m_resultLbl->setAlignment(Qt::AlignCenter);
    m_resultLbl->setStyleSheet("color:#FFA600; font-weight:bold;");
    m_resultLbl->hide();
    root->addWidget(m_resultLbl);

    auto* btnRow = new QHBoxLayout;
    m_startBtn   = new QPushButton(tr("Start"));
    auto* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setProperty("flat", true);
    btnRow->addStretch();
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(closeBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    connect(m_startBtn, &QPushButton::clicked, this, &SensitivityTesterDialog::onStart);
    connect(closeBtn,   &QPushButton::clicked, this, &QDialog::accept);

    m_timer.setInterval(k_pollMs);
    connect(&m_timer, &QTimer::timeout, this, &SensitivityTesterDialog::onPoll);
}

void SensitivityTesterDialog::onStart()
{
    m_phase     = Phase::Waiting;
    m_sumDelta  = 0.0;
    m_samples   = 0;
    m_elapsedMs = 0;
    m_lastPos   = QCursor::pos();

    m_startBtn->setEnabled(false);
    m_resultLbl->hide();
    m_bar->setValue(0);
    m_infoLbl->setText(tr("Move your mouse over the crosshairs\nand keep it still."));
    m_timer.start();
}

void SensitivityTesterDialog::onPoll()
{
    QPoint cur    = QCursor::pos();
    QPoint center = m_crosshair->centerInScreen();
    double dist   = std::hypot(double(cur.x() - center.x()),
                               double(cur.y() - center.y()));
    bool nearCenter = dist <= k_hoverPx;
    m_crosshair->setHighlighted(nearCenter);

    if (m_phase == Phase::Waiting) {
        if (nearCenter) {
            m_phase     = Phase::Measuring;
            m_lastPos   = cur;
            m_sumDelta  = 0.0;
            m_samples   = 0;
            m_elapsedMs = 0;
            m_infoLbl->setText(tr("Measuring — keep your mouse still…"));
        }
    } else if (m_phase == Phase::Measuring) {
        double d = std::hypot(double(cur.x() - m_lastPos.x()),
                              double(cur.y() - m_lastPos.y()));
        m_sumDelta  += d;
        m_samples++;
        m_lastPos    = cur;
        m_elapsedMs += k_pollMs;
        m_bar->setValue(m_elapsedMs * 100 / k_durationMs);
        if (m_elapsedMs >= k_durationMs)
            finishMeasurement();
    }
}

void SensitivityTesterDialog::finishMeasurement()
{
    m_timer.stop();
    m_phase = Phase::Done;
    m_crosshair->setHighlighted(false);
    m_bar->setValue(100);

    double avg     = m_samples > 0 ? m_sumDelta / m_samples : 1.0;
    int recommended = qBound(1, static_cast<int>(std::ceil(avg * 2.0)), 100);

    m_resultLbl->setText(tr("Sensitivity set to %1 px").arg(recommended));
    m_resultLbl->show();
    m_startBtn->setText(tr("Retest"));
    m_startBtn->setEnabled(true);

    emit sensitivityChosen(recommended);
}

// ─────────────────────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(const AppSettings& current,
                               QTranslator* appTranslator,
                               QWidget* parent)
    : QDialog(parent), m_settings(current), m_appTranslator(appTranslator)
{
    setWindowTitle(tr("TrackClick — Settings"));
    setModal(true);
    setStyleSheet(buildStyle(kBaseFontPx));
    buildUi();
    loadFrom(current);

    // Seize sole control of the qApp translator for the dialog's lifetime.
    // Removing MainWindow's translator here ensures that when the user picks
    // English the preview system can install nothing and tr() correctly falls
    // through to source strings — even if the app was previously in another
    // language.  Screen repaints are deferred until the event loop runs, so
    // the two rapid LanguageChange events below produce no visible flicker.
    if (m_appTranslator)
        qApp->removeTranslator(m_appTranslator);

    // Warm up the preview for the starting language so the dialog reflects
    // the current language immediately.  Connected AFTER this call so the
    // combo's initial value doesn't fire a duplicate preview.
    applyLanguagePreview(current.language);

    connect(m_cmbLanguage, &QComboBox::currentIndexChanged, this, [this](){
        applyLanguagePreview(m_cmbLanguage->currentData().toString());
    });

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this](){
        m_settings = readUi();
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_resetBtn, &QPushButton::clicked, this, [this](){
        loadFrom(AppSettings{});
    });

    // ── Live input-level meter (calibration) ──────────────────
    m_meterTimer.setInterval(40);
    connect(&m_meterTimer, &QTimer::timeout, this, [this](){
        m_meterTarget *= 0.82;   // smooth decay so the bar falls back gently
        m_audioMeter->setValue(static_cast<int>(qBound(0.0, m_meterTarget, 1.0) * 100));
    });
#ifdef HAVE_MULTIMEDIA
    m_meterListener = new AudioClickListener(this);
    connect(m_meterListener, &AudioClickListener::level, this, [this](double l){
        if (l > m_meterTarget) m_meterTarget = l;   // instant rise, timed fall
    });
    // Only open the microphone while the Audio Click tab (index 3) is showing.
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx){
        if (idx == 3) startAudioMeter();
        else          stopAudioMeter();
    });
#endif
}

// ── Language preview ──────────────────────────────────────────────────────────

void SettingsDialog::applyLanguagePreview(const QString& lang)
{
    // Swap only the preview translator — do NOT restore m_appTranslator here.
    // m_appTranslator stays out of qApp for the entire dialog lifetime so that
    // choosing English (no preview translator) correctly shows English rather
    // than falling back to the previously-active language.
    if (m_previewTranslator) {
        qApp->removeTranslator(m_previewTranslator);
        delete m_previewTranslator;
        m_previewTranslator = nullptr;
    }

    if (lang != "en") {
        m_previewTranslator = loadBestTranslator(lang, this);
        if (m_previewTranslator)
            qApp->installTranslator(m_previewTranslator);
    }
    // Qt automatically broadcasts QEvent::LanguageChange to this dialog when
    // the translator list changes, which triggers changeEvent → retranslateUi().
}

void SettingsDialog::cleanupPreviewTranslator()
{
    if (m_previewTranslator) {
        qApp->removeTranslator(m_previewTranslator);
        delete m_previewTranslator;
        m_previewTranslator = nullptr;
    }
    // Hand the app translator back to qApp so that after the dialog closes
    // the main window reverts to its previous language (on Cancel) or
    // MainWindow::installLanguage can remove and replace it (on Accept).
    if (m_appTranslator) {
        qApp->installTranslator(m_appTranslator);
        m_appTranslator = nullptr; // ownership stays with MainWindow
    }
}

void SettingsDialog::done(int result)
{
    // Always clean up before closing so that MainWindow::installLanguage()
    // (called on Accept) starts with only its own translator installed, and
    // on Cancel the app reverts to whatever translator was active before the
    // dialog opened.
    cleanupPreviewTranslator();
    stopAudioMeter();   // release the microphone when the dialog closes
    QDialog::done(result);
}

// ── Retranslation ─────────────────────────────────────────────────────────────

void SettingsDialog::changeEvent(QEvent* e)
{
    QDialog::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(tr("TrackClick — Settings"));

    m_tabs->setTabText(0, tr("Dwell Clicking"));
    m_lblDwellTime->setText(tr("Dwell time:"));
    m_lblSensitivity->setText(tr("Sensitivity:"));
    m_btnSensTester->setText(tr("Sensitivity Tester…"));
    m_lblHoverSelect->setText(tr("Hover to switch:"));
    m_hoverSelectPct->setToolTip(tr("How long a toolbar button must be hovered before the "
                                    "selection switches to it, as a percentage of the dwell time."));
    m_lblScrollRepeat->setText(tr("Scroll repeat:"));
    m_lblRepeatMode->setText(tr("Repeat click:"));
#ifdef Q_OS_MAC
    m_lblPermissions->setText(tr("Permissions:"));
    m_btnAccessibility->setText(tr("Open Accessibility Settings…"));
#endif

    m_tabs->setTabText(1, tr("Buttons"));
    m_lblVisibleButtons->setText(tr("Visible Buttons (check to show, Move Up/Down to reorder)"));
    m_btnMoveUp->setText(tr("Move Up"));
    m_btnMoveDown->setText(tr("Move Down"));
    for (int i = 0; i < m_btnOrderList->count(); ++i) {
        QListWidgetItem* it = m_btnOrderList->item(i);
        it->setText(clickButtonLabel(it->data(Qt::UserRole).toString()));
    }

    m_lblCustomHotkeys->setText(tr("Custom Hotkeys"));
    for (int i = 0; i < 3; ++i) {
        m_chkHotkey[i]->setText(tr("Hotkey %1").arg(i + 1));
        m_edtHotkeyLabel[i]->setPlaceholderText(tr("Label (optional)"));
    }

    m_tabs->setTabText(2, tr("Window"));
    m_lblEdgeLock->setText(tr("Lock to screen edge:"));
    m_cmbEdgeLock->setItemText(0, tr("None"));
    m_cmbEdgeLock->setItemText(1, tr("Left edge"));
    m_cmbEdgeLock->setItemText(2, tr("Right edge"));
    m_chkEdgeHide->setText(tr("Slide off screen when idle"));
    m_chkAlwaysOnTop->setText(tr("Always on top"));
    m_chkStartMinimized->setText(tr("Start minimized to tray"));
    m_chkXMinimizesApp->setText(tr("Top X minimizes app"));
    m_chkLaunchOnStartup->setText(tr("Launch on system startup (Windows)"));
    m_chkAudio->setText(tr("Audio feedback on click"));
    m_chkClickIndicator->setText(tr("Show click indicator ring (Windows)"));
    m_chkIconsOnly->setText(tr("Icons only (hide button labels)"));
    m_chkLargeButtons->setText(tr("Large buttons"));
    m_lblOpacity->setText(tr("Opacity:"));
    m_lblBtnLayout->setText(tr("Button layout:"));
    m_cmbLayout->setItemText(0, tr("Rectangle (grid)"));
    m_cmbLayout->setItemText(1, tr("Horizontal (one row)"));
    m_cmbLayout->setItemText(2, tr("Vertical (one column)"));
    m_cmbLayout->setItemText(3, tr("Vertical (two columns)"));
    m_lblLanguage->setText(tr("Language:"));
    m_lblZoom->setText(tr("Zoom:"));
    m_okBtn->setText(tr("OK"));
    m_cancelBtn->setText(tr("Cancel"));
    m_resetBtn->setText(tr("Reset to Defaults"));
    m_btnOnScreenKbd->setText(tr("Open On-Screen Keyboard"));

    m_tabs->setTabText(3, tr("Audio Click"));
    m_chkAudioClick->setText(tr("Trigger the selected action with a loud sound"));
    m_lblAudioClickInfo->setText(audioClickInfoText());
    m_lblAudioDevice->setText(tr("Input device:"));
    m_cmbAudioDevice->setItemText(0, tr("System default"));
    m_lblAudioThreshold->setText(tr("Loudness threshold:"));
    m_lblAudioMeter->setText(tr("Input level:"));
}

QString SettingsDialog::audioClickInfoText() const
{
#ifdef HAVE_MULTIMEDIA
    return tr("When enabled, the selected action fires when the microphone hears "
              "a loud sound (such as a clap or a pop) instead of waiting for the "
              "dwell timer. Turn on Dwell Active to start listening. Any loud "
              "noise triggers the action — speech is not interpreted.");
#else
    return tr("Audio support is not available in this build, so audio click "
              "cannot be used.");
#endif
}

void SettingsDialog::startAudioMeter()
{
#ifdef HAVE_MULTIMEDIA
    if (!m_meterListener) return;
    if (!m_meterListener->isRunning()) {
        // Capture from whichever device is selected in the picker.
        m_meterListener->setPreferredDeviceId(m_cmbAudioDevice->currentData().toString());
        m_meterListener->start();   // harmless if there is no input device
    }
    m_meterTimer.start();
#endif
}

void SettingsDialog::stopAudioMeter()
{
#ifdef HAVE_MULTIMEDIA
    m_meterTimer.stop();
    if (m_meterListener) m_meterListener->stop();
#endif
    m_meterTarget = 0.0;
    if (m_audioMeter) m_audioMeter->setValue(0);
}

// ── UI construction ───────────────────────────────────────────────────────────

// Renders a settings checkbox read-only: unchecked, disabled, and dimmed so it
// clearly can't be toggled. Used for options we currently keep locked off but
// leave visible (Right Double, Right Drag, Top X minimizes app) so they can be
// re-enabled in code later — see the call sites and loadFrom().
static void makeCheckboxReadOnly(QCheckBox* c)
{
    c->setChecked(false);
    c->setEnabled(false);
    auto* fade = new QGraphicsOpacityEffect(c);
    fade->setOpacity(0.4);   // dim to signal the disabled/read-only state
    c->setGraphicsEffect(fade);
}

void SettingsDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(14, 14, 14, 14);

    // ── Brand header ──────────────────────────────────────────
    auto* header = new QWidget;
    header->setObjectName("appHeader");
    header->setStyleSheet(
        "QWidget#appHeader { background: #1A1A1A; border-radius: 6px; }");

    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(14, 10, 14, 10);
    hLay->setSpacing(14);

    // OptiTrack logo rendered from embedded SVG
    QSvgRenderer svgRend(QString(":/icons/optitrack_logo.svg"));
    const int logoH = 30;
    const int logoW = qRound(svgRend.defaultSize().width()
                             * logoH / double(svgRend.defaultSize().height()));
    QPixmap logoPx(logoW, logoH);
    logoPx.fill(Qt::transparent);
    QPainter logoPainter(&logoPx);
    svgRend.render(&logoPainter);

    auto* logoLbl = new QLabel;
    logoLbl->setPixmap(logoPx);
    logoLbl->setStyleSheet("background: transparent;");
    hLay->addWidget(logoLbl);

    // Vertical divider
    auto* vSep = new QFrame;
    vSep->setFrameShape(QFrame::VLine);
    vSep->setStyleSheet("color: #444;");
    hLay->addWidget(vSep);

    // App name + version
    auto* nameLay = new QVBoxLayout;
    nameLay->setSpacing(1);
    auto* appNameLbl = new QLabel("TrackClick");
    appNameLbl->setStyleSheet(
        "color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent;");
#ifdef BUILD_NUMBER
#  define TC_STR_(x) #x
#  define TC_STR(x) TC_STR_(x)
    auto* versionLbl = new QLabel("Version 0.9.5 (build " TC_STR(BUILD_NUMBER) ")");
#else
    auto* versionLbl = new QLabel("Version 0.9.5");
#endif
    versionLbl->setStyleSheet(
        "color: #666666; font-size: 11px; background: transparent;");
    nameLay->addWidget(appNameLbl);
    nameLay->addWidget(versionLbl);
    hLay->addLayout(nameLay);
    hLay->addStretch(1);

    root->addWidget(header);

    // ── Language selector ─────────────────────────────────────
    // Placed above the tab widget (not inside the Window tab) so the language
    // can be changed no matter which tab is open.  Native-script names are
    // intentionally not translated.
    m_cmbLanguage = new QComboBox;
    m_cmbLanguage->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbLanguage->setMinimumWidth(130);
    m_cmbLanguage->addItem("English",   "en");
    m_cmbLanguage->addItem("Čeština",   "cs");
    m_cmbLanguage->addItem("Français",  "fr");
    m_cmbLanguage->addItem("Español",   "es");
    m_cmbLanguage->addItem("中文简体",   "zh_CN");
    m_cmbLanguage->addItem("日本語",     "ja");
    m_cmbLanguage->addItem("한국어",     "ko");
    m_cmbLanguage->addItem("हिन्दी",    "hi");
    m_cmbLanguage->addItem("العربية",   "ar");
    m_cmbLanguage->addItem("বাংলা",     "bn");
    m_cmbLanguage->addItem("Português", "pt");
    m_cmbLanguage->addItem("Русский",   "ru");
    m_cmbLanguage->addItem("اردو",      "ur");
    m_lblLanguage = new QLabel(tr("Language:"));

    // ── Font zoom ─────────────────────────────────────────────
    // Sits next to the language selector so both top-level view options share
    // one row.  The "-"/"+" buttons scale the settings window's own text (see
    // applyFontScale); the value label shows the current percentage.
    m_lblZoom      = new QLabel(tr("Zoom:"));
    m_btnZoomOut   = new QPushButton(QString(QChar(0x2212)));  // U+2212 minus sign
    m_btnZoomIn    = new QPushButton(QStringLiteral("+"));
    m_lblZoomValue = new QLabel;
    m_lblZoomValue->setAlignment(Qt::AlignCenter);
    m_lblZoomValue->setMinimumWidth(44);
    // Dedicated look so the glyphs read clearly: large, bold, accent-orange on a
    // bordered button.  A per-button stylesheet also keeps them a fixed size
    // (they don't grow when the zoom re-applies the dialog stylesheet).
    for (QPushButton* b : { m_btnZoomOut, m_btnZoomIn }) {
        b->setFixedSize(30, 26);
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet(
            "QPushButton {"
            "  background: #3D3D3D; color: #FFFFFF;"
            "  border: 1px solid #666; border-radius: 4px;"
            "  padding: 0; font-size: 18px; font-weight: bold;"
            "}"
            "QPushButton:hover    { background: #4D4D4D; border-color: #888; }"
            "QPushButton:pressed  { background: #2A2A2A; }"
            "QPushButton:disabled { color: #6A6A6A; border-color: #444; }");
    }
    connect(m_btnZoomOut, &QPushButton::clicked, this, [this]{
        m_fontScale -= kZoomStepPct;
        applyFontScale();
    });
    connect(m_btnZoomIn, &QPushButton::clicked, this, [this]{
        m_fontScale += kZoomStepPct;
        applyFontScale();
    });

    auto* langRow = new QHBoxLayout;
    langRow->addStretch(1);
    langRow->addWidget(m_lblLanguage);
    langRow->addWidget(m_cmbLanguage);
    langRow->addSpacing(18);
    langRow->addWidget(m_lblZoom);
    langRow->addWidget(m_btnZoomOut);
    langRow->addWidget(m_lblZoomValue);
    langRow->addWidget(m_btnZoomIn);
    langRow->addStretch(1);
    root->addLayout(langRow);

    // ── Tabbed sections ───────────────────────────────────────
    // Each former group box becomes a tab page so the dialog shows one section
    // at a time instead of the whole stack at once.  Tab labels carry the
    // section names and are retranslated in retranslateUi().
    m_tabs = new QTabWidget;
    root->addWidget(m_tabs);

    // ── Dwell / AutoMouse ─────────────────────────────────────
    auto* pageDwell = new QWidget;
    auto* fl        = new QFormLayout(pageDwell);
    fl->setSpacing(6);

    m_dwellMs      = new QSpinBox; m_dwellMs->setRange(100, 10000); m_dwellMs->setSuffix(" ms");
    m_sensitivPx   = new QSpinBox; m_sensitivPx->setRange(1, 100);  m_sensitivPx->setSuffix(" px");
    m_hoverSelectPct = new QSpinBox; m_hoverSelectPct->setRange(10, 100); m_hoverSelectPct->setSuffix(" %");
    m_hoverSelectPct->setToolTip(tr("How long a toolbar button must be hovered before the "
                                    "selection switches to it, as a percentage of the dwell time."));
    m_scrollRepeat = new QSpinBox; m_scrollRepeat->setRange(1, 20);

    m_chkRepeatMode = new QCheckBox;

    m_lblDwellTime    = new QLabel(tr("Dwell time:"));
    m_lblSensitivity  = new QLabel(tr("Sensitivity:"));
    m_lblHoverSelect  = new QLabel(tr("Hover to switch:"));
    m_lblScrollRepeat = new QLabel(tr("Scroll repeat:"));
    m_lblRepeatMode   = new QLabel(tr("Repeat click:"));
    m_btnSensTester = new QPushButton(tr("Sensitivity Tester…"));
    m_btnSensTester->setProperty("flat", true);
    connect(m_btnSensTester, &QPushButton::clicked, this, [this](){
        auto* dlg = new SensitivityTesterDialog(this);
        connect(dlg, &SensitivityTesterDialog::sensitivityChosen,
                this, [this](int px){ m_sensitivPx->setValue(px); });
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    fl->addRow(m_lblDwellTime,    m_dwellMs);
    fl->addRow(m_lblSensitivity,  m_sensitivPx);
    fl->addRow(m_btnSensTester);
    fl->addRow(m_lblHoverSelect,  m_hoverSelectPct);
    fl->addRow(m_lblScrollRepeat, m_scrollRepeat);
    fl->addRow(m_lblRepeatMode,   m_chkRepeatMode);

#ifdef Q_OS_MAC
    m_lblPermissions   = new QLabel(tr("Permissions:"));
    m_btnAccessibility = new QPushButton(tr("Open Accessibility Settings…"));
    m_btnAccessibility->setProperty("flat", true);
    connect(m_btnAccessibility, &QPushButton::clicked, this, [](){
        QDesktopServices::openUrl(QUrl(
            "x-apple.systempreferences:"
            "com.apple.preference.security?Privacy_Accessibility"));
    });
    fl->addRow(m_lblPermissions, m_btnAccessibility);
#endif

    m_tabs->addTab(pageDwell, tr("Dwell Clicking"));

    // ── Button Visibility ─────────────────────────────────────
    auto* pageBtns = new QWidget;
    auto* grid     = new QGridLayout(pageBtns);
    grid->setSpacing(6);

    // Section header for the reorderable click-button list.
    m_lblVisibleButtons = new QLabel(tr("Visible Buttons (check to show, Move Up/Down to reorder)"));
    m_lblVisibleButtons->setStyleSheet(
        "color: #FFA600; font-weight: bold; background: transparent;");
    grid->addWidget(m_lblVisibleButtons, 0, 0, 1, 3);

    // Reorderable, checkable list of the click-action buttons.  Items (order and
    // check state) are added in loadFrom(); the read-only Right Double / Right
    // Drag placeholders are intentionally omitted from this list.
    m_btnOrderList = new QListWidget;
    m_btnOrderList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_btnOrderList->setMinimumHeight(200);
    connect(m_btnOrderList, &QListWidget::currentRowChanged,
            this, [this](int){ updateMoveButtons(); });
    grid->addWidget(m_btnOrderList, 1, 0, 1, 2);

    m_btnMoveUp   = new QPushButton(tr("Move Up"));
    m_btnMoveDown = new QPushButton(tr("Move Down"));
    m_btnMoveUp->setProperty("flat", true);
    m_btnMoveDown->setProperty("flat", true);
    m_btnMoveUp->setFocusPolicy(Qt::NoFocus);
    m_btnMoveDown->setFocusPolicy(Qt::NoFocus);
    connect(m_btnMoveUp,   &QPushButton::clicked, this, [this]{ moveSelectedButton(-1); });
    connect(m_btnMoveDown, &QPushButton::clicked, this, [this]{ moveSelectedButton(+1); });
    auto* moveCol = new QVBoxLayout;
    moveCol->setSpacing(6);
    moveCol->addWidget(m_btnMoveUp);
    moveCol->addWidget(m_btnMoveDown);
    moveCol->addStretch(1);
    auto* moveHolder = new QWidget;
    moveHolder->setLayout(moveCol);
    grid->addWidget(moveHolder, 1, 2, 1, 1, Qt::AlignTop);

    int row = 2, col = 0;   // rows 0-1 hold the header and the reorder list

    // ── Custom Hotkeys section ────────────────────────────────
    auto* hotkeysSep = new QFrame;
    hotkeysSep->setFrameShape(QFrame::HLine);
    hotkeysSep->setStyleSheet("QFrame { color: #555; }");
    grid->addWidget(hotkeysSep, row++, 0, 1, 3);

    m_lblCustomHotkeys = new QLabel(tr("Custom Hotkeys"));
    m_lblCustomHotkeys->setStyleSheet(
        "color: #FFA600; font-weight: bold; background: transparent;");
    grid->addWidget(m_lblCustomHotkeys, row++, 0, 1, 3);

    for (int i = 0; i < 3; ++i) {
        m_chkHotkey[i] = new QCheckBox(tr("Hotkey %1").arg(i + 1));

        m_edtHotkeyLabel[i] = new QLineEdit;
        m_edtHotkeyLabel[i]->setPlaceholderText(tr("Label (optional)"));
        m_edtHotkeyLabel[i]->setToolTip(
            tr("Button label shown on the toolbar. Defaults to the key name if left blank."));

        m_kseHotkey[i] = new QKeySequenceEdit;
        m_kseHotkey[i]->setToolTip(
            tr("Click here and press the desired key combination (e.g. F12, Ctrl+A, Alt+Z)."));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        m_kseHotkey[i]->setMaximumSequenceLength(1);
#endif
        auto updateRowEnabled = [this, i](bool on) {
            m_edtHotkeyLabel[i]->setEnabled(on);
            m_kseHotkey[i]->setEnabled(on);
        };
        connect(m_chkHotkey[i], &QCheckBox::toggled, this, updateRowEnabled);

        grid->addWidget(m_chkHotkey[i],      row, 0);
        grid->addWidget(m_edtHotkeyLabel[i], row, 1);
        grid->addWidget(m_kseHotkey[i],      row, 2);
        row++;
    }

    grid->setRowStretch(row, 1);

    m_tabs->addTab(pageBtns, tr("Buttons"));

    // ── Window ────────────────────────────────────────────────
    auto* pageWin = new QWidget;
    auto* wfl     = new QFormLayout(pageWin);
    wfl->setSpacing(6);
    wfl->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    wfl->setLabelAlignment(Qt::AlignHCenter);

    auto* opRow = new QHBoxLayout;
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(20, 100);
    m_opacityLabel  = new QLabel("100%");
    m_opacityLabel->setFixedWidth(36);
    opRow->addWidget(m_opacitySlider);
    opRow->addWidget(m_opacityLabel);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v){
        m_opacityLabel->setText(QString::number(v) + "%");
    });

    m_lblEdgeLock = new QLabel(tr("Lock to screen edge:"));
    m_cmbEdgeLock = new QComboBox;
    m_cmbEdgeLock->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbEdgeLock->setMinimumWidth(110);
    m_cmbEdgeLock->addItem(tr("None"),       static_cast<int>(EdgeLock::None));
    m_cmbEdgeLock->addItem(tr("Left edge"),  static_cast<int>(EdgeLock::Left));
    m_cmbEdgeLock->addItem(tr("Right edge"), static_cast<int>(EdgeLock::Right));
    m_chkEdgeHide = new QCheckBox(tr("Slide off screen when idle"));
    connect(m_cmbEdgeLock, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){ m_chkEdgeHide->setEnabled(idx != 0); });
    wfl->addRow(m_lblEdgeLock, m_cmbEdgeLock);
    wfl->addRow(m_chkEdgeHide);

    m_chkAlwaysOnTop    = new QCheckBox(tr("Always on top"));
    m_chkStartMinimized = new QCheckBox(tr("Start minimized to tray"));
    m_chkXMinimizesApp  = new QCheckBox(tr("Top X minimizes app"));
    // Kept read-only for now: the top X always quits (see makeCheckboxReadOnly).
    // To re-enable, drop this call and restore its loadFrom() line.
    makeCheckboxReadOnly(m_chkXMinimizesApp);
    m_chkLaunchOnStartup= new QCheckBox(tr("Launch on system startup (Windows)"));
    m_chkAudio          = new QCheckBox(tr("Audio feedback on click"));
    m_chkClickIndicator = new QCheckBox(tr("Show click indicator ring (Windows)"));
    m_chkIconsOnly      = new QCheckBox(tr("Icons only (hide button labels)"));
    m_chkLargeButtons  = new QCheckBox(tr("Large buttons"));

    m_cmbLayout = new QComboBox;
    m_cmbLayout->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbLayout->setMinimumWidth(160);
    m_cmbLayout->addItem(tr("Rectangle (grid)"));
    m_cmbLayout->addItem(tr("Horizontal (one row)"));
    m_cmbLayout->addItem(tr("Vertical (one column)"));
    m_cmbLayout->addItem(tr("Vertical (two columns)"));

    m_lblOpacity   = new QLabel(tr("Opacity:"));
    m_lblBtnLayout = new QLabel(tr("Button layout:"));

    wfl->addRow(m_lblOpacity, opRow);
    wfl->addRow(m_chkAlwaysOnTop);
    // "Start minimized to tray" hidden from the UI — not added to the layout.
    // The checkbox is still constructed above so load/save/retranslate refs stay valid.
    // wfl->addRow(m_chkStartMinimized);
    wfl->addRow(m_chkXMinimizesApp);
    wfl->addRow(m_chkLaunchOnStartup);
    wfl->addRow(m_chkAudio);
    wfl->addRow(m_chkClickIndicator);
    wfl->addRow(m_chkIconsOnly);
    wfl->addRow(m_chkLargeButtons);
    wfl->addRow(m_lblBtnLayout, m_cmbLayout);

    m_btnOnScreenKbd = new QPushButton(tr("Open On-Screen Keyboard"));
    m_btnOnScreenKbd->setFlat(true);
    wfl->addRow(m_btnOnScreenKbd);
    connect(m_btnOnScreenKbd, &QPushButton::clicked, this, [this]() {
#if defined(Q_OS_WIN)
        // Build full path from %SystemRoot% so PATH resolution isn't needed.
        const QString sysRoot = QString::fromLocal8Bit(qgetenv("SystemRoot"));
        const QString osk = (sysRoot.isEmpty() ? QString("C:\\Windows") : sysRoot)
                            + "\\System32\\osk.exe";
        QProcess::startDetached(osk, {});
#elif defined(Q_OS_MAC)
        // The macOS Accessibility Keyboard is a system service, not a standalone app.
        // Navigate to Accessibility > Keyboard in System Settings so the user can
        // enable it — once enabled it appears as a persistent floating keyboard.
        const int macMajor = QSysInfo::productVersion().split('.').value(0).toInt();
        if (macMajor >= 13) {
            // macOS 13 Ventura+ uses the new System Settings URL scheme
            QDesktopServices::openUrl(QUrl(
                "x-apple.systempreferences:"
                "com.apple.Accessibility-Settings.extension?Keyboard"));
        } else {
            QDesktopServices::openUrl(QUrl(
                "x-apple.systempreferences:"
                "com.apple.preference.universalaccess?Keyboard"));
        }
#else
        // Prefer Wayland-native keyboards on Wayland to avoid X11 keyboards
        // starting (returning true) and then immediately crashing on pure Wayland.
        const bool onWayland = !qgetenv("WAYLAND_DISPLAY").isEmpty();
        const QStringList kbs = onWayland
            ? QStringList{"onboard", "squeekboard", "wvkbd",
                          "maliit-keyboard", "florence", "kvkbd", "matchbox-keyboard"}
            : QStringList{"onboard", "florence", "xvkbd", "kvkbd", "matchbox-keyboard"};
        for (const QString& kb : kbs) {
            if (QProcess::startDetached(kb, {}))
                return;
        }
        promptInstallOnScreenKeyboard();
#endif
    });

    m_tabs->addTab(pageWin, tr("Window"));

    // ── Audio Click ───────────────────────────────────────────
    auto* pageAudio = new QWidget;
    auto* aufl      = new QVBoxLayout(pageAudio);
    aufl->setSpacing(8);

    m_chkAudioClick = new QCheckBox(tr("Trigger the selected action with a loud sound"));
    aufl->addWidget(m_chkAudioClick);

    m_lblAudioClickInfo = new QLabel;
    m_lblAudioClickInfo->setWordWrap(true);
    m_lblAudioClickInfo->setStyleSheet("color:#E6E6E6;");
    m_lblAudioClickInfo->setText(audioClickInfoText());
    aufl->addWidget(m_lblAudioClickInfo);

    // Device picker, threshold slider and live input meter share one grid so the
    // controls line up in a column (the slider handle sits directly above the
    // meter bar — both run on the same 0–100 scale — letting the user set the
    // threshold just below where their noise peaks).
    auto* calGrid = new QGridLayout;
    calGrid->setHorizontalSpacing(8);

    m_lblAudioDevice = new QLabel(tr("Input device:"));
    m_cmbAudioDevice = new QComboBox;
    // Fill the grid column rather than growing the dialog to the widest device
    // name (Linux device descriptions can be very long).
    m_cmbAudioDevice->setMinimumWidth(180);
    m_cmbAudioDevice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cmbAudioDevice->addItem(tr("System default"), QString());   // empty id
#ifdef HAVE_MULTIMEDIA
    for (const AudioInputInfo& d : AudioClickListener::availableInputs())
        m_cmbAudioDevice->addItem(d.name, d.id);
#endif
    calGrid->addWidget(m_lblAudioDevice, 0, 0);
    calGrid->addWidget(m_cmbAudioDevice, 0, 1, 1, 2);

    m_lblAudioThreshold = new QLabel(tr("Loudness threshold:"));
    m_audioThreshSlider = new QSlider(Qt::Horizontal);
    m_audioThreshSlider->setRange(1, 100);
    m_audioThreshValue  = new QLabel("50%");
    m_audioThreshValue->setFixedWidth(36);
    calGrid->addWidget(m_lblAudioThreshold, 1, 0);
    calGrid->addWidget(m_audioThreshSlider, 1, 1);
    calGrid->addWidget(m_audioThreshValue,  1, 2);

    m_lblAudioMeter = new QLabel(tr("Input level:"));
    m_audioMeter    = new QProgressBar;
    m_audioMeter->setRange(0, 100);
    m_audioMeter->setValue(0);
    m_audioMeter->setTextVisible(false);
    m_audioMeter->setFixedHeight(10);
    m_audioMeter->setStyleSheet(
        "QProgressBar{background:#1A1A1A;border:1px solid #555;border-radius:3px;}"
        "QProgressBar::chunk{background:#FFA600;border-radius:2px;}");
    calGrid->addWidget(m_lblAudioMeter, 2, 0);
    calGrid->addWidget(m_audioMeter,    2, 1);
    calGrid->setColumnStretch(1, 1);
    aufl->addLayout(calGrid);
    aufl->addStretch(1);

    connect(m_audioThreshSlider, &QSlider::valueChanged, this, [this](int v){
        m_audioThreshValue->setText(QString::number(v) + "%");
    });
    // Switching the input device while the meter is live restarts capture on the
    // newly-chosen microphone so calibration reflects it immediately.
    connect(m_cmbAudioDevice, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){
#ifdef HAVE_MULTIMEDIA
        if (m_meterListener && m_meterListener->isRunning()) {
            stopAudioMeter();
            startAudioMeter();
        }
#endif
    });
    // The threshold only matters when audio click is enabled.
    auto updateAudioEnabled = [this](bool on){
        m_lblAudioThreshold->setEnabled(on);
        m_audioThreshSlider->setEnabled(on);
        m_audioThreshValue->setEnabled(on);
    };
    connect(m_chkAudioClick, &QCheckBox::toggled, this, updateAudioEnabled);

#ifndef HAVE_MULTIMEDIA
    // No audio support compiled in — show the option but make clear it is inert,
    // and hide the live meter (there is nothing to display).
    m_chkAudioClick->setEnabled(false);
    m_audioThreshSlider->setEnabled(false);
    m_cmbAudioDevice->setEnabled(false);
    m_lblAudioMeter->hide();
    m_audioMeter->hide();
#endif

    m_tabs->addTab(pageAudio, tr("Audio Click"));

    // ── Buttons ───────────────────────────────────────────────
    m_buttons   = new QDialogButtonBox();
    m_okBtn     = m_buttons->addButton(tr("OK"),     QDialogButtonBox::AcceptRole);
    m_cancelBtn = m_buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_resetBtn  = m_buttons->addButton(tr("Reset to Defaults"), QDialogButtonBox::ResetRole);
    root->addWidget(m_buttons);
}

#ifdef Q_OS_LINUX
void SettingsDialog::promptInstallOnScreenKeyboard()
{
    // Build the install command for "onboard" (the keyboard the launcher tries
    // first, packaged on every major distro) using whichever package manager is
    // present.  Elevation is done with pkexec so the user gets a graphical
    // password prompt rather than needing a terminal.
    const QString pkexec = QStandardPaths::findExecutable("pkexec");
    QStringList   installArgs;
    if (!QStandardPaths::findExecutable("apt-get").isEmpty())
        installArgs = {"env", "DEBIAN_FRONTEND=noninteractive",
                       "apt-get", "install", "-y", "onboard"};
    else if (!QStandardPaths::findExecutable("dnf").isEmpty())
        installArgs = {"dnf", "install", "-y", "onboard"};
    else if (!QStandardPaths::findExecutable("zypper").isEmpty())
        installArgs = {"zypper", "--non-interactive", "install", "onboard"};
    else if (!QStandardPaths::findExecutable("pacman").isEmpty())
        installArgs = {"pacman", "-S", "--noconfirm", "onboard"};

    // Without pkexec or a recognised package manager, fall back to advising.
    if (pkexec.isEmpty() || installArgs.isEmpty()) {
        QMessageBox::information(this, tr("On-Screen Keyboard"),
            tr("No on-screen keyboard was found.\n"
               "Please install 'onboard' or 'florence'."));
        return;
    }

    if (QMessageBox::question(this, tr("On-Screen Keyboard"),
            tr("No on-screen keyboard is installed. Install 'onboard' now?\n"
               "You'll be asked for your password."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
        return;

    // Run the install asynchronously behind a busy dialog so the settings
    // window stays responsive during the download.
    auto* proc     = new QProcess(this);
    auto* progress = new QProgressDialog(tr("Installing on-screen keyboard…"),
                                         QString(), 0, 0, this);
    progress->setWindowTitle(tr("On-Screen Keyboard"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setCancelButton(nullptr);   // apt cannot be safely interrupted mid-run
    progress->show();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, progress](int code, QProcess::ExitStatus status) {
        progress->close();
        progress->deleteLater();
        proc->deleteLater();
        if (status == QProcess::NormalExit && code == 0) {
            if (!QProcess::startDetached("onboard", {}))
                QMessageBox::information(this, tr("On-Screen Keyboard"),
                    tr("'onboard' was installed. Click the button again to open it."));
        } else {
            QMessageBox::warning(this, tr("On-Screen Keyboard"),
                tr("The on-screen keyboard could not be installed automatically.\n"
                   "You can install it from a terminal with:  sudo apt install onboard"));
        }
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, progress](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;   // other errors are followed by finished(), handled above
        progress->close();
        progress->deleteLater();
        proc->deleteLater();
        QMessageBox::warning(this, tr("On-Screen Keyboard"),
            tr("Could not launch the installer (pkexec).\n"
               "You can install it from a terminal with:  sudo apt install onboard"));
    });

    proc->start(pkexec, installArgs);
}
#endif

void SettingsDialog::applyFontScale()
{
    m_fontScale = qBound(kZoomMinPct, m_fontScale, kZoomMaxPct);
    setStyleSheet(buildStyle(qRound(kBaseFontPx * m_fontScale / 100.0)));
    if (m_lblZoomValue)
        m_lblZoomValue->setText(QString::number(m_fontScale) + "%");
    // Grey out a button once its limit is reached.
    if (m_btnZoomOut) m_btnZoomOut->setEnabled(m_fontScale > kZoomMinPct);
    if (m_btnZoomIn)  m_btnZoomIn->setEnabled(m_fontScale < kZoomMaxPct);
}

// Display label for a click-button id.  These strings intentionally match the
// former checkbox labels so existing translations keep working.
QString SettingsDialog::clickButtonLabel(const QString& id) const
{
    if (id == QLatin1String("no_click"))      return tr("No Click");
    if (id == QLatin1String("left_click"))    return tr("Left Click");
    if (id == QLatin1String("left_double"))   return tr("Left Double");
    if (id == QLatin1String("left_drag"))     return tr("Left Drag");
    if (id == QLatin1String("right_click"))   return tr("Right Click");
    if (id == QLatin1String("right_double"))  return tr("Right Double");
    if (id == QLatin1String("right_drag"))    return tr("Right Drag");
    if (id == QLatin1String("middle_click"))  return tr("Middle Click");
    if (id == QLatin1String("middle_double")) return tr("Middle Double");
    if (id == QLatin1String("scroll_up"))     return tr("Scroll Up");
    if (id == QLatin1String("scroll_down"))   return tr("Scroll Down");
    if (id == QLatin1String("scroll_horiz"))  return tr("Scroll Left/Right");
    if (id == QLatin1String("ctrl"))          return tr("Ctrl Modifier");
    if (id == QLatin1String("alt"))           return tr("Alt Modifier");
    if (id == QLatin1String("shift"))         return tr("Shift Modifier");
    if (id == QLatin1String("dwell_active"))  return tr("Dwell Active Button");
    if (id == QLatin1String("quit"))          return tr("Quit Button");
    return id;
}

void SettingsDialog::moveSelectedButton(int delta)
{
    const int r = m_btnOrderList->currentRow();
    if (r < 0) return;
    const int nr = r + delta;
    if (nr < 0 || nr >= m_btnOrderList->count()) return;
    // takeItem/insertItem preserves the item's check state and stored id.
    QListWidgetItem* item = m_btnOrderList->takeItem(r);
    m_btnOrderList->insertItem(nr, item);
    m_btnOrderList->setCurrentRow(nr);   // keep the moved row selected
    updateMoveButtons();
}

void SettingsDialog::updateMoveButtons()
{
    const int r = m_btnOrderList->currentRow();
    const int n = m_btnOrderList->count();
    if (m_btnMoveUp)   m_btnMoveUp->setEnabled(r > 0);
    if (m_btnMoveDown) m_btnMoveDown->setEnabled(r >= 0 && r < n - 1);
}

void SettingsDialog::loadFrom(const AppSettings& s)
{
    m_dwellMs->setValue(s.dwellMs);
    m_sensitivPx->setValue(s.sensitivityPx);
    m_hoverSelectPct->setValue(s.hoverSelectPercent);
    m_scrollRepeat->setValue(s.scrollRepeat);
    m_chkRepeatMode->setChecked(s.repeatOnDwell);

    // Populate the reorder list in the saved order, with saved visibility as the
    // check state.  Right Double / Right Drag stay read-only and off, so they're
    // not listed here.
    m_btnOrderList->clear();
    for (const QString& id : orderedClickButtonIds(s)) {
        if (id == QLatin1String("right_double") || id == QLatin1String("right_drag"))
            continue;
        auto* item = new QListWidgetItem(clickButtonLabel(id), m_btnOrderList);
        item->setData(Qt::UserRole, id);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        bool on = false;
        if (bool AppSettings::* memb = showMemberForId(id))
            on = s.*memb;
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    if (m_btnOrderList->count() > 0)
        m_btnOrderList->setCurrentRow(0);
    updateMoveButtons();

    m_cmbEdgeLock->setCurrentIndex(static_cast<int>(s.edgeLock));
    m_chkEdgeHide->setChecked(s.edgeHide);
    m_chkEdgeHide->setEnabled(s.edgeLock != EdgeLock::None);

    m_opacitySlider->setValue(static_cast<int>(s.windowOpacity * 100));
    m_chkAlwaysOnTop->setChecked(s.alwaysOnTop);
    m_chkStartMinimized->setChecked(s.startMinimized);
    // Top X minimizes app is intentionally left read-only and unchecked (see
    // buildUi); don't drive it from settings. Restore this line to re-enable.
    m_chkLaunchOnStartup->setChecked(s.launchOnStartup);
    m_chkAudio->setChecked(s.audioFeedback);
    m_chkClickIndicator->setChecked(s.showClickIndicator);
    m_chkIconsOnly->setChecked(s.iconsOnly);
    m_chkLargeButtons->setChecked(s.largeButtons);
    m_cmbLayout->setCurrentIndex(static_cast<int>(s.buttonLayout));
    for (int i = 0; i < m_cmbLanguage->count(); ++i) {
        if (m_cmbLanguage->itemData(i).toString() == s.language) {
            m_cmbLanguage->setCurrentIndex(i);
            break;
        }
    }

    m_fontScale = s.settingsFontScale;
    applyFontScale();

    m_chkAudioClick->setChecked(s.audioClickEnabled);
    m_audioThreshSlider->setValue(s.audioClickThreshold);
    m_audioThreshValue->setText(QString::number(s.audioClickThreshold) + "%");
    m_lblAudioThreshold->setEnabled(s.audioClickEnabled);
    m_audioThreshSlider->setEnabled(s.audioClickEnabled);
    m_audioThreshValue->setEnabled(s.audioClickEnabled);

    // Select the saved input device (index 0 / "System default" if not found,
    // e.g. the device was unplugged or the id came from another machine).
    int devIdx = m_cmbAudioDevice->findData(s.audioInputDevice);
    m_cmbAudioDevice->setCurrentIndex(devIdx >= 0 ? devIdx : 0);

    for (int i = 0; i < 3; ++i) {
        m_chkHotkey[i]->setChecked(s.hotkeys[i].enabled);
        m_edtHotkeyLabel[i]->setText(s.hotkeys[i].label);
        m_kseHotkey[i]->setKeySequence(QKeySequence(s.hotkeys[i].keySequence));
        m_edtHotkeyLabel[i]->setEnabled(s.hotkeys[i].enabled);
        m_kseHotkey[i]->setEnabled(s.hotkeys[i].enabled);
    }
}

AppSettings SettingsDialog::readUi() const
{
    AppSettings s;
    s.dwellMs       = m_dwellMs->value();
    s.sensitivityPx = m_sensitivPx->value();
    s.hoverSelectPercent = m_hoverSelectPct->value();
    s.scrollRepeat  = m_scrollRepeat->value();
    s.repeatOnDwell = m_chkRepeatMode->isChecked();

    // Read click-button order and per-button visibility from the reorder list.
    // Right Double / Right Drag aren't listed, so they keep their default (off).
    s.buttonOrder.clear();
    for (int i = 0; i < m_btnOrderList->count(); ++i) {
        QListWidgetItem* item = m_btnOrderList->item(i);
        const QString id = item->data(Qt::UserRole).toString();
        s.buttonOrder << id;
        if (bool AppSettings::* memb = showMemberForId(id))
            s.*memb = (item->checkState() == Qt::Checked);
    }

    s.edgeLock = static_cast<EdgeLock>(m_cmbEdgeLock->currentIndex());
    s.edgeHide = m_chkEdgeHide->isChecked() && (s.edgeLock != EdgeLock::None);

    s.windowOpacity   = m_opacitySlider->value() / 100.0;
    s.alwaysOnTop      = m_chkAlwaysOnTop->isChecked();
    s.startMinimized   = m_chkStartMinimized->isChecked();
    s.xMinimizesApp    = m_chkXMinimizesApp->isChecked();
    s.launchOnStartup  = m_chkLaunchOnStartup->isChecked();
    s.audioFeedback       = m_chkAudio->isChecked();
    s.showClickIndicator  = m_chkClickIndicator->isChecked();
    s.iconsOnly           = m_chkIconsOnly->isChecked();
    s.largeButtons    = m_chkLargeButtons->isChecked();
    s.buttonLayout    = static_cast<ButtonLayout>(m_cmbLayout->currentIndex());
    s.language        = m_cmbLanguage->currentData().toString();
    s.settingsFontScale = m_fontScale;

    s.audioClickEnabled   = m_chkAudioClick->isChecked();
    s.audioClickThreshold = m_audioThreshSlider->value();
    s.audioInputDevice    = m_cmbAudioDevice->currentData().toString();

    for (int i = 0; i < 3; ++i) {
        s.hotkeys[i].enabled     = m_chkHotkey[i]->isChecked();
        s.hotkeys[i].label       = m_edtHotkeyLabel[i]->text().trimmed();
        s.hotkeys[i].keySequence = m_kseHotkey[i]->keySequence()
                                       .toString(QKeySequence::PortableText);
    }
    return s;
}

#include "settingsdialog.moc"
