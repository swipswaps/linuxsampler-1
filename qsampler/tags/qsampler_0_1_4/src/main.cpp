// main.cpp
//
/****************************************************************************
   Copyright (C) 2004-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include <qapplication.h>
#include <qtextcodec.h>

#include "qsamplerAbout.h"
#include "qsamplerOptions.h"
#include "qsamplerMainForm.h"

//-------------------------------------------------------------------------
// main - The main program trunk.
//

int main ( int argc, char **argv )
{
    QApplication app(argc, argv);

    // Load translation support.
    QTranslator translator(0);
    QString sLocale = QTextCodec::locale();
    if (sLocale != "C") {
        QString sLocName = "qsampler_" + sLocale;
        if (!translator.load(sLocName, ".")) {
            QString sLocPath = CONFIG_PREFIX "/share/locale";
            if (!translator.load(sLocName, sLocPath))
                fprintf(stderr, "Warning: no locale found: %s/%s.qm\n", sLocPath.latin1(), sLocName.latin1());
        }
        app.installTranslator(&translator);
    }

    // Construct default settings; override with command line arguments.
    qsamplerOptions options;
    if (!options.parse_args(app.argc(), app.argv())) {
        app.quit();
        return 1;
    }

    // Construct, setup and show the main form.
    qsamplerMainForm w;
	app.setMainWidget(&w);
    w.setup(&options);
    w.show();

    // Register the quit signal/slot.
    // app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));

    return app.exec();
}

// end of main.cpp
