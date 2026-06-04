/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_DEMO_H
#define ENGINE_DEMO_H

#include "kernel.h"

enum
{
	MAX_TIMELINE_MARKERS=64,
	MAX_DEMO_BOOKMARKS=128,
	MAX_BOOKMARK_NAME=64,
	MAX_DEMO_PATH=512,
};

struct CDemoBookmark
{
	int m_Tick;
	char m_aName[MAX_BOOKMARK_NAME];
};

struct CDemoHeader
{
	unsigned char m_aMarker[7];
	unsigned char m_Version;
	char m_aNetversion[64];
	char m_aMapName[64];
	unsigned char m_aMapSize[4];
	unsigned char m_aMapCrc[4];
	char m_aType[8];
	unsigned char m_aLength[4];
	char m_aTimestamp[20];
	unsigned char m_aNumTimelineMarkers[4];
	unsigned char m_aTimelineMarkers[MAX_TIMELINE_MARKERS][4];
};

class IDemoPlayer : public IInterface
{
	MACRO_INTERFACE("demoplayer", 0)
public:
	class CInfo
	{
	public:
		bool m_Paused;
		float m_Speed;
		int m_SpeedIndex;

		int m_FirstTick;
		int m_CurrentTick;
		int m_LastTick;

		int m_NumTimelineMarkers;
		int m_aTimelineMarkers[MAX_TIMELINE_MARKERS];
	};

	enum
	{
		DEMOTYPE_INVALID=0,
		DEMOTYPE_CLIENT,
		DEMOTYPE_SERVER,
	};

	~IDemoPlayer() {}
	virtual void SetSpeed(float Speed) = 0;
	virtual void SetSpeedIndex(int Offset) = 0;
	virtual int SetPos(float Percent) = 0;
	virtual int SetPos(int WantedTick) = 0;
	virtual void Pause() = 0;
	virtual void Unpause() = 0;
	virtual const CInfo *BaseInfo() const = 0;
	virtual void GetDemoName(char *pBuffer, int BufferSize) const = 0;
	virtual bool GetDemoInfo(const char *pFilename, int StorageType, CDemoHeader *pDemoHeader) const = 0;
	virtual int GetDemoType() const = 0;

	virtual int AddBookmark(int Tick, const char *pName) = 0;
	virtual bool RemoveBookmark(int Index) = 0;
	virtual bool RenameBookmark(int Index, const char *pName) = 0;
	virtual int GotoBookmark(int Index) = 0;
	virtual int GetNumBookmarks() const = 0;
	virtual const CDemoBookmark *GetBookmark(int Index) const = 0;
	virtual const char *GetDemoPath() const = 0;
	virtual void SaveBookmarks() = 0;

	static void DeleteBookmarkFile(class IStorage *pStorage, const char *pDemoPath);
	static void RenameBookmarkFile(class IStorage *pStorage, const char *pOldDemoPath, const char *pNewDemoPath);
};

class IDemoRecorder : public IInterface
{
	MACRO_INTERFACE("demorecorder", 0)
public:
	~IDemoRecorder() {}
	virtual bool IsRecording() const = 0;
	virtual int Stop() = 0;
	virtual int Length() const = 0;
};

#endif
