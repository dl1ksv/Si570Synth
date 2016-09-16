/*
 * File:   main.cpp
 * Author: schroer
 *
 * Created on 8. Januar 2011, 16:44
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include <QApplication>
#include "si570synth.h"

int main(int argc, char *argv[])
{
  // initialize resources, if needed
  // Q_INIT_RESOURCE(resfile);

  QApplication app(argc, argv);
  Si570Synth synth;
  // create and show your widgets here
  synth.show();
  return app.exec();
}
