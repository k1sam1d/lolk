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
#include <QGraphicsEllipseItem>
#include <cmath>
#include <random>
#include <QInputDialog>
#include <QLocale>
#include <QInputMethod>
#include <QDebug>
#include <stack>

AntonovCNC::AntonovCNC(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::AntonovCNCClass),
    programProgress(0),
    programRunning(false),
    inInches(false),
    currentProgramRow(0),
    spindleSpeed(1000),
    feedRate(100),
    feedRateMultiplier(1.0),
    spindleSpeedMultiplier(1.0),
    xValueCurrent(0),
    yValueCurrent(0),
    zValueCurrent(0),
    xValueFinal(10),
    yValueFinal(10),
    zValueFinal(10),
    coordinateSystem("СКС"),
    interfaceLanguage("Русский"),
    startFromRowEnabled(false),
    scene(new QGraphicsScene(this))
{
    ui->setupUi(this);
    ui->graphView_simul->setScene(scene);

    connect(ui->slider_spindle_speed, &QSlider::valueChanged, this, &AntonovCNC::handleSpindleSpeedChange);
    connect(ui->slider_feed_rate, &QSlider::valueChanged, this, &AntonovCNC::handleFeedRateChange);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AntonovCNC::updateTime);
    timer->start(1000);

    connect(ui->button_numeration, &QPushButton::clicked, this, &AntonovCNC::handleNumeration);
    connect(ui->button_smenaekrana, &QPushButton::clicked, this, &AntonovCNC::handleSmenaEkrana);
    connect(ui->button_mmdyum, &QPushButton::clicked, this, &AntonovCNC::handleMmDyum);
    connect(ui->button_changesk, &QPushButton::clicked, this, &AntonovCNC::handleChangeSK);
    connect(ui->button_priv, &QPushButton::clicked, this, &AntonovCNC::handlePriv);
    connect(ui->button_startkadr, &QPushButton::clicked, this, &AntonovCNC::handleStartKadr);
    connect(ui->button_korrekt, &QPushButton::clicked, this, &AntonovCNC::handleKorrekt);
    connect(ui->button_smesh, &QPushButton::clicked, this, &AntonovCNC::handleSmesh);
    connect(ui->button_selectprog, &QPushButton::clicked, this, &AntonovCNC::loadProgram);
    connect(ui->button_back, &QPushButton::clicked, this, &AntonovCNC::handleBack);
    connect(ui->commandLinkButton_stop, &QPushButton::clicked, this, &AntonovCNC::handleStop);
    connect(ui->commandLinkButton_start, &QPushButton::clicked, this, &AntonovCNC::handleStart);
    connect(ui->commandLinkButton_reset, &QPushButton::clicked, this, &AntonovCNC::handleReset);
    connect(ui->commandLinkButton_resetalarms, &QPushButton::clicked, this, &AntonovCNC::handleResetAlarm);
    connect(ui->button_korrekt, &QPushButton::clicked, this, &AntonovCNC::switchToCorrectionScreen);
    connect(ui->button_backk2, &QPushButton::clicked, this, &AntonovCNC::handleBackK2);
    connect(ui->button_upwiget2, &QPushButton::clicked, this, &AntonovCNC::handleCorrectionUp);
    connect(ui->button_downwiget2, &QPushButton::clicked, this, &AntonovCNC::handleCorrectionDown);
    connect(ui->button_leftwiget2, &QPushButton::clicked, this, &AntonovCNC::handleMoveLeft);
    connect(ui->button_rightwiget2, &QPushButton::clicked, this, &AntonovCNC::handleMoveRight);
    connect(ui->button_wiborwiget2, &QPushButton::clicked, this, &AntonovCNC::handleCorrectionSelect);
    connect(ui->button_podtverkorrert, &QPushButton::clicked, this, &AntonovCNC::handleCorrectionConfirm);
    connect(ui->button_otmenakorrert, &QPushButton::clicked, this, &AntonovCNC::handleCorrectionCancel);


    updateCoordinatesDisplay();
    updateUnits();
    updateProgramStatus();
}

AntonovCNC::~AntonovCNC() {
    delete ui;
}

void AntonovCNC::updateTime() {
    QDateTime currentTime = QDateTime::currentDateTime();
    ui->label_date->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time->setText(currentTime.toString("HH:mm:ss"));
    ui->label_date_2->setText(currentTime.toString("dd.MM.yyyy"));
    ui->label_time_2->setText(currentTime.toString("HH:mm:ss"));
}

void AntonovCNC::handleResetAlarm() {
    QMessageBox::information(this, tr("Reset Alarm"), tr("Все ошибки сброшены."));
    qDebug() << "Сигналы тревоги сброшены";
}

void AntonovCNC::handleNumeration() {
    static bool numbered = false;
    for (int i = 0; i < ui->listWidget_program->count(); ++i) {
        QListWidgetItem* item = ui->listWidget_program->item(i);
        if (!numbered) {
            item->setText(QString::number(i + 1) + ": " + item->text());
        }
        else {
            QStringList parts = item->text().split(": ");
            if (parts.size() > 1) {
                item->setText(parts[1]);
            }
        }
    }
    numbered = !numbered;
}

void AntonovCNC::handleSmenaEkrana() {
    int currentIndex = ui->stackedWidget_winchan->currentIndex();
    ui->stackedWidget_winchan->setCurrentIndex(currentIndex == 0 ? 1 : 0);
}

void AntonovCNC::handleMmDyum() {
    inInches = !inInches;
    updateUnits();
    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);
    }
}

void AntonovCNC::handlePriv() {
    if (coordinateSystem == "СКС") {
        coordinateSystem = "СКД";
    }
    else {
        coordinateSystem = "СКС";
    }
    updateInfoLine(tr("Смена системы координат: %1").arg(coordinateSystem));
    ui->label_sksskd->setText(coordinateSystem);
}

void AntonovCNC::handleChangeSK() {
    static QStringList feedUnits = { "мм/об", "мм/мин", "мм/зубр" };
    feedUnitIndex = (feedUnitIndex + 1) % feedUnits.size();
    ui->label_changessk->setText(feedUnits[feedUnitIndex]);
}

void AntonovCNC::handleStartKadr() {
    bool ok;
    int startRow = QInputDialog::getInt(this, tr("Start from Row"), tr("Enter starting row:"), 0, 0, loadedProgram.size() - 1, 1, &ok);
    if (ok) {
        currentProgramRow = startRow;
        startFromRowEnabled = true;
        programRunning = true;
        programProgress = 0;
        updateProgramStatus();
        updateProgressBar();
        ui->label_rejim_isp->setText("AUTO");
        ui->label_rejim_isp_4->setText("AUTO");
    }
}


void AntonovCNC::handleKorrekt() {
    ui->stackedWidget_3->setCurrentWidget(ui->page_7);
    pageHistory.push(2); 
}

void AntonovCNC::handleSmesh() {
    ui->stackedWidget->setCurrentWidget(ui->page_4);
    pageHistory = {};
    pageHistory.push(0);
    pageHistory.push(1);
}

void AntonovCNC::handleBackK2() {
    if (pageHistory.size() > 1) {
        pageHistory.pop();
        int previousPage = pageHistory.top();

        if (previousPage == 1) {
            ui->stackedWidget_3->setCurrentWidget(ui->page_8);
        }
        else if (previousPage == 0) {
            ui->stackedWidget->setCurrentWidget(ui->page_3);
        }
    }
    else {
        ui->stackedWidget->setCurrentWidget(ui->page_3);
    }
}

void AntonovCNC::moveSelection(int rowOffset, int colOffset, QTableWidget* table) {
    int rowCount = table->rowCount();
    int colCount = table->columnCount();

    int newRow = currentRow + rowOffset;
    int newCol = currentColumn + colOffset;

    if (newRow >= 0 && newRow < rowCount && newCol >= 0 && newCol < colCount) {
        currentRow = newRow;
        currentColumn = newCol;
        table->setCurrentCell(currentRow, currentColumn);
    }
}

void AntonovCNC::handleMoveUp() {
    int row = ui->tableWidget->currentRow();
    if (row > 0) {
        ui->tableWidget->setCurrentCell(row - 1, ui->tableWidget->currentColumn());
    }
}

void AntonovCNC::handleMoveDown() {
    int row = ui->tableWidget->currentRow();
    if (row < ui->tableWidget->rowCount() - 1) {
        ui->tableWidget->setCurrentCell(row + 1, ui->tableWidget->currentColumn());
    }
}

void AntonovCNC::handleMoveLeft() {
    int col = ui->tableWidget->currentColumn();
    if (col > 0) {
        ui->tableWidget->setCurrentCell(ui->tableWidget->currentRow(), col - 1);
    }
}

void AntonovCNC::handleMoveRight() {
    int col = ui->tableWidget->currentColumn();
    if (col < ui->tableWidget->columnCount() - 1) {
        ui->tableWidget->setCurrentCell(ui->tableWidget->currentRow(), col + 1);
    }
}

void AntonovCNC::handleSelectCell() {
    QTableWidgetItem* currentItem = ui->tableWidget->currentItem();
    if (currentItem) {
        previousValue = currentItem->text();
        bool ok;
        QString newValue = QInputDialog::getText(this, tr("Edit Cell"), tr("Enter new value:"), QLineEdit::Normal, currentItem->text(), &ok);
        if (ok) {
            currentItem->setText(newValue);
        }
    }
}

void AntonovCNC::handleConfirm() {
    QMessageBox::information(this, tr("Confirm Changes"), tr("Changes have been saved."));
}

void AntonovCNC::handleCancel() {
    QTableWidgetItem* currentItem = ui->tableWidget->currentItem();
    if (currentItem) {
        currentItem->setText(previousValue);
    }
    QMessageBox::information(this, tr("Cancel Changes"), tr("Changes have been canceled."));
}

void AntonovCNC::handleBack()
{
    qDebug() << "handleBack вызвана";
}

void AntonovCNC::handleStart() {
    if (!programRunning && !loadedProgram.isEmpty()) {
        programRunning = true;
        programProgress = 0;
        updateProgramStatus();
        updateProgressBar();
        ui->label_rejim_isp->setText("AUTO");
        ui->label_rejim_isp_4->setText("AUTO");
    }
}


void AntonovCNC::keyPressEvent(QKeyEvent* event) {
    updateLanguageLabel();

    QMainWindow::keyPressEvent(event);
}

void AntonovCNC::updateLanguageLabel() {
    QInputMethod* inputMethod = QGuiApplication::inputMethod();
    QLocale currentLocale = inputMethod->locale();

    if (currentLocale.language() == QLocale::Russian) {
        ui->label_language->setText("RU");
    }
    else if (currentLocale.language() == QLocale::English) {
        ui->label_language->setText("EN");
    }
    else {
        ui->label_language->setText(currentLocale.nativeLanguageName());
    }

    qDebug() << "Current input language:" << currentLocale.nativeLanguageName();
}

void AntonovCNC::handleStop() {
    if (programRunning) {
        programRunning = false;
        ui->label_rejim_isp->setText("PROG");
        ui->label_rejim_isp_4->setText("PROG");
        qDebug() << "Программа остановлена";
    }
}


void AntonovCNC::updateProgramStatus() {
    if (loadedProgram.isEmpty()) {
        ui->label_sostoyanine->setText("WAIT FOR LOADING");
        ui->label_sostoyanine_4->setText("WAIT FOR LOADING");
    }
    else {
        bool errors = analyzeProgramForErrors();
        if (errors) {
            ui->label_sostoyanine->setText("UNABLE TO START");
            ui->label_sostoyanine_4->setText("UNABLE TO START");
        }
        else {
            ui->label_sostoyanine->setText("READY TO START");
            ui->label_sostoyanine_4->setText("READY TO START");
        }
    }
}

bool AntonovCNC::analyzeProgramForErrors() {
    for (const QString& line : loadedProgram) {
        if (line.contains("G") || line.contains("M") || line.contains(QRegularExpression("N\\d+"))) {
            ui->label_error->setText("Ошибок нет");
            ui->label_error_4->setText("Ошибок нет");
            return false;
        }
    }
    ui->label_error->setText("Найдены ошибки, проверьте исполняющую программу");
    ui->label_error_4->setText("Найдены ошибки, проверьте исполняющую программу");
    return true;
}


void AntonovCNC::loadProgram() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Выбрать программу"), "", tr("Файлы программы (*.txt *.nc);;Все файлы (*.*)"));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            loadedProgram.clear();
            ui->listWidget_program->clear();
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                loadedProgram.append(line);
                ui->listWidget_program->addItem(line);
            }
            updateInfoLine("Программа загружена успешно");
            analyzeProgram();
            updateProgramStatus();
        }
    }
}

void AntonovCNC::updateInfoLine(const QString& message) {
    ui->info_line->setText(message);
}

void AntonovCNC::handleReset() {
    programProgress = 0;
    currentProgramRow = 0;
    updateStatusBar(0);
    programRunning = false;

    xValueCurrent = 0;
    yValueCurrent = 0;
    zValueCurrent = 0;
    xValueFinal = 10;
    yValueFinal = 10;
    zValueFinal = 10;

    updateCoordinatesDisplay();

    if (ui->listWidget_program->count() > 0) {
        QListWidgetItem* item = ui->listWidget_program->item(0);
        ui->listWidget_program->setCurrentItem(item);
    }
}

void AntonovCNC::updateProgressBar() {
    if (programRunning && currentProgramRow < loadedProgram.size()) {
        int progressPercentage = static_cast<int>((static_cast<double>(currentProgramRow) / loadedProgram.size()) * 100);
        updateStatusBar(progressPercentage);
        highlightCurrentProgramRow(currentProgramRow);
        currentProgramRow++;
        QTimer::singleShot(500, this, &AntonovCNC::updateProgressBar);
    }
    else {
        programRunning = false;
    }
}

void AntonovCNC::updateStatusBar(int value) {
    ui->progressBar_runtime->setValue(value);
    ui->progressBar_runtime_2->setValue(value);
}

void AntonovCNC::highlightCurrentProgramRow(int row) {
    if (row < ui->listWidget_program->count()) {
        QListWidgetItem* item = ui->listWidget_program->item(row);
        ui->listWidget_program->setCurrentItem(item);
        extractCoordinatesAndSpeed(loadedProgram[row]);
        extractNextCoordinates();
        parseAndDrawTrajectory(loadedProgram[row]);
    }
}

void AntonovCNC::handleSpindleSpeedChange() {
    spindleSpeedMultiplier = 1.0 + (ui->slider_spindle_speed->value() / 100.0);
    ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
}

void AntonovCNC::handleFeedRateChange() {
    feedRateMultiplier = 1.0 + (ui->slider_feed_rate->value() / 100.0);
    ui->label_feed_value->setText(QString::number(feedRate * feedRateMultiplier));
}

void AntonovCNC::analyzeProgram() {
    scene->clear();
    xValueCurrent = 0;
    yValueCurrent = 0;
    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);
        parseAndDrawTrajectory(line);
    }
    displayActiveFunctions();
}

void AntonovCNC::extractCoordinatesAndSpeed(const QString& line, bool isNext) {
    QRegularExpression regexX(R"(X(-?\d+\.?\d*))");
    QRegularExpression regexY(R"(Y(-?\d+\.?\d*))");
    QRegularExpression regexZ(R"(Z(-?\d+\.?\d*))");
    QRegularExpression regexF(R"(F(\d+))");
    QRegularExpression regexS(R"(S(\d+))");

    QRegularExpressionMatch matchX = regexX.match(line);
    if (matchX.hasMatch()) {
        double value = matchX.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) xValueFinal = value; else xValueCurrent = value;
    }

    QRegularExpressionMatch matchY = regexY.match(line);
    if (matchY.hasMatch()) {
        double value = matchY.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) yValueFinal = value; else yValueCurrent = value;
    }

    QRegularExpressionMatch matchZ = regexZ.match(line);
    if (matchZ.hasMatch()) {
        double value = matchZ.captured(1).toDouble();
        if (inInches) value *= 25.4;
        if (isNext) zValueFinal = value; else zValueCurrent = value;
    }

    QRegularExpressionMatch matchF = regexF.match(line);
    if (matchF.hasMatch()) {
        feedRate = matchF.captured(1).toInt();
        ui->label_feed_value->setText(QString::number(feedRate * feedRateMultiplier));
    }

    QRegularExpressionMatch matchS = regexS.match(line);
    if (matchS.hasMatch()) {
        spindleSpeed = matchS.captured(1).toInt();
        ui->label_spindle_value->setText(QString::number(spindleSpeed * spindleSpeedMultiplier));
    }

    updateCoordinatesDisplay();
}

void AntonovCNC::switchToCorrectionScreen() {
    ui->stackedWidget->setCurrentIndex(1);
}

void AntonovCNC::switchToMainScreen() {
    ui->stackedWidget->setCurrentIndex(0);
}

void AntonovCNC::handleCorrectionUp() {
    int currentRow = ui->listWidget_program->currentRow();
    if (currentRow > 0) {
        ui->listWidget_program->setCurrentRow(currentRow - 1);
    }
}

void AntonovCNC::handleCorrectionDown() {
    int currentRow = ui->listWidget_program->currentRow();
    if (currentRow < ui->listWidget_program->count() - 1) {
        ui->listWidget_program->setCurrentRow(currentRow + 1);
    }
}

void AntonovCNC::handleCorrectionSelect() {
    QListWidgetItem* currentItem = ui->listWidget_program->currentItem();
    if (currentItem) {
        bool ok;
        double newValue = QInputDialog::getDouble(
            this, tr("Edit Parameter"),
            tr("Enter new value:"), currentItem->text().toDouble(), -99999, 99999, 2, &ok
        );
        if (ok) {
            currentItem->setText(QString::number(newValue, 'f', 2));
        }
    }
}

void AntonovCNC::handleCorrectionConfirm() {
    QMessageBox::information(this, tr("Confirm Changes"), tr("Changes have been saved."));
    switchToMainScreen();
}

void AntonovCNC::handleCorrectionCancel() {
    QMessageBox::warning(this, tr("Cancel Changes"), tr("Changes have been discarded."));
    switchToMainScreen();
}


void AntonovCNC::parseAndDrawTrajectory(const QString& line) {
    QRegularExpression regexG0(R"(G0\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*))");
    QRegularExpression regexG1(R"(G1\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*))");
    QRegularExpression regexG2(R"(G2\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*)\s*R(-?\d+\.?\d*))");
    QRegularExpression regexG3(R"(G3\s*X(-?\d+\.?\d*)\s*Y(-?\d+\.?\d*)\s*R(-?\d+\.?\d*))");

    QRegularExpressionMatch matchG0 = regexG0.match(line);
    QRegularExpressionMatch matchG1 = regexG1.match(line);
    QRegularExpressionMatch matchG2 = regexG2.match(line);
    QRegularExpressionMatch matchG3 = regexG3.match(line);

    double x = xValueCurrent, y = yValueCurrent, radius = 0;

    if (matchG0.hasMatch()) {
        x = matchG0.captured(1).toDouble();
        y = matchG0.captured(2).toDouble();
    }
    else if (matchG1.hasMatch()) {
        x = matchG1.captured(1).toDouble();
        y = matchG1.captured(2).toDouble();
    }
    else if (matchG2.hasMatch() || matchG3.hasMatch()) {
        x = matchG2.hasMatch() ? matchG2.captured(1).toDouble() : matchG3.captured(1).toDouble();
        y = matchG2.hasMatch() ? matchG2.captured(2).toDouble() : matchG3.captured(2).toDouble();
        radius = matchG2.hasMatch() ? matchG2.captured(3).toDouble() : matchG3.captured(3).toDouble();

        drawArc(xValueCurrent, yValueCurrent, x, y, radius, matchG2.hasMatch());
        xValueCurrent = x;
        yValueCurrent = y;
        return;
    }

    if (inInches) {
        x *= 25.4;
        y *= 25.4;
    }

    if (matchG0.hasMatch() || matchG1.hasMatch()) {
        scene->addLine(xValueCurrent, -yValueCurrent, x, -y, QPen(Qt::red));
        xValueCurrent = x;
        yValueCurrent = y;
    }
}

void AntonovCNC::drawArc(double xStart, double yStart, double xEnd, double yEnd, double radius, bool clockwise) {
    double centerX = (xStart + xEnd) / 2;
    double centerY = (yStart + yEnd) / 2;

    double startAngle = std::atan2(yStart - centerY, xStart - centerX) * 180 / M_PI;
    double endAngle = std::atan2(yEnd - centerY, xEnd - centerX) * 180 / M_PI;

    if (clockwise) {
        if (endAngle > startAngle) endAngle -= 360;
    }
    else {
        if (endAngle < startAngle) endAngle += 360;
    }

    scene->addEllipse(centerX - radius, -centerY - radius, radius * 2, radius * 2,
        QPen(Qt::blue), QBrush(Qt::NoBrush));
}

void AntonovCNC::updateUnits() {
    if (inInches) {
        ui->label_x_axis->setText("X (in)");
        ui->label_y_axis->setText("Y (in)");
        ui->label_z_axis->setText("Z (in)");
    }
    else {
        ui->label_x_axis->setText("X (mm)");
        ui->label_y_axis->setText("Y (mm)");
        ui->label_z_axis->setText("Z (mm)");
    }
}

void AntonovCNC::updateCoordinatesDisplay() {
    ui->label_x_value_current->setText(QString::number(xValueCurrent, 'f', 2));
    ui->label_y_value_current->setText(QString::number(yValueCurrent, 'f', 2));
    ui->label_z_value_current->setText(QString::number(zValueCurrent, 'f', 2));

    ui->label_x_value_final->setText(QString::number(xValueFinal, 'f', 2));
    ui->label_y_value_final->setText(QString::number(yValueFinal, 'f', 2));
    ui->label_z_value_final->setText(QString::number(zValueFinal, 'f', 2));
}

void AntonovCNC::extractNextCoordinates() {
    if (currentProgramRow + 1 < loadedProgram.size()) {
        QString nextLine = loadedProgram[currentProgramRow + 1];
        extractCoordinatesAndSpeed(nextLine, true);
    }
    else {
        xValueFinal = xValueCurrent;
        yValueFinal = yValueCurrent;
        zValueFinal = zValueCurrent;
    }
    updateCoordinatesDisplay();
}


void AntonovCNC::displayActiveFunctions() {
    QSet<QString> gCodes, mCodes, tCodes, dCodes;

    for (const QString& line : loadedProgram) {
        extractCoordinatesAndSpeed(line);

        QRegularExpression regexG(R"(G\d{1,3})");
        QRegularExpression regexM(R"(M\d{1,3})");
        QRegularExpression regexT(R"(T\d{1,2})");
        QRegularExpression regexD(R"(D\d{1,2})");

        QRegularExpressionMatch matchG = regexG.match(line);
        if (matchG.hasMatch()) {
            gCodes.insert(matchG.captured(0));
        }

        QRegularExpressionMatch matchM = regexM.match(line);
        if (matchM.hasMatch()) {
            mCodes.insert(matchM.captured(0));
        }

        QRegularExpressionMatch matchT = regexT.match(line);
        if (matchT.hasMatch()) {
            tCodes.insert(matchT.captured(0));
        }

        QRegularExpressionMatch matchD = regexD.match(line);
        if (matchD.hasMatch()) {
            dCodes.insert(matchD.captured(0));
        }
    }

    QList<QLabel*> gLabels = {
        ui->label_g_code, ui->label_g_code_1, ui->label_g_code_2, ui->label_g_code_3, ui->label_g_code_4,
        ui->label_g_code_5, ui->label_g_code_6, ui->label_g_code_7, ui->label_g_code_8, ui->label_g_code_9,
        ui->label_g_code_10, ui->label_g_code_11, ui->label_g_code_12, ui->label_g_code_13, ui->label_g_code_14,
        ui->label_g_code_15, ui->label_g_code_16, ui->label_g_code_17, ui->label_g_code_18, ui->label_g_code_19,
        ui->label_g_code_20, ui->label_g_code_21, ui->label_g_code_22, ui->label_g_code_23, ui->label_g_code_24,
        ui->label_g_code_25, ui->label_g_code_26, ui->label_g_code_27, ui->label_g_code_28, ui->label_g_code_29
    };
    QList<QLabel*> mLabels = {
        ui->label_m_code, ui->label_m_code_1, ui->label_m_code_2, ui->label_m_code_3,
        ui->label_m_code_4, ui->label_m_code_5, ui->label_m_code_6
    };
    QList<QLabel*> tLabels = {
        ui->label_t_code, ui->label_t_code_1, ui->label_t_code_2, ui->label_t_code_3,
        ui->label_t_code_4, ui->label_t_code_5, ui->label_t_code_6
    };
    QList<QLabel*> dLabels = {
        ui->label_d_code, ui->label_d_code_1, ui->label_d_code_2, ui->label_d_code_3, ui->label_d_code_4
    };

    auto assignRandomLabels = [](const QSet<QString>& codes, QList<QLabel*>& labels) {
        QList<int> availableIndexes;
        for (int i = 0; i < labels.size(); ++i) {
            availableIndexes.append(i);
        }
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(availableIndexes.begin(), availableIndexes.end(), g);

        int index = 0;
        for (const QString& code : codes) {
            if (index < availableIndexes.size()) {
                labels[availableIndexes[index]]->setText(code);
                index++;
            }
        }

        for (int i = index; i < labels.size(); ++i) {
            labels[availableIndexes[i]]->setText("");
        }
        };

    assignRandomLabels(gCodes, gLabels);
    assignRandomLabels(mCodes, mLabels);
    assignRandomLabels(tCodes, tLabels);
    assignRandomLabels(dCodes, dLabels);
}


