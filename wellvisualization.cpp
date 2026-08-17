#include "wellvisualization.h"
#include <QGraphicsScene>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QResizeEvent>
#include <QGraphicsTextItem>
#include <QtGlobal>

WellVisualization::WellVisualization(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(QColor(245, 245, 245));
    
    // ✅ ИСПРАВЛЕНО: Инициализация таймера для отложенного обновления
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(50); // 50ms задержка
    connect(m_resizeTimer, &QTimer::timeout, this, &WellVisualization::fitToScreen);
}

void WellVisualization::setWellConstruction(const WellConstruction &wc)
{
    m_casings.clear();
    
    const auto casings = wc.casings();
    for (const Casing &c : casings) {
        CasingItem item;
        item.startDepth = c.startDepth;
        item.endDepth = c.endDepth;
        item.outerDiameter = c.outerDiameter;
        item.innerDiameter = c.innerDiameter;
        item.cavernosity = c.cavernosity;
        item.isOpenHole = c.isOpenHole;
        m_casings.append(item);
    }
    
    rebuildScene();
}

void WellVisualization::setLayout(const QVector<LayoutItem> &layout)
{
    m_layout = layout;
    rebuildScene();
}

void WellVisualization::zoomIn()
{
    scale(1.2, 1.2);
}

void WellVisualization::zoomOut()
{
    scale(1.0 / 1.2, 1.0 / 1.2);
}

void WellVisualization::fitToScreen()
{
    if (!scene())
        return;
    
    QRectF r = sceneRect();
    if (r.isNull() || r.isEmpty())
        return;
    
    // Сохранение пропорций изображения
    fitInView(r.adjusted(-20, -20, 20, 20), Qt::KeepAspectRatio);
}

void WellVisualization::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    
    // ✅ ИСПРАВЛЕНО: Используем таймер для отложенного обновления
    // Это предотвращает множественные вызовы fitToScreen при изменении размера
    if (m_resizeTimer) {
        m_resizeTimer->start(); // Перезапускает таймер
    }
}

void WellVisualization::rebuildScene()
{
    if (!m_scene)
        return;
    
    m_scene->clear();
    
    // Масштаб отображения:
    // depthScale - пикселей на 1 метр глубины
    // diameterScale - пикселей на 1 мм диаметра
    const double depthScale = 8.0;
    
    // Коэффициент расширения по горизонтали.
    // 3.0 - увеличить ширину примерно в 3 раза.
    // Если нужно ещё шире, поставьте 4.0, 5.0 и т.д.
    const double horizontalBoost = 21.0;
    const double diameterScale = 0.7 * horizontalBoost;
    
    double maxDepth = 0.0;
    double maxHalfWidth = 60.0;
    
    // Анализируем конструкцию скважины
    for (const auto &c : m_casings) {
        if (c.endDepth > maxDepth)
            maxDepth = c.endDepth;
        
        double w = c.outerDiameter * diameterScale;
        if (c.isOpenHole) {
            if (c.cavernosity > 1.0)
                w *= c.cavernosity;
        }
        
        if (w / 2.0 > maxHalfWidth)
            maxHalfWidth = w / 2.0;
    }
    
    // Анализируем компоновку инструмента
    double layoutDepth = 0.0;
    for (const auto &item : m_layout) {
        double length = item.length * item.quantity;
        layoutDepth += length;
        
        double w = item.tool.outerDiameter() * diameterScale;
        if (w / 2.0 > maxHalfWidth)
            maxHalfWidth = w / 2.0;
    }
    
    if (layoutDepth > maxDepth)
        maxDepth = layoutDepth;
    
    if (maxDepth <= 0.0)
        maxDepth = 100.0;
    
    double sceneTop = -40.0;
    double sceneBottom = maxDepth * depthScale + 40.0;
    double sceneLeft = -(maxHalfWidth + 120.0);
    double sceneRight = maxHalfWidth + 120.0;
    
    // Общий фон области рисования
    m_scene->addRect(
        sceneLeft,
        sceneTop,
        sceneRight - sceneLeft,
        sceneBottom - sceneTop,
        QPen(Qt::NoPen),
        QBrush(QColor(250, 250, 250))
    );
    
    // Осевая линия
    m_scene->addLine(
        0.0,
        0.0,
        0.0,
        maxDepth * depthScale,
        QPen(QColor(100, 100, 100), 1.0, Qt::DashLine)
    );
    
    // Подбор шага глубинных отметок
    double depthStep = 100.0;
    if (maxDepth <= 100.0)
        depthStep = 10.0;
    else if (maxDepth <= 500.0)
        depthStep = 50.0;
    else if (maxDepth <= 2000.0)
        depthStep = 100.0;
    else if (maxDepth <= 5000.0)
        depthStep = 500.0;
    else
        depthStep = 1000.0;
    
    QFont smallFont;
    smallFont.setPointSize(7);
    
    // Горизонтальные отметки глубины
    for (double d = 0.0; d <= maxDepth + 1e-6; d += depthStep) {
        double y = d * depthScale;
        
        m_scene->addLine(
            -maxHalfWidth - 20.0,
            y,
            maxHalfWidth + 20.0,
            y,
            QPen(QColor(210, 210, 210), 1.0)
        );
        
        QGraphicsTextItem *textItem = m_scene->addText(QString::number(d, 'f', 0));
        textItem->setDefaultTextColor(QColor(80, 80, 80));
        textItem->setFont(smallFont);
        textItem->setPos(-maxHalfWidth - 90.0, y - 8.0);
    }
    
    // Отрисовка конструкции скважины
    for (const auto &c : m_casings) {
        double top = c.startDepth * depthScale;
        double bottom = c.endDepth * depthScale;
        double height = bottom - top;
        
        if (height <= 0.0)
            continue;
        
        double width = c.outerDiameter * diameterScale;
        if (c.isOpenHole && c.cavernosity > 1.0)
            width *= c.cavernosity;
        
        QRectF outerRect(-width / 2.0, top, width, height);
        
        QColor casingColor;
        if (c.isOpenHole)
            casingColor = QColor(210, 180, 140, 160);
        else
            casingColor = QColor(180, 210, 240, 160);
        
        m_scene->addRect(
            outerRect,
            QPen(QColor(70, 70, 70), 1.2),
            QBrush(casingColor)
        );
        
        // Внутренняя часть обсадной колонны, если это не голый ствол
        if (!c.isOpenHole && c.innerDiameter > 0.0) {
            double innerWidth = c.innerDiameter * diameterScale;
            
            QRectF innerRect(
                -innerWidth / 2.0,
                top,
                innerWidth,
                height
            );
            
            m_scene->addRect(
                innerRect,
                QPen(QColor(120, 120, 120), 0.8),
                QBrush(QColor(255, 255, 255, 140))
            );
        }
    }
    
    // Отрисовка компоновки инструмента
    double currentDepth = 0.0;
    for (const auto &item : m_layout) {
        double length = item.length * item.quantity;
        double height = length * depthScale;
        
        if (height <= 0.0)
            continue;
        
        double width = item.tool.outerDiameter() * diameterScale * 0.85;
        
        QRectF toolRect(
            -width / 2.0,
            currentDepth,
            width,
            height
        );
        
        m_scene->addRect(
            toolRect,
            QPen(QColor(40, 40, 40), 1.2),
            QBrush(QColor(150, 170, 190, 190))
        );
        
        // Линия между элементами компоновки
        m_scene->addLine(
            -width / 2.0,
            currentDepth + height,
            width / 2.0,
            currentDepth + height,
            QPen(QColor(60, 60, 60), 1.0)
        );
        
        currentDepth += height;
    }
    
    m_scene->setSceneRect(
        sceneLeft,
        sceneTop,
        sceneRight - sceneLeft,
        sceneBottom - sceneTop
    );
    
    // Первоначальное вписывание изображения
    if (m_needInitialFit) {
        fitToScreen();
        m_needInitialFit = false;
    } else {
        if (viewport())
            viewport()->update();
    }
}
