/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8
 * sts=4 sw=4: */

/*
    Rosegarden
    A MIDI and audio sequencer and musical notation editor.
    Copyright 2000-2018 the Rosegarden development team.

    Other copyrights also apply to some parts of this work.
   Please see the AUTHORS file and individual file headers for
   details.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.  See
   the file COPYING included with this distribution for more
   information.
*/

#define RG_MODULE_STRING "[RosegardenSequencer]"

#include "RosegardenSequencer.h"

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <sys/mman.h>
//#include <unistd.h>
//#include <errno.h>

#include "ControlBlock.h"
#include "Instrument.h"
#include "InstrumentStaticSignals.h"
#include "MappedEventInserter.h"
#include "MappedInstrument.h"
#include "SequencerDataBlock.h"
//#include "PluginFactory.h"
#include "Profiler.h"
//#include "SoundDriver.h"
//#include "SoundDriverFactory.h"
#include "StudioControl.h"

//#include "gui/application/RosegardenApplication.h"
//#include "gui/application/RosegardenMainWindow.h"

//#include "rosegarden-version.h"

// #define DEBUG_ROSEGARDEN_SEQUENCER

namespace Rosegarden {

RosegardenSequencer *RosegardenSequencer::m_instance = nullptr;

RosegardenSequencer::RosegardenSequencer()
  : m_driver( nullptr ),
    m_transportStatus( STOPPED ),
    m_songPosition( 0, 0 ),
    m_lastFetchSongPosition( 0, 0 ),
    // 160 msecs for low latency mode.  Historically, we used
    // 500 msecs for "high" latency mode.
    m_readAhead( 0, 160000000 ),
    // 60 msecs for low latency mode.  Historically, we used
    // 400 msecs for "high" latency mode.
    m_audioMix( 0, 60000000 ),
    m_audioRead( 2, 500000000 ), // 2.5 secs
    m_audioWrite( 4, 0 ),        // 4.0 secs
    m_smallFileSize( 256 ),      // 256 kbytes
    m_loopStart( 0, 0 ),
    m_loopEnd( 0, 0 ),
    m_studio( new MappedStudio() ),
    m_transportToken( 1 ),
    m_isEndOfCompReached( false ) {
  // Initialise the MappedStudio
  //
  initialiseStudio();

  // Creating this object also initialises the Rosegarden
  // ALSA/JACK interface for both playback and recording.
  // MappedStudio audio faders are also created.
  //
  // m_driver = SoundDriverFactory::createDriver( m_studio );
  m_studio->setSoundDriver( m_driver );

  if( !m_driver ) {
    m_transportStatus = QUIT;
    return;
  }

  // m_driver->setAudioBufferSizes( m_audioMix, m_audioRead,
  //                               m_audioWrite, m_smallFileSize
  //                               );

  // m_driver->setExternalTransportControl( this );

  // Connect for high-frequency control change notifications.
  // Note that we must use a DirectConnection or else the signals
  // may get lost.  I assume this is because the sequencer thread
  // doesn't handle queued signals.  Perhaps it doesn't have a Qt
  // event loop? Regardless, we really don't want something as
  // high-frequency as this going through a message queue anyway.
  // We should probably request a DirectConnection for every
  // controlChange() connection.
  // connect( Instrument::getStaticSignals().data(),
  //         &InstrumentStaticSignals::controlChange, this,
  //         &RosegardenSequencer::slotControlChange,
  //         Qt::DirectConnection );
}

RosegardenSequencer::~RosegardenSequencer() {
  // SEQUENCER_DEBUG << "RosegardenSequencer - shutting down";
  cleanup();
}

void RosegardenSequencer::cleanup() {
  // if( m_driver ) m_driver->shutdown();
  delete m_studio;
  m_studio = nullptr;
  // delete m_driver;
  m_driver = nullptr;
}

RosegardenSequencer *RosegardenSequencer::getInstance() {
  if( !m_instance ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
    // SEQUENCER_DEBUG
    //    << "RosegardenSequencer::getInstance: Creating";
#endif
    m_instance = new RosegardenSequencer();
  }
  return m_instance;
}

void RosegardenSequencer::lock() {}

void RosegardenSequencer::unlock() {}

// "Public" (ex-DCOP, locks required) functions first

void RosegardenSequencer::quit() {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "RosegardenSequencer::quit()";
#endif
  // and break out of the loop next time around
  m_transportStatus = QUIT;
}

// We receive a starting time from the GUI which we use as the
// basis of our first fetch of events from the GUI core. Assuming
// this works we set our internal state to PLAYING and go ahead
// and play the piece until we get a signal to stop.
//
bool RosegardenSequencer::play( const RealTime &time ) {
  // ??? Precondition: readAhead should be larger than the JACK
  //     (m_driver) period size.

  if( m_transportStatus == PLAYING ||
      m_transportStatus == STARTING_TO_PLAY )
    return true;

  // Check for record toggle (punch out)
  //
  if( m_transportStatus == RECORDING ) {
    m_transportStatus = PLAYING;
    return punchOut();
  }

  // To play from the given song position sets up the internal
  // play state to "STARTING_TO_PLAY" which is then caught in
  // the main event loop
  //
  m_songPosition = time;

  SequencerDataBlock::getInstance()->setPositionPointer(
      m_songPosition );

  if( m_transportStatus != RECORDING &&
      m_transportStatus != STARTING_TO_RECORD ) {
    m_transportStatus = STARTING_TO_PLAY;
  }

  // m_driver->stopClocks();

  // m_driver->setAudioBufferSizes( m_audioMix, m_audioRead,
  //                               m_audioWrite, m_smallFileSize
  //                               );

  // report
  //
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG
      << "RosegardenSequencer::play() - starting to play\n";
#endif
  //!!!
  //    dumpFirstSegment();

  // keep it simple
  return true;
}

bool RosegardenSequencer::record( const RealTime &time,
                                  long            recordMode ) {
  return false;
}

void RosegardenSequencer::stop() {}

bool RosegardenSequencer::punchOut() {
  // Check for record toggle (punch out)
  //
  if( m_transportStatus == RECORDING ) {
    // m_driver->punchOut();
    m_transportStatus = PLAYING;
    return true;
  }
  return false;
}

// Sets the Sequencer object and this object to the new time
// from where playback can continue.
//
void RosegardenSequencer::jumpTo( const RealTime &pos ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "RosegardenSequencer::jumpTo(" << pos
                  << ")\n";
#endif
  if( pos < RealTime::zeroTime ) return;

  // m_driver->stopClocks();

  RealTime oldPosition = m_songPosition;

  m_songPosition = m_lastFetchSongPosition = pos;

  SequencerDataBlock::getInstance()->setPositionPointer(
      m_songPosition );

  // m_driver->resetPlayback( oldPosition, m_songPosition );

  // if( m_driver->isPlaying() ) {
  //  // Now prebuffer as in startPlaying:

  //  MappedEventList c;
  //  fetchEvents( c, m_songPosition, m_songPosition +
  //  m_readAhead,
  //               true );

  //  // process whether we need to or not as this also processes
  //  // the audio queue for us
  //  //
  //  m_driver->processEventsOut( c, m_songPosition,
  //                              m_songPosition + m_readAhead );
  //}

  incrementTransportToken();

  //    SEQUENCER_DEBUG << "RosegardenSequencer::jumpTo: pausing
  //    to simulate high-load environment";
  //    ::sleep(1);

  // m_driver->startClocks();

  return;
}

void RosegardenSequencer::setLoop( const RealTime &loopStart,
                                   const RealTime &loopEnd ) {
  m_loopStart = loopStart;
  m_loopEnd   = loopEnd;

  // m_driver->setLoop( loopStart, loopEnd );
}

// Return the status of the sound systems (audio and MIDI)
//
unsigned int RosegardenSequencer::getSoundDriverStatus(
    const std::string &guiVersion ) {
  return 0;
}

// Add an audio file to the sequencer
bool RosegardenSequencer::addAudioFile(
    const std::string &fileName, int id ) {
  // call SoundDriver->addAudioFile()
  // return m_driver->addAudioFile( fileName.toUtf8().data(), id
  // );
  return false;
}

bool RosegardenSequencer::removeAudioFile( int id ) {
  // return m_driver->removeAudioFile( id );
  return false;
}

void RosegardenSequencer::clearAllAudioFiles() {
  // m_driver->clearAudioFiles();
}

void RosegardenSequencer::setMappedInstrument(
    int type, unsigned int id ) {
  InstrumentId               mID = (InstrumentId)id;
  Instrument::InstrumentType mType =
      (Instrument::InstrumentType)type;

  // m_driver->setMappedInstrument(
  //    new MappedInstrument( mType, 0, mID ) );
}

void RosegardenSequencer::processMappedEvent( MappedEvent mE ) {
  m_asyncOutQueue.push_back( new MappedEvent( mE ) );
  //    SEQUENCER_DEBUG << "processMappedEvent: Have " <<
  //    m_asyncOutQueue.size()
  //                    << " events in async out queue" << endl;
}

int RosegardenSequencer::canReconnect(
    Device::DeviceType type ) {
  // return m_driver->canReconnect( type );
  return 0;
}

bool RosegardenSequencer::addDevice(
    Device::DeviceType type, DeviceId id,
    InstrumentId                baseInstrumentId,
    MidiDevice::DeviceDirection direction ) {
  // return m_driver->addDevice( type, id, baseInstrumentId,
  //                            direction );
  return false;
}

void RosegardenSequencer::removeDevice( unsigned int deviceId ) {
  // m_driver->removeDevice( deviceId );
}

void RosegardenSequencer::removeAllDevices() {
  // m_driver->removeAllDevices();
}

void RosegardenSequencer::renameDevice( unsigned int deviceId,
                                        std::string  name ) {
  // m_driver->renameDevice( deviceId, name );
}

unsigned int RosegardenSequencer::getConnections(
    Device::DeviceType          type,
    MidiDevice::DeviceDirection direction ) {
  // return m_driver->getConnections( type, direction );
  return 0;
}

std::string RosegardenSequencer::getConnection(
    Device::DeviceType          type,
    MidiDevice::DeviceDirection direction,
    unsigned int                connectionNo ) {
  // return m_driver->getConnection( type, direction,
  //                                connectionNo );
  return "";
}

std::string RosegardenSequencer::getConnection( DeviceId id ) {
  // return m_driver->getConnection( id );
  return "";
}

void RosegardenSequencer::setConnection(
    unsigned int deviceId, std::string connection ) {
  // m_driver->setConnection( deviceId, connection );
}

void RosegardenSequencer::setPlausibleConnection(
    unsigned int deviceId, std::string connection ) {
  bool recordDevice =
      false; // we only want this from connectSomething()
  // m_driver->setPlausibleConnection( deviceId, connection,
  //                                  recordDevice );
}

void RosegardenSequencer::connectSomething() {
  // m_driver->connectSomething();
}

unsigned int RosegardenSequencer::getTimers() {
  // return m_driver->getTimers();
  return 0;
}

std::string RosegardenSequencer::getTimer( unsigned int n ) {
  // return m_driver->getTimer( n );
  return "";
}

std::string RosegardenSequencer::getCurrentTimer() {
  // return m_driver->getCurrentTimer();
  return "";
}

void RosegardenSequencer::setCurrentTimer( std::string timer ) {
  // m_driver->setCurrentTimer( timer );
}

RealTime RosegardenSequencer::getAudioPlayLatency() {
  // return m_driver->getAudioPlayLatency();
  return {};
}

RealTime RosegardenSequencer::getAudioRecordLatency() {
  // return m_driver->getAudioRecordLatency();
  return {};
}

void RosegardenSequencer::setMappedProperty(
    int id, const std::string &property, float value ) {
  // RG_DEBUG << "setMappedProperty(int, std::string, float): id
  // = "
  // << id << "; property = \"" << property << "\"" << "; value =
  // " << value;

  MappedObject *object = m_studio->getObjectById( id );

  if( object ) object->setProperty( property, value );
}

void RosegardenSequencer::setMappedProperties(
    const MappedObjectIdList &      ids,
    const MappedObjectPropertyList &properties,
    const MappedObjectValueList &   values ) {
  MappedObject * object = nullptr;
  MappedObjectId prevId = 0;

  for( size_t i = 0; i < ids.size() && i < properties.size() &&
                     i < values.size();
       ++i ) {
    if( i == 0 || ids[i] != prevId ) {
      object = m_studio->getObjectById( ids[i] );
      prevId = ids[i];
    }

    if( object ) {
      object->setProperty( properties[i], values[i] );
    }
  }
}

void RosegardenSequencer::setMappedProperty(
    int id, const std::string &property,
    const std::string &value ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "setProperty: id = " << id
                  << " : property = \"" << property << "\""
                  << ", value = " << value << endl;
#endif
  MappedObject *object = m_studio->getObjectById( id );

  if( object ) object->setStringProperty( property, value );
}

std::string RosegardenSequencer::setMappedPropertyList(
    int id, const std::string &property,
    const MappedObjectPropertyList &values ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "setPropertyList: id = " << id
                  << " : property list size = \""
                  << values.size() << "\"" << endl;
#endif
  MappedObject *object = m_studio->getObjectById( id );

  if( object ) {
    try {
      object->setPropertyList( property, values );
    } catch( std::string err ) { return err; }
    return "";
  }

  //    return "(object not found)";

  //!!! This is where the "object not found" error is coming from
  //! when changing
  // the category combo.  I suspect something isn't wired quite
  // right in here somewhere in the chain, and that's what's
  // causing this error to come up, but testing with this simply
  // disabled, everything seems to be working as expected if we
  // ignore the error and move right along.  I have to admit I
  // have only a very tenuous grasp on any of this, however.
  return "";
}

int RosegardenSequencer::getMappedObjectId( int type ) {
  int value = -1;

  MappedObject *object = m_studio->getObjectOfType(
      MappedObject::MappedObjectType( type ) );

  if( object ) { value = int( object->getId() ); }

  return value;
}

std::vector<std::string> RosegardenSequencer::getPropertyList(
    int id, const std::string &property ) {
  std::vector<std::string> list;

  MappedObject *object = m_studio->getObjectById( id );

  if( object ) { list = object->getPropertyList( property ); }

#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "getPropertyList - return " << list.size()
                  << " items" << endl;
#endif
  return list;
}

std::vector<std::string>
RosegardenSequencer::getPluginInformation() {
  std::vector<std::string> list;

  // PluginFactory::enumerateAllPlugins( list );

  return list;
}

std::string RosegardenSequencer::getPluginProgram(
    int id, int bank, int program ) {
  MappedObject *object = m_studio->getObjectById( id );

  if( object ) {
    MappedPluginSlot *slot =
        dynamic_cast<MappedPluginSlot *>( object );
    if( slot ) { return slot->getProgram( bank, program ); }
  }

  return std::string();
}

unsigned long RosegardenSequencer::getPluginProgram(
    int id, const std::string &name ) {
  MappedObject *object = m_studio->getObjectById( id );

  if( object ) {
    MappedPluginSlot *slot =
        dynamic_cast<MappedPluginSlot *>( object );
    if( slot ) { return slot->getProgram( name ); }
  }

  return 0;
}

void RosegardenSequencer::setMappedPort( int           pluginId,
                                         unsigned long portId,
                                         float         value ) {
  MappedObject *object = m_studio->getObjectById( pluginId );

  MappedPluginSlot *slot =
      dynamic_cast<MappedPluginSlot *>( object );

  if( slot ) {
    slot->setPort( portId, value );
  } else {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
    SEQUENCER_DEBUG << "no such slot";
#endif
  }
}

float RosegardenSequencer::getMappedPort(
    int pluginId, unsigned long portId ) {
  MappedObject *object = m_studio->getObjectById( pluginId );

  MappedPluginSlot *slot =
      dynamic_cast<MappedPluginSlot *>( object );

  if( slot ) {
    return slot->getPort( portId );
  } else {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
    SEQUENCER_DEBUG << "no such slot";
#endif
  }

  return 0;
}

// Creates an object of a type
//
int RosegardenSequencer::createMappedObject( int type ) {
  MappedObject *object = m_studio->createObject(
      MappedObject::MappedObjectType( type ) );

  if( object ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
    SEQUENCER_DEBUG << "createMappedObject - type = " << type
                    << ", object id = " << object->getId()
                    << endl;
#endif
    return object->getId();
  }

  return 0;
}

// Destroy an object
//
bool RosegardenSequencer::destroyMappedObject( int id ) {
  return m_studio->destroyObject( MappedObjectId( id ) );
}

// Connect two objects
//
void RosegardenSequencer::connectMappedObjects( int id1,
                                                int id2 ) {
  m_studio->connectObjects( MappedObjectId( id1 ),
                            MappedObjectId( id2 ) );

  // When this happens we need to resynchronise our audio
  // processing, and this is the easiest (and most brutal) way to
  // do it.
  // if( m_transportStatus == PLAYING ||
  //    m_transportStatus == RECORDING ) {
  //  RealTime seqTime = m_driver->getSequencerTime();
  //  jumpTo( seqTime );
  //}
}

// Disconnect two objects
//
void RosegardenSequencer::disconnectMappedObjects( int id1,
                                                   int id2 ) {
  m_studio->disconnectObjects( MappedObjectId( id1 ),
                               MappedObjectId( id2 ) );
}

// Disconnect an object from everything
//
void RosegardenSequencer::disconnectMappedObject( int id ) {
  m_studio->disconnectObject( MappedObjectId( id ) );
}

unsigned int RosegardenSequencer::getSampleRate() const {
  // if( m_driver ) return m_driver->getSampleRate();

  return 0;
}

void RosegardenSequencer::clearStudio() {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "clearStudio()";
#endif
  m_studio->clear();
  SequencerDataBlock::getInstance()->clearTemporaries();
}

// Set the MIDI Clock period in microseconds
//
void RosegardenSequencer::setQuarterNoteLength( RealTime rt ) {
#ifdef DEBUG_ROSEGARDEN_SEQUENCER
  SEQUENCER_DEBUG << "RosegardenSequencer::setQuarterNoteLength"
                  << rt << endl;
#endif
  // m_driver->setMIDIClockInterval( rt / 24 );
}

std::string RosegardenSequencer::getStatusLog() {
  // return m_driver->getStatusLog();
  return "";
}

void RosegardenSequencer::dumpFirstSegment() {
  unsigned int i = 0;

  std::set<std::shared_ptr<MappedEventBuffer> > segs =
      m_metaIterator.getSegments();
  if( segs.empty() ) { return; }

  std::shared_ptr<MappedEventBuffer> firstMappedEventBuffer =
      *segs.begin();

  MappedEventBuffer::iterator it( ( firstMappedEventBuffer ) );

  for( ; !it.atEnd(); ++it ) {
    MappedEvent evt = ( *it );

    ++i;
  }
}

void RosegardenSequencer::segmentModified(
    std::shared_ptr<MappedEventBuffer> mapper ) {
  if( !mapper ) return;

  /* We don't force an immediate rewind while recording.  It
     would be "the right thing" soundwise, but historically we
     haven't, there's been no demand and nobody knows what subtle
     problems might be introduced. */
  bool immediate = ( m_transportStatus == PLAYING );
  m_metaIterator.resetIteratorForSegment( mapper, immediate );
}

void RosegardenSequencer::segmentAdded(
    std::shared_ptr<MappedEventBuffer> mapper ) {
  if( !mapper ) return;

  // m_metaIterator takes ownership of the mapper, shared with
  // other MappedBufMetaIterators
  m_metaIterator.addSegment( mapper );
}

void RosegardenSequencer::segmentAboutToBeDeleted(
    std::shared_ptr<MappedEventBuffer> mapper ) {
  if( !mapper ) return;

  // This deletes mapper just if no other metaiterator owns it.
  m_metaIterator.removeSegment( mapper );
}

void RosegardenSequencer::compositionAboutToBeDeleted() {
  m_metaIterator.clear();
}

void RosegardenSequencer::remapTracks() {
  rationalisePlayingAudio();
}

bool RosegardenSequencer::getNextTransportRequest(
    TransportRequest &request, RealTime &time ) {
  if( m_transportRequests.empty() ) return false;
  TransportPair pair = *m_transportRequests.begin();
  m_transportRequests.pop_front();
  request = pair.first;
  time    = pair.second;

  //!!! review transport token management -- jumpToTime has an
  // extra incrementTransportToken() below

  return true; // fix "control reaches end of non-void function
               // warning"
}

MappedEventList
RosegardenSequencer::pullAsynchronousMidiQueue() {
  MappedEventList mq = m_asyncInQueue;
  m_asyncInQueue     = MappedEventList();
  return mq;
}

// END of public API

// Get a slice of events from the composition into a
// MappedEventList.
void RosegardenSequencer::fetchEvents(
    MappedEventList &mappedEventList, const RealTime &start,
    const RealTime &end, bool firstFetch ) {
  // Always return nothing if we're stopped
  //
  if( m_transportStatus == STOPPED ||
      m_transportStatus == STOPPING )
    return;

  getSlice( mappedEventList, start, end, firstFetch );
  applyLatencyCompensation( mappedEventList );
}

void RosegardenSequencer::getSlice(
    MappedEventList &mappedEventList, const RealTime &start,
    const RealTime &end, bool firstFetch ) {
  if( firstFetch || ( start < m_lastStartTime ) ) {
    m_metaIterator.jumpToTime( start );
  }

  MappedEventInserter inserter( mappedEventList );

  m_metaIterator.fetchEvents( inserter, start, end );

  // don't do this, it breaks recording because
  // playing stops right after it starts.
  //  m_isEndOfCompReached = eventsRemaining;

  m_lastStartTime = start;
}

void RosegardenSequencer::applyLatencyCompensation(
    MappedEventList &mappedEventList ) {}

// The first fetch of events from the core/ and initialisation
// for this session of playback.  We fetch up to m_readAhead
// ahead at first at then top up at each slice.
//
bool RosegardenSequencer::startPlaying() {
  // Fetch up to m_readAhead microseconds worth of events
  m_lastFetchSongPosition = m_songPosition + m_readAhead;

  // This will reset the Sequencer's internal clock
  // ready for new playback
  // m_driver->initialisePlayback( m_songPosition );

  MappedEventList c;
  fetchEvents( c, m_songPosition, m_songPosition + m_readAhead,
               true );

  // process whether we need to or not as this also processes
  // the audio queue for us
  // m_driver->processEventsOut( c, m_songPosition,
  //                            m_songPosition + m_readAhead );

  std::vector<MappedEvent> audioEvents;
  m_metaIterator.getAudioEvents( audioEvents );
  // m_driver->initialiseAudioQueue( audioEvents );

  // and only now do we signal to start the clock
  // m_driver->startClocks();

  incrementTransportToken();

  return true; // !m_isEndOfCompReached;
}

bool RosegardenSequencer::keepPlaying() {
  Profiler profiler( "RosegardenSequencer::keepPlaying" );

  MappedEventList c;

  RealTime fetchEnd = m_songPosition + m_readAhead;
  if( isLooping() && fetchEnd >= m_loopEnd ) {
    fetchEnd = m_loopEnd - RealTime( 0, 1 );
  }
  if( fetchEnd > m_lastFetchSongPosition ) {
    fetchEvents( c, m_lastFetchSongPosition, fetchEnd, false );
  }

  // Again, process whether we need to or not to keep
  // the Sequencer up-to-date with audio events
  //
  // m_driver->processEventsOut( c, m_lastFetchSongPosition,
  //                            fetchEnd );

  if( fetchEnd > m_lastFetchSongPosition ) {
    m_lastFetchSongPosition = fetchEnd;
  }

  return true; // !m_isEndOfCompReached; - until we sort this
               // out, we don't stop at end of comp.
}

// Return current Sequencer time in GUI compatible terms
//
void RosegardenSequencer::updateClocks() {}

void RosegardenSequencer::sleep( const RealTime &rt ) {
  // m_driver->sleep( rt );
}

void RosegardenSequencer::processRecordedMidi() {}

void RosegardenSequencer::routeEvents(
    MappedEventList *mappedEventList, bool recording ) {}

// Send an update
//
void RosegardenSequencer::processRecordedAudio() {
  // Nothing to do here: the recording time is sent back to the
  // GUI in the sequencer mapper as a normal case.
}

void RosegardenSequencer::processAsynchronousEvents() {}

void RosegardenSequencer::applyFiltering(
    MappedEventList *mC, MidiFilter filter,
    bool filterControlDevice ) {
  // For each event in the list
  for( MappedEventList::iterator i = mC->begin(); i != mC->end();
       /* increment in loop */ ) {
    // Hold on to the current event for processing.
    MappedEventList::iterator j = i;
    // Move to the next in case the current is erased.
    ++i;

    // If this event matches the filter, erase it from the list
    if( ( ( *j )->getType() & filter ) ||
        ( filterControlDevice && ( ( *j )->getRecordedDevice() ==
                                   Device::CONTROL_DEVICE ) ) ) {
      mC->erase( j );
    }
  }
}

// Initialise the virtual studio with a few audio faders and
// create a plugin manager.  For the moment this is pretty
// arbitrary but eventually we'll drive this from the gui
// and rg file "Studio" entries.
//
void RosegardenSequencer::initialiseStudio() {
  // clear down the studio before we start adding anything
  //
  m_studio->clear();
}

void RosegardenSequencer::checkForNewClients() {
  // Don't do this check if any of these conditions hold
  //
  if( m_transportStatus == PLAYING ||
      m_transportStatus == RECORDING )
    return;

  // if( m_driver->checkForNewClients() ) {}
}

void RosegardenSequencer::rationalisePlayingAudio() {
  std::vector<MappedEvent> audioEvents;
  m_metaIterator.getAudioEvents( audioEvents );
  // m_driver->initialiseAudioQueue( audioEvents );
}

ExternalTransport::TransportToken
RosegardenSequencer::transportChange(
    TransportRequest request ) {
  TransportPair pair( request, RealTime::zeroTime );
  m_transportRequests.push_back( pair );

  if( request == TransportNoChange )
    return m_transportToken;
  else
    return m_transportToken + 1;
}

ExternalTransport::TransportToken
RosegardenSequencer::transportJump( TransportRequest request,
                                    RealTime         rt ) {
  TransportPair pair( request, rt );
  m_transportRequests.push_back( pair );

  if( request == TransportNoChange )
    return m_transportToken + 1;
  else
    return m_transportToken + 2;
}

bool RosegardenSequencer::isTransportSyncComplete(
    TransportToken token ) {
  return m_transportToken >= token;
}

void RosegardenSequencer::incrementTransportToken() {
  ++m_transportToken;
}

void RosegardenSequencer::slotControlChange(
    Instrument *instrument, int cc ) {
  if( !instrument ) return;

  // MIDI
  if( instrument->getType() == Instrument::Midi ) {
    // RG_DEBUG << "slotControlChange(): cc = " << cc << " value
    // = " << instrument->getControllerValue(cc);

    instrument->sendController(
        cc, instrument->getControllerValue( cc ) );

    return;
  }

  // Audio or SoftSynth
  // if( instrument->getType() == Instrument::Audio ||
  //    instrument->getType() == Instrument::SoftSynth ) {
  //  if( cc == MIDI_CONTROLLER_VOLUME ) {
  //    setMappedProperty( instrument->getMappedId(),
  //                       MappedAudioFader::FaderLevel,
  //                       instrument->getLevel() );

  //  } else if( cc == MIDI_CONTROLLER_PAN ) {
  //    setMappedProperty(
  //        instrument->getMappedId(), MappedAudioFader::Pan,
  //        static_cast<float>( instrument->getPan() ) - 100 );
  //  }

  //  return;
  //}
}

} // namespace Rosegarden
