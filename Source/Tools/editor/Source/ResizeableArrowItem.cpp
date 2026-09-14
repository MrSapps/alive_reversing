#include "ResizeableArrowItem.hpp"
#include <QCursor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include "Model.hpp"
#include "ISyncPropertiesToTree.hpp"
#include "GridSnapSettings.hpp"
#include "ItemPositionData.hpp"
#include "GridPlacement.hpp"
#include <QDebug>
#include <algorithm>
#include <cmath>

ResizeableArrowItem::ResizeableArrowItem(QGraphicsView* pView, CollisionObject* pLine, ISyncPropertiesToTree& propSyncer, int transparency, GridSnapSettings& snapSettings, IGridPointSnapper& snapper)
    : mView(pView)
    , mLine(pLine)
    , mPropSyncer(propSyncer)
    , mSnapSettings(snapSettings)
    , mSnapper(snapper)
{
    SyncFromCollisionItem();

    Init();
    setZValue(2.0);
    SetTransparency(this, transparency);
}

void ResizeableArrowItem::hoverLeaveEvent( QGraphicsSceneHoverEvent* aEvent )
{
    SetViewCursor( Qt::ArrowCursor );
    QGraphicsItem::hoverLeaveEvent( aEvent );
}

void ResizeableArrowItem::hoverMoveEvent( QGraphicsSceneHoverEvent* aEvent )
{
    if ( !m_MouseIsDown )
    {
        m_MouseDownLine = line();
        CalcWhichEndOfLineClicked( aEvent->pos(), aEvent->modifiers() );
        if ( m_endOfLineClicked == eLinePoints_None )
        {
            SetViewCursor( Qt::OpenHandCursor );
        }
        else
        {
            SetViewCursor( Qt::CrossCursor );
        }
        return;
    }
    QGraphicsItem::hoverMoveEvent( aEvent );
}

void ResizeableArrowItem::mousePressEvent( QGraphicsSceneMouseEvent* aEvent )
{
    if ( aEvent->button() == Qt::LeftButton )
    {
        m_MouseIsDown = true;
        SetViewCursor( Qt::ClosedHandCursor );
        CalcWhichEndOfLineClicked( aEvent->pos(), aEvent->modifiers() );
    }
    QGraphicsLineItem::mousePressEvent( aEvent );
}

void ResizeableArrowItem::mouseMoveEvent( QGraphicsSceneMouseEvent* aEvent )
{
    if ( m_endOfLineClicked == eLinePoints_None )
    {
        QGraphicsLineItem::mouseMoveEvent(aEvent);

        QPointF tl = pos();
        setPos(QPointF());

        QLineF tmp = m_MouseDownLine;
        tmp.translate(tl);

        // The line's raw position right after Qt's own default move above - the same raw delta
        // every other selected item was just moved by too (Qt applies one shared delta to the
        // whole selection). Captured before snap/bounds-clamp below so their combined effect on
        // just this item can be measured and reapplied to the rest of a multi-selection further
        // down.
        const qreal rawLeft = std::min(tmp.x1(), tmp.x2());
        const qreal rawTop = std::min(tmp.y1(), tmp.y2());

        const bool multiSelect = scene()->selectedItems().count() > 1;

        // Snap the whole line's bounding box to the grid as a rigid unit, the same way a single
        // rect's body drag snaps (ResizeableRectItem::mouseMoveEvent) - snapping each endpoint
        // independently could distort the line's angle if the grid moved each end by a
        // different sub-grid amount. Previously this only happened for the single-endpoint
        // resize branch below; a whole-line body drag silently ignored CollisionSnapping
        // entirely.
        {
            const qreal left = std::min(tmp.x1(), tmp.x2());
            const qreal top = std::min(tmp.y1(), tmp.y2());
            const int snappedLeft = mSnapper.SnapX(mSnapSettings.CollisionSnapping().mSnapX, static_cast<int>(left));
            const int snappedTop = mSnapper.SnapY(mSnapSettings.CollisionSnapping().mSnapY, static_cast<int>(top));
            tmp.translate(snappedLeft - left, snappedTop - top);
        }

        // Keep the whole line within the map bounds by shifting it back in as a rigid unit,
        // preserving its exact shape - clamping each endpoint independently here could
        // collapse the line to a point if its bounding box straddles a boundary (the same bug
        // fixed for line creation in AddCollisionCommand). pos() is zero at this point (just
        // reset above), so tmp's coordinates are already scene-equivalent. Skipped here while
        // multiple items are selected - see the matching comment in
        // ResizeableRectItem::mouseMoveEvent for why: this item's own base-class move above
        // already translated every other selected item by the same raw delta, unclamped, so
        // clamping only this one here (to its own bounds) would distort the group's relative
        // layout instead of preserving it.
        if (!multiSelect)
        {
            const qreal left = std::min(tmp.x1(), tmp.x2());
            const qreal top = std::min(tmp.y1(), tmp.y2());
            const qreal width = std::abs(tmp.x2() - tmp.x1());
            const qreal height = std::abs(tmp.y2() - tmp.y1());
            const int clampedLeft = mSnapper.ClampRangeStartX(static_cast<int>(left), static_cast<int>(width));
            const int clampedTop = mSnapper.ClampRangeStartY(static_cast<int>(top), static_cast<int>(height));
            tmp.translate(clampedLeft - left, clampedTop - top);
        }

        setLine(tmp);

        PosOrLineChanged();

        if (multiSelect)
        {
            // Snap (above) can nudge this grabbed line beyond the raw delta every other
            // selected item already got from Qt's own default move - apply that same nudge to
            // the rest of the selection too, same fix as ResizeableRectItem::mouseMoveEvent, or
            // a previously-aligned group silently drifts apart by it.
            const qreal finalLeft = std::min(tmp.x1(), tmp.x2());
            const qreal finalTop = std::min(tmp.y1(), tmp.y2());
            TranslateOtherSelectedItems(scene(), this, finalLeft - rawLeft, finalTop - rawTop);

            // Then clamp the whole selection's union bounding box as one rigid group, live on
            // every move (not just once when the drag finishes).
            ClampSelectedItemsToMapBounds(scene(), mSnapper);
        }
        return;
    }

    QLineF newLine = m_endOfLineClicked == eLinePoints_P1  ? QLineF( aEvent->pos(), m_AnchorPoint ) : QLineF( m_AnchorPoint, aEvent->pos() );
    const auto kMinLineLength = 15;
    if ( newLine.length() <= kMinLineLength )
    {
        if ( m_endOfLineClicked == eLinePoints_P2 )
        {
            newLine.setLength( kMinLineLength );
        }
        else
        {
            newLine = QLineF( newLine.p2(), newLine.p1() );
            newLine.setLength( kMinLineLength );
            newLine = QLineF( newLine.p2(), newLine.p1() );
        }
    }

    if (m_endOfLineClicked == eLinePoints_P1)
    {
        QPoint tmp = newLine.p1().toPoint();
        tmp.setX(mSnapper.SnapX(mSnapSettings.CollisionSnapping().mSnapX, tmp.x()));
        tmp.setY(mSnapper.SnapY(mSnapSettings.CollisionSnapping().mSnapY, tmp.y()));
        // Only this one endpoint moves (the other end is anchored), so clamping it alone can't
        // collapse the line the way independently clamping both ends could.
        tmp = ClampToMapBounds(tmp);
        newLine.setP1(tmp);
    }
    else
    {
        QPoint tmp = newLine.p2().toPoint();
        tmp.setX(mSnapper.SnapX(mSnapSettings.CollisionSnapping().mSnapX, tmp.x()));
        tmp.setY(mSnapper.SnapY(mSnapSettings.CollisionSnapping().mSnapY, tmp.y()));
        tmp = ClampToMapBounds(tmp);
        newLine.setP2(tmp);
    }

    setLine( newLine );
    PosOrLineChanged();
}

void ResizeableArrowItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* aEvent )
{
    if ( aEvent->button() == Qt::LeftButton )
    {
        m_MouseIsDown = false;
    }
    QGraphicsLineItem::mouseReleaseEvent( aEvent );
}

void ResizeableArrowItem::paint( QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget /*= nullptr*/ )
{
    Q_UNUSED( aWidget );

    // Only draw what is required
    aPainter->setClipRect( aOption->exposedRect );
    switch (mLine->mLine.mLineType)
    {
        case eLineTypes::eArt_9:
            aPainter->setBrush(QColor(100, 100, 100, 255));
            break;

        case eLineTypes::eBackgroundFloor_4:
        case eLineTypes::eBackgroundWallLeft_5:
        case eLineTypes::eBackgroundWallRight_6:
        case eLineTypes::eBackgroundCeiling_7:
            aPainter->setBrush(QColor(150, 150, 75, 255));
            break;

        case eLineTypes::eTrackLine_8:
            aPainter->setBrush(QColor(0, 215, 215, 255));
            break;

        case eLineTypes::eBulletWall_10:
            aPainter->setBrush(QColor(255, 70, 70, 255));
            break;

        case eLineTypes::eMineCarFloor_11:
        case eLineTypes::eMineCarWall_12:
        case eLineTypes::eMineCarCeiling_13:
        case eLineTypes::eBackgroundMineCarFloor_14:
        case eLineTypes::eBackgroundMineCarWall_15:
        case eLineTypes::eBackgroundMineCarCeiling_16:
            aPainter->setBrush(QColor(190, 70, 255, 255));
            break;

        case eLineTypes::eFlyingObjectWall_17:
        case eLineTypes::eBackgroundFlyingObjectWall_18:
            aPainter->setBrush(QColor(30, 200, 15, 255));
            break;

        case eLineTypes::eFloor_0:
        case eLineTypes::eWallLeft_1:
        case eLineTypes::eWallRight_2:
        case eLineTypes::eCeiling_3:
        default:
            aPainter->setBrush(QColor(255, 255, 100, 255));
            break;
    }

    // Change the pen depending on selection
    if ( isSelected() )
    {
        aPainter->setPen( QPen ( Qt::red, 2, Qt::DashLine )  );
    }
    else
    {
        QPen p( Qt::black, 2, Qt::SolidLine );
        p.setJoinStyle( Qt::RoundJoin );
        aPainter->setPen( p );
    }

    // Use the painter path for rendering
    aPainter->drawPath( shape() );
}

QPainterPath ResizeableArrowItem::shape() const
{
    // Calc arrow head lines based on the angle of the current line
    QLineF cLine = line();

    const auto kArrowHeadLength = 8;
    const auto kArrowHeadAngle = 32;

    const qreal cLineAngle = cLine.angle();
    QLineF head1 = cLine;
    QLineF head2 = cLine;
    head1.setLength( kArrowHeadLength );
    head1.setAngle( cLineAngle+-kArrowHeadAngle );
    head2.setLength( kArrowHeadLength );
    head2.setAngle( cLineAngle+kArrowHeadAngle );

    // Create paths for each section of the arrow
    QPainterPath mainLine;
    mainLine.moveTo( cLine.p2() );
    mainLine.lineTo( cLine.p1() );


    QPainterPath headLine1;
    headLine1.moveTo( cLine.p1() );
    headLine1.lineTo( head1.p2() );

    QPainterPath headLine2;
    headLine2.moveTo( cLine.p1() );
    headLine2.lineTo( head2.p2() );

    QPainterPathStroker stroker;
    stroker.setWidth( 4 );

    // Join them together
    QPainterPath stroke = stroker.createStroke( mainLine );
    stroke.addPath( stroker.createStroke( headLine1 ) );
    stroke.addPath( stroker.createStroke( headLine2 ) );

    return stroke.simplified();
}

QRectF ResizeableArrowItem::boundingRect() const
{
    QPainterPath pPath = shape();

    QRectF bRect = pPath.controlPointRect();

    QRectF adjusted( bRect.x()-1, bRect.y()-1, bRect.width()+2, bRect.height()+2 );

    return adjusted;
}

QVariant ResizeableArrowItem::itemChange(GraphicsItemChange aChange, const QVariant& aValue)
{
    if (aChange == ItemPositionHasChanged)
    {
        PosOrLineChanged();
    }
    return QGraphicsLineItem::itemChange(aChange, aValue);
}

void ResizeableArrowItem::Init()
{
    setToolTip("Click and drag an edge of the line to resize it, or hold shift and click and drag to move the line");

    QPen p( Qt::black, 2, Qt::SolidLine );
    p.setJoinStyle( Qt::RoundJoin );
    setPen( p );

    setAcceptHoverEvents( true );

    SetViewCursor( Qt::CrossCursor );

    // Allow select and move.
    setFlags( ItemSendsScenePositionChanges | ItemSendsGeometryChanges | ItemIsMovable | ItemIsSelectable );

    prepareGeometryChange();

    m_endOfLineClicked = eLinePoints_None;
    m_MouseIsDown = false;

    // TODO: Use QPixmapCache instead
    setCacheMode( ItemCoordinateCache );
}

QPoint ResizeableArrowItem::ClampToMapBounds(QPoint pt) const
{
    const QPointF scenePos = QPointF(pt) + pos();
    const int clampedX = mSnapper.ClampX(static_cast<int>(scenePos.x()));
    const int clampedY = mSnapper.ClampY(static_cast<int>(scenePos.y()));
    return QPoint(clampedX - static_cast<int>(pos().x()), clampedY - static_cast<int>(pos().y()));
}

void ResizeableArrowItem::CalcWhichEndOfLineClicked( QPointF aPos, Qt::KeyboardModifiers aMods )
{
    if ( aMods == Qt::ShiftModifier )
    {
        m_endOfLineClicked = eLinePoints_None;
        return;
    }

    // Figure out which part of the line we clicked, either one of the ends or the middle!
    const QPointF mousePos = aPos;

    // Get the distance from each end of the line
    const QLineF p1Distance( mousePos, line().p1() );
    const QLineF p2Distance( mousePos, line().p2() );

    // Ensure the distance is near the end, if not then only allow moving rather than resizing.
    if ( line().length() > 10 )
    {
        const qreal tolerance = 10;
        const qreal shortestDistance = p1Distance.length() > p2Distance.length() ? p2Distance.length() : p1Distance.length();
        if ( shortestDistance > tolerance )
        {
            m_endOfLineClicked = eLinePoints_None;
            return;
        }
    }

    m_endOfLineClicked = p1Distance.length() > p2Distance.length()  ? eLinePoints_P2 : eLinePoints_P1;
    m_AnchorPoint = m_endOfLineClicked == eLinePoints_P2 ? line().p1() : line().p2();
}

void ResizeableArrowItem::SetViewCursor(Qt::CursorShape cursor)
{
    mView->setCursor(cursor);
}

QLineF ResizeableArrowItem::SaveLine() const
{
    return line();
}

void ResizeableArrowItem::RestoreLine(const QLineF& line)
{
    setLine(line);
    PosOrLineChanged();
}

void ResizeableArrowItem::Visit(IReflector& f)
{
    f.Visit("Id", mLine->mId);
    f.Visit("x1", mLine->mLine.mRect.x);
    f.Visit("y1", mLine->mLine.mRect.y);
    f.Visit("x2", mLine->mLine.mRect.w);
    f.Visit("y2", mLine->mLine.mRect.h);

    f.Visit("Type", mLine->mLine.mLineType);
    f.Visit("Next", mLine->mLine.mNext);
    f.Visit("Previous", mLine->mLine.mPrevious);
    f.Visit("Length", mLine->mLine.mLineLength);
}

void ResizeableArrowItem::SyncFromCollisionItem()
{
    // The model can be written to directly with no bounds checking at all - e.g. the properties
    // panel's spin boxes write straight into mLine, bypassing every mouse-driven resize/drag/paste
    // path's mSnapper clamp entirely, and a freshly-loaded path can already contain an
    // out-of-bounds line (e.g. hand-edited JSON, or one that was out of bounds on the map before
    // it shrank). Apply the same invariants here so any such value can't leave the line outside
    // the map - same "shrink an oversized extent first, then reposition" idea as
    // ResizeableRectItem::SyncFromMapObject, via GridPlacement::FitLineToBounds, clamping each
    // axis through mSnapper (this item doesn't know its tab's raw pixel size, only how to clamp
    // against it) rather than GridPlacement::FitLineToMapBounds's raw-map-size overload (used by
    // ChangeMapSizeDialog, which does have that on hand).
    const int x1 = mLine->X1();
    const int y1 = mLine->Y1();
    const int x2 = mLine->X2();
    const int y2 = mLine->Y2();

    const GridPlacement::ClampedLine fitted = GridPlacement::FitLineToBounds(
        x1, y1, x2, y2,
        [this](int length) { return mSnapper.ClampLengthX(length); },
        [this](int length) { return mSnapper.ClampLengthY(length); },
        [this](int start, int length) { return mSnapper.ClampRangeStartX(start, length); },
        [this](int start, int length) { return mSnapper.ClampRangeStartY(start, length); });
    setLine(fitted.x2, fitted.y2, fitted.x1, fitted.y1);

    // Write any correction back into the model - otherwise an out-of-bounds value would only
    // look fixed on screen and still round-trip to disk as-is.
    SyncToCollisionItem();

    // Full repaint because the line type may have changed which changes the line colour
    update();
}

void ResizeableArrowItem::SyncToCollisionItem()
{
    QLineF curLine = line();

    // Sync the model to the graphics item
    mLine->SetX1(static_cast<int>(curLine.x2() + pos().x()));
    mLine->SetY1(static_cast<int>(curLine.y2() + pos().y()));
    mLine->SetX2(static_cast<int>(curLine.x1() + pos().x()));
    mLine->SetY2(static_cast<int>(curLine.y1() + pos().y()));

    mLine->CalculateLength();
}

void ResizeableArrowItem::PosOrLineChanged()
{
    SyncToCollisionItem();

    // Update the property tree view
    mPropSyncer.Sync(this);
}
