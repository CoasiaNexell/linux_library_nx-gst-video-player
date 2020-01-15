#include <QApplication>
#include "MainFrame.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MainFrame w;

	w.show();

	return a.exec();
}
