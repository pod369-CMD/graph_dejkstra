#ifndef GRAPH_H
#define GRAPH_H

#include <QPointF>
#include <QVector>
#include <QColor>
#include <vector>
#include <utility>
#include <limits>

class QGraphicsScene;
class QGraphicsEllipseItem;
class QGraphicsLineItem;
class QGraphicsTextItem;

class Graph
{
public:
    Graph() = default;
    bool loadFromFile(const QString &filename);

    void updateWeights();
    void saveMatrix(const QString &filename) const;

    QVector<int> shortestPath(int src, int dst) const;

    void drawGraph(QGraphicsScene *scene,
                   QVector<QGraphicsEllipseItem*> &vertexItems,
                   QVector<QGraphicsTextItem*> &labelItems,
                   QVector<QGraphicsLineItem*> &edgeItems,
                   QVector<QGraphicsTextItem*> &weightItems,
                   double vertexRadius,
                   const QColor &vertexColor = Qt::yellow,
                   const QColor &edgeColor = Qt::black);

    void highlightPath(QGraphicsScene *scene,
                       const QVector<int> &path,
                       const QVector<QGraphicsEllipseItem*> &vertexItems,
                       const QVector<QGraphicsLineItem*> &edgeItems,
                       const QColor &pathColor = Qt::red,
                       const QColor &baseColor = Qt::yellow);

    void setVertexPositions(const QVector<QPointF> &newPos);
    void setVertexPosition(int index, const QPointF &pos);

    int vertexCount() const { return N; }
    const QVector<QPointF>& getVertexPositions() const { return vertices; }
    const std::vector<std::vector<double>>& getAdjMatrix() const { return adjMatrix; }

    struct Edge { int u, v; double w; };
    const QVector<Edge>& getEdges() const { return edges; }

private:
    int N = 0;
    QVector<QPointF> vertices;
    std::vector<std::vector<double>> adjMatrix;
    QVector<Edge> edges;
};

#endif // GRAPH_H
