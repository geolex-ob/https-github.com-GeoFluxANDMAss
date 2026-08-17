#ifndef WELLVISUALIZATION_H
#define WELLVISUALIZATION_H

#include <QGraphicsView>
#include <QVector>
#include <QTimer>
#include "wellconstruction.h"
#include "circulation.h"

class QGraphicsScene;
class QResizeEvent;

class WellVisualization : public QGraphicsView
{
    Q_OBJECT

public:
    explicit WellVisualization(QWidget *parent = nullptr);

    void setWellConstruction(const WellConstruction &wc);
    void setLayout(const QVector<LayoutItem> &layout);

public slots:
    void zoomIn();
    void zoomOut();
    void fitToScreen();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct CasingItem {
        double startDepth = 0.0;
        double endDepth = 0.0;
        double outerDiameter = 0.0;
        double innerDiameter = 0.0;
        double cavernosity = 1.0;
        bool isOpenHole = false;
    };

    void rebuildScene();

    QGraphicsScene *m_scene = nullptr;
    QVector<CasingItem> m_casings;
    QVector<LayoutItem> m_layout;
    bool m_needInitialFit = true;
    QTimer *m_resizeTimer = nullptr;
};

#endif // WELLVISUALIZATION_H
