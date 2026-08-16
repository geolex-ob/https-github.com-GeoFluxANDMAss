#ifndef WELLVISUALIZATION_H
#define WELLVISUALIZATION_H

#include <QGraphicsView>
#include <QVector>

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
        double startDepth;
        double endDepth;
        double outerDiameter;
        double innerDiameter;
        double cavernosity;
        bool isOpenHole;
    };

    void rebuildScene();

    QGraphicsScene *m_scene = nullptr;

    QVector<CasingItem> m_casings;
    QVector<LayoutItem> m_layout;

    bool m_needInitialFit = true;
};

#endif // WELLVISUALIZATION_H
