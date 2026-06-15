#include "settingsdialog.h"
#include "translations/tsparser.h"
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
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
)";

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

    m_grpDwell->setTitle(tr("AutoMouse / Dwell Clicking"));
    m_lblDwellTime->setText(tr("Dwell time:"));
    m_lblSensitivity->setText(tr("Sensitivity:"));
    m_lblScrollRepeat->setText(tr("Scroll repeat:"));
    m_chkRepeatMode->setText(tr("Repeat click while cursor stays still"));
#ifdef Q_OS_MAC
    m_lblPermissions->setText(tr("Permissions:"));
    m_btnAccessibility->setText(tr("Open Accessibility Settings…"));
#endif

    m_grpBtns->setTitle(tr("Visible Buttons"));
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
    m_chkModCtrl->setText(tr("Ctrl modifier"));
    m_chkModAlt->setText(tr("Alt modifier"));
    m_chkModShift->setText(tr("Shift modifier"));
    m_chkExitButton->setText(tr("Exit button"));
    m_chkQuitButton->setText(tr("Quit button"));
    m_chkDwellActiveBtn->setText(tr("Dwell Active button"));

    m_grpWin->setTitle(tr("Window"));
    m_chkAlwaysOnTop->setText(tr("Always on top"));
    m_chkStartMinimized->setText(tr("Start minimized to tray"));
    m_chkLaunchOnStartup->setText(tr("Launch on system startup"));
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
    auto* versionLbl = new QLabel("Version 0.9.0 (build " TC_STR(BUILD_NUMBER) ")");
#else
    auto* versionLbl = new QLabel("Version 0.9.0");
#endif
    versionLbl->setStyleSheet(
        "color: #666666; font-size: 11px; background: transparent;");
    nameLay->addWidget(appNameLbl);
    nameLay->addWidget(versionLbl);
    hLay->addLayout(nameLay);
    hLay->addStretch(1);

    root->addWidget(header);

    // ── Dwell / AutoMouse ─────────────────────────────────────
    m_grpDwell   = new QGroupBox(tr("AutoMouse / Dwell Clicking"));
    auto* fl     = new QFormLayout(m_grpDwell);
    fl->setSpacing(6);

    m_dwellMs      = new QSpinBox; m_dwellMs->setRange(100, 10000); m_dwellMs->setSuffix(" ms");
    m_sensitivPx   = new QSpinBox; m_sensitivPx->setRange(1, 100);  m_sensitivPx->setSuffix(" px");
    m_scrollRepeat = new QSpinBox; m_scrollRepeat->setRange(1, 20);

    m_chkRepeatMode = new QCheckBox(tr("Repeat click while cursor stays still"));

    m_lblDwellTime    = new QLabel(tr("Dwell time:"));
    m_lblSensitivity  = new QLabel(tr("Sensitivity:"));
    m_lblScrollRepeat = new QLabel(tr("Scroll repeat:"));
    fl->addRow(m_lblDwellTime,    m_dwellMs);
    fl->addRow(m_lblSensitivity,  m_sensitivPx);
    fl->addRow(m_lblScrollRepeat, m_scrollRepeat);
    fl->addRow(m_chkRepeatMode);

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

    root->addWidget(m_grpDwell);

    // ── Button Visibility ─────────────────────────────────────
    m_grpBtns    = new QGroupBox(tr("Visible Buttons"));
    auto* grid   = new QGridLayout(m_grpBtns);
    grid->setSpacing(6);

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
    m_chkModCtrl     = new QCheckBox(tr("Ctrl modifier"));
    m_chkModAlt      = new QCheckBox(tr("Alt modifier"));
    m_chkModShift    = new QCheckBox(tr("Shift modifier"));
    m_chkExitButton      = new QCheckBox(tr("Exit button"));
    m_chkQuitButton      = new QCheckBox(tr("Quit button"));
    m_chkDwellActiveBtn  = new QCheckBox(tr("Dwell Active button"));

    int row = 0, col = 0;
    auto addChk = [&](QCheckBox* c){
        grid->addWidget(c, row, col);
        col++;
        if (col == 3) { col = 0; row++; }
    };
    addChk(m_chkLeftClick);   addChk(m_chkLeftDouble);  addChk(m_chkLeftDrag);
    addChk(m_chkRightClick);  addChk(m_chkRightDouble); addChk(m_chkRightDrag);
    addChk(m_chkMiddleClick); addChk(m_chkMiddleDouble);addChk(m_chkScrollUp);
    addChk(m_chkScrollDown);  addChk(m_chkScrollHoriz); addChk(m_chkModCtrl);
    addChk(m_chkModAlt);      addChk(m_chkModShift);    addChk(m_chkExitButton);
    addChk(m_chkQuitButton);  addChk(m_chkDwellActiveBtn);

    root->addWidget(m_grpBtns);

    // ── Window ────────────────────────────────────────────────
    m_grpWin     = new QGroupBox(tr("Window"));
    auto* wfl    = new QFormLayout(m_grpWin);
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

    m_chkAlwaysOnTop    = new QCheckBox(tr("Always on top"));
    m_chkStartMinimized = new QCheckBox(tr("Start minimized to tray"));
    m_chkLaunchOnStartup= new QCheckBox(tr("Launch on system startup"));
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
    m_cmbLanguage->addItem("English",  "en");
    m_cmbLanguage->addItem("Čeština",  "cs");
    m_cmbLanguage->addItem("Français", "fr");
    m_cmbLanguage->addItem("Español",  "es");
    m_cmbLanguage->addItem("中文简体",  "zh_CN");
    m_cmbLanguage->addItem("日本語",    "ja");
    m_cmbLanguage->addItem("한국어",    "ko");

    m_lblOpacity   = new QLabel(tr("Opacity:"));
    m_lblBtnLayout = new QLabel(tr("Button layout:"));
    m_lblLanguage  = new QLabel(tr("Language:"));

    wfl->addRow(m_lblOpacity, opRow);
    wfl->addRow(m_chkAlwaysOnTop);
    wfl->addRow(m_chkStartMinimized);
    wfl->addRow(m_chkLaunchOnStartup);
    wfl->addRow(m_chkAudio);
    wfl->addRow(m_chkIconsOnly);
    wfl->addRow(m_chkLargeButtons);
    wfl->addRow(m_lblBtnLayout, m_cmbLayout);
    wfl->addRow(m_lblLanguage,  m_cmbLanguage);
    root->addWidget(m_grpWin);

    // ── Buttons ───────────────────────────────────────────────
    m_buttons  = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_resetBtn = m_buttons->addButton(tr("Reset to Defaults"), QDialogButtonBox::ResetRole);
    root->addWidget(m_buttons);
}

void SettingsDialog::loadFrom(const AppSettings& s)
{
    m_dwellMs->setValue(s.dwellMs);
    m_sensitivPx->setValue(s.sensitivityPx);
    m_scrollRepeat->setValue(s.scrollRepeat);
    m_chkRepeatMode->setChecked(s.repeatOnDwell);

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

    m_opacitySlider->setValue(static_cast<int>(s.windowOpacity * 100));
    m_chkAlwaysOnTop->setChecked(s.alwaysOnTop);
    m_chkStartMinimized->setChecked(s.startMinimized);
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
}

AppSettings SettingsDialog::readUi() const
{
    AppSettings s;
    s.dwellMs       = m_dwellMs->value();
    s.sensitivityPx = m_sensitivPx->value();
    s.scrollRepeat  = m_scrollRepeat->value();
    s.repeatOnDwell = m_chkRepeatMode->isChecked();

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

    s.windowOpacity   = m_opacitySlider->value() / 100.0;
    s.alwaysOnTop      = m_chkAlwaysOnTop->isChecked();
    s.startMinimized   = m_chkStartMinimized->isChecked();
    s.launchOnStartup  = m_chkLaunchOnStartup->isChecked();
    s.audioFeedback   = m_chkAudio->isChecked();
    s.iconsOnly       = m_chkIconsOnly->isChecked();
    s.largeButtons    = m_chkLargeButtons->isChecked();
    s.buttonLayout    = static_cast<ButtonLayout>(m_cmbLayout->currentIndex());
    s.language        = m_cmbLanguage->currentData().toString();
    return s;
}
