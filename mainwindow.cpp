#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsPolygonItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMessageBox>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    scene = new QGraphicsScene(this);
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setDragMode(QGraphicsView::NoDrag);

    ui->graphicsView->viewport()->installEventFilter(this);

    connect(ui->btnReset, &QPushButton::clicked, this, [this](){
        startVertex = -1;
        endVertex = -1;
        ui->lblInfo->setText("Старт: - | Финиш: -");
        recomputeAndHighlightPath();
    });

    if (!graph.loadFromFile("input.txt")) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить файл input.txt");
    }

    carTimer = new QTimer(this);
    connect(carTimer, &QTimer::timeout, this, &MainWindow::animateCar);

    QTimer::singleShot(0, this, &MainWindow::initialize);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::initialize() {
    updateLayout();
    recomputeAndHighlightPath();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateLayout();
    recomputeAndHighlightPath();
}

void MainWindow::wheelEvent(QWheelEvent *event) {
    double factor = 1.1;
    if (event->angleDelta().y() > 0) ui->graphicsView->scale(factor, factor);
    else ui->graphicsView->scale(1/factor, 1/factor);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event){
    if (watched == ui->graphicsView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPointF scenePos = ui->graphicsView->mapToScene(mouseEvent->pos());
            didDrag = false;

            QList<QGraphicsItem*> itemsAtPos = scene->items(scenePos);
            for (QGraphicsItem *item : itemsAtPos) {
                if (item->type() == QGraphicsEllipseItem::Type) {
                    QGraphicsEllipseItem *ellipse = static_cast<QGraphicsEllipseItem*>(item);

                    bool ok;
                    int idx = ellipse->data(0).toInt(&ok);
                    if (ok && idx >= 0 && idx < vertexItems.size()) {
                        isDragging = true;
                        draggedVertex = idx;
                        draggedItem = ellipse;
                        dragStartPos = scenePos;
                        return true;
                    }
                }
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            if (isDragging && draggedItem) {
                didDrag = true;
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                QPointF scenePos = ui->graphicsView->mapToScene(mouseEvent->pos());
                QPointF delta = scenePos - dragStartPos;
                QPointF newPos = graph.getVertexPositions()[draggedVertex] + delta;
                graph.setVertexPosition(draggedVertex, newPos);

                QPointF center = newPos;
                draggedItem->setRect(center.x() - vertexRadius, center.y() - vertexRadius,
                                     2*vertexRadius, 2*vertexRadius);

                if (draggedVertex < labelItems.size()) {
                    QRectF rect = labelItems[draggedVertex]->boundingRect();
                    labelItems[draggedVertex]->setPos(center.x() - rect.width()/2, center.y() - rect.height()/2);
                }

                for (auto *line : edgeItems) { scene->removeItem(line); delete line; }
                for (auto *w : weightItems) { scene->removeItem(w); delete w; }
                edgeItems.clear(); weightItems.clear();

                const auto &edges = graph.getEdges();
                for (const auto &e : edges) {
                    QPointF p1 = graph.getVertexPositions()[e.u];
                    QPointF p2 = graph.getVertexPositions()[e.v];
                    QGraphicsLineItem *line = new QGraphicsLineItem(p1.x(), p1.y(), p2.x(), p2.y());
                    line->setPen(QPen(Qt::black, 2));
                    line->setZValue(-1);
                    scene->addItem(line);
                    edgeItems.push_back(line);

                    QPointF mid = (p1 + p2) / 2.0;
                    QGraphicsTextItem *wLabel = new QGraphicsTextItem(QString::number(e.w, 'f', 0));
                    QFont font = wLabel->font(); font.setPointSize(8);
                    wLabel->setFont(font); wLabel->setDefaultTextColor(Qt::darkBlue);
                    QRectF r = wLabel->boundingRect();
                    wLabel->setPos(mid.x() - r.width()/2, mid.y() - r.height()/2);
                    wLabel->setZValue(0);
                    scene->addItem(wLabel);
                    weightItems.push_back(wLabel);
                }

                dragStartPos = scenePos;
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            if (isDragging) {
                isDragging = false;
                if (!didDrag) {
                    if (startVertex == -1 || (startVertex != -1 && endVertex != -1)) {
                        startVertex = draggedVertex; endVertex = -1;
                    } else if (startVertex != draggedVertex) {
                        endVertex = draggedVertex;
                    }
                    ui->lblInfo->setText(QString("Старт: %1 | Финиш: %2")
                                             .arg(startVertex != -1 ? QString::number(startVertex) : "-")
                                             .arg(endVertex != -1 ? QString::number(endVertex) : "-"));
                }
                draggedItem = nullptr;
                recomputeAndHighlightPath();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateLayout(){
    if (!ui->graphicsView) return;

    // 1. Принудительно отключаем ползунки прокрутки (именно они вызывают обрезку!)
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QSize size = ui->graphicsView->viewport()->size();
    double w = size.width();
    double h = size.height();
    if (w < 10 || h < 10) return;

    // 2. Центрируем сцену строго по размеру окна
    scene->setSceneRect(-w/2, -h/2, w, h);

    // 3. Вычисляем радиус большого круга, оставляя "поля" в 40 пикселей,
    // чтобы крайние вершины не обрезались при сужении окна
    bigCircleRadius = std::min(w, h) * 0.45 - 40;
    if (bigCircleRadius < 10) bigCircleRadius = 10; // Защита от отрицательного радиуса

    int N = graph.vertexCount();
    if (N > 0) {
        double baseRadius = std::min(w, h) * 0.035;
        vertexRadius = baseRadius / std::sqrt((double)N);
        if (vertexRadius < 8) vertexRadius = 8;
        if (vertexRadius > 30) vertexRadius = 30;
    } else {
        vertexRadius = 12;
    }

    updateVertexPositions();
    graph.updateWeights();
    redrawGraph();
}

void MainWindow::updateVertexPositions(){
    int N = graph.vertexCount();
    if (N == 0) return;

    if (!nodesInitialized) {
        QVector<QPointF> newPos(N);
        for (int i = 0; i < N; ++i) {
            double angle = 2 * M_PI * i / N - M_PI/2;
            // Центр всегда (0,0)
            newPos[i] = QPointF(bigCircleRadius * std::cos(angle), bigCircleRadius * std::sin(angle));
        }
        graph.setVertexPositions(newPos);
        nodesInitialized = true;
        lastBigRadius = bigCircleRadius;
    } else {
        // Пропорциональное сжатие относительно центра (0,0)
        double scaleRatio = bigCircleRadius / (lastBigRadius > 0 ? lastBigRadius : 1.0);
        QVector<QPointF> currentPos = graph.getVertexPositions();
        QVector<QPointF> newPos(N);
        for (int i = 0; i < N; ++i) {
            newPos[i] = QPointF(currentPos[i].x() * scaleRatio, currentPos[i].y() * scaleRatio);
        }
        graph.setVertexPositions(newPos);
        lastBigRadius = bigCircleRadius;
    }
}

void MainWindow::redrawGraph(){
    if (carItem) { scene->removeItem(carItem); delete carItem; carItem = nullptr; }
    for (auto *item : vertexItems) { scene->removeItem(item); delete item; }
    for (auto *item : labelItems) { scene->removeItem(item); delete item; }
    for (auto *item : edgeItems) { scene->removeItem(item); delete item; }
    for (auto *item : weightItems) { scene->removeItem(item); delete item; }
    vertexItems.clear(); labelItems.clear(); edgeItems.clear(); weightItems.clear();

    if (graph.vertexCount() == 0) return;

    graph.drawGraph(scene, vertexItems, labelItems, edgeItems, weightItems, vertexRadius, Qt::yellow, Qt::black);

    for (int i = 0; i < labelItems.size(); ++i) {
        QPointF pos = graph.getVertexPositions()[i];
        QRectF rect = labelItems[i]->boundingRect();
        labelItems[i]->setPos(pos.x() - rect.width()/2, pos.y() - rect.height()/2);
    }
}

void MainWindow::recomputeAndHighlightPath(){
    if (graph.vertexCount() < 2) return;

    redrawGraph();

    if (startVertex != -1 && startVertex < vertexItems.size()) {
        vertexItems[startVertex]->setBrush(QBrush(Qt::green));
    }
    if (endVertex != -1 && endVertex < vertexItems.size()) {
        vertexItems[endVertex]->setBrush(QBrush(Qt::cyan));
    }

    if (startVertex == -1 || endVertex == -1) {
        carTimer->stop();
        if (carItem) carItem->hide();
        return;
    }

    QVector<int> path = graph.shortestPath(startVertex, endVertex);

    if (path.isEmpty()) {
        carTimer->stop();
        if (carItem) carItem->hide();
        return;
    }

    graph.highlightPath(scene, path, vertexItems, edgeItems, Qt::red, Qt::yellow);

    vertexItems[startVertex]->setBrush(QBrush(Qt::green));
    vertexItems[endVertex]->setBrush(QBrush(Qt::cyan));

    graph.saveMatrix("matrix.txt");

    carPathNodes = path;
    carPathNodes = path;
    if (!carItem) {
        // ЗАГРУЖАЕМ НАСТОЯЩУЮ КАРТИНКУ
        QPixmap pix("car.png"); // Файл должен лежать рядом с .exe

        // --- ДОБАВЬ ЭТУ СТРОЧКУ ПРЯМО СЮДА ---
        pix.setMask(pix.createMaskFromColor(Qt::white));
        // -------------------------------------

        if(pix.isNull()) {
            qDebug() << "ОШИБКА: Картинка car.png не найдена в папке сборки!";
        }

        carItem = new QGraphicsPixmapItem(pix);
        // Сдвигаем центр картинки (чтобы машинка крутилась вокруг своего центра)
        carItem->setOffset(-pix.width() / 2.0, -pix.height() / 2.0);
        carItem->setZValue(10);
        scene->addItem(carItem);
    }
        currentSegment = 0;
        progress = 0.0;
        carItem->setPos(graph.getVertexPositions()[carPathNodes[0]]);

        // Масштабируем картинку так, чтобы она была чуть больше вершины
        double targetWidth = vertexRadius * 2.5;
        if (!carItem->pixmap().isNull()) {
            double scaleF = targetWidth / carItem->pixmap().width();
            carItem->setScale(scaleF);
        }

        carItem->show();
        carTimer->start(30);
    }

void MainWindow::animateCar(){
    if (carPathNodes.size() < 2 || !carItem) return;

    if (currentSegment >= carPathNodes.size() - 1) {
        currentSegment = 0;
        progress = 0.0;
    }

    QPointF p1 = graph.getVertexPositions()[carPathNodes[currentSegment]];
    QPointF p2 = graph.getVertexPositions()[carPathNodes[currentSegment + 1]];

    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double dist = std::sqrt(dx*dx + dy*dy);

    if (dist > 0) progress += carSpeed / dist;
    else progress = 1.0;

    if (progress >= 1.0) {
        currentSegment++;
        progress = 0.0;
        if (currentSegment >= carPathNodes.size() - 1) {
            currentSegment = 0;
            p1 = graph.getVertexPositions()[carPathNodes[currentSegment]];
            p2 = graph.getVertexPositions()[carPathNodes[currentSegment + 1]];
        }
    }

    // Линейная интерполяция позиции
    double currentX = p1.x() + (p2.x() - p1.x()) * progress;
    double currentY = p1.y() + (p2.y() - p1.y()) * progress;
    carItem->setPos(currentX, currentY);

    // ПОВОРОТ МАШИНКИ ПО НАПРАВЛЕНИЮ ДВИЖЕНИЯ
    if (dist > 0) {
        double angle = std::atan2(dy, dx) * 180.0 / M_PI;
        // Добавляем 90 градусов, чтобы машинка ехала капотом вперед.
        // Если она едет задом, поменяй 90 на -90 или 270.
        carItem->setRotation(angle + 90);
    }

    // ИСПРАВЛЕННЫЙ МАСШТАБ МАШИНКИ (зависит от реальной картинки)
    if (!carItem->pixmap().isNull()) {
        double targetWidth = vertexRadius * 2.5; // Желаемая ширина машинки на экране
        carItem->setScale(targetWidth / carItem->pixmap().width());
    }
}
