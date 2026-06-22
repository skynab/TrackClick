#include "settingsdialog.h"
#include "translations/tsparser.h"
#include "audioclicklistener.h"
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
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
static const char* STYLE = R"(
QDialog {
    background: #2D2D2D;
    color: #979797;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QGroupBox {
    color: #FFA600;
    border: 1px solid #979797;
    border-radius: 4px;
    margin-top: 10px;
    padding-top: 6px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}
QLabel  { color: #979797; }
QCheckBox { color: #979797; spacing: 6px; }
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
QSpinBox, QDoubleSpinBox, QComboBox {
    background: #1A1A1A;
    color: #979797;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 2px 4px;
}
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: #2D2D2D;
    color: #979797;
    border: 1px solid #555;
    selection-background-color: #FFA600;
    selection-color: #1A1A1A;
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
    color: #979797;
    padding: 6px 14px;
    border: 1px solid #555;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected { background: #2D2D2D; color: #FFA600; }
QTabBar::tab:hover    { color: #FFB833; }
)";

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
    setStyleSheet(STYLE);
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
    setStyleSheet(STYLE);
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
    m_lblScrollRepeat->setText(tr("Scroll repeat:"));
    m_lblRepeatMode->setText(tr("Repeat click:"));
#ifdef Q_OS_MAC
    m_lblPermissions->setText(tr("Permissions:"));
    m_btnAccessibility->setText(tr("Open Accessibility Settings…"));
#endif

    m_tabs->setTabText(1, tr("Buttons"));
    m_lblVisibleButtons->setText(tr("Visible Buttons"));
    m_chkNoClick->setText(tr("No Click"));
    m_chkLeftClick->setText(tr("Left Click"));
    m_chkLeftDouble->setText(tr("Left Double"));
    m_chkLeftDrag->setText(tr("Left Drag"));
    m_chkRightClick->setText(tr("Right Click"));
    m_chkRightDouble->setText(tr("Right Double"));
    m_chkRightDrag->setText(tr("Right Drag"));
    m_chkMiddleClick->setText(tr("Middle Click"));
    m_chkMiddleDouble->setText(tr("Middle Double"));
    m_chkScrollUp->setText(tr("Scroll Up"));
    m_chkScrollDown->setText(tr("Scroll Down"));
    m_chkScrollHoriz->setText(tr("Scroll Left/Right"));
    m_chkModCtrl->setText(tr("Ctrl Modifier"));
    m_chkModAlt->setText(tr("Alt Modifier"));
    m_chkModShift->setText(tr("Shift Modifier"));
    m_chkExitButton->setText(tr("Exit Button"));
    m_chkQuitButton->setText(tr("Quit Button"));
    m_chkDwellActiveBtn->setText(tr("Dwell Active Button"));

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
    m_chkIconsOnly->setText(tr("Icons only (hide button labels)"));
    m_chkLargeButtons->setText(tr("Large buttons"));
    m_lblOpacity->setText(tr("Opacity:"));
    m_lblBtnLayout->setText(tr("Button layout:"));
    m_cmbLayout->setItemText(0, tr("Rectangle (grid)"));
    m_cmbLayout->setItemText(1, tr("Horizontal (one row)"));
    m_cmbLayout->setItemText(2, tr("Vertical (one column)"));
    m_cmbLayout->setItemText(3, tr("Vertical (two columns)"));
    m_lblLanguage->setText(tr("Language:"));
    m_resetBtn->setText(tr("Reset to Defaults"));
    m_btnOnScreenKbd->setText(tr("Open On-Screen Keyboard"));

    m_tabs->setTabText(3, tr("Audio Click"));
    m_chkAudioClick->setText(tr("Trigger the selected action with a loud sound"));
    m_lblAudioClickInfo->setText(audioClickInfoText());
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
    if (!m_meterListener->isRunning())
        m_meterListener->start();   // harmless if there is no input device
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
    auto* versionLbl = new QLabel("Version 0.9.3 (build " TC_STR(BUILD_NUMBER) ")");
#else
    auto* versionLbl = new QLabel("Version 0.9.3");
#endif
    versionLbl->setStyleSheet(
        "color: #666666; font-size: 11px; background: transparent;");
    nameLay->addWidget(appNameLbl);
    nameLay->addWidget(versionLbl);
    hLay->addLayout(nameLay);
    hLay->addStretch(1);

    root->addWidget(header);

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
    m_scrollRepeat = new QSpinBox; m_scrollRepeat->setRange(1, 20);

    m_chkRepeatMode = new QCheckBox;

    m_lblDwellTime    = new QLabel(tr("Dwell time:"));
    m_lblSensitivity  = new QLabel(tr("Sensitivity:"));
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

    // Section header — the tab is just labelled "Buttons", so spell out what
    // these checkboxes control here.
    m_lblVisibleButtons = new QLabel(tr("Visible Buttons"));
    m_lblVisibleButtons->setStyleSheet(
        "color: #FFA600; font-weight: bold; background: transparent;");
    grid->addWidget(m_lblVisibleButtons, 0, 0, 1, 3);

    m_chkNoClick     = new QCheckBox(tr("No Click"));
    m_chkLeftClick   = new QCheckBox(tr("Left Click"));
    m_chkLeftDouble  = new QCheckBox(tr("Left Double"));
    m_chkLeftDrag    = new QCheckBox(tr("Left Drag"));
    m_chkRightClick  = new QCheckBox(tr("Right Click"));
    m_chkRightDouble = new QCheckBox(tr("Right Double"));
    m_chkRightDrag   = new QCheckBox(tr("Right Drag"));
    m_chkMiddleClick = new QCheckBox(tr("Middle Click"));
    m_chkMiddleDouble= new QCheckBox(tr("Middle Double"));
    m_chkScrollUp    = new QCheckBox(tr("Scroll Up"));
    m_chkScrollDown  = new QCheckBox(tr("Scroll Down"));
    m_chkScrollHoriz = new QCheckBox(tr("Scroll Left/Right"));
    m_chkModCtrl     = new QCheckBox(tr("Ctrl Modifier"));
    m_chkModAlt      = new QCheckBox(tr("Alt Modifier"));
    m_chkModShift    = new QCheckBox(tr("Shift Modifier"));
    m_chkExitButton      = new QCheckBox(tr("Exit Button"));
    m_chkQuitButton      = new QCheckBox(tr("Quit Button"));
    m_chkDwellActiveBtn  = new QCheckBox(tr("Dwell Active Button"));

    int row = 1, col = 0;   // row 0 holds the "Visible Buttons" section header
    auto addChk = [&](QCheckBox* c){
        grid->addWidget(c, row, col);
        col++;
        if (col == 3) { col = 0; row++; }
    };
    addChk(m_chkNoClick);
    addChk(m_chkLeftClick);   addChk(m_chkLeftDouble);  addChk(m_chkLeftDrag);
    addChk(m_chkRightClick);  addChk(m_chkRightDouble); addChk(m_chkRightDrag);
    addChk(m_chkMiddleClick); addChk(m_chkMiddleDouble);addChk(m_chkScrollUp);
    addChk(m_chkScrollDown);  addChk(m_chkScrollHoriz); addChk(m_chkModCtrl);
    addChk(m_chkModAlt);      addChk(m_chkModShift);    addChk(m_chkExitButton);
    addChk(m_chkQuitButton);  addChk(m_chkDwellActiveBtn);

    // Absorb any leftover vertical space in a trailing empty row so the tab's
    // extra height doesn't get spread across the content rows (which otherwise
    // makes the title row balloon to fill half the tab).
    grid->setRowStretch(row + 1, 1);

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
    m_chkLaunchOnStartup= new QCheckBox(tr("Launch on system startup (Windows)"));
    m_chkAudio         = new QCheckBox(tr("Audio feedback on click"));
    m_chkIconsOnly     = new QCheckBox(tr("Icons only (hide button labels)"));
    m_chkLargeButtons  = new QCheckBox(tr("Large buttons"));

    m_cmbLayout = new QComboBox;
    m_cmbLayout->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbLayout->setMinimumWidth(160);
    m_cmbLayout->addItem(tr("Rectangle (grid)"));
    m_cmbLayout->addItem(tr("Horizontal (one row)"));
    m_cmbLayout->addItem(tr("Vertical (one column)"));
    m_cmbLayout->addItem(tr("Vertical (two columns)"));

    // Language names are shown in their native script — intentionally not tr()
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

    m_lblOpacity   = new QLabel(tr("Opacity:"));
    m_lblBtnLayout = new QLabel(tr("Button layout:"));
    m_lblLanguage  = new QLabel(tr("Language:"));

    wfl->addRow(m_lblOpacity, opRow);
    wfl->addRow(m_chkAlwaysOnTop);
    // "Start minimized to tray" hidden from the UI — not added to the layout.
    // The checkbox is still constructed above so load/save/retranslate refs stay valid.
    // wfl->addRow(m_chkStartMinimized);
    wfl->addRow(m_chkXMinimizesApp);
    wfl->addRow(m_chkLaunchOnStartup);
    wfl->addRow(m_chkAudio);
    wfl->addRow(m_chkIconsOnly);
    wfl->addRow(m_chkLargeButtons);
    wfl->addRow(m_lblBtnLayout, m_cmbLayout);
    wfl->addRow(m_lblLanguage,  m_cmbLanguage);

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
    m_lblAudioClickInfo->setStyleSheet("color:#979797;");
    m_lblAudioClickInfo->setText(audioClickInfoText());
    aufl->addWidget(m_lblAudioClickInfo);

    // Threshold slider and live input meter share one grid so the slider handle
    // lines up directly above the meter bar (both run on the same 0–100 scale),
    // letting the user set the threshold just below where their noise peaks.
    auto* calGrid = new QGridLayout;
    calGrid->setHorizontalSpacing(8);
    m_lblAudioThreshold = new QLabel(tr("Loudness threshold:"));
    m_audioThreshSlider = new QSlider(Qt::Horizontal);
    m_audioThreshSlider->setRange(1, 100);
    m_audioThreshValue  = new QLabel("50%");
    m_audioThreshValue->setFixedWidth(36);
    calGrid->addWidget(m_lblAudioThreshold, 0, 0);
    calGrid->addWidget(m_audioThreshSlider, 0, 1);
    calGrid->addWidget(m_audioThreshValue,  0, 2);

    m_lblAudioMeter = new QLabel(tr("Input level:"));
    m_audioMeter    = new QProgressBar;
    m_audioMeter->setRange(0, 100);
    m_audioMeter->setValue(0);
    m_audioMeter->setTextVisible(false);
    m_audioMeter->setFixedHeight(10);
    m_audioMeter->setStyleSheet(
        "QProgressBar{background:#1A1A1A;border:1px solid #555;border-radius:3px;}"
        "QProgressBar::chunk{background:#FFA600;border-radius:2px;}");
    calGrid->addWidget(m_lblAudioMeter, 1, 0);
    calGrid->addWidget(m_audioMeter,    1, 1);
    calGrid->setColumnStretch(1, 1);
    aufl->addLayout(calGrid);
    aufl->addStretch(1);

    connect(m_audioThreshSlider, &QSlider::valueChanged, this, [this](int v){
        m_audioThreshValue->setText(QString::number(v) + "%");
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
    m_lblAudioMeter->hide();
    m_audioMeter->hide();
#endif

    m_tabs->addTab(pageAudio, tr("Audio Click"));

    // ── Buttons ───────────────────────────────────────────────
    m_buttons  = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
#ifdef Q_OS_LINUX
    // On Linux/GTK the system theme injects icons into standard buttons; remove them.
    for (QAbstractButton* btn : m_buttons->buttons())
        btn->setIcon(QIcon());
#endif
    m_resetBtn = m_buttons->addButton(tr("Reset to Defaults"), QDialogButtonBox::ResetRole);
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

void SettingsDialog::loadFrom(const AppSettings& s)
{
    m_dwellMs->setValue(s.dwellMs);
    m_sensitivPx->setValue(s.sensitivityPx);
    m_scrollRepeat->setValue(s.scrollRepeat);
    m_chkRepeatMode->setChecked(s.repeatOnDwell);

    m_chkNoClick->setChecked(s.showNoClick);
    m_chkLeftClick->setChecked(s.showLeftClick);
    m_chkLeftDouble->setChecked(s.showLeftDouble);
    m_chkLeftDrag->setChecked(s.showLeftDrag);
    m_chkRightClick->setChecked(s.showRightClick);
    m_chkRightDouble->setChecked(s.showRightDouble);
    m_chkRightDrag->setChecked(s.showRightDrag);
    m_chkMiddleClick->setChecked(s.showMiddleClick);
    m_chkMiddleDouble->setChecked(s.showMiddleDouble);
    m_chkScrollUp->setChecked(s.showScrollUp);
    m_chkScrollDown->setChecked(s.showScrollDown);
    m_chkScrollHoriz->setChecked(s.showScrollHoriz);
    m_chkModCtrl->setChecked(s.showModCtrl);
    m_chkModAlt->setChecked(s.showModAlt);
    m_chkModShift->setChecked(s.showModShift);
    m_chkExitButton->setChecked(s.showExitButton);
    m_chkQuitButton->setChecked(s.showQuitButton);
    m_chkDwellActiveBtn->setChecked(s.showDwellActiveBtn);

    m_cmbEdgeLock->setCurrentIndex(static_cast<int>(s.edgeLock));
    m_chkEdgeHide->setChecked(s.edgeHide);
    m_chkEdgeHide->setEnabled(s.edgeLock != EdgeLock::None);

    m_opacitySlider->setValue(static_cast<int>(s.windowOpacity * 100));
    m_chkAlwaysOnTop->setChecked(s.alwaysOnTop);
    m_chkStartMinimized->setChecked(s.startMinimized);
    m_chkXMinimizesApp->setChecked(s.xMinimizesApp);
    m_chkLaunchOnStartup->setChecked(s.launchOnStartup);
    m_chkAudio->setChecked(s.audioFeedback);
    m_chkIconsOnly->setChecked(s.iconsOnly);
    m_chkLargeButtons->setChecked(s.largeButtons);
    m_cmbLayout->setCurrentIndex(static_cast<int>(s.buttonLayout));
    for (int i = 0; i < m_cmbLanguage->count(); ++i) {
        if (m_cmbLanguage->itemData(i).toString() == s.language) {
            m_cmbLanguage->setCurrentIndex(i);
            break;
        }
    }

    m_chkAudioClick->setChecked(s.audioClickEnabled);
    m_audioThreshSlider->setValue(s.audioClickThreshold);
    m_audioThreshValue->setText(QString::number(s.audioClickThreshold) + "%");
    m_lblAudioThreshold->setEnabled(s.audioClickEnabled);
    m_audioThreshSlider->setEnabled(s.audioClickEnabled);
    m_audioThreshValue->setEnabled(s.audioClickEnabled);
}

AppSettings SettingsDialog::readUi() const
{
    AppSettings s;
    s.dwellMs       = m_dwellMs->value();
    s.sensitivityPx = m_sensitivPx->value();
    s.scrollRepeat  = m_scrollRepeat->value();
    s.repeatOnDwell = m_chkRepeatMode->isChecked();

    s.showNoClick     = m_chkNoClick->isChecked();
    s.showLeftClick   = m_chkLeftClick->isChecked();
    s.showLeftDouble  = m_chkLeftDouble->isChecked();
    s.showLeftDrag    = m_chkLeftDrag->isChecked();
    s.showRightClick  = m_chkRightClick->isChecked();
    s.showRightDouble = m_chkRightDouble->isChecked();
    s.showRightDrag   = m_chkRightDrag->isChecked();
    s.showMiddleClick = m_chkMiddleClick->isChecked();
    s.showMiddleDouble= m_chkMiddleDouble->isChecked();
    s.showScrollUp    = m_chkScrollUp->isChecked();
    s.showScrollDown  = m_chkScrollDown->isChecked();
    s.showScrollHoriz = m_chkScrollHoriz->isChecked();
    s.showModCtrl     = m_chkModCtrl->isChecked();
    s.showModAlt      = m_chkModAlt->isChecked();
    s.showModShift    = m_chkModShift->isChecked();
    s.showExitButton     = m_chkExitButton->isChecked();
    s.showQuitButton     = m_chkQuitButton->isChecked();
    s.showDwellActiveBtn = m_chkDwellActiveBtn->isChecked();

    s.edgeLock = static_cast<EdgeLock>(m_cmbEdgeLock->currentIndex());
    s.edgeHide = m_chkEdgeHide->isChecked() && (s.edgeLock != EdgeLock::None);

    s.windowOpacity   = m_opacitySlider->value() / 100.0;
    s.alwaysOnTop      = m_chkAlwaysOnTop->isChecked();
    s.startMinimized   = m_chkStartMinimized->isChecked();
    s.xMinimizesApp    = m_chkXMinimizesApp->isChecked();
    s.launchOnStartup  = m_chkLaunchOnStartup->isChecked();
    s.audioFeedback   = m_chkAudio->isChecked();
    s.iconsOnly       = m_chkIconsOnly->isChecked();
    s.largeButtons    = m_chkLargeButtons->isChecked();
    s.buttonLayout    = static_cast<ButtonLayout>(m_cmbLayout->currentIndex());
    s.language        = m_cmbLanguage->currentData().toString();

    s.audioClickEnabled   = m_chkAudioClick->isChecked();
    s.audioClickThreshold = m_audioThreshSlider->value();
    return s;
}

#include "settingsdialog.moc"
