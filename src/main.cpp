/*
  rg2midi
  A CLI tool to export Rosegarden files to MIDI.
  Copyright 2019 by David P. Sicilia

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.  See the
  file COPYING included with this distribution for more
  information.
*/

#include "MidiFile.h"
#include "RosegardenDocument.h"

#include <iostream>

using namespace std;

#define CHECK( a, msg ) \
  if( !( a ) ) { die( string() + msg ); }

void die( string const& msg ) {
  cerr << "Error: " << msg << "\n";
  exit( 1 );
}

int main( int argc, char** argv ) {
  CHECK( argc == 3, "Usage: rg2midi in-file.rg out-file.mid" );

  string rg  = argv[1];
  string mid = argv[2];

  Rosegarden::RosegardenDocument doc(
      /*skipAutoload=*/true,
      /*clearCommandHistory=*/true,
      /*m_useSequencer=*/false );

  bool ok;

  ok = doc.openDocument( rg, /*permanent=*/false,
                         /*squelchProgressDialog=*/true,
                         /*enableLock=*/false );
  CHECK( ok, "opening " + rg );

  Rosegarden::MidiFile midiFile;
  ok = midiFile.convertToMidi( doc, mid );
  CHECK( ok, "writing midi file " + mid );

  return 0;
}
