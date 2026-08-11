#ifndef WELLVISUALIZATION_H
#define WELLVISUALIZATION_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include "wellconstruction.h"
#include "circulation.h"

class WellVisualization : public QWidget
{
    Q_OBJECT

public:
    explicit WellVisualization(QWidget *parent = nullptr);

    void setWellConstruction(const WellConstruction &wc);
    void setLayout(const QVector<LayoutItem> &items);
    void clear();

    void zoomIn();
    void zoomOut();
    void fitToScreen();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawWellbore(QPainter &painter);
    void drawToolInWell(QPainter &painter);
    void drawDepthScale(QPainter &painter);
    void drawLegend(QPainter &painter);

    WellConstruction m_wellConstruction;
    QVector<LayoutItem> m_layout;

    double m_scaleX;
    double m_scaleY;
    double m_offsetX;
    double m_offsetY;
    
    bool m_dragging;
    QPoint m_lastMousePos;

    double m_maxOuterDiameter;
    double m_totalDepth;

    QColor m_soilColor;
    QColor m_casingColor;
    QColor m_cementColor;
    QColor m_toolColor;
    QColor m_openHoleColor;
    QColor m_fluidColor;
};

#endif // WELLVISUALIZATION_H
