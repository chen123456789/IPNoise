#include "simpleViewer.h"
#include <qapplication.h>

// http://www.libqglviewer.com/examples/index.html

int main(int argc, char** argv)
{
    // Read command lines arguments.
    QApplication application(argc,argv);

    // Instantiate the viewer.
    Viewer viewer;

#if QT_VERSION < 0x040000
    // Set the viewer as the application main widget.
    application.setMainWidget(&viewer);
#else
    viewer.setWindowTitle("simpleViewer");
#endif

    // Make the viewer window visible on screen.
    viewer.show();

    // Run main loop.
    return application.exec();
}
