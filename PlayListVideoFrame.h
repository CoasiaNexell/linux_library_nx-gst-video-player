#ifndef PLAYLISTVIDEOFRAME_H
#define PLAYLISTVIDEOFRAME_H

#include <QFrame>
#include <QListWidgetItem>

//	Base UI
#include <CNX_StatusBar.h>

#include "CNX_FileList.h"
#include "NxEvent.h"

namespace Ui {
class PlayListVideoFrame;
}

class PlayListVideoFrame : public QFrame
{
	Q_OBJECT

signals:
	void signalPlayListAccept();
	void signalPlayListReject();

public:
	explicit PlayListVideoFrame(QWidget *parent = 0);
	~PlayListVideoFrame();

public:
	bool event(QEvent *e);
	void resizeEvent(QResizeEvent *event);
	void RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk));
	void RegisterRequestVolume(void (*cbFunc)(void));

public:
	void setList(CNX_FileList *pFileList);
	void setCurrentIndex(int32_t idx);
	int32_t getCurrentIndex();

public:
	QModelIndex	m_selectIdx;
	int32_t			m_pretIdx;
	int32_t			m_selectCount;

private slots:
	void on_btnCancel_released();
	void on_btnOk_released();
	void on_listWidget_itemClicked(QListWidgetItem *item);

private:
	//	UI Status Bar
	CNX_StatusBar* m_pStatusBar;

	// Terminate
	void (*m_pRequestLauncherShow)(bool *bOk);
	void (*m_pRequestVolume)(void);

	void SetupUI();

private:
	Ui::PlayListVideoFrame *ui;
};

#endif // PLAYLISTVIDEOFRAME_H
