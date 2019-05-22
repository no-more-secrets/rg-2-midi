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

#include "MappedInstrument.h"

namespace Rosegarden {

MappedInstrument::MappedInstrument()
  : m_type( Instrument::Midi ),
    m_id( 0 ),
    m_name( std::string( "" ) ),
    m_audioChannels( 0 ) {}

MappedInstrument::MappedInstrument(
    Instrument::InstrumentType type, MidiByte /*channel*/,
    InstrumentId               id )
  : m_type( type ),
    m_id( id ),
    m_name( std::string( "" ) ),
    m_audioChannels( 0 ) {}

MappedInstrument::MappedInstrument(
    Instrument::InstrumentType type, MidiByte /*channel*/,
    InstrumentId id, const std::string &name, DeviceId device )
  : m_type( type ),
    m_id( id ),
    m_name( name ),
    m_device( device ),
    m_audioChannels( 0 ) {}

MappedInstrument::MappedInstrument( const Instrument &instr )
  : m_type( instr.getType() ),
    m_id( instr.getId() ),
    m_name( instr.getName() ),
    m_device( ( instr.getDevice() )->getId() ),
    m_audioChannels( instr.getAudioChannels() ) {}

MappedInstrument::MappedInstrument( Instrument *instr )
  : m_type( instr->getType() ),
    m_id( instr->getId() ),
    m_name( instr->getName() ),
    m_device( instr->getDevice()->getId() ),
    m_audioChannels( instr->getAudioChannels() ) {}

} // namespace Rosegarden

