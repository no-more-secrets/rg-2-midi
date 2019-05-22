/*
  rg2midi
  A CLI tool to export Rosegarden files to MIDI.
  Copyright 2019 by David P. Sicilia

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#include <iostream>

#include "MidiFile.h"
#include "RosegardenDocument.h"

using namespace std;

int main() {
  // Create a new blank document
  Rosegarden::RosegardenDocument *doc =
      new Rosegarden::RosegardenDocument(
          true,                       // skipAutoload
          true,                       // clearCommandHistory
          /*m_useSequencer=*/false ); // enableSound

  // Read the document from the file.
  bool readOk = doc->openDocument(
      "/home/dsicilia/dev/revolution-now/assets/music/midi/"
      "fife-and-drum-1.rg",
      false, true, false );

  if( !readOk ) {
    cerr << "error opening document.\n";
    return 1;
  }

  Rosegarden::MidiFile midiFile;

  if( !midiFile.convertToMidi( *doc, "out.mid" ) ) {
    cerr << "error writing midi file.\n";
    return 1;
  }
  return 0;
}
