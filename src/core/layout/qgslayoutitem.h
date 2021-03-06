/***************************************************************************
                              qgslayoutitem.h
                             -------------------
    begin                : June 2017
    copyright            : (C) 2017 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLAYOUTITEM_H
#define QGSLAYOUTITEM_H

#include "qgis_core.h"
#include "qgslayoutobject.h"
#include "qgslayoutsize.h"
#include "qgslayoutpoint.h"
#include "qgsrendercontext.h"
#include "qgslayoutundocommand.h"
#include "qgslayoutmeasurement.h"
#include <QGraphicsRectItem>

class QgsLayout;
class QPainter;

/**
 * \ingroup core
 * \class QgsLayoutItem
 * \brief Base class for graphical items within a QgsLayout.
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsLayoutItem : public QgsLayoutObject, public QGraphicsRectItem, public QgsLayoutUndoObjectInterface
{
#ifdef SIP_RUN
#include <qgslayoutitemshape.h>
#include <qgslayoutitempage.h>
#endif


#ifdef SIP_RUN
    SIP_CONVERT_TO_SUBCLASS_CODE
    // the conversions have to be static, because they're using multiple inheritance
    // (seen in PyQt4 .sip files for some QGraphicsItem classes)
    switch ( sipCpp->type() )
    {
      // really, these *should* use the constants from QgsLayoutItemRegistry, but sip doesn't like that!
      case QGraphicsItem::UserType + 101:
        sipType = sipType_QgsLayoutItemPage;
        *sipCppRet = static_cast<QgsLayoutItemPage *>( sipCpp );
        break;
      default:
        sipType = 0;
    }
    SIP_END
#endif

    Q_OBJECT
    Q_PROPERTY( bool locked READ isLocked WRITE setLocked NOTIFY lockChanged )

  public:

    //! Fixed position reference point
    enum ReferencePoint
    {
      UpperLeft, //!< Upper left corner of item
      UpperMiddle, //!< Upper center of item
      UpperRight, //!< Upper right corner of item
      MiddleLeft, //!< Middle left of item
      Middle, //!< Center of item
      MiddleRight, //!< Middle right of item
      LowerLeft, //!< Lower left corner of item
      LowerMiddle, //!< Lower center of item
      LowerRight, //!< Lower right corner of item
    };

    //! Layout item undo commands, used for collapsing undo commands
    enum UndoCommand
    {
      UndoIncrementalMove = 1, //!< Layout item incremental movement, e.g. as a result of a keypress
    };

    /**
     * Constructor for QgsLayoutItem, with the specified parent \a layout.
     *
     * If \a manageZValue is true, the z-Value of this item will be managed by the layout.
     * Generally this is the desired behavior.
     */
    explicit QgsLayoutItem( QgsLayout *layout, bool manageZValue = true );

    ~QgsLayoutItem();

    /**
     * Return correct graphics item type
     * \see stringType()
     */
    int type() const override;

    /**
     * Return the item type as a string.
     *
     * This string must be a unique, single word, character only representation of the item type, eg "LayoutScaleBar"
     * \see type()
     */
    virtual QString stringType() const = 0;

    /**
     * Returns the item identification string. This is a unique random string set for the item
     * upon creation.
     * \note There is no corresponding setter for the uuid - it's created automatically.
     * \see id()
     * \see setId()
    */
    QString uuid() const { return mUuid; }

    /**
     * Returns the item's ID name. This is not necessarily unique, and duplicate ID names may exist
     * for a layout.
     * \see setId()
     * \see uuid()
     */
    QString id() const { return mId; }

    /**
     * Set the item's \a id name. This is not necessarily unique, and duplicate ID names may exist
     * for a layout.
     * \see id()
     * \see uuid()
     */
    virtual void setId( const QString &id );

    /**
     * Get item display name. This is the item's id if set, and if
     * not, a user-friendly string identifying item type.
     * \see id()
     * \see setId()
     */
    virtual QString displayName() const;

    /**
     * Sets whether the item should be selected.
     */
    virtual void setSelected( bool selected );

    /**
     * Sets whether the item is \a visible.
     * \note QGraphicsItem::setVisible should not be called directly
     * on a QgsLayoutItem, as some item types (e.g., groups) need to override
     * the visibility toggle.
     */
    virtual void setVisibility( const bool visible );

    /**
     * Sets whether the item is \a locked, preventing mouse interactions with the item.
     * \see isLocked()
     * \see lockChanged()
     */
    void setLocked( const bool locked );

    /**
     * Returns true if the item is locked, and cannot be interacted with using the mouse.
     * \see setLocked()
     * \see lockChanged()
     */
    bool isLocked() const { return mIsLocked; }

    /**
     * Handles preparing a paint surface for the layout item and painting the item's
     * content. Derived classes must not override this method, but instead implement
     * the pure virtual method QgsLayoutItem::draw.
     */
    void paint( QPainter *painter, const QStyleOptionGraphicsItem *itemStyle, QWidget *pWidget ) override;

    /**
     * Sets the reference \a point for positioning of the layout item. This point is also
     * fixed during resizing of the item, and any size changes will be performed
     * so that the position of the reference point within the layout remains unchanged.
     * \see referencePoint()
     */
    void setReferencePoint( const ReferencePoint &point );

    /**
     * Returns the reference point for positioning of the layout item. This point is also
     * fixed during resizing of the item, and any size changes will be performed
     * so that the position of the reference point within the layout remains unchanged.
     * \see setReferencePoint()
     */
    ReferencePoint referencePoint() const { return mReferencePoint; }

    /**
     * Returns the fixed size of the item, if applicable, or an empty size if item can be freely
     * resized.
     * \see setFixedSize()
     * \see minimumSize()
    */
    QgsLayoutSize fixedSize() const { return mFixedSize; }

    /**
     * Returns the minimum allowed size of the item, if applicable, or an empty size if item can be freely
     * resized.
     * \see setMinimumSize()
     * \see fixedSize()
    */
    virtual QgsLayoutSize minimumSize() const { return mMinimumSize; }

    /**
     * Attempts to resize the item to a specified target \a size. Note that the final size of the
     * item may not match the specified target size, as items with a fixed or minimum
     * size will place restrictions on the allowed item size. Data defined item size overrides
     * will also override the specified target size.
     * \see minimumSize()
     * \see fixedSize()
     * \see attemptMove()
     * \see sizeWithUnits()
    */
    virtual void attemptResize( const QgsLayoutSize &size );

    /**
     * Attempts to move the item to a specified \a point. This method respects the item's
     * reference point, in that the item will be moved so that its current reference
     * point is placed at the specified target point.
     * Note that the final position of the item may not match the specified target position,
     * as data defined item position may override the specified value.
     * \see attemptResize()
     * \see referencePoint()
     * \see positionWithUnits()
    */
    virtual void attemptMove( const QgsLayoutPoint &point );

    /**
     * Returns the item's current position, including units. The position returned
     * is the position of the item's reference point, which may not necessarily be the top
     * left corner of the item.
     * \see attemptMove()
     * \see referencePoint()
     * \see sizeWithUnits()
    */
    QgsLayoutPoint positionWithUnits() const { return mItemPosition; }

    /**
     * Returns the item's current size, including units.
     * \see attemptResize()
     * \see positionWithUnits()
     */
    QgsLayoutSize sizeWithUnits() const { return mItemSize; }

    /**
     * Returns the current rotation for the item, in degrees clockwise.
     * \see setItemRotation()
     */
    //TODO
    double itemRotation() const;

    /**
     * Stores the item state in a DOM element.
     * \param parentElement parent DOM element (e.g. 'Layout' element)
     * \param document DOM document
     * \param context read write context
     * \see readXml()
     * \note Subclasses should ensure that they call writePropertiesToElement() in their implementation.
     */
    virtual bool writeXml( QDomElement &parentElement, QDomDocument &document, const QgsReadWriteContext &context ) const;

    /**
     * Sets the item state from a DOM element.
     * \param itemElement is the DOM node corresponding to item (e.g. 'LayoutItem' element)
     * \param document DOM document
     * \param context read write context
     * \see writeXml()
     * \note Subclasses should ensure that they call readPropertiesFromElement() in their implementation.
     */
    virtual bool readXml( const QDomElement &itemElement, const QDomDocument &document, const QgsReadWriteContext &context );

    QgsAbstractLayoutUndoCommand *createCommand( const QString &text, int id, QUndoCommand *parent = nullptr ) override SIP_FACTORY;

    /**
     * Returns true if the item includes a frame.
     * \see setFrameEnabled()
     * \see frameStrokeWidth()
     * \see frameJoinStyle()
     * \see frameStrokeColor()
     */
    bool hasFrame() const { return mFrame; }

    /**
     * Sets whether this item has a frame drawn around it or not.
     * \see hasFrame()
     * \see setFrameStrokeWidth()
     * \see setFrameJoinStyle()
     * \see setFrameStrokeColor()
     */
    virtual void setFrameEnabled( bool drawFrame );

    /**
     * Sets the frame stroke \a color.
     * \see frameStrokeColor()
     * \see setFrameEnabled()
     * \see setFrameJoinStyle()
     * \see setFrameStrokeWidth()
     */
    void setFrameStrokeColor( const QColor &color );

    /**
     * Returns the frame's stroke color. This is only used if hasFrame() returns true.
     * \see hasFrame()
     * \see setFrameStrokeColor()
     * \see frameJoinStyle()
     * \see setFrameStrokeColor()
     */
    QColor frameStrokeColor() const { return mFrameColor; }

    /**
     * Sets the frame stroke \a width.
     * \see frameStrokeWidth()
     * \see setFrameEnabled()
     * \see setFrameJoinStyle()
     * \see setFrameStrokeColor()
     */
    virtual void setFrameStrokeWidth( const QgsLayoutMeasurement &width );

    /**
     * Returns the frame's stroke width. This is only used if hasFrame() returns true.
     * \see hasFrame()
     * \see setFrameStrokeWidth()
     * \see frameJoinStyle()
     * \see frameStrokeColor()
     */
    QgsLayoutMeasurement frameStrokeWidth() const { return mFrameWidth; }

    /**
     * Returns the join style used for drawing the item's frame.
     * \see hasFrame()
     * \see setFrameJoinStyle()
     * \see frameStrokeWidth()
     * \see frameStrokeColor()
     */
    Qt::PenJoinStyle frameJoinStyle() const { return mFrameJoinStyle; }

    /**
     * Sets the join \a style used when drawing the item's frame.
     * \see setFrameEnabled()
     * \see frameJoinStyle()
     * \see setFrameStrokeWidth()
     * \see setFrameStrokeColor()
     */
    void setFrameJoinStyle( const Qt::PenJoinStyle style );

    /**
     * Returns true if the item has a background.
     * \see setBackgroundEnabled()
     * \see backgroundColor()
     */
    bool hasBackground() const { return mBackground; }

    /**
     * Sets whether this item has a background drawn under it or not.
     * \see hasBackground()
     * \see setBackgroundColor()
     */
    void setBackgroundEnabled( bool drawBackground ) { mBackground = drawBackground; }

    /**
     * Returns the background color for this item. This is only used if hasBackground()
     * returns true.
     * \see setBackgroundColor()
     * \see hasBackground()
     */
    QColor backgroundColor() const { return mBackgroundColor; }

    /**
     * Sets the background \a color for this item.
     * \see backgroundColor()
     * \see setBackgroundEnabled()
     */
    void setBackgroundColor( const QColor &color );

    /**
     * Returns the estimated amount the item's frame bleeds outside the item's
     * actual rectangle. For instance, if the item has a 2mm frame stroke, then
     * 1mm of this frame is drawn outside the item's rect. In this case the
     * return value will be 1.0.
     *
     * Returned values are in layout units.

     * \see rectWithFrame()
     */
    virtual double estimatedFrameBleed() const;

    /**
     * Returns the item's rectangular bounds, including any bleed caused by the item's frame.
     * The bounds are returned in the item's coordinate system (see Qt's QGraphicsItem docs for
     * more details about QGraphicsItem coordinate systems). The results differ from Qt's rect()
     * function, as rect() makes no allowances for the portion of outlines which are drawn
     * outside of the item.
     *
     * \see estimatedFrameBleed()
     */
    virtual QRectF rectWithFrame() const;

  public slots:

    /**
     * Refreshes the item, causing a recalculation of any property overrides and
     * recalculation of its position and size.
     */
    void refresh() override;

    /**
     * Triggers a redraw (update) of the item.
     */
    virtual void redraw();

    /**
     * Refreshes a data defined \a property for the item by reevaluating the property's value
     * and redrawing the item with this new value. If \a property is set to
     * QgsLayoutObject::AllProperties then all data defined properties for the item will be
     * refreshed.
    */
    virtual void refreshDataDefinedProperty( const QgsLayoutObject::DataDefinedProperty property = QgsLayoutObject::AllProperties );

    /**
     * Sets the layout item's \a rotation, in degrees clockwise. This rotation occurs around the center of the item.
     * \see itemRotation()
     * \see rotateItem()
    */
    virtual void setItemRotation( const double rotation );

    /**
     * Rotates the item by a specified \a angle in degrees clockwise around a specified reference point.
     * \see setItemRotation()
     * \see itemRotation()
    */
    virtual void rotateItem( const double angle, const QPointF &transformOrigin );

  signals:

    /**
     * Emitted if the item's frame style changes.
     */
    void frameChanged();

    /**
     * Emitted if the item's lock status changes.
     * \see isLocked()
     * \see setLocked()
     */
    void lockChanged();

    /**
     * Emitted on item rotation change.
     */
    void rotationChanged( double newRotation );

    /**
     * Emitted when the item's size or position changes.
     */
    void sizePositionChanged();

  protected:

    /**
     * Draws a debugging rectangle of the item's current bounds within the specified
     * painter.
     * @param painter destination QPainter
     */
    virtual void drawDebugRect( QPainter *painter );

    /**
     * Draws the item's contents using the specified render \a context.
     * Note that the context's painter has been scaled so that painter units are pixels.
     * Use the QgsRenderContext methods to convert from millimeters or other units to the painter's units.
     */
    virtual void draw( QgsRenderContext &context, const QStyleOptionGraphicsItem *itemStyle = nullptr ) = 0;

    /**
     * Draws the frame around the item.
     */
    virtual void drawFrame( QgsRenderContext &context );

    /**
     * Draws the background for the item.
     */
    virtual void drawBackground( QgsRenderContext &context );

    /**
     * Sets a fixed \a size for the layout item, which prevents it from being freely
     * resized. Set an empty size if item can be freely resized.
     * \see fixedSize()
     * \see setMinimumSize()
    */
    virtual void setFixedSize( const QgsLayoutSize &size );

    /**
     * Sets the minimum allowed \a size for the layout item. Set an empty size if item can be freely
     * resized.
     * \see minimumSize()
     * \see setFixedSize()
    */
    virtual void setMinimumSize( const QgsLayoutSize &size );

    /**
     * Refreshes an item's size by rechecking it against any possible item fixed
     * or minimum sizes.
     * \see setFixedSize()
     * \see setMinimumSize()
     * \see refreshItemPosition()
     */
    void refreshItemSize();

    /**
     * Refreshes an item's position by rechecking it against any possible overrides
     * such as data defined positioning.
     * \see refreshItemSize()
    */
    void refreshItemPosition();

    /**
     * Refreshes an item's rotation by rechecking it against any possible overrides
     * such as data defined rotation.
     * \see refreshItemSize()
     * \see refreshItemPosition()
     */
    void refreshItemRotation();

    /**
     * Refresh item's frame, considering data defined colors and frame size.
     * If \a updateItem is set to false, the item will not be automatically updated
     * after the frame is set and a later call to update() must be made.
     */
    void refreshFrame( bool updateItem = true );

    /**
     * Refresh item's background color, considering data defined colors.
     * If \a updateItem is set to false, the item will not be automatically updated
     * after the frame color is set and a later call to update() must be made.
     */
    void refreshBackgroundColor( bool updateItem = true );

    /**
     * Adjusts the specified \a point at which a \a reference position of the item
     * sits and returns the top left corner of the item, if reference point where placed at the specified position.
     */
    QPointF adjustPointForReferencePosition( const QPointF &point, const QSizeF &size, const ReferencePoint &reference ) const;

    /**
     * Returns the current position (in layout units) of a \a reference point for the item.
    */
    QPointF positionAtReferencePoint( const ReferencePoint &reference ) const;

    /**
     * Stores item state within an XML DOM element.
     * \param element is the DOM element to store the item's properties in
     * \param document DOM document
     * \param context read write context
     * \see writeXml()
     * \see readPropertiesFromElement()
     * \note derived classes must call this base implementation when overriding this method
     */
    virtual bool writePropertiesToElement( QDomElement &element, QDomDocument &document, const QgsReadWriteContext &context ) const;

    /**
     * Sets item state from a DOM element.
     * \param element is the DOM element for the item
     * \param document DOM document
     * \param context read write context
     * \see writePropertiesToElement()
     * \see readXml()
     * \note derived classes must call this base implementation when overriding this method
     */
    virtual bool readPropertiesFromElement( const QDomElement &element, const QDomDocument &document, const QgsReadWriteContext &context );

  private:

    // true if layout manages the z value for this item
    bool mLayoutManagesZValue = false;

    //! id (not necessarily unique)
    QString mId;

    //! Unique id
    QString mUuid;

    ReferencePoint mReferencePoint = UpperLeft;
    QgsLayoutSize mFixedSize;
    QgsLayoutSize mMinimumSize;

    QgsLayoutSize mItemSize;
    QgsLayoutPoint mItemPosition;
    double mItemRotation = 0.0;

    QImage mItemCachedImage;
    double mItemCacheDpi = -1;

    bool mIsLocked = false;

    //! True if item has a frame
    bool mFrame = false;
    //! Item frame color
    QColor mFrameColor = QColor( 0, 0, 0 );
    //! Item frame width
    QgsLayoutMeasurement mFrameWidth = QgsLayoutMeasurement( 0.3, QgsUnitTypes::LayoutMillimeters );
    //! Frame join style
    Qt::PenJoinStyle mFrameJoinStyle = Qt::MiterJoin;

    //! True if item has a background
    bool mBackground = true;
    //! Background color
    QColor mBackgroundColor = QColor( 255, 255, 255 );

    bool mBlockUndoCommands = false;

    void initConnectionsToLayout();

    //! Prepares a painter by setting rendering flags
    void preparePainter( QPainter *painter );
    bool shouldDrawAntialiased() const;
    bool shouldDrawDebugRect() const;

    QSizeF applyMinimumSize( const QSizeF &targetSize );
    QSizeF applyFixedSize( const QSizeF &targetSize );
    QgsLayoutPoint applyDataDefinedPosition( const QgsLayoutPoint &position );
    QgsLayoutSize applyDataDefinedSize( const QgsLayoutSize &size );
    double applyDataDefinedRotation( const double rotation );
    void updateStoredItemPosition();
    QPointF itemPositionAtReferencePoint( const ReferencePoint reference, const QSizeF &size ) const;
    void setScenePos( const QPointF &destinationPos );
    bool shouldBlockUndoCommands() const;

    friend class TestQgsLayoutItem;
};

#endif //QGSLAYOUTITEM_H



