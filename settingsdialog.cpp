#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

// ── TrackIR palette ──────────────────────────────────────────
static const char* STYLE = R"(
QDialog {
    background: #2D2D2D;
    color: #FFFFFF;
    font-family: "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QGroupBox {
    color: #FFA600;
    border: 1px solid #FFA600;
    border-radius: 4px;
    margin-top: 10px;
    padding-top: 6px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}
QLabel  { color: #FFFFFF; }
QCheckBox { color: #FFFFFF; spacing: 6px; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #FFA600;
    border-radius: 2px;
    background: #2D2D2D;
}
QCheckBox::indicator:checked { background: #FFA600; }
QSpinBox, QDoubleSpinBox {
    background: #1A1A1A;
    color: #FFA600;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 2px 4px;
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

SettingsDialog::SettingsDialog(const AppSettings& current, QWidget* parent)
    : QDialog(parent), m_settings(current)
{
    setWindowTitle("TrackClick — Settings");
    setModal(true);
    setStyleSheet(STYLE);
    buildUi();
    loadFrom(current);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this](){
        m_settings = readUi();
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(14, 14, 14, 14);

    // ── Dwell / AutoMouse ─────────────────────────────────────
    auto* grpDwell = new QGroupBox("AutoMouse / Dwell Clicking");
    auto* fl       = new QFormLayout(grpDwell);
    fl->setSpacing(6);

    m_dwellMs   = new QSpinBox; m_dwellMs->setRange(100, 10000); m_dwellMs->setSuffix(" ms");
    m_sensitivPx= new QSpinBox; m_sensitivPx->setRange(1, 50);   m_sensitivPx->setSuffix(" px");
    fl->addRow("Dwell time:",    m_dwellMs);
    fl->addRow("Sensitivity:",   m_sensitivPx);
    root->addWidget(grpDwell);

    // ── Button Visibility ─────────────────────────────────────
    auto* grpBtns = new QGroupBox("Visible Buttons");
    auto* grid    = new QGridLayout(grpBtns);
    grid->setSpacing(6);

    m_chkLeftClick   = new QCheckBox("Left Click");
    m_chkLeftDouble  = new QCheckBox("Left Double");
    m_chkLeftDrag    = new QCheckBox("Left Drag");
    m_chkRightClick  = new QCheckBox("Right Click");
    m_chkRightDouble = new QCheckBox("Right Double");
    m_chkRightDrag   = new QCheckBox("Right Drag");
    m_chkMiddleClick = new QCheckBox("Middle Click");
    m_chkMiddleDouble= new QCheckBox("Middle Double");
    m_chkScrollUp    = new QCheckBox("Scroll Up");
    m_chkScrollDown  = new QCheckBox("Scroll Down");
    m_chkScrollHoriz = new QCheckBox("Scroll Left/Right");
    m_chkModCtrl     = new QCheckBox("Ctrl modifier");
    m_chkModAlt      = new QCheckBox("Alt modifier");
    m_chkModShift    = new QCheckBox("Shift modifier");
    m_chkExitButton  = new QCheckBox("Exit button");

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

    root->addWidget(grpBtns);

    // ── Window ────────────────────────────────────────────────
    auto* grpWin = new QGroupBox("Window");
    auto* wfl    = new QFormLayout(grpWin);
    wfl->setSpacing(6);

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

    m_chkAlwaysOnTop   = new QCheckBox("Always on top");
    m_chkStartMinimized= new QCheckBox("Start minimized to tray");
    m_chkAudio         = new QCheckBox("Audio feedback on click");

    wfl->addRow("Opacity:", opRow);
    wfl->addRow(m_chkAlwaysOnTop);
    wfl->addRow(m_chkStartMinimized);
    wfl->addRow(m_chkAudio);
    root->addWidget(grpWin);

    // ── Buttons ───────────────────────────────────────────────
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(m_buttons);
}

void SettingsDialog::loadFrom(const AppSettings& s)
{
    m_dwellMs->setValue(s.dwellMs);
    m_sensitivPx->setValue(s.sensitivityPx);

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

    m_opacitySlider->setValue(static_cast<int>(s.windowOpacity * 100));
    m_chkAlwaysOnTop->setChecked(s.alwaysOnTop);
    m_chkStartMinimized->setChecked(s.startMinimized);
    m_chkAudio->setChecked(s.audioFeedback);
}

AppSettings SettingsDialog::readUi() const
{
    AppSettings s;
    s.dwellMs       = m_dwellMs->value();
    s.sensitivityPx = m_sensitivPx->value();

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
    s.showExitButton  = m_chkExitButton->isChecked();

    s.windowOpacity   = m_opacitySlider->value() / 100.0;
    s.alwaysOnTop     = m_chkAlwaysOnTop->isChecked();
    s.startMinimized  = m_chkStartMinimized->isChecked();
    s.audioFeedback   = m_chkAudio->isChecked();
    return s;
}
