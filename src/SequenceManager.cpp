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

#define RG_MODULE_STRING "[SequenceManager]"

//#define DEBUG_SEQUENCE_MANAGER 1
#if !defined DEBUG_SEQUENCE_MANAGER
#  define RG_NO_DEBUG_PRINT 1
#endif

#include "SequenceManager.h"

#include "ControlBlock.h"
#include "Midi.h" // for MIDI_SYSTEM_RESET
//#include "misc/Strings.h"  // for qStrToBool()
//#include "ConfigGroups.h"
#include "Composition.h"
#include "CompositionMapper.h"
#include "Device.h"
#include "Exception.h"
#include "Instrument.h"
#include "MidiProgram.h" // for MidiFilter
#include "RealTime.h"
#include "RosegardenDocument.h"
#include "Segment.h"
#include "Studio.h"
#include "Track.h"
#include "TriggerSegment.h"
//#include "document/CommandHistory.h"
//#include "gui/dialogs/AudioManagerDialog.h"
//#include "gui/dialogs/CountdownDialog.h"
//#include "gui/application/RosegardenMainWindow.h"
//#include "gui/widgets/StartupLogo.h"
#include "StudioControl.h"
//#include "gui/widgets/WarningWidget.h"
#include "MarkerMapper.h"
#include "RosegardenSequencer.h"
//#include "MetronomeMapper.h"
#include "TempoSegmentMapper.h"
#include "TimeSigSegmentMapper.h"
//#include "sound/AudioFile.h"  // For AudioFileId
#include "MappedEvent.h"
#include "MappedEventList.h"
//#include "MappedInstrument.h"

//#include "rosegarden-version.h"  // for VERSION

#include <algorithm>
#include <utility> // For std::pair.

namespace Rosegarden {

SequenceManager::SequenceManager()
  : m_doc( nullptr ),
    // m_soundDriverStatus(NO_DRIVER),
    m_compositionMapper(),
    m_metronomeMapper( nullptr ),
    m_tempoSegmentMapper( nullptr ),
    m_timeSigSegmentMapper( nullptr ),
    m_refreshRequested( true ),
    m_segments(),
    m_triggerSegments(),
    m_addedSegments(),
    m_removedSegments(),
    m_shownOverrunWarning( false ),
    // m_reportTimer(nullptr),
    m_canReport( true ),
    m_transportStatus( STOPPED ),
    m_lastRewoundAt( clock() ),
    m_countdownDialog( nullptr ),
    // m_countdownTimer(nullptr),
    // m_recordTime(new QTime()),
    m_lastTransportStartPosition( 0 ),
    m_sampleRate( 0 ),
    m_tempo( 0 ) {}

SequenceManager::~SequenceManager() {}

void SequenceManager::setDocument( RosegardenDocument *doc ) {
  m_doc = doc;
  m_doc->setSequenceManager( this );
}

void SequenceManager::stop() {}

void SequenceManager::rewind() {
  if( !m_doc ) return;

  Composition &composition = m_doc->getComposition();

  timeT position = composition.getPosition();

  // Subtract one from position to make sure we go back one bar
  // if we are stopped and sitting at the beginning of a bar.
  std::pair<timeT, timeT> barRange =
      composition.getBarRangeForTime( position - 1 );

  if( m_transportStatus == PLAYING ) {
    // Compute elapsed time since last rewind press.
    clock_t now = clock();
    int     elapsed =
        ( now - m_lastRewoundAt ) * 1000 / CLOCKS_PER_SEC;

    // RG_DEBUG << "rewind(): That was " << m_lastRewoundAt << ",
    // this is " << now << ", elapsed is " << elapsed;

    // If we had a rewind press less than 200ms ago...
    if( elapsed >= 0 && elapsed <= 200 ) {
      timeT halfway = barRange.first +
                      ( barRange.second - barRange.first ) / 2;

      // If we're less than half way into the bar
      if( position <= halfway ) {
        // Rewind an additional bar.
        barRange =
            composition.getBarRangeForTime( barRange.first - 1 );
      }
    }

    m_lastRewoundAt = now;
  }

  if( barRange.first < composition.getStartMarker() ) {
    m_doc->slotSetPointerPosition(
        composition.getStartMarker() );
  } else {
    m_doc->slotSetPointerPosition( barRange.first );
  }
}

void SequenceManager::fastforward() {
  if( !m_doc ) return;

  Composition &composition = m_doc->getComposition();

  timeT position    = composition.getPosition();
  timeT newPosition = composition.getBarEndForTime( position );

  // Don't skip past end marker.
  if( newPosition > composition.getEndMarker() )
    newPosition = composition.getEndMarker();

  m_doc->slotSetPointerPosition( newPosition );
}

void SequenceManager::jumpTo( const RealTime &time ) {}

void SequenceManager::record( bool toggled ) {}

void SequenceManager::processAsynchronousMidi(
    const MappedEventList &mC,
    AudioManagerDialog *   audioManagerDialog ) {}

void SequenceManager::rewindToBeginning() {
  m_doc->slotSetPointerPosition(
      m_doc->getComposition().getStartMarker() );
}

void SequenceManager::fastForwardToEnd() {
  Composition &comp = m_doc->getComposition();
  m_doc->slotSetPointerPosition( comp.getEndMarker() );
}

void SequenceManager::setLoop( const timeT &lhs,
                               const timeT &rhs ) {
  // !!!  So who disabled the following, why?  Are loops with
  // JACK transport
  //      sync no longer hideously broken?
  //
  // do not set a loop if JACK transport sync is enabled, because
  // this is completely broken, and apparently broken due to a
  // limitation of JACK transport itself.  #1240039 - DMM
  //    QSettings settings;
  //    settings.beginGroup( SequencerOptionsConfigGroup );
  //

  //    if ( qStrToBool( settings.value("jacktransport", "false"
  //    ) ) )
  //    {
  //	// !!! message box should go here to inform user of why
  // the loop was
  //	//     not set, but I can't add it at the moment due to
  // to the pre-release
  //	//     freeze - DMM
  //    settings.endGroup();
  //	return;
  //    }

  RealTime loopStart =
      m_doc->getComposition().getElapsedRealTime( lhs );
  RealTime loopEnd =
      m_doc->getComposition().getElapsedRealTime( rhs );
}

// void SequenceManager::checkSoundDriverStatus( bool warnUser )
// {
//  // Update local copy of status.
//  // ??? Can we get rid of this member?  Only record() uses it.
//  // Why
//  //     not just let record() call
//  getSoundDriverStatus(VERSION)
//  //     directly? Then we can get rid of all the callers that
//  //     call this with warnUser set to false.  Then we can get
//  //     rid of warnUser.  No, it's worse than that.  There's
//  //     also a getSoundDriverStatus() in here that provides
//  //     access to this copy.  We would have to that over to
//  //     providing RosegardenSequencer::getSoundDriverStatus().
//  //     Then probably inline that into each caller.  It's
//  //     doable, but a bit more involved than it appears at
//  first
//  //     glance.  I'm also a little worried that this local
//  //     status has more values than the RosegardenSequencer
//  one.
//  //     Or perhaps it is out of sync and there's a reason.
//  // m_soundDriverStatus =
//  // RosegardenSequencer::getInstance()->getSoundDriverStatus(
//  //        VERSION );

//  RG_DEBUG << "Sound driver status:"
//           << "MIDI"
//           << ( ( ( m_soundDriverStatus & MIDI_OK ) != 0 )
//                    ? "ok"
//                    : "NOT OK" )
//           << "|"
//           << "Audio"
//           << ( ( ( m_soundDriverStatus & AUDIO_OK ) != 0 )
//                    ? "ok"
//                    : "NOT OK" )
//           << "|"
//           << "Version"
//           << ( ( ( m_soundDriverStatus & VERSION_OK ) != 0 )
//                    ? "ok"
//                    : "NOT OK" );

//  if( !warnUser ) return;

//#ifdef HAVE_LIBJACK
//  if( ( m_soundDriverStatus &
//        ( AUDIO_OK | MIDI_OK | VERSION_OK ) ) ==
//      ( AUDIO_OK | MIDI_OK | VERSION_OK ) )
//    return;
//#else
//  if( ( m_soundDriverStatus & ( MIDI_OK | VERSION_OK ) ) ==
//      ( MIDI_OK | VERSION_OK ) )
//    return;
//#endif

//  StartupLogo::hideIfStillThere();

//  QString text;
//  QString informativeText;

//  if( m_soundDriverStatus == NO_DRIVER ) {
//    text = tr( "<h3>Sequencer engine unavailable!</h3>" );
//    informativeText = tr(
//        "<p>Both MIDI and Audio subsystems have failed to "
//        "initialize.</p><p>If you wish to run with no sequencer
//        " "by design, then use \"rosegarden --nosound\" to
//        avoid " "seeing this error in the future.  Otherwise,
//        we " "recommend that you repair your system
//        configuration " "and start Rosegarden again.</p>" );
//  } else if( !( m_soundDriverStatus & MIDI_OK ) ) {
//    text = tr( "<h3>MIDI sequencing unavailable!</h3>" );
//    informativeText = tr(
//        "<p>The MIDI subsystem has failed to "
//        "initialize.</p><p>You may continue without the "
//        "sequencer, but we suggest closing Rosegarden, running
//        "
//        "\"modprobe snd-seq-midi\" as root, and starting "
//        "Rosegarden again.  If you wish to run with no "
//        "sequencer by design, then use \"rosegarden --nosound\"
//        " "to avoid seeing this error in the future.</p>" );
//  }

//  if( !text.isEmpty() ) {
//    sendWarning( WarningWidget::Midi, text,
//                      informativeText );
//    return;
//  }

//#ifdef HAVE_LIBJACK

//  if( !( m_soundDriverStatus & AUDIO_OK ) ) {
//    // This is to avoid us ever showing the same dialog more
//    than
//    // once during a single run of the program -- it's quite
//    // separate from the suppression function
//    // ??? But this routine is only ever called with "true"
//    once.
//    // From
//    //     RMW's ctor.  There is no need for this.
//    static bool showJackWarning = true;

//    if( showJackWarning ) {
//      text =
//          tr( "<h3>Audio sequencing and synth plugins "
//              "unavailable!</h3>" );
//      informativeText = tr(
//          "<p>Rosegarden could not connect to the JACK audio "
//          "server.  This probably means that Rosegarden was "
//          "unable to start the audio server due to a problem "
//          "with your configuration, your system installation, "
//          "or both.</p><p>If you want to be able to play or "
//          "record audio files or use plugins, we suggest that "
//          "you exit Rosegarden and use the JACK Control utility
//          "
//          "(qjackctl) to try different settings until you "
//          "arrive at a configuration that permits JACK to "
//          "start.  You may also need to install a realtime "
//          "kernel, edit your system security configuration, and
//          " "so on.  Unfortunately, this is an extremely
//          complex " "subject.</p><p> Once you establish a
//          working JACK " "configuration, Rosegarden will be
//          able to start the " "audio server automatically in
//          the future.</p>" );
//      sendWarning( WarningWidget::Audio, text,
//                        informativeText );

//      showJackWarning = false;
//    }
//  }
//#endif
//}

// void SequenceManager::preparePlayback() {
//  // ??? rename: setMappedInstruments()

//  // ??? Where does this function really belong?  It iterates
//  // over the
//  //     Instrument's in the Studio and calls
//  //     RosegardenSequencer::setMappedInstrument().  Seems
//  like
//  //     RosegardenSequencer might be a better place.  Or would
//  //     Studio make more sense?

//  Studio &             studio = m_doc->getStudio();
//  const InstrumentList list   = studio.getAllInstruments();

//  // Send the MappedInstruments full information to the
//  Sequencer InstrumentList::const_iterator it = list.begin();
//  for( ; it != list.end(); ++it ) {
//    StudioControl::sendMappedInstrument(
//        MappedInstrument( *it ) );
//  }
//}

void SequenceManager::resetMidiNetwork() {
  MappedEventList mC;

  // ??? This should send resets on all MIDI channels on all MIDI
  //     Device's.  As it is now, it only does the first Device.

  for( unsigned int i = 0; i < 16; ++i ) {
    MappedEvent *mE = new MappedEvent(
        MidiInstrumentBase + i, MappedEvent::MidiController,
        MIDI_SYSTEM_RESET, 0 );

    mC.insert( mE );

    // Display the first one on the TransportDialog.
    // if( i == 0 ) signalMidiOutLabel( mE );
  }

  // Send it out.
  StudioControl::sendMappedEventList( mC );
}

void SequenceManager::reinitialiseSequencerStudio() {}

void SequenceManager::panic() {}

void SequenceManager::setTempo( const tempoT tempo ) {
  if( m_tempo == tempo ) return;
  m_tempo = tempo;

  // Send the quarter note length to the sequencer.
  // Quarter Note Length is sent (MIDI CLOCK) at 24ppqn.
  //
  double   qnD    = 60.0 / Composition::getTempoQpm( tempo );
  RealTime qnTime = RealTime(
      long( qnD ),
      long( ( qnD - double( long( qnD ) ) ) * 1000000000.0 ) );

  StudioControl::sendQuarterNoteLength( qnTime );

  // set the tempo in the transport
  // signalTempoChanged( tempo );
}

void SequenceManager::resetCompositionMapper() {
  RosegardenSequencer::getInstance()
      ->compositionAboutToBeDeleted();

  m_compositionMapper.reset( new CompositionMapper( m_doc ) );

  resetMetronomeMapper();
  resetTempoSegmentMapper();
  resetTimeSigSegmentMapper();

  // Reset ControlBlock.
  ControlBlock::getInstance()->setDocument( m_doc );
}

void SequenceManager::populateCompositionMapper() {
  Composition &comp = m_doc->getComposition();

  for( Composition::iterator i = comp.begin(); i != comp.end();
       ++i ) {
    segmentAdded( *i );
  }

  for( Composition::triggersegmentcontaineriterator i =
           comp.getTriggerSegments().begin();
       i != comp.getTriggerSegments().end(); ++i ) {
    m_triggerSegments.insert( SegmentRefreshMap::value_type(
        ( *i )->getSegment(),
        ( *i )->getSegment()->getNewRefreshStatusId() ) );
  }
}
void SequenceManager::resetMetronomeMapper() {
  // if( m_metronomeMapper ) {
  //  RosegardenSequencer::getInstance()->segmentAboutToBeDeleted(
  //      std::static_pointer_cast<Rosegarden::MappedEventBuffer>(
  //          m_metronomeMapper ) );
  //}

  // m_metronomeMapper = std::shared_ptr<MetronomeMapper>(
  //    new MetronomeMapper( m_doc ) );
  // RosegardenSequencer::getInstance()->segmentAdded(
  //    m_metronomeMapper );
}

void SequenceManager::resetTempoSegmentMapper() {
  if( m_tempoSegmentMapper ) {
    RosegardenSequencer::getInstance()->segmentAboutToBeDeleted(
        std::static_pointer_cast<Rosegarden::MappedEventBuffer>(
            m_tempoSegmentMapper ) );
  }

  m_tempoSegmentMapper = std::shared_ptr<TempoSegmentMapper>(
      new TempoSegmentMapper( m_doc ) );
  RosegardenSequencer::getInstance()->segmentAdded(
      m_tempoSegmentMapper );
}

void SequenceManager::resetTimeSigSegmentMapper() {
  if( m_timeSigSegmentMapper ) {
    RosegardenSequencer::getInstance()->segmentAboutToBeDeleted(
        std::static_pointer_cast<Rosegarden::MappedEventBuffer>(
            m_timeSigSegmentMapper ) );
  }

  m_timeSigSegmentMapper = std::shared_ptr<TimeSigSegmentMapper>(
      new TimeSigSegmentMapper( m_doc ) );
  RosegardenSequencer::getInstance()->segmentAdded(
      m_timeSigSegmentMapper );
}

// bool SequenceManager::event( QEvent *e ) {
//  if( e->type() == QEvent::User ) {
//    if( m_refreshRequested ) {
//      refresh();
//      m_refreshRequested = false;
//    }
//    return true;
//  } else {
//    return QObject::event( e );
//  }
//}

void SequenceManager::update() {
  //// schedule a refresh-status check for the next event loop
  // QEvent *e = new QEvent( QEvent::User );
  //// Let the handler know we want a refresh().
  //// ??? But we always want a refresh().  When wouldn't we? Are
  ////     we getting QEvent::User events from elsewhere?
  // m_refreshRequested = true;
  // QApplication::postEvent( this, e );
}

void SequenceManager::refresh() {
  Composition &comp = m_doc->getComposition();

  // ??? See Segment::m_refreshStatusArray for insight into what
  // this
  //     is doing.

  // Look at trigger segments first: if one of those has changed,
  // we'll need to be aware of it when scanning segments
  // subsequently

  // List of Segments modified by changes to trigger Segments.
  // These will need a refresh.
  // ??? Instead of gathering these, can't we just set the
  // refresh
  //     status for the Segment?  Or would that cause others to
  //     do refreshes?  Can we just set *our* refresh status?
  // ??? rename: triggerRefreshSet
  TriggerSegmentRec::SegmentRuntimeIdSet ridset;

  // List of all trigger Segments.
  SegmentRefreshMap newTriggerMap;

  // For each trigger Segment in the composition
  for( Composition::triggersegmentcontaineriterator i =
           comp.getTriggerSegments().begin();
       i != comp.getTriggerSegments().end(); ++i ) {
    Segment *s = ( *i )->getSegment();

    // If we don't have this one
    if( m_triggerSegments.find( s ) ==
        m_triggerSegments.end() ) {
      // Make a new trigger Segment entry.
      newTriggerMap[s] = s->getNewRefreshStatusId();
    } else {
      // Use the existing entry.
      newTriggerMap[s] = m_triggerSegments[s];
    }

    // If this trigger Segment needs a refresh
    if( s->getRefreshStatus( newTriggerMap[s] )
            .needsRefresh() ) {
      // Collect all the Segments this will affect.
      TriggerSegmentRec::SegmentRuntimeIdSet &thisSet =
          ( *i )->getReferences();
      ridset.insert( thisSet.begin(), thisSet.end() );

      // Clear the trigger Segment's refresh flag.
      s->getRefreshStatus( newTriggerMap[s] )
          .setNeedsRefresh( false );
    }
  }

  m_triggerSegments = newTriggerMap;

#ifdef DEBUG_SEQUENCE_MANAGER
  int x = 0;
  for( TriggerSegmentRec::SegmentRuntimeIdSet::iterator i =
           ridset.begin();
       i != ridset.end(); ++i ) {
    ++x;
  }
#endif

  std::vector<Segment *>::iterator i;

  // Removed Segments

  for( i = m_removedSegments.begin();
       i != m_removedSegments.end(); ++i ) {
    segmentDeleted( *i );
  }
  m_removedSegments.clear();

  // Current Segments

  for( SegmentRefreshMap::iterator i = m_segments.begin();
       i != m_segments.end(); ++i ) {
    // If this Segment needs a refresh
    if( i->first->getRefreshStatus( i->second ).needsRefresh() ||
        ridset.find( i->first->getRuntimeId() ) !=
            ridset.end() ) {
      segmentModified( i->first );
      i->first->getRefreshStatus( i->second )
          .setNeedsRefresh( false );
    }
  }

  // Added Segments

  for( i = m_addedSegments.begin(); i != m_addedSegments.end();
       ++i ) {
    segmentAdded( *i );
  }
  m_addedSegments.clear();
}

void SequenceManager::segmentModified( Segment *s ) {
  bool sizeChanged = m_compositionMapper->segmentModified( s );

  RosegardenSequencer::getInstance()->segmentModified(
      m_compositionMapper->getMappedEventBuffer( s ) );
}

void SequenceManager::segmentAdded( const Composition *,
                                    Segment *s ) {
  m_addedSegments.push_back( s );
}

void SequenceManager::segmentRemoved( const Composition *,
                                      Segment *s ) {
  // !!! WARNING !!!
  // The segment pointer "s" is about to be deleted by
  // Composition::deleteSegment(Composition::iterator).  After
  // this routine ends, this pointer cannot be dereferenced.
  m_removedSegments.push_back( s );

  std::vector<Segment *>::iterator i = std::find(
      m_addedSegments.begin(), m_addedSegments.end(), s );
  if( i != m_addedSegments.end() ) {
    m_addedSegments.erase( i );
  }
}

void SequenceManager::segmentRepeatChanged( const Composition *,
                                            Segment *s,
                                            bool     repeat ) {
  segmentModified( s );
}

void SequenceManager::segmentRepeatEndChanged(
    const Composition *, Segment *s, timeT newEndTime ) {
  segmentModified( s );
}

void SequenceManager::segmentEventsTimingChanged(
    const Composition *, Segment *s, timeT t, RealTime ) {
  segmentModified( s );
  if( s && s->getType() == Segment::Audio &&
      m_transportStatus == PLAYING ) {
    RosegardenSequencer::getInstance()->remapTracks();
  }
}

void SequenceManager::segmentTransposeChanged(
    const Composition *, Segment *s, int transpose ) {
  segmentModified( s );
}

void SequenceManager::segmentTrackChanged( const Composition *,
                                           Segment *s,
                                           TrackId  id ) {
  segmentModified( s );
  if( s && s->getType() == Segment::Audio &&
      m_transportStatus == PLAYING ) {
    RosegardenSequencer::getInstance()->remapTracks();
  }
}

void SequenceManager::segmentEndMarkerChanged(
    const Composition *, Segment *s, bool ) {
  segmentModified( s );
}

void SequenceManager::segmentInstrumentChanged( Segment *s ) {
  // Quick and dirty: Redo the whole segment.
  segmentModified( s );
}

void SequenceManager::segmentAdded( Segment *s ) {
  m_compositionMapper->segmentAdded( s );

  RosegardenSequencer::getInstance()->segmentAdded(
      m_compositionMapper->getMappedEventBuffer( s ) );

  // Add to segments map
  int id = s->getNewRefreshStatusId();
  m_segments.insert( SegmentRefreshMap::value_type( s, id ) );
}

void SequenceManager::segmentDeleted( Segment *s ) {
  // !!! WARNING !!!
  // The "s" segment pointer that is coming in to this routine
  // has already been deleted.  This is a POINTER TO DELETED
  // MEMORY.  It cannot be dereferenced in any way.  Each of the
  // following lines of code will be explained to make it clear
  // that the pointer is not being dereferenced.
  // ??? This needs to be fixed.  Passing around pointers that
  // point to
  //     nowhere is just asking for trouble.  E.g. what if the
  //     same memory address is allocated to a new Segment, that
  //     Segment is added, then this routine is called for the
  //     old Segment?  We remove the new Segment.

  {
    // getMappedEventBuffer() uses the segment pointer value as
    // an index into a map.  So this is not a dereference.
    std::shared_ptr<MappedEventBuffer> mapper =
        m_compositionMapper->getMappedEventBuffer( s );
    // segmentDeleted() has been reviewed and should only be
    // using the pointer as an index into a container.
    // segmentDeleted() doesn't delete the mapper, which the
    // metaiterators own.
    m_compositionMapper->segmentDeleted( s );
    RosegardenSequencer::getInstance()->segmentAboutToBeDeleted(
        mapper );
    // Now mapper may have been deleted.
  }
  // Remove from segments map
  // This uses "s" as an index.  It is not dereferenced.
  m_segments.erase( s );
}

void SequenceManager::endMarkerTimeChanged( const Composition *,
                                            bool /*shorten*/ ) {
  if( m_transportStatus == RECORDING ) {
    // When recording, we just need to extend the metronome
    // segment to include the new bars.
    // ??? This is pretty extreme as it destroys and recreates
    // the
    //     segment.  Can't we just add events to the existing
    //     segment instead?  Maybe always have one or two bars
    //     extra so there is no interruption.  As it is, this
    //     will likely cause an interruption in metronome events
    //     when the composition is expanded.
    resetMetronomeMapper();
  } else {
    // Reset the composition mapper.  The main thing this does is
    // update the metronome segment.  There appear to be other
    // important things that need to be done as well.
    slotScheduledCompositionMapperReset();
  }
}

void SequenceManager::timeSignatureChanged(
    const Composition * ) {
  resetMetronomeMapper();
}

void SequenceManager::tracksAdded(
    const Composition *c, std::vector<TrackId> &trackIds ) {
  // For each track added, call ControlBlock::updateTrackData()
  for( unsigned i = 0; i < trackIds.size(); ++i ) {
    Track *t = c->getTrackById( trackIds[i] );
    ControlBlock::getInstance()->updateTrackData( t );

    // ??? Can we move this out of this for loop and call it once
    // after
    //     we are done calling updateTrackData() for each track?
    if( m_transportStatus == PLAYING ) {
      RosegardenSequencer::getInstance()->remapTracks();
    }
  }
}

void SequenceManager::trackChanged( const Composition *,
                                    Track *t ) {
  ControlBlock::getInstance()->updateTrackData( t );

  if( m_transportStatus == PLAYING ) {
    RosegardenSequencer::getInstance()->remapTracks();
  }
}

void SequenceManager::tracksDeleted(
    const Composition *, std::vector<TrackId> &trackIds ) {
  for( unsigned i = 0; i < trackIds.size(); ++i ) {
    ControlBlock::getInstance()->setTrackDeleted( trackIds[i],
                                                  true );
  }
}

void SequenceManager::metronomeChanged( InstrumentId id,
                                        bool regenerateTicks ) {
  // This method is called when the user has changed the
  // metronome instrument, pitch etc

  if( regenerateTicks ) resetMetronomeMapper();

  Composition &comp = m_doc->getComposition();
  ControlBlock::getInstance()->setInstrumentForMetronome( id );

  if( m_transportStatus == PLAYING ) {
    ControlBlock::getInstance()->setMetronomeMuted(
        !comp.usePlayMetronome() );
  } else {
    ControlBlock::getInstance()->setMetronomeMuted(
        !comp.useRecordMetronome() );
  }

  // m_metronomeMapper->refresh();
  m_timeSigSegmentMapper->refresh();
  m_tempoSegmentMapper->refresh();
}

void SequenceManager::metronomeChanged(
    const Composition *comp ) {
  // This method is called when the muting status in the
  // composition has changed -- the metronome itself has not
  // actually changed

  if( !comp ) comp = &m_doc->getComposition();
  // ControlBlock::getInstance()->setInstrumentForMetronome(
  //    m_metronomeMapper->getMetronomeInstrument() );

  if( m_transportStatus == PLAYING ) {
    ControlBlock::getInstance()->setMetronomeMuted(
        !comp->usePlayMetronome() );
  } else {
    ControlBlock::getInstance()->setMetronomeMuted(
        !comp->useRecordMetronome() );
  }
}

void SequenceManager::filtersChanged( MidiFilter thruFilter,
                                      MidiFilter recordFilter ) {
  ControlBlock::getInstance()->setThruFilter( thruFilter );
  ControlBlock::getInstance()->setRecordFilter( recordFilter );
}

void SequenceManager::selectedTrackChanged(
    const Composition *composition ) {
  TrackId selectedTrackId = composition->getSelectedTrack();
  ControlBlock::getInstance()->setSelectedTrack(
      selectedTrackId );
}

void SequenceManager::tempoChanged( const Composition *c ) {
  // Refresh all segments
  //
  for( SegmentRefreshMap::iterator i = m_segments.begin();
       i != m_segments.end(); ++i ) {
    segmentModified( i->first );
  }

  // and metronome, time sig and tempo
  //
  // m_metronomeMapper->refresh();
  m_timeSigSegmentMapper->refresh();
  m_tempoSegmentMapper->refresh();

  if( c->isLooping() )
    setLoop( c->getLoopStart(), c->getLoopEnd() );
  else if( m_transportStatus == PLAYING ) {
    // Tempo has changed during playback.

    // Reset the playback position because the sequencer keeps
    // track of position in real time (seconds) and we want to
    // maintain the same position in musical time (bars/beats).
    m_doc->slotSetPointerPosition( c->getPosition() );
  }
}

// void SequenceManager::sendTransportControlStatuses() {
//  // ??? static function.  Where does this really belong?  I
//  // suspect
//  //     RosegardenSequencer.

//  QSettings settings;
//  //settings.beginGroup( SequencerOptionsConfigGroup );

//  // Get the settings values
//  //
//  bool jackTransport =
//      qStrToBool( settings.value( "jacktransport", "false" ) );
//  bool jackMaster =
//      qStrToBool( settings.value( "jackmaster", "false" ) );

//  int mmcMode = settings.value( "mmcmode", 0 ).toInt();
//  int mtcMode = settings.value( "mtcmode", 0 ).toInt();

//  int  midiClock    = settings.value( "midiclock", 0 ).toInt();
//  bool midiSyncAuto = qStrToBool(
//      settings.value( "midisyncautoconnect", "false" ) );

//  // Send JACK transport
//  //
//  int jackValue = 0;
//  if( jackTransport && jackMaster )
//    jackValue = 2;
//  else {
//    if( jackTransport )
//      jackValue = 1;
//    else
//      jackValue = 0;
//  }

//  MappedEvent mEjackValue( MidiInstrumentBase, // InstrumentId
//                           MappedEvent::SystemJackTransport,
//                           MidiByte( jackValue ) );
//  StudioControl::sendMappedEvent( mEjackValue );

//  // Send MMC transport
//  //
//  MappedEvent mEmmcValue( MidiInstrumentBase, // InstrumentId
//                          MappedEvent::SystemMMCTransport,
//                          MidiByte( mmcMode ) );

//  StudioControl::sendMappedEvent( mEmmcValue );

//  // Send MTC transport
//  //
//  MappedEvent mEmtcValue( MidiInstrumentBase, // InstrumentId
//                          MappedEvent::SystemMTCTransport,
//                          MidiByte( mtcMode ) );

//  StudioControl::sendMappedEvent( mEmtcValue );

//  // Send MIDI Clock
//  //
//  MappedEvent mEmidiClock( MidiInstrumentBase, // InstrumentId
//                           MappedEvent::SystemMIDIClock,
//                           MidiByte( midiClock ) );

//  StudioControl::sendMappedEvent( mEmidiClock );

//  // Send MIDI Sync Auto-Connect
//  //
//  MappedEvent mEmidiSyncAuto( MidiInstrumentBase, //
//  InstrumentId
//                              MappedEvent::SystemMIDISyncAuto,
//                              MidiByte( midiSyncAuto ? 1 : 0 )
//                              );

//  StudioControl::sendMappedEvent( mEmidiSyncAuto );

//  settings.endGroup();
//}

void SequenceManager::slotCountdownTimerTimeout() {
  // Set the elapsed time in seconds
  //
  // m_countdownDialog->setElapsedTime( m_recordTime->elapsed() /
  //                                   1000 );
}

void SequenceManager::slotScheduledCompositionMapperReset() {
  // ??? Inline into only caller.
  resetCompositionMapper();
  populateCompositionMapper();
}

int SequenceManager::getSampleRate() const {
  // Get from cache if it's there.
  if( m_sampleRate != 0 ) return m_sampleRate;

  // Cache the result to avoid locks.
  m_sampleRate =
      RosegardenSequencer::getInstance()->getSampleRate();

  return m_sampleRate;
}

bool SequenceManager::shouldWarnForImpreciseTimer() {
  // QSettings settings;
  // settings.beginGroup( SequencerOptionsConfigGroup );

  // QString timer = settings.value( "timer" ).toString();
  // settings.endGroup();

  // if( timer == "(auto)" || timer == "" )
  //  return true;
  // else
  //  return false; // if the user has chosen the timer, leave
  //  them
  //                // alone
  return false;
}

// Return a new metaiterator on the current composition (suitable
// for MidiFile)
MappedBufMetaIterator *SequenceManager::makeTempMetaiterator() {
  MappedBufMetaIterator *metaiterator =
      new MappedBufMetaIterator;
  // Add the mappers we know of.  Not the metronome because we
  // don't export that.
  metaiterator->addSegment( m_tempoSegmentMapper );
  metaiterator->addSegment( m_timeSigSegmentMapper );
  // We don't hold on to the marker mapper because we only use it
  // when exporting.
  metaiterator->addSegment( std::shared_ptr<MarkerMapper>(
      new MarkerMapper( m_doc ) ) );
  typedef CompositionMapper::SegmentMappers container;
  typedef container::iterator               iterator;
  container &                               mapperContainer =
      m_compositionMapper->m_segmentMappers;
  for( iterator i = mapperContainer.begin();
       i != mapperContainer.end(); ++i ) {
    metaiterator->addSegment( i->second );
  }
  return metaiterator;
  // m_compositionMapper
  // m_tempoSegmentMapper
  // m_timeSigSegmentMapper
}

} // namespace Rosegarden
