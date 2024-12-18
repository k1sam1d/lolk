#ifndef ANTONOVCNC_H
#define ANTONOVCNC_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QTimer>
#include <QLabel>
#include <QSet>
#include <QGraphicsScene>
#include <stack>
#include <QTableWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class AntonovCNCClass; }
QT_END_NAMESPACE

class AntonovCNC : public QMainWindow {
    Q_OBJECT

public:
    AntonovCNC(QWidget* parent = nullptr);
    ~AntonovCNC();

private slots:
    void updateTime();
    void handleNumeration();
    void handleSmenaEkrana();
    void handleMmDyum();
    void handleChangeSK();
    void handlePriv();
    void handleStartKadr();
    void handleKorrekt();
    void handleSmesh();
    void handleBackK2();
    void moveSelection(int rowOffset, int colOffset, QTableWidget* table);
    void handleMoveUp();
    void handleMoveDown();
    void handleMoveLeft();
    void handleMoveRight();
    void handleSelectCell();
    void handleConfirm();
    void handleCancel();
    void handleBack();
    void handleStart();
    void keyPressEvent(QKeyEvent* event);
    void updateLanguageLabel();
    void handleStop();
    void handleReset();
    void handleResetAlarm();
    void loadProgram();
    void handleSpindleSpeedChange();
    void handleFeedRateChange();
    void updateProgressBar();
    void updateProgramStatus();
    void updateUnits();
    void updateInfoLine(const QString& message);

private:
    Ui::AntonovCNCClass* ui;
    QTimer* timer;
    QGraphicsScene* scene;
    std::stack<int> pageHistory;

    int feedUnitIndex;
    int programProgress;
    int currentRow = 0;
    int currentColumn = 0;
    bool programRunning;
    bool inInches;
    int currentProgramRow;
    int spindleSpeed;
    int feedRate;
    double feedRateMultiplier;
    double spindleSpeedMultiplier;
    double xValueCurrent;
    double yValueCurrent;
    double zValueCurrent;
    double xValueFinal;
    double yValueFinal;
    double zValueFinal;
    QStringList loadedProgram;

    QString coordinateSystem;
    QString feedUnit;
    QString interfaceLanguage;
    QString currentSK;
    QString previousValue;
    bool startFromRowEnabled;

    void switchToCorrectionScreen();
    void switchToMainScreen();
    void handleCorrectionUp();
    void handleCorrectionDown();
    void handleCorrectionSelect();
    void handleCorrectionConfirm();
    void handleCorrectionCancel();
    void updateStatusBar(int value);
    void highlightCurrentProgramRow(int row);
    void analyzeProgram();
    void extractCoordinatesAndSpeed(const QString& line, bool isNext = false);
    void drawArc(double xStart, double yStart, double xEnd, double yEnd, double radius, bool clockwise);
    void parseAndDrawTrajectory(const QString& line);
    void resetCoordinateOffsets();
    void updateCoordinatesDisplay();
    void extractNextCoordinates();
    void displayActiveFunctions();
    bool analyzeProgramForErrors();
    void changeCoordinateSystem();
    void changeFeedUnit();
    void switchToSecondScreen();
    void syncLanguage();
};

#endif // ANTONOVCNC_H