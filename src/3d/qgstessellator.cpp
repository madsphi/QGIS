/***************************************************************************
  qgstessellator.cpp
  --------------------------------------
  Date                 : July 2017
  Copyright            : (C) 2017 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstessellator.h"

#include "qgscurve.h"
#include "qgsgeometry.h"
#include "qgspoint.h"
#include "qgspolygon.h"

#include "poly2tri/poly2tri.h"

#include <QtDebug>

#include <QVector3D>

static void make_quad( float x0, float y0, float x1, float y1, float zLow, float zHigh, QVector<float> &data, bool addNormals )
{
  float dx = x1 - x0;
  float dy = -( y1 - y0 );

  // perpendicular vector in plane to [x,y] is [-y,x]
  QVector3D vn( -dy, 0, dx );
  vn.normalize();

  // triangle 1
  data << x0 << zHigh << -y0;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();
  data << x1 << zHigh << -y1;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();
  data << x0 << zLow  << -y0;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();

  // triangle 2
  data << x0 << zLow  << -y0;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();
  data << x1 << zHigh << -y1;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();
  data << x1 << zLow  << -y1;
  if ( addNormals )
    data << vn.x() << vn.y() << vn.z();
}


QgsTessellator::QgsTessellator( double originX, double originY, bool addNormals )
  : mOriginX( originX )
  , mOriginY( originY )
  , mAddNormals( addNormals )
{
  mStride = 3 * sizeof( float );
  if ( addNormals )
    mStride += 3 * sizeof( float );
}


static bool _isRingCounterClockWise( const QgsCurve &ring )
{
  double a = 0;
  int count = ring.numPoints();
  QgsVertexId::VertexType vt;
  QgsPoint pt, ptPrev;
  ring.pointAt( 0, ptPrev, vt );
  for ( int i = 1; i < count + 1; ++i )
  {
    ring.pointAt( i % count, pt, vt );
    a += ptPrev.x() * pt.y() - ptPrev.y() * pt.x();
    ptPrev = pt;
  }
  return a > 0; // clockwise if a is negative
}

static void _makeWalls( const QgsCurve &ring, bool ccw, float extrusionHeight, QVector<float> &data, bool addNormals, double originX, double originY )
{
  // we need to find out orientation of the ring so that the triangles we generate
  // face the right direction
  // (for exterior we want clockwise order, for holes we want counter-clockwise order)
  bool is_counter_clockwise = _isRingCounterClockWise( ring );

  QgsVertexId::VertexType vt;
  QgsPoint pt;

  QgsPoint ptPrev;
  ring.pointAt( is_counter_clockwise == ccw ? 0 : ring.numPoints() - 1, ptPrev, vt );
  for ( int i = 1; i < ring.numPoints(); ++i )
  {
    ring.pointAt( is_counter_clockwise == ccw ? i : ring.numPoints() - i - 1, pt, vt );
    float x0 = ptPrev.x() - originX, y0 = ptPrev.y() - originY;
    float x1 = pt.x() - originX, y1 = pt.y() - originY;
    float height = pt.z();
    // make a quad
    make_quad( x0, y0, x1, y1, height, height + extrusionHeight, data, addNormals );
    ptPrev = pt;
  }
}

void QgsTessellator::addPolygon( const QgsPolygonV2 &polygon, float extrusionHeight )
{
  // At this point we assume that input polygons are valid according to the OGC definition.
  // This means e.g. no duplicate points, polygons are simple (no butterfly shaped polygon with self-intersection),
  // internal rings are inside exterior rings, rings do not cross each other, no dangles.

  // There is however an issue with polygons where rings touch:
  //  +---+
  //  |   |
  //  | +-+-+
  //  | | | |
  //  | +-+ |
  //  |     |
  //  +-----+
  // This is a valid polygon with one exterior and one interior ring that touch at one point,
  // but poly2tri library does not allow interior rings touch each other or exterior ring.
  // TODO: Handle the situation better - rather than just detecting the problem, try to fix
  // it by converting touching rings into one ring.

  if ( polygon.numInteriorRings() > 0 )
  {
    QList<QgsGeometry> geomRings;
    geomRings << QgsGeometry( polygon.exteriorRing()->clone() );
    for ( int i = 0; i < polygon.numInteriorRings(); ++i )
      geomRings << QgsGeometry( polygon.interiorRing( i )->clone() );

    for ( int i = 0; i < geomRings.count(); ++i )
      for ( int j = i + 1; j < geomRings.count(); ++j )
      {
        if ( geomRings[i].intersects( geomRings[j] ) )
        {
          // skip the polygon - it would cause a crash inside poly2tri library
          qDebug() << "polygon rings intersect each other - skipping";
          return;
        }
      }
  }

  const QgsCurve *exterior = polygon.exteriorRing();

  QList< std::vector<p2t::Point *> > polylinesToDelete;
  QHash<p2t::Point *, float> z;

  std::vector<p2t::Point *> polyline;
  polyline.reserve( exterior->numPoints() );

  QgsVertexId::VertexType vt;
  QgsPoint pt, ptPrev;

  for ( int i = 0; i < exterior->numPoints() - 1; ++i )
  {
    exterior->pointAt( i, pt, vt );
    if ( i == 0 || pt != ptPrev )
    {
      p2t::Point *pt2 = new p2t::Point( pt.x() - mOriginX, pt.y() - mOriginY );
      polyline.push_back( pt2 );
      float zPt = qIsNaN( pt.z() ) ? 0 : pt.z();
      z[pt2] = zPt;
    }
    ptPrev = pt;
  }
  polylinesToDelete << polyline;

  p2t::CDT *cdt = new p2t::CDT( polyline );

  // polygon holes
  for ( int i = 0; i < polygon.numInteriorRings(); ++i )
  {
    std::vector<p2t::Point *> holePolyline;
    holePolyline.reserve( exterior->numPoints() );
    const QgsCurve *hole = polygon.interiorRing( i );
    for ( int j = 0; j < hole->numPoints() - 1; ++j )
    {
      hole->pointAt( j, pt, vt );
      if ( j == 0 || pt != ptPrev )
      {
        p2t::Point *pt2 = new p2t::Point( pt.x() - mOriginX, pt.y() - mOriginY );
        holePolyline.push_back( pt2 );
        float zPt = qIsNaN( pt.z() ) ? 0 : pt.z();
        z[pt2] = zPt;
      }
      ptPrev = pt;
    }
    cdt->AddHole( holePolyline );
    polylinesToDelete << holePolyline;
  }

  // TODO: robustness (no nearly duplicate points, invalid geometries ...)

  cdt->Triangulate();

  std::vector<p2t::Triangle *> triangles = cdt->GetTriangles();

  for ( size_t i = 0; i < triangles.size(); ++i )
  {
    p2t::Triangle *t = triangles[i];
    for ( int j = 0; j < 3; ++j )
    {
      p2t::Point *p = t->GetPoint( j );
      float zPt = z[p];
      mData << p->x << extrusionHeight + zPt << -p->y;
      if ( mAddNormals )
        mData << 0.f << 1.f << 0.f;
    }
  }

  delete cdt;
  for ( int i = 0; i < polylinesToDelete.count(); ++i )
    qDeleteAll( polylinesToDelete[i] );

  // add walls if extrusion is enabled
  if ( extrusionHeight != 0 )
  {
    _makeWalls( *exterior, false, extrusionHeight, mData, mAddNormals, mOriginX, mOriginY );

    for ( int i = 0; i < polygon.numInteriorRings(); ++i )
      _makeWalls( *polygon.interiorRing( i ), true, extrusionHeight, mData, mAddNormals, mOriginX, mOriginY );
  }
}
