#include "graph.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <queue>
#include <algorithm>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QPen>
#include <QBrush>
#include <QFont>

bool Graph::loadFromFile(const QString &filename){
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    in >> N;
    if (N <= 0) return false;

    vertices.clear(); vertices.resize(N);
    adjMatrix.assign(N, std::vector<double>(N, 0.0));
    edges.clear();

    int M;
    in >> M;
    for (int i = 0; i < M; ++i) {
        int u, v;
        in >> u >> v;
        edges.push_back({u, v, 0.0});
    }
    return true;
}

void Graph::updateWeights(){
    for (auto &e : edges) {
        QPointF p1 = vertices[e.u];
        QPointF p2 = vertices[e.v];
        double dx = p1.x() - p2.x();
        double dy = p1.y() - p2.y();
        e.w = std::sqrt(dx*dx + dy*dy);
        adjMatrix[e.u][e.v] = e.w;
        adjMatrix[e.v][e.u] = e.w;
    }
}

void Graph::saveMatrix(const QString &filename) const{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            out << QString::number(adjMatrix[i][j], 'f', 2) << "\t";
        }
        out << "\n";
    }
}

QVector<int> Graph::shortestPath(int src, int dst) const{
    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> dist(N, INF);
    std::vector<int> prev(N, -1);
    std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> pq;

    dist[src] = 0.0;
    pq.push({0.0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        for (int v = 0; v < N; ++v) {
            double w = adjMatrix[u][v];
            if (w == 0.0 && u != v) continue;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    QVector<int> path;
    if (dist[dst] == INF) return path;
    for (int v = dst; v != -1; v = prev[v]) {
        path.push_back(v);
        if (v == src) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void Graph::drawGraph(QGraphicsScene *scene,
                      QVector<QGraphicsEllipseItem*> &vertexItems,
                      QVector<QGraphicsTextItem*> &labelItems,
                      QVector<QGraphicsLineItem*> &edgeItems,
                      QVector<QGraphicsTextItem*> &weightItems,
                      double vertexRadius,
                      const QColor &vertexColor,
                      const QColor &edgeColor){
    for (const auto &e : edges) {
        QPointF p1 = vertices[e.u];
        QPointF p2 = vertices[e.v];
        QGraphicsLineItem *line = new QGraphicsLineItem(p1.x(), p1.y(), p2.x(), p2.y());
        line->setPen(QPen(edgeColor, 2));
        line->setZValue(-1);
        scene->addItem(line);
        edgeItems.push_back(line);

        // Отрисовка текста веса (расстояния) над ребром
        QPointF mid = (p1 + p2) / 2.0;
        QGraphicsTextItem *wLabel = new QGraphicsTextItem(QString::number(e.w, 'f', 0));
        QFont font = wLabel->font();
        font.setPointSize(8);
        wLabel->setFont(font);
        wLabel->setDefaultTextColor(Qt::darkBlue);

        QRectF rect = wLabel->boundingRect();
        wLabel->setPos(mid.x() - rect.width()/2, mid.y() - rect.height()/2);
        wLabel->setZValue(0);
        scene->addItem(wLabel);
        weightItems.push_back(wLabel);
    }

    for (int i = 0; i < N; ++i) {
        QPointF pos = vertices[i];
        QGraphicsEllipseItem *ellipse = new QGraphicsEllipseItem(
            pos.x() - vertexRadius, pos.y() - vertexRadius,
            2*vertexRadius, 2*vertexRadius);
        ellipse->setBrush(QBrush(vertexColor));
        ellipse->setPen(QPen(Qt::black, 1));
        ellipse->setData(0, i);
        scene->addItem(ellipse);
        vertexItems.push_back(ellipse);

        QGraphicsTextItem *label = new QGraphicsTextItem(QString::number(i));
        label->setAcceptedMouseButtons(Qt::NoButton);
        scene->addItem(label);
        labelItems.push_back(label);
    }
}

void Graph::highlightPath(QGraphicsScene *scene,
                          const QVector<int> &path,
                          const QVector<QGraphicsEllipseItem*> &vertexItems,
                          const QVector<QGraphicsLineItem*> &edgeItems,
                          const QColor &pathColor,
                          const QColor &baseColor){
    if (path.size() < 2) return;

    for (auto *line : edgeItems) line->setPen(QPen(Qt::black, 2));
    for (auto *ellipse : vertexItems) ellipse->setBrush(QBrush(baseColor));

    for (int v : path) {
        if (v >= 0 && v < vertexItems.size())
            vertexItems[v]->setBrush(QBrush(pathColor));
    }

    for (int i = 0; i < path.size() - 1; ++i) {
        int u = path[i], v = path[i+1];
        QPointF pU = vertices[u];
        QPointF pV = vertices[v];
        for (auto *line : edgeItems) {
            QLineF l = line->line();
            if ((l.p1() == pU && l.p2() == pV) || (l.p1() == pV && l.p2() == pU)) {
                line->setPen(QPen(pathColor, 4));
                break;
            }
        }
    }
}

void Graph::setVertexPositions(const QVector<QPointF> &newPos){
    vertices = newPos;
    updateWeights();
}

void Graph::setVertexPosition(int index, const QPointF &pos){
    if (index < 0 || index >= vertices.size()) return;
    vertices[index] = pos;
    updateWeights();
}
