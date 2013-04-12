#pragma once

#include "../shared/KOSocketMgr.h"
#include "../shared/database/OdbcConnection.h"

#include "GameSocket.h"

#include "MAP.h"
#include "Pathfind.h"

#include "../shared/STLMap.h"

class CNpcThread;
class CNpcTable;

typedef std::vector <CNpcThread*>			NpcThreadArray;
typedef CSTLMap <CNpcTable>					NpcTableArray;
typedef CSTLMap <CNpc>						NpcArray;
typedef CSTLMap <_MAGIC_TABLE>				MagictableArray;
typedef CSTLMap <_MAGIC_TYPE1>				Magictype1Array;
typedef CSTLMap <_MAGIC_TYPE2>				Magictype2Array;
typedef CSTLMap <_MAGIC_TYPE3>				Magictype3Array;
typedef CSTLMap	<_MAGIC_TYPE4>				Magictype4Array;
typedef CSTLMap <_PARTY_GROUP>				PartyArray;
typedef CSTLMap <_MAKE_WEAPON>				MakeWeaponItemTableArray;
typedef CSTLMap <_MAKE_ITEM_GRADE_CODE>		MakeGradeItemTableArray;
typedef CSTLMap <_MAKE_ITEM_LARE_CODE>		MakeLareItemTableArray;
typedef std::list <int>						ZoneNpcInfoList;
typedef CSTLMap <MAP>						ZoneArray;
typedef CSTLMap <_K_MONSTER_ITEM>			NpcItemArray;
typedef CSTLMap <_MAKE_ITEM_GROUP>			MakeItemGroupArray;

class CServerDlg
{
private:
	void ResumeAI();
	BOOL CreateNpcThread();
	BOOL GetMagicTableData();
	BOOL GetMagicType1Data();
	BOOL GetMagicType2Data();
	BOOL GetMagicType3Data();
	BOOL GetMagicType4Data();
	BOOL GetNpcTableData(bool bNpcData = true);
	BOOL GetNpcItemTable();
	BOOL GetMakeItemGroupTable();
	BOOL GetMakeWeaponItemTableData();
	BOOL GetMakeDefensiveItemTableData();
	BOOL GetMakeGradeItemTableData();
	BOOL GetMakeLareItemTableData();
	BOOL MapFileLoad();
	void GetServerInfoIni();
	
	void SyncTest();
	void RegionCheck();		// region�ȿ� ������ ���� üũ (�����忡�� FindEnermy()�Լ��� ���ϸ� ���̱� ���� �Ǽ�)
// Construction
public:
	CServerDlg();
	bool Startup();

	bool LoadSpawnCallback(OdbcCommand *dbCommand);
	void GameServerAcceptThread();
	BOOL AddObjectEventNpc(_OBJECT_EVENT* pEvent, int zone_number);
	void AllNpcInfo();
	CUser* GetUserPtr(int nid);
	CNpc*  GetEventNpcPtr();
	BOOL   SetSummonNpcData(CNpc* pNpc, int zone, float fx, float fz);
	MAP * GetZoneByID(int zonenumber);
	int GetServerNumber( int zonenumber );

	void CheckAliveTest();
	void DeleteUserList(int uid);
	void DeleteAllUserList(CGameSocket *pSock = NULL);
	void Send(Packet * pkt);
	void SendSystemMsg( char* pMsg, int type=0, int who=0 );
	void ResetBattleZone();

	void OnTimer(UINT nIDEvent);
	~CServerDlg();

public:
	NpcArray			m_arNpc;
	NpcTableArray		m_arMonTable;
	NpcTableArray		m_arNpcTable;
	NpcThreadArray		m_arNpcThread;
	NpcThreadArray		m_arEventNpcThread;	// Event Npc Logic
	PartyArray			m_arParty;
	ZoneNpcInfoList		m_ZoneNpcList;
	MagictableArray		m_MagictableArray;
	Magictype1Array		m_Magictype1Array;
	Magictype2Array		m_Magictype2Array;
	Magictype3Array		m_Magictype3Array;
	Magictype4Array		m_Magictype4Array;
	MakeWeaponItemTableArray	m_MakeWeaponItemArray;
	MakeWeaponItemTableArray	m_MakeDefensiveItemArray;
	MakeGradeItemTableArray	m_MakeGradeItemArray;
	MakeLareItemTableArray	m_MakeLareItemArray;
	ZoneArray				g_arZone;
	NpcItemArray			m_NpcItemArray;
	MakeItemGroupArray		m_MakeItemGroupArray;
	HANDLE m_hZoneEventThread;		// zone

	char m_strGameDSN[32], m_strGameUID[32], m_strGamePWD[32];
	OdbcConnection m_GameDB;

	CUser* m_pUser[MAX_USER];

	// ���� ��ü ����	//BOOL			m_bNpcExit;
	uint16			m_TotalNPC;			// DB���ִ� �� ��
	long			m_CurrentNPCError;	// ���ÿ��� ������ ��
	long			m_CurrentNPC;		// ���� ���ӻ󿡼� ������ ���õ� ��
	short			m_sTotalMap;		// Zone �� 
	short			m_sMapEventNpc;		// Map���� �о���̴� event npc ��

	BOOL			m_bFirstServerFlag;		// ������ ó�������� �� ���Ӽ����� ���� ��쿡�� 1, ���� ���� ��� 0
	BYTE  m_byBattleEvent;				   // ���� �̺�Ʈ ���� �÷���( 1:�������� �ƴ�, 0:������)
	short m_sKillKarusNpc, m_sKillElmoNpc; // ���ﵿ�ȿ� ���� npc����

	uint16	m_iYear, m_iMonth, m_iDate, m_iHour, m_iMin, m_iAmount;
	uint8 m_iWeather;
	BYTE	m_byNight;			// ������,, �������� �Ǵ�... 1:��, 2:��

	static KOSocketMgr<CGameSocket> s_socketMgr;

private:
	BYTE				m_byZone;
};

extern CServerDlg * g_pMain;