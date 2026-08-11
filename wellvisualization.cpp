#include "wellvisualization.h"
#include <cmath>
#include <algorithm>

WellVisualization::WellVisualization(QWidget *parent)
    : QWidget(parent)
    , m_scaleX(0.5)
    , m_scaleY(1.5)
    , m_offsetX(50)
    , m_offsetY(20)
    , m_dragging(false)
    , m_maxOuterDiameter(500)
    , m_totalDepth(2000)
    , m_soilColor(139, 119, 80)
    , m_casingColor(180, 180, 180)
    , m_cementColor(200, 200, 200)
    , m_toolColor(70, 130, 180)
    , m_openHoleColor(160, 140, 100)
    , m_fluidColor(100, 149, 237)
{
    setMinimumSize(400, 400);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void WellVisualization::setWellConstruction(const WellConstruction &wc)
{
    m_wellConstruction = wc;
    
    m_maxOuterDiameter = 100;
    m_totalDepth = 0;
    
    for (const auto &c : m_wellConstruction.casings()) {
        m_maxOuterDiameter = std::max(m_maxOuterDiameter, c.outerDiameter);
        m_totalDepth = std::max(m_totalDepth, c.endDepth);
    }
    
    for (const auto &item : m_layout) {
        m_maxOuterDiameter = std::max(m_maxOuterDiameter, item.tool.outerDiameter());
    }
    
    fitToScreen();
    update();
}

void WellVisualization::setLayout(const QVector<LayoutItem> &items)
{
    m_layout = items;
    
    for (const auto &item : m_layout) {
        m_maxOuterDiameter = std::max(m_maxOuterDiameter, item.tool.outerDiameter());
    }
    
    update();
}

void WellVisualization::clear()
{
    m_wellConstruction.clear();
    m_layout.clear();
    update();
}

void WellVisualization::zoomIn()
{
    m_scaleX *= 1.2;
    m_scaleY *= 1.2;
    update();
}

void WellVisualization::zoomOut()
{
    m_scaleX /= 1.2;
    m_scaleY /= 1.2;
    update();
}

void WellVisualization::fitToScreen()
{
    if (m_totalDepth <= 0) return;
    
    double availableHeight = height() - 120;
    m_scaleY = availableHeight / m_totalDepth;
    m_scaleX = 0.4;
    m_scaleX = std::max(m_scaleX, 0.1);
    m_scaleY = std::max(m_scaleY, 0.5);
    m_offsetX = 80;
    m_offsetY = 60;
    update();
}

void WellVisualization::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(245, 245, 245));
    
    if (m_wellConstruction.casings().isEmpty()) {
        painter.setPen(Qt::gray);
        painter.setFont(QFont("Arial", 12));
        painter.drawText(rect(), Qt::AlignCenter, 
                        "Нет данных для отображения\nДобавьте конструкцию скважины");
        return;
    }
    
    drawDepthScale(painter);
    
    painter.save();
    int wellCenterX = width() / 2;
    painter.translate(wellCenterX, m_offsetY);
    drawWellbore(painter);
    
    if (!m_layout.isEmpty()) {
        drawToolInWell(painter);
    }
    
    painter.restore();
    
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(0, 10, width(), 25, Qt::AlignHCenter, "Конструкция скважины с буровым инструментом");
    
    drawLegend(painter);
}

void WellVisualization::drawWellbore(QPainter &painter)
{
    if (m_wellConstruction.casings().isEmpty()) return;
    
    double maxWidth = m_maxOuterDiameter * m_scaleX;
    
    for (const auto &casing : m_wellConstruction.casings()) {
        double yStart = casing.startDepth * m_scaleY;
        double yEnd = casing.endDepth * m_scaleY;
        double height = yEnd - yStart;
        
        if (height <= 0) continue;
        
        if (casing.isOpenHole) {
            double drillBitRadius = (casing.outerDiameter / 2) * m_scaleX;
            double holeRadius = drillBitRadius * casing.cavernosity;
            
            painter.setBrush(m_soilColor);
            painter.setPen(QPen(Qt::darkGray, 1));
            painter.drawRect(QRectF(-maxWidth, yStart, maxWidth * 2, height));
            
            painter.setBrush(m_openHoleColor);
            painter.setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
            painter.drawRect(QRectF(-holeRadius, yStart, holeRadius * 2, height));
            
            painter.setPen(QPen(Qt::darkGray, 0.5, Qt::DotLine));
            painter.drawLine(-drillBitRadius, yStart, -drillBitRadius, yEnd);
            painter.drawLine(drillBitRadius, yStart, drillBitRadius, yEnd);
            
            painter.setBrush(m_fluidColor);
            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(-holeRadius, yStart, holeRadius * 2, height));
            
            painter.setPen(Qt::black);
            painter.setFont(QFont("Arial", 7));
            QString label = casing.name + "\n" + 
                          QString::number(casing.outerDiameter, 'f', 0) + "мм" +
                          " K=" + QString::number(casing.cavernosity, 'f', 2);
            
            QRectF textRect(-maxWidth - 120, yStart, 115, height);
            painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
            
        } else {
            double outerRadius = (casing.outerDiameter / 2) * m_scaleX;
            double innerRadius = (casing.innerDiameter / 2) * m_scaleX;
            
            painter.setBrush(m_cementColor);
            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(-outerRadius, yStart, outerRadius * 2, height));
            
            painter.setBrush(m_casingColor);
            painter.setPen(QPen(Qt::darkGray, 2));
            painter.drawRect(QRectF(-outerRadius, yStart, outerRadius * 2, height));
            
            painter.setBrush(m_fluidColor);
            painter.setPen(QPen(Qt::gray, 1));
            painter.drawRect(QRectF(-innerRadius, yStart, innerRadius * 2, height));
            
            painter.setPen(Qt::black);
            painter.setFont(QFont("Arial", 7));
            QString label = casing.name + "\n" + 
                          QString::number(casing.outerDiameter, 'f', 0) + "×" +
                          QString::number(casing.innerDiameter, 'f', 0) + "мм";
            
            QRectF textRect(-maxWidth - 120, yStart, 115, height);
            painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }
}

void WellVisualization::drawToolInWell(QPainter &painter)
{
    if (m_layout.isEmpty()) return;
    
    // Начинаем от устья (глубина 0) и идем вниз к забою
    double currentDepth = 0;
    
    // Проходим от начала списка к концу: 0, 1, 2, ..., N-1
    // Элемент 0 (СБТ) — устье, рисуется первым (вверху)
    // Элемент N-1 (долото) — забой, рисуется последним (внизу)
    for (int i = 0; i < m_layout.size(); ++i) {
        const auto &item = m_layout[i];
        
        double yStart = currentDepth * m_scaleY;
        double yEnd = (currentDepth + item.length) * m_scaleY;
        double height = yEnd - yStart;
        
        if (height < 2) {
            currentDepth += item.length;
            continue;
        }
        
        double outerRadius = (item.tool.outerDiameter() / 2) * m_scaleX;
        double innerRadius = (item.tool.innerDiameter() / 2) * m_scaleX;
        
        // Тень
        painter.setBrush(QColor(50, 90, 140));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(-outerRadius + 1, yStart + 1, outerRadius * 2, height));
        
        // Тело инструмента с градиентом
        QLinearGradient gradient(-outerRadius, 0, outerRadius, 0);
        gradient.setColorAt(0, m_toolColor.darker(120));
        gradient.setColorAt(0.3, m_toolColor.lighter(120));
        gradient.setColorAt(0.5, m_toolColor.lighter(150));
        gradient.setColorAt(0.7, m_toolColor.lighter(120));
        gradient.setColorAt(1, m_toolColor.darker(120));
        
        painter.setBrush(gradient);
        painter.setPen(QPen(Qt::darkBlue, 1));
        painter.drawRect(QRectF(-outerRadius, yStart, outerRadius * 2, height));
        
        // Внутренний канал
        if (innerRadius > 0) {
            painter.setBrush(m_fluidColor.darker(105));
            painter.setPen(QPen(Qt::darkBlue, 0.5));
            painter.drawRect(QRectF(-innerRadius, yStart, innerRadius * 2, height));
        }
        
        // Подписи
        if (height > 20) {
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 6, QFont::Bold));
            QString nameText = item.tool.name();
            if (nameText.length() > 12) nameText = nameText.left(11) + ".";
            QRectF textRect(-outerRadius, yStart, outerRadius * 2, height);
            painter.drawText(textRect, Qt::AlignCenter, nameText);
        }
        
        // Выноска с длиной
        painter.setPen(QPen(Qt::darkBlue, 0.5, Qt::DashLine));
        double lineX = outerRadius + 5;
        painter.drawLine(lineX, yStart, lineX + 25, yStart);
        painter.drawLine(lineX + 25, yStart, lineX + 25, yStart + height);
        painter.drawLine(lineX + 25, yStart + height, lineX, yStart + height);
        
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 7));
        QString lengthText = QString::number(item.length, 'f', 1) + "м";
        painter.drawText(lineX + 28, yStart + height/2 - 8, 60, 16,
                        Qt::AlignLeft | Qt::AlignVCenter, lengthText);
        
        currentDepth += item.length;
    }
}

void WellVisualization::drawDepthScale(QPainter &painter)
{
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 8));
    
    double depthStep = 100;
    if (m_totalDepth > 5000) depthStep = 500;
    else if (m_totalDepth > 2000) depthStep = 200;
    
    int leftX = 10;
    int rightX = 60;
    
    for (double depth = 0; depth <= m_totalDepth + depthStep; depth += depthStep) {
        double y = depth * m_scaleY + m_offsetY;
        if (y >= 0 && y <= height()) {
            painter.drawLine(leftX, (int)y, rightX, (int)y);
            QString text = QString::number(depth, 'f', 0);
            painter.drawText(leftX + 5, (int)y - 10, 50, 20, 
                           Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }
}

void WellVisualization::drawLegend(QPainter &painter)
{
    int legendX = width() - 170;
    int legendY = 30;
    
    painter.setFont(QFont("Arial", 8));
    
    struct { QString text; QColor color; } items[] = {
        {"Обсадная колонна", m_casingColor},
        {"Цемент", m_cementColor},
        {"Голый ствол", m_openHoleColor},
        {"Буровой раствор", m_fluidColor},
        {"Инструмент", m_toolColor}
    };
    
    for (int i = 0; i < 5; ++i) {
        int y = legendY + i * 18;
        painter.setBrush(items[i].color);
        painter.setPen(Qt::black);
        painter.drawRect(legendX, y, 12, 12);
        painter.setPen(Qt::black);
        painter.drawText(legendX + 18, y, 130, 12, 
                        Qt::AlignLeft | Qt::AlignVCenter, items[i].text);
    }
}

void WellVisualization::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn();
        else zoomOut();
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void WellVisualization::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void WellVisualization::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        m_lastMousePos = event->pos();
        update();
        event->accept();
    }
}

void WellVisualization::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
}

void WellVisualization::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_wellConstruction.casings().isEmpty()) {
        fitToScreen();
    }
}

#include "wellvisualization.moc"
