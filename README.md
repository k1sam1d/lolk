#include "AntonovCNC.h"
#include "ui_AntonovCNC.h"
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QGraphicsLineItem>
#include <cmath>
#include <QInputDialog>
#include <QLocale>
#include <QInputMethod>
#include <random>

AntonovCNC::AntonovCNC(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::AntonovCNCClass),
    scene(new QGraphicsScene(this)),
    timer(new QTimer(this)),
    programRunning(false),
    inInches(false),
    spindleSpeed(1000),
    feedRate(100),
    feedRateMultiplier(1.0),
    spindleSpeedMultiplier(1.0),
    xValueCurrent(0), yValueCurrent(0), zValueCurrent(0)
{
    ui->setupUi(this);
    ui->graphView_simul->setScene(scene);

    connect(timer, &QTimer::timeout, this, &AntonovCNC::updateTime);
    timer->start(1000);

    connect(ui->slider_spindle_speed, &QSlider::valueChanged, this, &AntonovCNC::handleSpindleSpeedChange);
    connect(ui->slider_feed_rate, &QSlider::valueChanged, this, &AntonovCNC::handleFeedRateChange);

    connect(ui->button_startkadr, &QPushButton::clicked, [this]() {
        handleStartKadr();
    });

    connect(ui->button_back, &QPushButton::clicked, [this]() {
        handleBack();
    });

    connect(ui->commandLinkButton_start, &QPushButton::clicked, [this]() {
        handleStart();
    });

    connect(ui->commandLinkButton_stop, &QPushButton::clicked, [this]() {
        handleStop();
    });

    connect(ui->commandLinkButton_reset, &QPushButton::clicked, [this]() {
        handleReset();
    });

    updateCoordinatesDisplay();
    updateProgramStatus();
    updateUnits();
}

AntonovCNC::~AntonovCNC() {
    delete ui;
}

void AntonovCNC::updateTime() {
    const QDateTime currentTime = QDateTime::currentDateTime();
    ui->label_date->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time->setText(currentTime.toString("HH:mm:ss"));
}

// Упрощенные методы для кнопок
void AntonovCNC::handleStartKadr() {
    bool ok;
    const int startRow = QInputDialog::getInt(this, tr("Start from Row"), tr("Enter starting row:"), 0, 0, loadedProgram.size() - 1, 1, &ok);
    if (ok) {
        currentProgramRow = startRow;
        programRunning = true;
        updateProgramStatus();
    }
}

void AntonovCNC::handleSpindleSpeedChange() {
    spindleSpeedMultiplier = 1.0 + (ui->slider_spindle_speed->value() / 100.0);
    ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
}

void AntonovCNC::handleReset() {
    programRunning = false;
    programProgress = 0;
    xValueCurrent = yValueCurrent = zValueCurrent = 0;
    updateCoordinatesDisplay();
    qDebug() << "Reset program";
}

void AntonovCNC::handleStart() {
    if (!programRunning && !loadedProgram.isEmpty()) {
        programRunning = true;
        qDebug() << "Program started";
    }
}

void AntonovCNC::handleStop() {
    if (programRunning) {
        programRunning = false;
        qDebug() << "Program stopped";
    }
}

void AntonovCNC::updateCoordinatesDisplay() {
    ui->label_x_value_current->setText(QString::number(xValueCurrent, 'f', 2));
    ui->label_y_value_current->setText(QString::number(yValueCurrent, 'f', 2));
    ui->label_z_value_current->setText(QString::number(zValueCurrent, 'f', 2));
}

void AntonovCNC::updateUnits() {
    if (inInches) {
        ui->label_x_axis->setText("X (in)");
        ui->label_y_axis->setText("Y (in)");
        ui->label_z_axis->setText("Z (in)");
    } else {
        ui->label_x_axis->setText("X (mm)");
        ui->label_y_axis->setText("Y (mm)");
        ui->label_z_axis->setText("Z (mm)");
    }
}
