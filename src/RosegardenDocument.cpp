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

#define RG_MODULE_STRING "[RosegardenDocument]"
#include "RosegardenDocument.h"

//#include "CommandHistory.h"
#include "GzipFile.h"
#include "RoseXmlHandler.h"

#include "AudioDevice.h"
//#include "AudioPluginInstance.h"
#include "BaseProperties.h"
#include "Composition.h"
#include "Configuration.h"
#include "Device.h"
#include "Event.h"
#include "Exception.h"
#include "Instrument.h"
#include "MidiDevice.h"
#include "MidiProgram.h"
#include "MidiTypes.h"
#include "NotationTypes.h"
#include "Profiler.h"
#include "RealTime.h"
#include "Segment.h"
#include "SegmentLinker.h"
#include "SoftSynthDevice.h"
#include "Studio.h"
#include "Track.h"
#include "XmlExportable.h"
//#include "commands/edit/EventQuantizeCommand.h"
//#include "commands/notation/NormalizeRestsCommand.h"
//#include "commands/segment/AddTracksCommand.h"
//#include "commands/segment/SegmentInsertCommand.h"
//#include "commands/segment/SegmentRecordCommand.h"
//#include "commands/segment/ChangeCompositionLengthCommand.h"
//#include "gui/editors/segment/TrackEditor.h"
//#include "gui/editors/segment/TrackButtons.h"
//#include "gui/general/ClefIndex.h"
//#include "gui/application/TransportStatus.h"
//#include "gui/application/RosegardenMainWindow.h"
//#include "gui/application/RosegardenMainViewWidget.h"
//#include "gui/dialogs/UnusedAudioSelectionDialog.h"
//#include
//"gui/editors/segment/compositionview/AudioPeaksThread.h"
//#include "gui/editors/segment/TrackLabel.h"
//#include "gui/general/EditViewBase.h"
//#include "gui/general/GUIPalette.h"
//#include "gui/general/ResourceFinder.h"
//#include "gui/widgets/StartupLogo.h"
#include "SequenceManager.h"
//#include "gui/studio/AudioPluginManager.h"
#include "StudioControl.h"
//#include "gui/general/AutoSaveFinder.h"
#include "RosegardenSequencer.h"
//#include "sound/AudioFile.h"
//#include "sound/AudioFileManager.h"
//#include "sound/MappedCommon.h"
//#include "sound/MappedEventList.h"
//#include "sound/MappedDevice.h"
//#include "sound/MappedInstrument.h"
//#include "sound/MappedEvent.h"
//#include "sound/MappedStudio.h"
//#include "sound/PluginIdentifier.h"
//#include "sound/SoundDriver.h"
//#include "sound/Midi.h"
//#include "misc/AppendLabel.h"
//#include "misc/Debug.h"
//#include "misc/Strings.h"
//#include "document/Command.h"
#include "ConfigGroups.h"

//#include "rosegarden-version.h"

namespace Rosegarden {

using namespace BaseProperties;
using namespace std;

RosegardenDocument::RosegardenDocument( bool skipAutoload,
                                        bool clearCommandHistory,
                                        bool enableSound )
  : m_modified( false ),
    m_autoSaved( false ),
    // m_lockFile( nullptr ),
    // m_audioPeaksThread( &m_audioFileManager ),
    m_seqManager( nullptr ),
    m_audioRecordLatency( 0, 0 ),
    m_quickMarkerTime( -1 ),
    m_autoSavePeriod( 0 ),
    m_beingDestroyed( false ),
    m_clearCommandHistory( clearCommandHistory ),
    m_soundEnabled( enableSound ) {
  checkSequencerTimer();

  // connect( CommandHistory::getInstance(),
  //         SIGNAL( commandExecuted() ), this,
  //         SLOT( slotDocumentModified() ) );

  // connect( CommandHistory::getInstance(),
  //         &CommandHistory::documentRestored, this,
  //         &RosegardenDocument::slotDocumentRestored );

  // autoload a new document
  if( !skipAutoload ) performAutoload();

  // now set it up as a "new document"
  newDocument();
}

RosegardenDocument::~RosegardenDocument() {
  m_beingDestroyed = true;

  // m_audioPeaksThread.finish();
  // m_audioPeaksThread.wait();

  deleteEditViews();

  //     ControlRulerCanvasRepository::clear();

  // if( m_clearCommandHistory )
  //  CommandHistory::getInstance()
  //      ->clear(); // before Composition is deleted

  release();
}

unsigned int RosegardenDocument::getAutoSavePeriod() const {
  return 0;
}

void RosegardenDocument::attachView(
    RosegardenMainViewWidget *view ) {
  // m_viewList.append( view );
}

void RosegardenDocument::detachView(
    RosegardenMainViewWidget *view ) {
  // m_viewList.removeOne( view );
}

void RosegardenDocument::attachEditView( EditViewBase *view ) {
  // m_editViewList.append( view );
}

void RosegardenDocument::detachEditView( EditViewBase *view ) {
  // auto-deletion is disabled, as
  // the editview detaches itself when being deleted
  // m_editViewList.removeOne( view );
}

void RosegardenDocument::deleteEditViews() {}

void RosegardenDocument::setAbsFilePath(
    const std::string &filename ) {
  m_absFilePath = filename;
}

void RosegardenDocument::setTitle( const std::string &title ) {
  m_title = title;
}

const std::string &RosegardenDocument::getAbsFilePath() const {
  return m_absFilePath;
}

void RosegardenDocument::deleteAutoSaveFile() {}

const std::string &RosegardenDocument::getTitle() const {
  return m_title;
}

void RosegardenDocument::slotUpdateAllViews(
    RosegardenMainViewWidget *sender ) {}

void RosegardenDocument::setModified() {
  // Already set?  Bail.
  if( m_modified ) return;

  m_modified  = true;
  m_autoSaved = false;

  // Make sure the star (*) appears in the title bar.
  // RosegardenMainWindow::self()->slotUpdateTitle( true );
}

void RosegardenDocument::clearModifiedStatus() {
  m_modified  = false;
  m_autoSaved = true;

  // documentModified( false );
}

void RosegardenDocument::slotDocumentModified() {
  m_modified  = true;
  m_autoSaved = false;

  // documentModified( true );
}

void RosegardenDocument::slotDocumentRestored() {
  m_modified = false;

  // if we hit the bottom of the undo stack, emit this so the
  // modified flag will be cleared from assorted title bars
  // documentModified( false );
}

void RosegardenDocument::setQuickMarker() {
  m_quickMarkerTime = getComposition().getPosition();
}

void RosegardenDocument::jumpToQuickMarker() {
  if( m_quickMarkerTime >= 0 )
    slotSetPointerPosition( m_quickMarkerTime );
}

std::string RosegardenDocument::getAutoSaveFileName() {
  return "";
}

void RosegardenDocument::slotAutoSave() {}

bool RosegardenDocument::deleteOrphanedAudioFiles(
    bool documentWillNotBeSaved ) {
  return true;
}

void RosegardenDocument::newDocument() {
  m_modified = false;
  setAbsFilePath( "" );
  setTitle( "Untitled" );
  // if( m_clearCommandHistory )
  //  CommandHistory::getInstance()->clear();
}

void RosegardenDocument::performAutoload() {}

bool RosegardenDocument::openDocument(
    const std::string &filename, bool permanent,
    bool squelchProgressDialog, bool enableLock ) {
  if( filename.empty() ) return false;

  newDocument();

  m_absFilePath = filename;

  std::string fileContents;

  // Unzip
  bool okay = GzipFile::readFromFile( filename, fileContents );

  std::string errMsg;
  bool        cancelled = false;

  if( !okay ) {
    cerr << "Could not open Rosegarden file\n";
    return false;
  } else {
    // Parse the XML
    // cout << "xml: " << fileContents << "\n";
    okay =
        xmlParse( fileContents, errMsg, permanent, cancelled );
    if( !okay ) {
      cerr << "Error parsing xml\n";
      return false;
    }
  }

  if( !okay ) { return false; }

  if( m_composition.begin() != m_composition.end() ) {}

  return true;
}

void RosegardenDocument::stealLockFile(
    RosegardenDocument *other ) {}

// void
// RosegardenDocument::mergeDocument(RosegardenDocument *doc,
//                                  int options)
//{
//    MacroCommand *command = new MacroCommand(tr("Merge"));

//    timeT time0 = 0;
//    if (options & MERGE_AT_END) {
//        time0 =
//        getComposition().getBarEndForTime(getComposition().getDuration());
//    }

//    int myMaxTrack = getComposition().getNbTracks();
//    int yrMinTrack = 0;
//    int yrMaxTrack = doc->getComposition().getNbTracks();
//    int yrNrTracks = yrMaxTrack - yrMinTrack + 1;

//    int firstAlteredTrack = yrMinTrack;

//    if (options & MERGE_IN_NEW_TRACKS) {

//        //!!! worry about instruments and other studio stuff
//        later... if at all command->addCommand(new
//        AddTracksCommand
//                            (&getComposition(),
//                             yrNrTracks,
//                             MidiInstrumentBase,
//                             -1));

//        firstAlteredTrack = myMaxTrack + 1;

//    } else if (yrMaxTrack > myMaxTrack) {

//        command->addCommand(new AddTracksCommand
//                            (&getComposition(),
//                             yrMaxTrack - myMaxTrack,
//                             MidiInstrumentBase,
//                             -1));
//    }

//    TrackId firstNewTrackId = getComposition().getNewTrackId();
//    timeT lastSegmentEndTime = 0;

//    for (Composition::iterator i =
//    doc->getComposition().begin(), j = i;
//         i != doc->getComposition().end(); i = j) {

//        ++j;
//        Segment *s = *i;
//        timeT segmentEndTime = s->getEndMarkerTime();

//        int yrTrack = s->getTrack();
//        Track *t = doc->getComposition().getTrackById(yrTrack);
//        if (t) yrTrack = t->getPosition();

//        int myTrack = yrTrack;

//        if (options & MERGE_IN_NEW_TRACKS) {
//            myTrack = yrTrack - yrMinTrack + myMaxTrack + 1;
//        }

//        doc->getComposition().detachSegment(s);

//        if (options & MERGE_AT_END) {
//            s->setStartTime(s->getStartTime() + time0);
//            segmentEndTime += time0;
//        }
//        if (segmentEndTime > lastSegmentEndTime) {
//            lastSegmentEndTime = segmentEndTime;
//        }

//        Track *track =
//        getComposition().getTrackByPosition(myTrack); TrackId
//        tid = 0; if (track) tid = track->getId(); else tid =
//        firstNewTrackId + yrTrack - yrMinTrack;

//        command->addCommand(new
//        SegmentInsertCommand(&getComposition(), s, tid));
//    }

//    if (!(options & MERGE_KEEP_OLD_TIMINGS)) {
//        for (int i = getComposition().getTimeSignatureCount() -
//        1; i >= 0; --i) {
//            getComposition().removeTimeSignature(i);
//        }
//        for (int i = getComposition().getTempoChangeCount() -
//        1; i >= 0; --i) {
//            getComposition().removeTempoChange(i);
//        }
//    }

//    if (options & MERGE_KEEP_NEW_TIMINGS) {
//        for (int i = 0; i <
//        doc->getComposition().getTimeSignatureCount(); ++i) {
//            std::pair<timeT, TimeSignature> ts =
//                doc->getComposition().getTimeSignatureChange(i);
//            getComposition().addTimeSignature(ts.first + time0,
//            ts.second);
//        }
//        for (int i = 0; i <
//        doc->getComposition().getTempoChangeCount(); ++i) {
//            std::pair<timeT, tempoT> t =
//                doc->getComposition().getTempoChange(i);
//            getComposition().addTempoAtTime(t.first + time0,
//            t.second);
//        }
//    }

//    if (lastSegmentEndTime > getComposition().getEndMarker()) {
//        command->addCommand(new ChangeCompositionLengthCommand
//                            (&getComposition(),
//                             getComposition().getStartMarker(),
//                             lastSegmentEndTime,
//                             getComposition().autoExpandEnabled()));
//    }

//    CommandHistory::getInstance()->addCommand(command);

//    makeTrackVisible(firstAlteredTrack + yrNrTracks/2 +
//    1);
//}

void RosegardenDocument::initialiseStudio() {
  // Profiler profiler("initialiseStudio", true);

  // Destroy all the mapped objects in the studio.
  RosegardenSequencer::getInstance()->clearStudio();

  // To reduce the number of DCOP calls at this stage, we put
  // some of the float property values in a big list and commit
  // in one single call at the end.  We can only do this with
  // properties that aren't depended on by other port,
  // connection, or non-float properties during the
  // initialisation process.
  MappedObjectIdList       ids;
  MappedObjectPropertyList properties;
  MappedObjectValueList    values;

  // All the softsynths, audio instruments, and busses.
  // std::vector<PluginContainer *> pluginContainers;

  BussList busses = m_studio.getBusses();

  // For each buss (first one is master)
  for( size_t i = 0; i < busses.size(); ++i ) {
    MappedObjectId mappedId = StudioControl::createStudioObject(
        MappedObject::AudioBuss );

    StudioControl::setStudioObjectProperty(
        mappedId, MappedAudioBuss::BussId,
        MappedObjectValue( i ) );

    // Level
    ids.push_back( mappedId );
    properties.push_back( MappedAudioBuss::Level );
    values.push_back(
        MappedObjectValue( busses[i]->getLevel() ) );

    // Pan
    ids.push_back( mappedId );
    properties.push_back( MappedAudioBuss::Pan );
    values.push_back( MappedObjectValue( busses[i]->getPan() ) -
                      100.0 );

    busses[i]->setMappedId( mappedId );

    // pluginContainers.push_back( busses[i] );
  }

  RecordInList recordIns = m_studio.getRecordIns();

  // For each record in
  for( size_t i = 0; i < recordIns.size(); ++i ) {
    MappedObjectId mappedId = StudioControl::createStudioObject(
        MappedObject::AudioInput );

    StudioControl::setStudioObjectProperty(
        mappedId, MappedAudioInput::InputNumber,
        MappedObjectValue( i ) );

    recordIns[i]->setMappedId( mappedId );
  }

  InstrumentList list = m_studio.getAllInstruments();

  // For each instrument
  for( InstrumentList::iterator it = list.begin();
       it != list.end(); ++it ) {
    Instrument &instrument = **it;

    if( instrument.getType() == Instrument::Audio ||
        instrument.getType() == Instrument::SoftSynth ) {
      MappedObjectId mappedId =
          StudioControl::createStudioObject(
              MappedObject::AudioFader );

      instrument.setMappedId( mappedId );

      StudioControl::setStudioObjectProperty(
          mappedId, MappedObject::Instrument,
          MappedObjectValue( instrument.getId() ) );

      // Fader level
      ids.push_back( mappedId );
      properties.push_back( MappedAudioFader::FaderLevel );
      values.push_back( static_cast<MappedObjectValue>(
          instrument.getLevel() ) );

      // Fader record level
      ids.push_back( mappedId );
      properties.push_back( MappedAudioFader::FaderRecordLevel );
      values.push_back( static_cast<MappedObjectValue>(
          instrument.getRecordLevel() ) );

      // Channels
      ids.push_back( mappedId );
      properties.push_back( MappedAudioFader::Channels );
      values.push_back( static_cast<MappedObjectValue>(
          instrument.getAudioChannels() ) );

      // Pan
      ids.push_back( mappedId );
      properties.push_back( MappedAudioFader::Pan );
      values.push_back(
          static_cast<MappedObjectValue>( instrument.getPan() ) -
          100.0f );

      // Set up connections

      // Clear any existing connections (shouldn't be necessary,
      // but)
      StudioControl::disconnectStudioObject( mappedId );

      // Handle the output connection.
      BussId outputBuss = instrument.getAudioOutput();
      if( outputBuss < (unsigned int)busses.size() ) {
        MappedObjectId bussMappedId =
            busses[outputBuss]->getMappedId();

        if( bussMappedId > 0 )
          StudioControl::connectStudioObjects( mappedId,
                                               bussMappedId );
      }

      // Handle the input connection.
      bool isBuss;
      int  channel;
      int  input = instrument.getAudioInput( isBuss, channel );

      MappedObjectId inputMappedId = 0;

      if( isBuss ) {
        if( input < static_cast<int>( busses.size() ) )
          inputMappedId = busses[input]->getMappedId();
      } else {
        if( input < static_cast<int>( recordIns.size() ) )
          inputMappedId = recordIns[input]->getMappedId();
      }

      ids.push_back( mappedId );
      properties.push_back( MappedAudioFader::InputChannel );
      values.push_back( MappedObjectValue( channel ) );

      if( inputMappedId > 0 )
        StudioControl::connectStudioObjects( inputMappedId,
                                             mappedId );

      // pluginContainers.push_back( &instrument );
    }
  }

  std::set<InstrumentId> instrumentsSeen;

  // For each track in the composition, send the channel setup
  for( Composition::trackcontainer::const_iterator i =
           m_composition.getTracks().begin();
       i != m_composition.getTracks().end(); ++i ) {
    // const TrackId trackId = i->first;
    const Track *track = i->second;

    InstrumentId instrumentId = track->getInstrument();

    // If we've already seen this instrument, try the next track.
    if( instrumentsSeen.find( instrumentId ) !=
        instrumentsSeen.end() )
      continue;

    instrumentsSeen.insert( instrumentId );

    Instrument *instrument =
        m_studio.getInstrumentById( instrumentId );

    // If this instrument doesn't exist, try the next track.
    if( !instrument ) continue;

    // If this isn't a MIDI instrument, try the next track.
    if( instrument->getType() != Instrument::Midi ) continue;

    // If this instrument isn't in fixed channel mode, try the
    // next track.
    if( !instrument->hasFixedChannel() ) continue;

    // Call Instrument::sendChannelSetup() to make sure the
    // program change for this track has been sent out. The test
    // case (MIPP #35) for this is a bit esoteric:
    //   1. Load a composition and play it.
    //   2. Load a different composition, DO NOT play it.
    //   3. Select tracks in the new composition and play the
    //   MIDI
    //      input keyboard.
    //   4. Verify that you hear the programs for the new
    //   composition.
    // Without the following, you'll hear the programs for the
    // old composition.
    instrument->sendChannelSetup();
  }

  // For each softsynth, audio instrument, and buss
  // for( std::vector<PluginContainer *>::iterator
  //         pluginContainerIter = pluginContainers.begin();
  //     pluginContainerIter != pluginContainers.end();
  //     ++pluginContainerIter ) {
  //  PluginContainer &pluginContainer = **pluginContainerIter;

  //  // Initialise all the plugins for this Instrument or Buss

  //  // For each plugin within this instrument or buss
  //  for( PluginInstanceIterator pli =
  //           pluginContainer.beginPlugins();
  //       pli != pluginContainer.endPlugins(); ++pli ) {
  //    AudioPluginInstance &plugin = **pli;

  //    if( !plugin.isAssigned() ) continue;

  //    // Plugin Slot
  //    MappedObjectId pluginMappedId =
  //        StudioControl::createStudioObject(
  //            MappedObject::PluginSlot );

  //    plugin.setMappedId( pluginMappedId );

  //    // Position
  //    StudioControl::setStudioObjectProperty(
  //        pluginMappedId, MappedObject::Position,
  //        MappedObjectValue( plugin.getPosition() ) );

  //    // Instrument
  //    StudioControl::setStudioObjectProperty(
  //        pluginMappedId, MappedObject::Instrument,
  //        pluginContainer.getId() );

  //    // Identifier
  //    StudioControl::setStudioObjectProperty(
  //        pluginMappedId, MappedPluginSlot::Identifier,
  //        plugin.getIdentifier().c_str() );

  //    // plugin.setConfigurationValue(
  //    //    qstrtostr(
  //    // PluginIdentifier::RESERVED_PROJECT_DIRECTORY_KEY
  //    //        ),
  //    //    qstrtostr( getAudioFileManager().getAudioPath() )
  //    );

  //    // Set opaque string configuration data (e.g. for DSSI
  //    // plugin)

  //    MappedObjectPropertyList config;

  //    // for( AudioPluginInstance::ConfigMap::const_iterator i
  //    =
  //    //         plugin.getConfiguration().begin();
  //    //     i != plugin.getConfiguration().end(); ++i ) {
  //    //  config.push_back( strtoqstr( i->first ) );
  //    //  config.push_back( strtoqstr( i->second ) );
  //    //}

  //    std::string error =
  //        StudioControl::setStudioObjectPropertyList(
  //            pluginMappedId, MappedPluginSlot::Configuration,
  //            config );

  //    if( string( error ) != "" ) {
  //      // StartupLogo::hideIfStillThere();
  //      //                if (m_progressDialog)
  //      //                    m_progressDialog->hide();
  //      //                if (m_progressDialog)
  //      //                    m_progressDialog->show();
  //    }

  //    // Bypassed
  //    ids.push_back( pluginMappedId );
  //    properties.push_back( MappedPluginSlot::Bypassed );
  //    values.push_back(
  //        MappedObjectValue( plugin.isBypassed() ) );

  //    // Port Values

  //    for( PortInstanceIterator portIt = plugin.begin();
  //         portIt != plugin.end(); ++portIt ) {
  //      StudioControl::setStudioPluginPort( pluginMappedId,
  //                                          ( *portIt
  //                                          )->number, (
  //                                          *portIt )->value );
  //    }

  //    // Program
  //    // if( plugin.getProgram() != "" ) {
  //    //  StudioControl::setStudioObjectProperty(
  //    //      pluginMappedId, MappedPluginSlot::Program,
  //    //      strtoqstr( plugin.getProgram() ) );
  //    //}

  //    // Set the post-program port values
  //    // ??? Why?
  //    for( PortInstanceIterator portIt = plugin.begin();
  //         portIt != plugin.end(); ++portIt ) {
  //      if( ( *portIt )->changedSinceProgramChange ) {
  //        StudioControl::setStudioPluginPort(
  //            pluginMappedId, ( *portIt )->number,
  //            ( *portIt )->value );
  //      }
  //    }
  //  }
  //}

  // Now commit all the remaining changes
  StudioControl::setStudioObjectProperties( ids, properties,
                                            values );

  // Send System Audio Ports Event

  MidiByte ports = 0;

  MappedEvent mEports( MidiInstrumentBase,
                       MappedEvent::SystemAudioPorts, ports );
  StudioControl::sendMappedEvent( mEports );

  // Send System Audio File Format Event

  // MappedEvent mEff( MidiInstrumentBase,
  //                  MappedEvent::SystemAudioFileFormat,
  //                  audioFileFormat );
  // StudioControl::sendMappedEvent( mEff );
}

SequenceManager *RosegardenDocument::getSequenceManager() {
  return m_seqManager;
}

void RosegardenDocument::setSequenceManager(
    SequenceManager *sm ) {
  m_seqManager = sm;
}

// FILE FORMAT VERSION NUMBERS
//
// These should be updated when the file format changes.  The
// intent is to warn the user that they are loading a file that
// was saved with a newer version of Rosegarden, and data might
// be lost as a result.  See RoseXmlHandler::startElement().
//
// Increment the major version number only for updates so
// substantial that we shouldn't bother even trying to read a
// file saved with a newer major version number than our own.
// Older versions of Rosegarden *will not* try to load files with
// newer major version numbers.  Basically, this should be done
// only as a last resort to lock out all older versions of
// Rosegarden from reading in and completely mangling the
// contents of a file.
//
// Increment the minor version number for updates that may break
// compatibility such that we should warn when reading a file
// that was saved with a newer minor version than our own.
//
// Increment the point version number for updates that shouldn't
// break compatibility in either direction, just for
// informational purposes.
//
// When updating major, reset minor to zero; when updating minor,
// reset point to zero.
//
int RosegardenDocument::FILE_FORMAT_VERSION_MAJOR = 1;
int RosegardenDocument::FILE_FORMAT_VERSION_MINOR = 6;
int RosegardenDocument::FILE_FORMAT_VERSION_POINT = 4;

bool RosegardenDocument::saveDocument(
    const std::string &filename, std::string &errMsg,
    bool autosave ) {
  return true;
}

bool RosegardenDocument::saveDocumentActual(
    const std::string &filename, std::string &errMsg,
    bool autosave ) {
  return true;
}

bool RosegardenDocument::exportStudio(
    const std::string &filename, std::string &errMsg,
    std::vector<DeviceId> devices ) {
  return true;
}

// void RosegardenDocument::saveSegment(
//    QTextStream &outStream, Segment *segment,
//    long [>totalEvents*/, long & /*count<],
//    std::string extraAttributes ) {
//}

bool RosegardenDocument::saveAs( const std::string &newName,
                                 std::string &      errMsg ) {
  // Success.
  return true;
}

bool RosegardenDocument::isSoundEnabled() const {
  return m_soundEnabled;
}

bool RosegardenDocument::xmlParse( std::string  fileContents,
                                   std::string &errMsg,
                                   bool         permanent,
                                   bool &       cancelled ) {
  cancelled = false;

  unsigned int elementCount = 0;
  for( unsigned long i = 0; i < fileContents.length() - 1;
       ++i ) {
    if( fileContents[i] == '<' && fileContents[i + 1] != '/' ) {
      ++elementCount;
    }
  }

  RoseXmlHandler handler( this, elementCount, permanent );

  QXmlInputSource source;
  source.setData( QString::fromStdString( fileContents ) );
  QXmlSimpleReader reader;
  reader.setContentHandler( &handler );
  reader.setErrorHandler( &handler );

  bool ok = reader.parse( source );

  if( !ok ) {
    cerr << "xmlParse: " << handler.errorString().toStdString()
         << "\n";
  } else {
    getComposition().resetLinkedSegmentRefreshStatuses();
  }

  return ok;
}

void RosegardenDocument::insertRecordedMidi(
    const MappedEventList &mC ) {}

void RosegardenDocument::updateRecordingMIDISegment() {
  Profiler profiler(
      "RosegardenDocument::updateRecordingMIDISegment()" );

  if( m_recordMIDISegments.size() == 0 ) {
    // make this call once to create one
    insertRecordedMidi( MappedEventList() );
    if( m_recordMIDISegments.size() == 0 )
      return; // not recording any MIDI
  }

  NoteOnMap tweakedNoteOnEvents;
  for( NoteOnMap::iterator mi = m_noteOnEvents.begin();
       mi != m_noteOnEvents.end(); ++mi )
    for( ChanMap::iterator cm = mi->second.begin();
         cm != mi->second.end(); ++cm )
      for( PitchMap::iterator pm = cm->second.begin();
           pm != cm->second.end(); ++pm ) {
        // anything in the note-on map should be tweaked so as to
        // end at the recording pointer
        NoteOnRecSet rec_vec = pm->second;
        if( rec_vec.size() > 0 ) {
          tweakedNoteOnEvents[mi->first][cm->first][pm->first] =
              *adjustEndTimes( rec_vec,
                               m_composition.getPosition() );
        }
      }
  m_noteOnEvents = tweakedNoteOnEvents;
}

void RosegardenDocument::transposeRecordedSegment( Segment *s ) {
}

RosegardenDocument::NoteOnRecSet *
RosegardenDocument::adjustEndTimes( NoteOnRecSet &rec_vec,
                                    timeT         endTime ) {
  // Not too keen on profilers, but I'll give it a shot for
  // fun...
  Profiler profiler( "RosegardenDocument::adjustEndTimes()" );

  // Create a vector to hold the new note-on events for return.
  NoteOnRecSet *new_vector = new NoteOnRecSet();

  // For each note-on event
  for( NoteOnRecSet::const_iterator i = rec_vec.begin();
       i != rec_vec.end(); ++i ) {
    // ??? All this removing and re-inserting of Events from the
    // Segment
    //     seems like a serious waste.  Can't we just modify the
    //     Event in place?  Otherwise we are doing all of this:
    //        1. Segment::erase() notifications.
    //        2. Segment::insert() notifications.
    //        3. Event delete and new.

    Event *oldEvent = *( i->m_segmentIterator );

    timeT newDuration = endTime - oldEvent->getAbsoluteTime();

    // Don't allow zero duration events.
    if( newDuration == 0 ) newDuration = 1;

    // Make a new copy of the event in the segment and modify the
    // duration as needed.
    // ??? Can't we modify the Event in place in the Segment?
    //     No.  All setters are protected.  Events are read-only.
    Event *newEvent = new Event(
        *oldEvent,                   // reference Event object
        oldEvent->getAbsoluteTime(), // absoluteTime (preserved)
        newDuration                  // duration (adjusted)
    );

    // Remove the old event from the segment
    Segment *recordMIDISegment = i->m_segment;
    recordMIDISegment->erase( i->m_segmentIterator );

    // Insert the new event into the segment
    NoteOnRec noteRec;
    noteRec.m_segment = recordMIDISegment;
    // ??? Performance: This causes a slew of change
    // notifications to be
    //        sent out by Segment::insert().  That may be causing
    //        the performance issues when recording.  Try
    //        removing the notifications from insert() and see if
    //        things improve. Also take a look at
    //        Segment::erase() which is called above.
    noteRec.m_segmentIterator =
        recordMIDISegment->insert( newEvent );

    // don't need to transpose this event; it was copied from an
    // event that had been transposed already (in
    // storeNoteOnEvent)

    // Collect the new NoteOnRec objects for return.
    new_vector->push_back( noteRec );
  }

  return new_vector;
}

void RosegardenDocument::storeNoteOnEvent( Segment *         s,
                                           Segment::iterator it,
                                           int device,
                                           int channel ) {
  NoteOnRec record;
  record.m_segment         = s;
  record.m_segmentIterator = it;

  int pitch = ( *it )->get<Int>( PITCH );

  m_noteOnEvents[device][channel][pitch].push_back( record );
}

void RosegardenDocument::insertRecordedEvent( Event *ev,
                                              int    device,
                                              int    channel,
                                              bool   isNoteOn ) {
  Profiler profiler(
      "RosegardenDocument::insertRecordedEvent()" );

  Segment::iterator it;
  for( RecordingSegmentMap::const_iterator i =
           m_recordMIDISegments.begin();
       i != m_recordMIDISegments.end(); ++i ) {
    Segment *recordMIDISegment = i->second;
    TrackId  tid               = recordMIDISegment->getTrack();
    Track *  track = getComposition().getTrackById( tid );
    if( track ) {
      // Instrument *instrument =
      //    m_studio.getInstrumentById(track->getInstrument());
      int chan_filter = track->getMidiInputChannel();
      int dev_filter  = track->getMidiInputDevice();

      if( ( ( chan_filter < 0 ) ||
            ( chan_filter == channel ) ) &&
          ( ( dev_filter == int( Device::ALL_DEVICES ) ) ||
            ( dev_filter == device ) ) ) {
        // Insert the event into the segment.
        it = recordMIDISegment->insert( new Event( *ev ) );

        if( isNoteOn ) {
          // Add the event to m_noteOnEvents.
          // To match up with a note-off later.
          storeNoteOnEvent( recordMIDISegment, it, device,
                            channel );
        }

      } else {
      }
    }
  }
}

void RosegardenDocument::stopPlaying() {
  // pointerPositionChanged( m_composition.getPosition() );
}

void RosegardenDocument::stopRecordingMidi() {}

void RosegardenDocument::prepareAudio() {}

void RosegardenDocument::slotSetPointerPosition( timeT t ) {
  m_composition.setPosition( t );
  // pointerPositionChanged( t );
}

void RosegardenDocument::setLoop( timeT t0, timeT t1 ) {
  m_composition.setLoopStart( t0 );
  m_composition.setLoopEnd( t1 );
  // loopChanged( t0, t1 );
}

void RosegardenDocument::checkSequencerTimer() {}

void RosegardenDocument::addRecordMIDISegment( TrackId tid ) {}

// void RosegardenDocument::addRecordAudioSegment(
//    InstrumentId iid, AudioFileId auid ) {}

void RosegardenDocument::updateRecordingAudioSegments() {
  const Composition::recordtrackcontainer &tr =
      getComposition().getRecordTracks();

  for( Composition::recordtrackcontainer::const_iterator i =
           tr.begin();
       i != tr.end(); ++i ) {
    TrackId tid   = ( *i );
    Track * track = getComposition().getTrackById( tid );

    if( track ) {
      InstrumentId iid = track->getInstrument();

      if( m_recordAudioSegments[iid] ) {
        Segment *recordSegment = m_recordAudioSegments[iid];
        if( !recordSegment->getComposition() ) {
          // always insert straight away for audio
          m_composition.addSegment( recordSegment );
        }

        recordSegment->setAudioEndTime(
            m_composition.getRealTimeDifference(
                recordSegment->getStartTime(),
                m_composition.getPosition() ) );

      } else {
      }
    }
  }
}

void RosegardenDocument::stopRecordingAudio() {
  for( RecordingSegmentMap::iterator ri =
           m_recordAudioSegments.begin();
       ri != m_recordAudioSegments.end(); ++ri ) {
    Segment *recordSegment = ri->second;

    if( !recordSegment ) continue;

    // set the audio end time
    //
    recordSegment->setAudioEndTime(
        m_composition.getRealTimeDifference(
            recordSegment->getStartTime(),
            m_composition.getPosition() ) );

    // now add the Segment

    // now move the segment back by the record latency
    //
    /*!!!
      No.  I don't like this.

      The record latency doesn't always exist -- for example, if
      recording from a synth plugin there is no record latency,
      and we have no way here to distinguish.

      The record latency is a total latency figure that actually
      includes some play latency, and we compensate for that
      again on playback (see bug #1378766).

      The timeT conversion of record latency is approximate in
      frames, giving potential phase error.

      Cutting this out won't break any existing files, as the
      latency compensation there is already encoded into the
      file.

        RealTime adjustedStartTime =
            m_composition.getElapsedRealTime(recordSegment->getStartTime())
      - m_audioRecordLatency;

        timeT shiftedStartTime =
            m_composition.getElapsedTimeForRealTime(adjustedStartTime);

      shiftedStartTime
             << " clicks (from " << recordSegment->getStartTime()
             << " to " << shiftedStartTime << ")";

        recordSegment->setStartTime(shiftedStartTime);
    */
  }
  // stoppedAudioRecording();

  // pointerPositionChanged( m_composition.getPosition() );
}

void RosegardenDocument::finalizeAudioFile( InstrumentId iid ) {}

RealTime RosegardenDocument::getAudioPlayLatency() {
  return RosegardenSequencer::getInstance()
      ->getAudioPlayLatency();
}

RealTime RosegardenDocument::getAudioRecordLatency() {
  return RosegardenSequencer::getInstance()
      ->getAudioRecordLatency();
}

void RosegardenDocument::updateAudioRecordLatency() {
  m_audioRecordLatency = getAudioRecordLatency();
}

// std::stringList RosegardenDocument::getTimers() {
//  std::stringList list;

//  unsigned int count =
//      RosegardenSequencer::getInstance()->getTimers();

//  for( unsigned int i = 0; i < count; ++i ) {
//    list.push_back(
//        RosegardenSequencer::getInstance()->getTimer( i ) );
//  }

//  return list;
//}

std::string RosegardenDocument::getCurrentTimer() {
  return RosegardenSequencer::getInstance()->getCurrentTimer();
}

void RosegardenDocument::setCurrentTimer( std::string name ) {
  RosegardenSequencer::getInstance()->setCurrentTimer( name );
}

void RosegardenDocument::clearAllPlugins() {
  // InstrumentList  list = m_studio.getAllInstruments();
  // MappedEventList mC;

  // InstrumentList::iterator it = list.begin();
  // for( ; it != list.end(); ++it ) {
  //  if( ( *it )->getType() == Instrument::Audio ) {
  //    PluginInstanceIterator pIt = ( *it )->beginPlugins();

  //    for( ; pIt != ( *it )->endPlugins(); pIt++ ) {
  //      if( ( *pIt )->getMappedId() != -1 ) {
  //        if( StudioControl::destroyStudioObject(
  //                ( *pIt )->getMappedId() ) == false ) {}
  //      }
  //      ( *pIt )->clearPorts();
  //    }
  //    ( *it )->emptyPlugins();

  /*
   */
  //  }
  //}
}

void RosegardenDocument::slotDocColoursChanged() {
  // docColoursChanged();
}

void RosegardenDocument::addOrphanedRecordedAudioFile(
    std::string fileName ) {
  m_orphanedRecordedAudioFiles.push_back( fileName );
  // slotDocumentModified();
}

void RosegardenDocument::addOrphanedDerivedAudioFile(
    std::string fileName ) {
  m_orphanedDerivedAudioFiles.push_back( fileName );
  // slotDocumentModified();
}

// void RosegardenDocument::notifyAudioFileRemoval( AudioFileId
// id ) {
//}

// Get the instrument that plays the segment.
// @returns a pointer to the instrument object
Instrument *RosegardenDocument::getInstrument(
    Segment *segment ) {
  if( !segment || !( segment->getComposition() ) ) {
    return nullptr;
  }

  Studio &    studio     = getStudio();
  Instrument *instrument = studio.getInstrumentById(
      segment->getComposition()
          ->getTrackById( segment->getTrack() )
          ->getInstrument() );
  return instrument;
}

void RosegardenDocument::checkAudioPath( Track *track ) {}

// std::string RosegardenDocument::lockFilename(
//    const std::string &absFilePath ) // static
//{
//}

bool RosegardenDocument::lock() { return true; }

void RosegardenDocument::release() {
  // Remove the lock file
  // delete m_lockFile;
  // m_lockFile = nullptr;
}

} // namespace Rosegarden
