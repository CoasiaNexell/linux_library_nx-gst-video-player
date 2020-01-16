#include "MainFrame.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MainFrame w;

	QFont font = qApp->font();
	font.setFamily("Sans Serif");
	qApp->setFont(font);

	w.show();

	return a.exec();
}
