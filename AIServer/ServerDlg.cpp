#include "stdafx.h"
#include <time.h>
#include "GameSocket.h"
#include "Npc.h"
#include "User.h"
#include "NpcThread.h"

#include "../shared/database/OdbcRecordset.h"
#include "../shared/database/MagicTableSet.h"
#include "../shared/database/MagicType1Set.h"
#include "../shared/database/MagicType2Set.h"
#include "../shared/database/MagicType3Set.h"
#include "../shared/database/MagicType4Set.h"
#include "../shared/database/NpcPosSet.h"
#include "../shared/database/ZoneInfoSet.h"
#include "../shared/database/NpcItemSet.h"
#include "../shared/database/MakeItemGroupSet.h"
#include "../shared/database/NpcTableSet.h"
#include "../shared/database/MakeWeaponTableSet.h"
#include "../shared/database/MakeDefensiveTableSet.h"
#include "../shared/database/MakeGradeItemTableSet.h"
#include "../shared/database/MakeLareItemTableSet.h"
#include "Region.h"
#include "../shared/ini.h"
#include "../shared/packets.h"

using namespace std;

BOOL g_bNpcExit	= FALSE;
ZoneArray			g_arZone;

CRITICAL_SECTION g_User_critical, g_region_critical;

KOSocketMgr<CGameSocket> CServerDlg::s_socketMgr;

static DWORD s_dwCheckAliveID;

#define MONSTER_SPEED	1500

CServerDlg::CServerDlg()
{
	m_iYear = 0; 
	m_iMonth = 0;
	m_iDate = 0;
	m_iHour = 0;
	m_iMin = 0;
	m_iWeather = 0;
	m_iAmount = 0;
	m_byNight = 1;
	m_byZone = KARUS_ZONE;
	m_byBattleEvent = BATTLEZONE_CLOSE;
	m_sKillKarusNpc = 0;
	m_sKillElmoNpc = 0;
	m_hZoneEventThread = NULL;

	memset(m_strGameDSN, 0, sizeof(m_strGameDSN));
	memset(m_strGameUID, 0, sizeof(m_strGameUID));
	memset(m_strGamePWD, 0, sizeof(m_strGamePWD));
}

void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	g_pMain->OnTimer(idEvent);
}

bool CServerDlg::Startup()
{
	//----------------------------------------------------------------------
	//	Sets a random number starting point.
	//----------------------------------------------------------------------
	s_dwCheckAliveID = SetTimer(NULL, 1, 10000, &TimerProc);

	srand( (unsigned)time(NULL) );

	InitializeCriticalSection( &g_region_critical );
	InitializeCriticalSection( &g_User_critical );
	m_sMapEventNpc = 0;
	m_bFirstServerFlag = FALSE;			

	// User Point Init
	for(int i=0; i<MAX_USER; i++)
		m_pUser[i] = NULL;

	// Server Start
	//CTime time = CTime::GetCurrentTime();
	//AddToList("[AI ServerStart - %d-%d-%d, %02d:%02d]", time.GetYear(), time.GetMonth(), time.GetDay(), time.GetHour(), time.GetMinute() );

	//----------------------------------------------------------------------
	//	DB part initialize
	//----------------------------------------------------------------------
	GetServerInfoIni();

	if (!m_GameDB.Connect(m_strGameDSN, m_strGameUID, m_strGamePWD, false))
	{
		OdbcError *pError = m_GameDB.GetError();
		printf("ERROR: Could not connect to the database server, received error:\n%s\n", 
			pError->ErrorMessage.c_str());
		delete pError;
		return false;
	}

	// This probably isn't really even needed.
	if		(m_byZone == UNIFY_ZONE)	printf(" *** Server zone: ALL ***");
	else if (m_byZone == KARUS_ZONE)	printf(" *** Server zone: KARUS ***");
	else if (m_byZone == ELMORAD_ZONE)	printf(" *** Server zone: EL MORAD ***");
	else if (m_byZone == BATTLE_ZONE)	printf(" *** Server zone: BATTLE ***");
	else
	{
		printf("ERROR: Server zone unknown (%d).\n", m_byZone);
		return false;
	}

	printf("\n\n");

	//----------------------------------------------------------------------
	//	Communication Part Initialize ...
	//----------------------------------------------------------------------

	uint16 sPort = BATTLE_ZONE;
	if (m_byZone == KARUS_ZONE || m_byZone == UNIFY_ZONE)
		sPort = AI_KARUS_SOCKET_PORT;
	else if (m_byZone == ELMORAD_ZONE)
		sPort = AI_ELMO_SOCKET_PORT;

	if (!s_socketMgr.Listen(sPort, MAX_SOCKET))
		return false;

	//----------------------------------------------------------------------
	//	Load tables
	//----------------------------------------------------------------------
	if (!GetMagicTableData()
		|| !GetMagicType1Data()
		|| !GetMagicType2Data()
		|| !GetMagicType3Data()
		|| !GetMagicType4Data()
		|| !GetNpcItemTable()
		|| !GetMakeItemGroupTable()
		|| !GetMakeWeaponItemTableData()
		|| !GetMakeDefensiveItemTableData()
		|| !GetMakeGradeItemTableData()
		|| !GetMakeLareItemTableData()
		|| !GetNpcTableData(false)
		|| !GetNpcTableData(true)
		// Load maps
		|| !MapFileLoad()
		// Spawn NPC threads
		|| !CreateNpcThread())
		return false;

	//----------------------------------------------------------------------
	//	Start NPC THREAD
	//----------------------------------------------------------------------
	ResumeAI();
	return true; 
}

BOOL CServerDlg::GetMagicTableData()
{
	LOAD_TABLE(CMagicTableSet, &m_GameDB, &m_MagictableArray, false);
}

BOOL CServerDlg::GetMagicType1Data()
{
	LOAD_TABLE(CMagicType1Set, &m_GameDB, &m_Magictype1Array, false);
}

BOOL CServerDlg::GetMagicType2Data()
{
	LOAD_TABLE(CMagicType2Set, &m_GameDB, &m_Magictype2Array, false);
}

BOOL CServerDlg::GetMagicType3Data()
{
	LOAD_TABLE(CMagicType3Set, &m_GameDB, &m_Magictype3Array, false);
}

BOOL CServerDlg::GetMagicType4Data()
{
	LOAD_TABLE(CMagicType4Set, &m_GameDB, &m_Magictype4Array, false);
}

BOOL CServerDlg::GetMakeWeaponItemTableData()
{
	LOAD_TABLE(CMakeWeaponTableSet, &m_GameDB, &m_MakeWeaponItemArray, true);
}

BOOL CServerDlg::GetMakeDefensiveItemTableData()
{
	LOAD_TABLE(CMakeDefensiveTableSet, &m_GameDB, &m_MakeDefensiveItemArray, true);
}

BOOL CServerDlg::GetMakeGradeItemTableData()
{
	LOAD_TABLE(CMakeGradeItemTableSet, &m_GameDB, &m_MakeGradeItemArray, false);
}

BOOL CServerDlg::GetMakeLareItemTableData()
{
	LOAD_TABLE(CMakeLareItemTableSet, &m_GameDB, &m_MakeLareItemArray, false);
}

BOOL CServerDlg::GetNpcItemTable()
{
	LOAD_TABLE(CNpcItemSet, &m_GameDB, &m_NpcItemArray, false);
}

BOOL CServerDlg::GetMakeItemGroupTable()
{
	LOAD_TABLE(CMakeItemGroupSet, &m_GameDB, &m_MakeItemGroupArray, false);
}

BOOL CServerDlg::GetNpcTableData(bool bNpcData /*= true*/)
{
	if (bNpcData)	{ LOAD_TABLE(CNpcTableSet, &m_GameDB, &m_arNpcTable, false); }
	else			{ LOAD_TABLE(CMonTableSet, &m_GameDB, &m_arMonTable, false); }
}

BOOL CServerDlg::CreateNpcThread()
{
	m_TotalNPC = m_sMapEventNpc;
	m_CurrentNPC = 0;
	m_CurrentNPCError = 0;

	LOAD_TABLE_ERROR_ONLY(CNpcPosSet, &m_GameDB, NULL, false);
			
	int step = 0, nThreadNumber = 0;
	CNpcThread* pNpcThread = NULL;
	DWORD id;

	foreach_stlmap (itr, m_arNpc)
	{
		if (step == 0)
			pNpcThread = new CNpcThread;

		CNpc *pNpc = pNpcThread->m_pNpc[step] = itr->second;
		pNpc->m_sThreadNumber = nThreadNumber;
		pNpc->Init();
		
		++step;
		
		if( step == NPC_NUM )
		{
			pNpcThread->m_sThreadNumber = nThreadNumber++;
			pNpcThread->m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&NpcThreadProc, &(pNpcThread->m_ThreadInfo), CREATE_SUSPENDED, &id);
			m_arNpcThread.push_back( pNpcThread );
			step = 0;
		}
	}
	if( step != 0 )
	{
		pNpcThread->m_sThreadNumber = nThreadNumber++;
		pNpcThread->m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&NpcThreadProc, &(pNpcThread->m_ThreadInfo), CREATE_SUSPENDED, &id);
		m_arNpcThread.push_back( pNpcThread );
	}

	m_hZoneEventThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ZoneEventThreadProc, (LPVOID)(this), CREATE_SUSPENDED, &id);
	printf("[Monster Init - %d, threads=%d]\n", m_TotalNPC, m_arNpcThread.size());
	return TRUE;
}

bool CServerDlg::LoadSpawnCallback(OdbcCommand *dbCommand)
{
	// Avoid allocating stack space for these.
	// This method will only ever run in the same thread.
	static int nRandom = 0;
	static double dbSpeed = 0;
	static CNpcTable * pNpcTable = NULL;
	static CRoomEvent* pRoom = NULL;
	static char szPath[500];
	static float fRandom_X = 0.0f, fRandom_Z = 0.0f;

	// Unfortunately we cannot simply read what we need directly
	// into the CNpc instance. We have to resort to creating
	// copies of the data to allow for the way they handle multiple spawns...
	// Best we can do, I think, is to avoid allocating it on the stack.
	static uint8	bNumNpc, bZoneID, bActType, bRegenType, bDungeonFamily, bSpecialType,
					bTrapNumber, bDirection, bDotCnt;	
	static uint16	sSid, sRegTime;
	static uint32	nServerNum;
	static int32	iLeftX, iTopZ, iRightX, iBottomZ,
					iLimitMinX, iLimitMinZ, iLimitMaxX, iLimitMaxZ;

	dbCommand->FetchByte(1, bZoneID);
	dbCommand->FetchUInt16(2, sSid);
	dbCommand->FetchByte(3, bActType);
	dbCommand->FetchByte(4, bRegenType);
	dbCommand->FetchByte(5, bDungeonFamily);
	dbCommand->FetchByte(6, bSpecialType);
	dbCommand->FetchByte(7, bTrapNumber);
	dbCommand->FetchInt32(8, iLeftX);
	dbCommand->FetchInt32(9, iTopZ);
	dbCommand->FetchInt32(10, iRightX);
	dbCommand->FetchInt32(11, iBottomZ);
	dbCommand->FetchInt32(12, iLimitMinZ);
	dbCommand->FetchInt32(13, iLimitMinX);
	dbCommand->FetchInt32(14, iLimitMaxX);
	dbCommand->FetchInt32(15, iLimitMaxZ);
	dbCommand->FetchByte(16, bNumNpc);
	dbCommand->FetchUInt16(17, sRegTime);
	dbCommand->FetchByte(18, bDirection);
	dbCommand->FetchByte(19, bDotCnt);
	dbCommand->FetchString(20, szPath, sizeof(szPath));

	nServerNum = GetServerNumber(bZoneID);

	if (m_byZone == nServerNum || m_byZone == UNIFY_ZONE)
	{
		uint8 bPathSerial = 1;
		for (uint8 j = 0; j < bNumNpc; j++)
		{
			CNpc * pNpc = new CNpc();

			pNpc->m_byMoveType = bActType;
			pNpc->m_byInitMoveType = bActType;

			bool bMonster = (bActType < 100);
			if (bMonster)
			{
				pNpcTable = m_arMonTable.GetData(sSid);
			}
			else 
			{
				pNpc->m_byMoveType = bActType - 100;
				pNpcTable = m_arNpcTable.GetData(sSid);
			}

			if (pNpcTable == NULL)
			{
				printf("NPC %d not found in %s table.\n", pNpc->m_sNid, bMonster ? "K_MONSTER" : "K_NPC");
				delete pNpc;
				return false;
			}

			pNpc->Load(m_TotalNPC++, pNpcTable);
			pNpc->m_byBattlePos = 0;

			if (pNpc->m_byMoveType >= 2)
			{
				pNpc->m_byBattlePos = myrand(1, 3);
				pNpc->m_byPathCount = bPathSerial++;
			}

			pNpc->InitPos();
			
			pNpc->m_bCurZone = bZoneID;

			nRandom = abs(iLeftX - iRightX);
			if (nRandom <= 1)
				fRandom_X = (float)iLeftX;
			else
			{
				if (iLeftX < iRightX)
					fRandom_X = (float)myrand(iLeftX, iRightX);
				else
					fRandom_X = (float)myrand(iRightX, iLeftX);
			}

			nRandom = abs(iTopZ - iBottomZ);
			if (nRandom <= 1)
				fRandom_Z = (float)iTopZ;
			else
			{
				if (iTopZ < iBottomZ)
					fRandom_Z = (float)myrand(iTopZ, iBottomZ);
				else
					fRandom_Z = (float)myrand(iBottomZ, iTopZ);
			}

			pNpc->m_fCurX	= fRandom_X;
			pNpc->m_fCurY	= 0;
			pNpc->m_fCurZ	= fRandom_Z;
					
			pNpc->m_sRegenTime		= sRegTime * SECOND;
			pNpc->m_byDirection		= bDirection;
			pNpc->m_sMaxPathCount	= bDotCnt;

			if ((pNpc->m_byMoveType == 2 || pNpc->m_byMoveType == 3) && bDotCnt == 0)
			{
				pNpc->m_byMoveType = 1;
				TRACE("##### ServerDlg:CreateNpcThread - Path type Error :  nid=%d, sid=%d, name=%s, acttype=%d, path=%d #####\n", pNpc->m_sNid+NPC_BAND, pNpc->m_proto->m_sSid, pNpc->m_proto->m_strName, pNpc->m_byMoveType, pNpc->m_sMaxPathCount);
			}

			if (bDotCnt > 0)
			{
				int index = 0;
				for (int l = 0; l < bDotCnt; l++)
				{
					static char szX[5], szZ[5];

					memset(szX, 0, sizeof(szX));
					memset(szZ, 0, sizeof(szZ));

					memcpy(szX, szPath + index, 4);
					index += 4;

					memcpy(szZ, szPath + index, 4);
					index += 4;

					pNpc->m_PathList.pPattenPos[l].x = atoi(szX);
					pNpc->m_PathList.pPattenPos[l].z = atoi(szZ);
				}
			}

			pNpc->m_tItemPer		= pNpcTable->m_tItemPer;	// NPC Type
			pNpc->m_tDnPer			= pNpcTable->m_tDnPer;	// NPC Type

			pNpc->m_nInitMinX = pNpc->m_nLimitMinX		= iLeftX;
			pNpc->m_nInitMinY = pNpc->m_nLimitMinZ		= iTopZ;
			pNpc->m_nInitMaxX = pNpc->m_nLimitMaxX		= iRightX;
			pNpc->m_nInitMaxY = pNpc->m_nLimitMaxZ		= iBottomZ;

			// dungeon work
			pNpc->m_byDungeonFamily	= bDungeonFamily;
			pNpc->m_bySpecialType	= bSpecialType;
			pNpc->m_byRegenType		= bRegenType;
			pNpc->m_byTrapNumber    = bTrapNumber;

			if (pNpc->m_byDungeonFamily > 0)
			{
				pNpc->m_nLimitMinX = iLimitMinX;
				pNpc->m_nLimitMinZ = iLimitMinZ;
				pNpc->m_nLimitMaxX = iLimitMaxX;
				pNpc->m_nLimitMaxZ = iLimitMaxZ;
			}	
			
			pNpc->m_pZone = GetZoneByID(pNpc->m_bCurZone);
			if (pNpc->GetMap() == NULL)
			{
				printf(_T("Error: NPC %d in zone %d that does not exist."), sSid, bZoneID);
				delete pNpc;
				return false;
			}

			if (!m_arNpc.PutData(pNpc->m_sNid, pNpc))
			{
				--m_TotalNPC;
				TRACE("Npc PutData Fail - %d\n", pNpc->m_sNid);
				delete pNpc;
				continue;
			}

			pNpc->SetUid(pNpc->m_fCurX, pNpc->m_fCurZ, pNpc->m_sNid + NPC_BAND);

			if (pNpc->GetMap()->m_byRoomEvent > 0 && pNpc->m_byDungeonFamily > 0)
			{
				pRoom = pNpc->GetMap()->m_arRoomEventArray.GetData(pNpc->m_byDungeonFamily);
				if (pRoom == NULL)
				{
					printf("Error : CServerDlg,, Map Room Npc Fail!!\n");
					delete pNpc;
					return false;
				}

				// this is why their CSTLMap class sucks.
				int *pInt = new int;
				*pInt = pNpc->m_sNid;
				if (!pRoom->m_mapRoomNpcArray.PutData(pNpc->m_sNid, pInt))
				{
					delete pInt;
					TRACE("### Map - Room Array MonsterNid Fail : nid=%d, sid=%d ###\n", 
					pNpc->m_sNid, pNpc->m_proto->m_sSid);
				}
			}
		}
	}

	return true;
}


//	NPC Thread ���� �۵���Ų��.
void CServerDlg::ResumeAI()
{
	foreach (itr, m_arNpcThread)
	{
		for (int j = 0; j < NPC_NUM; j++)
			(*itr)->m_ThreadInfo.pNpc[j] = (*itr)->m_pNpc[j];

		ResumeThread((*itr)->m_hThread);
	}

	ResumeThread(m_hZoneEventThread);
}

void CServerDlg::DeleteUserList(int uid)
{
	if(uid < 0 || uid >= MAX_USER)	{
		TRACE("#### ServerDlg:DeleteUserList Uid Fail : uid=%d\n", uid);
		return;
	}

	EnterCriticalSection( &g_User_critical );

	CUser* pUser = NULL;
	pUser = m_pUser[uid];
	if( !pUser )	{
		LeaveCriticalSection( &g_User_critical );
		TRACE("#### ServerDlg:DeleteUserList UserPtr Fail : uid=%d\n", uid);
		return;
	}

	if( pUser->m_iUserId == uid )	{
		TRACE("*** UserLogOut���� ������ ��ȯ : uid=%d, %s ***\n", uid, pUser->m_strUserID);
		pUser->m_lUsed = 1;
		delete m_pUser[uid];
		m_pUser[uid] = NULL;
	}
	else
		TRACE("#### ServerDlg:DeleteUserList Not Uid Fail : uid=%d\n", uid);

	LeaveCriticalSection( &g_User_critical );
}

BOOL CServerDlg::MapFileLoad()
{
	ZoneInfoMap zoneMap;

	m_sTotalMap = 0;
	LOAD_TABLE_ERROR_ONLY(CZoneInfoSet, &m_GameDB, &zoneMap, false); 

	foreach (itr, zoneMap)
	{
		_ZONE_INFO *pZone = itr->second;

		MAP *pMap = new MAP();
		if (!pMap->Initialize(pZone))
		{
			printf("ERROR: Unable to load SMD - %s\n", pZone->m_MapName.c_str());
			delete pZone;
			delete pMap;
			g_arZone.DeleteAllData();
			m_sTotalMap = 0;
			return FALSE;
		}

		delete pZone;
		g_arZone.PutData(pMap->m_nZoneNumber, pMap);
		m_sTotalMap++;
	}

	return TRUE;
}

// game server�� ��� npc������ ����..
void CServerDlg::AllNpcInfo()
{
	Packet result(NPC_INFO_ALL);
	result.SByte();
	foreach_stlmap (itr, g_arZone)
	{
		uint32 nZone = itr->first;
		uint8 bCount = 0;

		result.clear();
		result << bCount;

		foreach_stlmap (itr, m_arNpc)
		{
			CNpc *pNpc = itr->second;
			if (pNpc == NULL
				|| pNpc->m_bCurZone != nZone)	
				continue;

			pNpc->FillNpcInfo(result);
			if (++bCount == NPC_NUM)
			{
				result.put(0, bCount);
				s_socketMgr.SendAllCompressed(&result);

				// Reset packet buffer
				bCount = 0;
				result.clear();
				result << bCount;
			}
		}	

		if (bCount != 0 && bCount < NPC_NUM)
		{
			result.put(0, bCount);
			s_socketMgr.SendAllCompressed(&result);
		}

		Packet serverInfo(AG_SERVER_INFO, uint8(nZone));
		serverInfo << uint16(m_TotalNPC);
		s_socketMgr.SendAll(&serverInfo);
	}
}

CUser* CServerDlg::GetUserPtr(int nid)
{
	CUser* pUser = NULL;

	if(nid < 0 || nid >= MAX_USER)	{
		if(nid != -1)		TRACE("### GetUserPtr :: User Array Overflow [%d] ###\n", nid );
		return NULL;
	}

	pUser = m_pUser[nid];
	if(pUser == NULL)	return NULL;
	if( pUser->m_lUsed == 1 ) return NULL;	// ������ ����� ���ġ ����.. (logout��)
	if(pUser->m_iUserId < 0 || pUser->m_iUserId >= MAX_USER)	return NULL;

	if( pUser->m_iUserId == nid )	return pUser;

	return NULL;
}

void CServerDlg::OnTimer(UINT nIDEvent) 
{
	if (nIDEvent == s_dwCheckAliveID)
		CheckAliveTest();
}

void CServerDlg::CheckAliveTest()
{
	Packet result(AG_CHECK_ALIVE_REQ);
	SessionMap & sessMap = s_socketMgr.GetActiveSessionMap();
	uint32 count = 0, sessCount = sessMap.size();
	foreach (itr, sessMap)
	{
		if (itr->second->Send(&result))
			count++;
	}
	s_socketMgr.ReleaseLock();

	if (sessCount > 0 && count == 0)
		DeleteAllUserList();

	RegionCheck();
}

void CServerDlg::DeleteAllUserList(CGameSocket *pSock)
{
	// If a server disconnected, show it...
	if (pSock != NULL)
	{
		printf("[GameServer disconnected = %s]\n", pSock->GetRemoteIP().c_str());
		return;
	}

	// Server didn't disconnect? 
	if (!m_bFirstServerFlag)
		return;

	// If there's no servers even connected, cleanup.
	TRACE("*** DeleteAllUserList - Start *** \n");
	foreach_stlmap (itr, g_arZone)
	{
		MAP * pMap = itr->second;
		if (pMap == NULL)	
			continue;
		for (int i=0; i<=pMap->GetXRegionMax(); i++ ) {
			for( int j=0; j<=pMap->GetZRegionMax(); j++ ) {
				pMap->m_ppRegion[i][j].m_RegionUserArray.DeleteAllData();
			}
		}
	}

	EnterCriticalSection( &g_User_critical );
	for (int i = 0; i < MAX_USER; i++)	
	{
		CUser *pUser = m_pUser[i];
		if (pUser == NULL)  
			continue;

		delete m_pUser[i];
		m_pUser[i] = NULL;
	}
	LeaveCriticalSection( &g_User_critical );

	// Party Array Delete 
	m_arParty.DeleteAllData();

	m_bFirstServerFlag = FALSE;
	TRACE("*** DeleteAllUserList - End *** \n");

	printf("[ DELETE All User List ]\n");
}

void CServerDlg::Send(Packet * pkt)
{
	s_socketMgr.SendAll(pkt);
}

void CServerDlg::GameServerAcceptThread()
{
	s_socketMgr.RunServer();
}

//	�߰��� ��ȯ���� �޸𸮸� �����ϱ����� �÷��װ� 0�� ���°͸� �ѱ��.
CNpc* CServerDlg::GetEventNpcPtr()
{
	CNpc* pNpc = NULL;
	for(int i = m_TotalNPC; i < m_arNpc.GetSize(); i++)		{
		pNpc = m_arNpc.GetData( i );
		if( !pNpc ) continue;

		if( pNpc->m_lEventNpc != 0 )	continue;

		pNpc->m_lEventNpc = 1;

		return pNpc;
	}
	return NULL;
}

BOOL CServerDlg::SetSummonNpcData(CNpc* pNpc, int zone, float fx, float fz)
{
	int  iCount = 0;
	CNpc* pEventNpc	= GetEventNpcPtr();

	if(pEventNpc == NULL)
	{
		TRACE("��ȯ�Ҽ� �ִ� ���� �ִ� 20�����Դϴ�.\n");
		return FALSE;
	}

	pEventNpc->m_proto	= pNpc->m_proto;
	pEventNpc->m_byMoveType = 1;
	pEventNpc->m_byInitMoveType = 1;
	pEventNpc->m_byBattlePos = 0;
	pEventNpc->m_sSize		= pNpc->m_sSize;			// ĳ������ ����(100 �ۼ�Ʈ ����)
	pEventNpc->m_iWeapon_1		= pNpc->m_iWeapon_1;	// ���빫��
	pEventNpc->m_iWeapon_2		= pNpc->m_iWeapon_2;	// ���빫��
	pEventNpc->m_byGroup		= pNpc->m_byGroup;		// �Ҽ�����
	pEventNpc->m_byActType		= pNpc->m_byActType;	// �ൿ����
	pEventNpc->m_byRank			= pNpc->m_byRank;		// ����
	pEventNpc->m_byTitle		= pNpc->m_byTitle;		// ����
	pEventNpc->m_iSellingGroup = pNpc->m_iSellingGroup;
	pEventNpc->m_iHP			= pNpc->m_iMaxHP;		// �ִ� HP
	pEventNpc->m_iMaxHP			= pNpc->m_iMaxHP;		// ���� HP
	pEventNpc->m_sMP			= pNpc->m_sMaxMP;		// �ִ� MP
	pEventNpc->m_sMaxMP			= pNpc->m_sMaxMP;		// ���� MP
	pEventNpc->m_sAttack		= pNpc->m_sAttack;		// ���ݰ�
	pEventNpc->m_sDefense		= pNpc->m_sDefense;		// ��
	pEventNpc->m_sHitRate		= pNpc->m_sHitRate;		// Ÿ�ݼ�����
	pEventNpc->m_sEvadeRate		= pNpc->m_sEvadeRate;	// ȸ�Ǽ�����
	pEventNpc->m_sDamage		= pNpc->m_sDamage;		// �⺻ ������
	pEventNpc->m_sAttackDelay	= pNpc->m_sAttackDelay; // ���ݵ�����
	pEventNpc->m_sSpeed			= pNpc->m_sSpeed;		// �̵��ӵ�
	pEventNpc->m_fSpeed_1		= pNpc->m_fSpeed_1;	// �⺻ �̵� Ÿ��
	pEventNpc->m_fSpeed_2		= pNpc->m_fSpeed_2;	// �ٴ� �̵� Ÿ��..
	pEventNpc->m_fOldSpeed_1	= pNpc->m_fOldSpeed_1;	// �⺻ �̵� Ÿ��
	pEventNpc->m_fOldSpeed_2	= pNpc->m_fOldSpeed_2;	// �ٴ� �̵� Ÿ��..
	pEventNpc->m_fSecForMetor   = 4.0f;					// �ʴ� �� �� �ִ� �Ÿ�..
	pEventNpc->m_sStandTime		= pNpc->m_sStandTime;	// ���ִ� �ð�
	pEventNpc->m_byFireR		= pNpc->m_byFireR;		// ȭ�� ���׷�
	pEventNpc->m_byColdR		= pNpc->m_byColdR;		// �ñ� ���׷�
	pEventNpc->m_byLightningR	= pNpc->m_byLightningR;	// ���� ���׷�
	pEventNpc->m_byMagicR		= pNpc->m_byMagicR;		// ���� ���׷�
	pEventNpc->m_byDiseaseR		= pNpc->m_byDiseaseR;	// ���� ���׷�
	pEventNpc->m_byPoisonR		= pNpc->m_byPoisonR;	// �� ���׷�
	pEventNpc->m_byLightR		= pNpc->m_byLightR;		// �� ���׷�
	pEventNpc->m_bySearchRange	= pNpc->m_bySearchRange;	// �� Ž�� ����
	pEventNpc->m_byAttackRange	= pNpc->m_byAttackRange;	// �����Ÿ�
	pEventNpc->m_byTracingRange	= pNpc->m_byTracingRange;	// �߰ݰŸ�
	pEventNpc->m_iMoney			= pNpc->m_iMoney;			// �������� ��
	pEventNpc->m_iItem			= pNpc->m_iItem;			// �������� ������
	pEventNpc->m_tNpcLongType    = pNpc->m_tNpcLongType;
	pEventNpc->m_byWhatAttackType = pNpc->m_byWhatAttackType;

	//////// MONSTER POS ////////////////////////////////////////
	pEventNpc->m_bCurZone = zone;
	pEventNpc->m_fCurX	= fx;
	pEventNpc->m_fCurY	= 0;
	pEventNpc->m_fCurZ	= fz;
 	pEventNpc->m_nInitMinX			= pNpc->m_nInitMinX;
	pEventNpc->m_nInitMinY			= pNpc->m_nInitMinY;
	pEventNpc->m_nInitMaxX			= pNpc->m_nInitMaxX;
	pEventNpc->m_nInitMaxY			= pNpc->m_nInitMaxY;
	pEventNpc->m_sRegenTime		= pNpc->m_sRegenTime;	// ��(DB)����-> �и��������
	pEventNpc->m_tItemPer		= pNpc->m_tItemPer;	// NPC Type
	pEventNpc->m_tDnPer			= pNpc->m_tDnPer;	// NPC Type

	pEventNpc->m_pZone		= GetZoneByID(zone);

	pEventNpc->m_NpcState = NPC_DEAD;	// ���´� ���������� �ؾ� �Ѵ�.. 
	pEventNpc->m_bFirstLive = 1;		// ó�� ��Ƴ� ���� ����� �Ѵ�..

	if (pEventNpc->GetMap() == NULL)
	{
		TRACE("Zone %d doesn't exist (NPC=%d)\n", zone, pNpc->m_proto->m_sSid);
		return FALSE;
	}

	pEventNpc->Init();

	BOOL bSuccess = FALSE;

	int test = 0;
	
	for(int i = 0; i < NPC_NUM; i++ ) {
		test = m_arEventNpcThread[0]->m_ThreadInfo.m_byNpcUsed[i];
		TRACE("setsummon == %d, used=%d\n", i, test);
		if( m_arEventNpcThread[0]->m_ThreadInfo.m_byNpcUsed[i] == 0 )	{
			m_arEventNpcThread[0]->m_ThreadInfo.m_byNpcUsed[i] = 1;
			bSuccess = TRUE;
			m_arEventNpcThread[0]->m_ThreadInfo.pNpc[i] = pEventNpc;
			break;
		}
	}

	if(!bSuccess)	{
		pEventNpc->m_lEventNpc = 0;
		TRACE("### ��ȯ�� �����߽��ϴ�. ###\n");
		return FALSE;
	}

	TRACE("*** %d, %s �� ��ȯ�Ͽ����ϴ�. state = %d ***\n", pEventNpc->m_sNid+NPC_BAND, pEventNpc->m_proto->m_strName, pEventNpc->m_NpcState);

	return TRUE;
}

void CServerDlg::RegionCheck()
{
	EnterCriticalSection( &g_User_critical );
	foreach_stlmap(itr, g_arZone)	
	{
		MAP *pMap = itr->second;
		if (pMap == NULL)
			continue;

		for (int i = 0; i <= pMap->GetXRegionMax(); i++)
			for (int j = 0; j <= pMap->GetZRegionMax(); j++)
				pMap->m_ppRegion[i][j].m_byMoving = (pMap->m_ppRegion[i][j].m_RegionUserArray.GetSize() > 0 ? 1 : 0);
	}
	LeaveCriticalSection( &g_User_critical );
}

BOOL CServerDlg::AddObjectEventNpc(_OBJECT_EVENT* pEvent, int zone_number)
{
	int i=0, j=0, objectid=0;
	int offset = 0;
	int nServerNum = 0;
	nServerNum = GetServerNumber( zone_number );

	int sSid = (pEvent->sType == OBJECT_ANVIL || pEvent->sType == OBJECT_ARTIFACT 
					? pEvent->sIndex : pEvent->sControlNpcID);
	if (sSid <= 0)
		return FALSE;

	CNpcTable * pNpcTable = m_arNpcTable.GetData(sSid);
	if(pNpcTable == NULL)	{
		// TRACE("#### AddObjectEventNpc Fail : [sid = %d], zone=%d #####\n", pEvent->sIndex, zone_number);
		return FALSE;
	}

	CNpc *pNpc = new CNpc();

	pNpc->m_byMoveType = 0;
	pNpc->m_byInitMoveType = 0;

	pNpc->m_byBattlePos = 0;

	pNpc->m_byObjectType = SPECIAL_OBJECT;
	pNpc->m_bCurZone	= zone_number;
	pNpc->m_byGateOpen	= (BYTE)pEvent->sStatus;
	pNpc->m_fCurX		= pEvent->fPosX;
	pNpc->m_fCurY		= pEvent->fPosY;
	pNpc->m_fCurZ		= pEvent->fPosZ;
	
 	pNpc->m_nInitMinX	= (int)pEvent->fPosX-1;
	pNpc->m_nInitMinY	= (int)pEvent->fPosZ-1;
	pNpc->m_nInitMaxX	= (int)pEvent->fPosX+1;
	pNpc->m_nInitMaxY	= (int)pEvent->fPosZ+1;	

	pNpc->Load(m_sMapEventNpc++, pNpcTable);

	if (pNpc->GetMap() == NULL
		|| !m_arNpc.PutData(pNpc->m_sNid, pNpc))
	{
		TRACE("Npc PutData Fail - %d\n", pNpc->m_sNid);
		delete pNpc;
		return FALSE;
	}

	m_TotalNPC = m_sMapEventNpc;
	return TRUE;
}

MAP * CServerDlg::GetZoneByID(int zonenumber)
{
	return g_arZone.GetData(zonenumber);
}

int CServerDlg::GetServerNumber( int zonenumber )
{
	MAP *pMap = GetZoneByID(zonenumber);
	if (pMap == NULL)
		return -1;

	return pMap->m_nServerNo;
}

void CServerDlg::GetServerInfoIni()
{
	CIni inifile("./server.ini");
	m_byZone = inifile.GetInt("SERVER", "ZONE", UNIFY_ZONE);
	inifile.GetString("ODBC", "GAME_DSN", "KN_online", m_strGameDSN, sizeof(m_strGameDSN), false);
	inifile.GetString("ODBC", "GAME_UID", "knight", m_strGameUID, sizeof(m_strGameUID), false);
	inifile.GetString("ODBC", "GAME_PWD", "knight", m_strGamePWD, sizeof(m_strGamePWD), false);
}

void CServerDlg::SendSystemMsg( char* pMsg, int type, int who )
{
	Packet result(AG_SYSTEM_MSG, uint8(type));
	result << int16(who) << pMsg;
	Send(&result);
}

void CServerDlg::ResetBattleZone()
{
	TRACE("ServerDlg - ResetBattleZone() : start \n");
	foreach_stlmap (itr, g_arZone)
	{
		MAP *pMap = itr->second;
		if (pMap == NULL || pMap->m_byRoomEvent == 0) 
			continue;
		//if( pMap->IsRoomStatusCheck() == TRUE )	continue;	// ��ü���� Ŭ���� �Ǿ��ٸ�
		pMap->InitializeRoom();
	}
	TRACE("ServerDlg - ResetBattleZone() : end \n");
}

CServerDlg::~CServerDlg() 
{
	KillTimer(NULL, s_dwCheckAliveID);

	g_bNpcExit = TRUE;

	foreach (itr, m_arNpcThread)
		WaitForSingleObject((*itr)->m_hThread, INFINITE);

	WaitForSingleObject(m_hZoneEventThread, INFINITE);

	// NpcThread Array Delete
	foreach (itr, m_arNpcThread)
		delete *itr;
	m_arNpcThread.clear();

	// User Array Delete
	for(int i = 0; i < MAX_USER; i++)	{
		if(m_pUser[i])	{
			delete m_pUser[i];
			m_pUser[i] = NULL;
		}
	}

	m_ZoneNpcList.clear();

	DeleteCriticalSection( &g_region_critical );
	DeleteCriticalSection( &g_User_critical );
}