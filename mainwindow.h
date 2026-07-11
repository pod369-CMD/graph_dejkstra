#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QVector>
#include <QEvent>
#include <QTimer>
#include "graph.h"
#include <QGraphicsPixmapItem>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void initialize();
    void animateCar();

private:
    void updateLayout();
    void updateVertexPositions();
    void redrawGraph();
    void recomputeAndHighlightPath();

    Ui::MainWindow *ui;
    Graph graph;
    QGraphicsScene *scene;

    QVector<QGraphicsEllipseItem*> vertexItems;
    QVector<QGraphicsTextItem*> labelItems;
    QVector<QGraphicsLineItem*> edgeItems;
    QVector<QGraphicsTextItem*> weightItems;

    bool isDragging = false;
    bool didDrag = false;
    int draggedVertex = -1;
    QPointF dragStartPos;
    QGraphicsEllipseItem *draggedItem = nullptr;

    int startVertex = -1;
    int endVertex = -1;

    double vertexRadius = 15.0;
    double bigCircleRadius = 200.0;
    bool nodesInitialized = false;
    double lastBigRadius = 1.0;

    QTimer *carTimer = nullptr;
    QGraphicsPixmapItem *carItem = nullptr; // ТЕПЕРЬ ЭТО КАРТИНКА (PIXMAP)
    QVector<int> carPathNodes;
    int currentSegment = 0;
    double progress = 0.0;
    double carSpeed = 6.0; // Чуть увеличил скорость
};

#endif // MAINWINDOW_H
