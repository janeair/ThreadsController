#include "parallelsystem.h"
#include <QtWidgets/QApplication>
#include <qstylefactory.h>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	qApp->setStyle(QStyleFactory::create("Fusion"));
	parallelsystem w;
	w.show();
	return a.exec();
}
