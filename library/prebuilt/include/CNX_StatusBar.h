#ifndef CNX_STATUSBAR_H
#define CNX_STATUSBAR_H

#include <QFrame>
#include <QDateTime>
#include <QTimer>

#include <CNX_DAudioStatus.h>

namespace Ui {
class CNX_StatusBar;
}

class CNX_StatusBar : public QFrame
{
	Q_OBJECT

public:
	enum ButtonType
	{
		ButtonType_All,
		ButtonType_Home,
		ButtonType_Back
	};

private slots:
	void slotUpdateDateTime();

	void slotUpdateDAudioStatus();

public:
	explicit CNX_StatusBar(QWidget *parent = 0);
	~CNX_StatusBar();

	void resize(int w, int h);

public:
	void RegOnClickedHome(void (*cbFunc)(void *));

	void RegOnClickedBack(void (*cbFunc)(void *));

	void RegOnClickedVolume(void (*cbFunc)(void *));

	void SetButtonEnabled(ButtonType type, bool enabled);

	void SetTitleName(QString text);

	QString GetTitleName();

	void SetVolume(int value);

	int GetVolume();

	void SetBTConnection(int value);

	int GetBTConnection();

private slots:
	void on_BUTTON_HOME_clicked();

	void on_BUTTON_BACK_clicked();

	void on_BUTTON_BT_CONNECTION_ICON_toggled(bool checked);

	void on_BUTTON_VOLUME_clicked();

private:
	void (*m_pCbHomeButtonClicked)(void *);

	void (*m_pCbBackButtonClicked)(void *);

	void (*m_pCbVolumeButtonClicked)(void *);

	QWidget* m_pParent;

	QTimer m_UpdateDateTimer;

	CNX_DAudioStatus* m_pDAudioStatus;

	QTimer m_UpdateDAudioStatus;

private:
	Ui::CNX_StatusBar *ui;
};

#endif // CNX_STATUSBAR_H
