/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

/*
  Rosegarden
  A sequencer and musical notation editor.
  Copyright 2000-2018 the Rosegarden development team.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#ifndef RG_MAPPEDCOMMON_H
#define RG_MAPPEDCOMMON_H

// Some Mapped types that gui and sound libraries use to communicate
// plugin and Studio information.  Putting them here so we can change
// MappedStudio regularly without having to rebuild the gui.
//
#include <vector>
#include <string>

namespace Rosegarden
{

    typedef std::string MappedObjectProperty;

typedef int          MappedObjectId;
typedef float        MappedObjectValue;

// typedef QVector<MappedObjectProperty> MappedObjectPropertyList;
// replaced with a std::vector<> for Qt2 compatibility

typedef std::vector<MappedObjectId> MappedObjectIdList;
typedef std::vector<MappedObjectProperty> MappedObjectPropertyList;
typedef std::vector<MappedObjectValue> MappedObjectValueList;

// The direction in which a port operates.
//
typedef enum
{
    ReadOnly,  // input port
    WriteOnly, // output port
    Duplex
} PortDirection;

}

#endif // RG_MAPPEDCOMMON_H
