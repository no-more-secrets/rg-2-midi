/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8
 * sts=4 sw=4: */
/*
  Rosegarden
  A sequencer and musical notation editor.
  Copyright 2000-2018 the Rosegarden development team.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.  See the
  file COPYING included with this distribution for more
  information.
*/

#include "MidiEvent.h"

#include "Midi.h"

namespace Rosegarden {

MidiEvent::MidiEvent()
  : m_time( 0 ),
    m_duration( 0 ),
    m_eventCode( 0 ),
    m_data1( 0 ),
    m_data2( 0 ),
    m_metaEventCode( 0 ),
    m_metaMessage() {}

MidiEvent::MidiEvent( timeT time, MidiByte eventCode,
                      MidiByte data1 )
  : m_time( time ),
    m_duration( 0 ),
    m_eventCode( eventCode ),
    m_data1( data1 ),
    m_data2( 0 ),
    m_metaEventCode( 0 ),
    m_metaMessage( "" ) {}

MidiEvent::MidiEvent( timeT time, MidiByte eventCode,
                      MidiByte data1, MidiByte data2 )
  : m_time( time ),
    m_duration( 0 ),
    m_eventCode( eventCode ),
    m_data1( data1 ),
    m_data2( data2 ),
    m_metaEventCode( 0 ),
    m_metaMessage( "" ) {}

MidiEvent::MidiEvent( timeT time, MidiByte eventCode,
                      MidiByte           metaEventCode,
                      const std::string &metaMessage )
  : m_time( time ),
    m_duration( 0 ),
    m_eventCode( eventCode ),
    m_data1( 0 ),
    m_data2( 0 ),
    m_metaEventCode( metaEventCode ),
    m_metaMessage( metaMessage ) {}

MidiEvent::MidiEvent( timeT time, MidiByte eventCode,
                      const std::string &sysEx )
  : m_time( time ),
    m_duration( 0 ),
    m_eventCode( eventCode ),
    m_data1( 0 ),
    m_data2( 0 ),
    m_metaEventCode( 0 ),
    m_metaMessage( sysEx ) {}

} // namespace Rosegarden
