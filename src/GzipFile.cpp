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

#include "GzipFile.h"
#include <zlib.h>
#include <string>
#include <vector>

namespace Rosegarden {

bool GzipFile::writeToFile( std::string file,
                            std::string text ) {
  std::string stext = std::string( text.data() );
  const char *ctext = stext.c_str();
  size_t      csize = stext.length();

  gzFile fd = gzopen( file.data(), "wb" );
  if( !fd ) return false;

  int actual = gzwrite( fd, (void *)ctext, csize );
  gzclose( fd );

  return ( (size_t)actual == csize );
}

bool GzipFile::readFromFile( std::string  file,
                             std::string &text ) {
  text      = "";
  gzFile fd = gzopen( file.data(), "rb" );
  if( !fd ) return false;

  std::vector<char> ba;
  char              buffer[100000];
  int               got = 0;

  while( ( got = gzread( fd, buffer, 100000 ) ) > 0 ) {
    for( int i = 0; i < got; i++ ) ba.push_back( buffer[i] );
  }

  bool ok = gzeof( fd );
  gzclose( fd );
  text = std::string( &ba[0], ba.size() );
  return ok;
}

} // namespace Rosegarden

