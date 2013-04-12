#include "stdafx.h"
#include "Map.h"
#include "Serverdlg.h"
#include "Region.h"
#include "Npc.h"
#include "User.h"
#include "RoomEvent.h"
#include "../shared/packets.h"
#include <fstream>
#include <set>
#include "../shared/SMDFile.h"

// This is more than a little convulated.
#define PARSE_ARGUMENTS(count, temp, buff, arg, id, index) for (int _i = 0; _i < count; _i++) { \
	index += ParseSpace(temp, buff + index); \
	arg[id++] = atoi(temp); \
}

inline int ParseSpace( char* tBuf, char* sBuf)
{
	int i = 0, index = 0;
	BOOL flag = FALSE;
	
	while(sBuf[index] == ' ' || sBuf[index] == '\t')index++;
	while(sBuf[index] !=' ' && sBuf[index] !='\t' && sBuf[index] !=(BYTE) 0){
		tBuf[i++] = sBuf[index++];
		flag = TRUE;
	}
	tBuf[i] = 0;

	while(sBuf[index] == ' ' || sBuf[index] == '\t')index++;
	if(!flag) return 0;	
	return index;
};

using namespace std;

extern CRITICAL_SECTION g_region_critical;

/* passthru methods */
int MAP::GetMapSize() { return m_smdFile->GetMapSize(); }
float MAP::GetUnitDistance() { return m_smdFile->GetUnitDistance(); }
int MAP::GetXRegionMax() { return m_smdFile->GetXRegionMax(); }
int MAP::GetZRegionMax() { return m_smdFile->GetZRegionMax(); }
short * MAP::GetEventIDs() { return m_smdFile->GetEventIDs(); }
int MAP::GetEventID(int x, int z) { return m_smdFile->GetEventID(x, z); }


MAP::MAP() : m_smdFile(NULL), m_ppRegion(NULL),
	m_fHeight(NULL), m_byRoomType(0), m_byRoomEvent(0),
	m_byRoomStatus(1), m_byInitRoomCount(0),
	m_nZoneNumber(0), m_sKarusRoom(0), m_sElmoradRoom(0)
{
}

bool MAP::Initialize(_ZONE_INFO *pZone)
{
	m_nServerNo = pZone->m_nServerNo;
	m_nZoneNumber = pZone->m_nZoneNumber;
	m_MapName = pZone->m_MapName;
	m_byRoomEvent = pZone->m_byRoomEvent;

	m_smdFile = SMDFile::Load(pZone->m_MapName);

	if (m_smdFile != NULL)
	{
		m_ppRegion = new CRegion*[m_smdFile->m_nXRegion];
		for (int i = 0; i < m_smdFile->m_nXRegion; i++)
			m_ppRegion[i] = new CRegion[m_smdFile->m_nZRegion]();
	}

	if (m_byRoomEvent > 0)
	{
		if (!LoadRoomEvent())
		{
			printf("ERROR: Unable to load room event (%d.aievt) for map - %s\n", 
				m_byRoomEvent, m_MapName.c_str());
			m_byRoomEvent = 0;
		}
		else
		{
			m_byRoomEvent = 1;
		}
	}

	return (m_smdFile != NULL);
}

MAP::~MAP()
{
	RemoveMapData();

	if (m_smdFile != NULL)
		m_smdFile->DecRef();
}

void MAP::RemoveMapData()
{
	if( m_ppRegion ) {
		for(int i=0; i <= GetXRegionMax(); i++) {
			delete[] m_ppRegion[i];
			m_ppRegion[i] = NULL;
		}
		delete[] m_ppRegion;
		m_ppRegion = NULL;
	}

	if (m_fHeight)
	{
		delete[] m_fHeight;
		m_fHeight = NULL;
	}
	
	m_ObjectEventArray.DeleteAllData();
	m_arRoomEventArray.DeleteAllData();
}

BOOL MAP::IsMovable(int dest_x, int dest_y)
{
	if(dest_x < 0 || dest_y < 0 ) return FALSE;
	if (dest_x >= GetMapSize() || dest_y >= GetMapSize()) return FALSE;

	return m_smdFile->GetEventID(dest_x, dest_y) == 0;
}

BOOL MAP::ObjectIntersect(float x1, float z1, float y1, float x2, float z2, float y2)
{
	return m_smdFile->ObjectCollision(x1, z1, y1, x2, z2, y2);
}

void MAP::RegionUserAdd(int rx, int rz, int uid)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return;

	int *pInt = NULL;

	EnterCriticalSection( &g_region_critical );

	pInt = new int;
	*pInt = uid;
	if (!m_ppRegion[rx][rz].m_RegionUserArray.PutData(uid, pInt))
		delete pInt;

	LeaveCriticalSection( &g_region_critical );
}

BOOL MAP::RegionUserRemove(int rx, int rz, int uid)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return FALSE;

	CRegion	*region = NULL;
	map < int, int* >::iterator		Iter;
	
	EnterCriticalSection( &g_region_critical );
	
	region = &m_ppRegion[rx][rz];
	region->m_RegionUserArray.DeleteData( uid );

	LeaveCriticalSection( &g_region_critical );

	return TRUE;
}

void MAP::RegionNpcAdd(int rx, int rz, int nid)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return;

	int *pInt = NULL;
	
	EnterCriticalSection( &g_region_critical );

	pInt = new int;
	*pInt = nid;
	if (!m_ppRegion[rx][rz].m_RegionNpcArray.PutData(nid, pInt))
		delete pInt;

	LeaveCriticalSection( &g_region_critical );
}

BOOL MAP::RegionNpcRemove(int rx, int rz, int nid)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return FALSE;

	CRegion	*region = NULL;
	map < int, int* >::iterator		Iter;
	
	EnterCriticalSection( &g_region_critical );

	region = &m_ppRegion[rx][rz];
	region->m_RegionNpcArray.DeleteData( nid );

	LeaveCriticalSection( &g_region_critical );

	return TRUE;
}

int  MAP::GetRegionUserSize(int rx, int rz)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return 0;

	EnterCriticalSection( &g_region_critical );
	CRegion	*region = NULL;
	region = &m_ppRegion[rx][rz];
	int nRet = region->m_RegionUserArray.GetSize();
	LeaveCriticalSection( &g_region_critical );

	return nRet;
}

int  MAP::GetRegionNpcSize(int rx, int rz)
{
	if (rx < 0 || rz < 0 || rx > GetXRegionMax() || rz > GetZRegionMax())
		return 0;

	EnterCriticalSection( &g_region_critical );
	CRegion	*region = NULL;
	region = &m_ppRegion[rx][rz];
	int nRet = region->m_RegionNpcArray.GetSize();
	LeaveCriticalSection( &g_region_critical );

	return nRet;
}

BOOL MAP::LoadRoomEvent()
{
	DWORD		length, count;
	string		filename = string_format(".\\MAP\\%d.aievt", m_byRoomEvent);
	char		byte;
	char		buf[4096];
	char		first[1024];
	char		temp[1024];
	int			index = 0;
	int			t_index = 0, logic=0, exec=0;
	int			event_num = 0, nation = 0;

	CRoomEvent*	pEvent = NULL;
	ifstream is(filename);
	if (!is)
		return FALSE;

	is.seekg(0, is.end);
    length = (DWORD)is.tellg();
    is.seekg (0, is.beg);

	count = 0;

	while (count < length)
	{
		is.read(&byte, 1);
		count ++;

		if( byte != '\r' && byte != '\n' ) buf[index++] = byte;

		if((byte == '\n' || count == length ) && index > 1 )	{
			buf[index] = (BYTE) 0;
			t_index = 0;

			if( buf[t_index] == ';' || buf[t_index] == '/' )	{		// �ּ��� ���� ó��
				index = 0;
				continue;
			}

			t_index += ParseSpace( first, buf + t_index );

			if( !strcmp( first, "ROOM" ) )	{
				logic = 0; exec = 0;
				t_index += ParseSpace( temp, buf + t_index );	event_num = atoi( temp );

				if( m_arRoomEventArray.IsExist(event_num) )	{
					TRACE("Event Double !!\n" );
					goto cancel_event_load;
				}
				
				pEvent = NULL;
				pEvent = SetRoomEvent( event_num );
			}
			else if( !strcmp( first, "TYPE" ) )	{
				t_index += ParseSpace( temp, buf + t_index );	m_byRoomType = atoi( temp );
			}
			else if( !strcmp( first, "L" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}
			}
			else if( !strcmp( first, "E" ) )	{
				if (!pEvent
					|| exec >= MAX_CHECK_EVENT)
					goto cancel_event_load;

				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Exec[exec].sNumber = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Exec[exec].sOption_1 = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Exec[exec].sOption_2 = atoi( temp );
				exec++;
			}
			else if( !strcmp( first, "A" ) )	{
				if (!pEvent
					|| logic >= MAX_CHECK_EVENT)
					goto cancel_event_load;

				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Logic[logic].sNumber = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Logic[logic].sOption_1 = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_Logic[logic].sOption_2 = atoi( temp );
				logic++;
				pEvent->m_byCheck = logic;
			}
			else if( !strcmp( first, "O" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}
			}
			else if( !strcmp( first, "NATION" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}

				t_index += ParseSpace( temp, buf + t_index );	nation = atoi( temp );
				if( nation == KARUS_ZONE )	{
					m_sKarusRoom++;
				}
				else if( nation == ELMORAD_ZONE )	{
					m_sElmoradRoom++;
				}
			}
			else if( !strcmp( first, "POS" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}

				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iInitMinX = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iInitMinZ = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iInitMaxX = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iInitMaxZ = atoi( temp );
			}
			else if( !strcmp( first, "POSEND" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}

				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iEndMinX = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iEndMinZ = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iEndMaxX = atoi( temp );
				t_index += ParseSpace( temp, buf + t_index );	pEvent->m_iEndMaxZ = atoi( temp );
			}
			else if( !strcmp( first, "END" ) )	{
				if( !pEvent )	{
					goto cancel_event_load;
				}
			}

			index = 0;
		}
	}

	is.close();

	return TRUE;

cancel_event_load:
	printf("Unable to load AI EVT (%d.aievt), failed in or near event number %d.\n", 
		m_byRoomEvent, event_num);
	is.close();
	return FALSE;
}

int MAP::IsRoomCheck(float fx, float fz)
{
	// dungeion work
	// ������ ���� ���������� �Ǵ�, �ƴϸ� ����ó��
	
	CRoomEvent* pRoom = NULL;

	int nSize = m_arRoomEventArray.GetSize();
	int nX = (int)fx;
	int nZ = (int)fz;
	int minX=0, minZ=0, maxX=0, maxZ=0;
	int room_number = 0;

	BOOL bFlag_1 = FALSE, bFlag_2 = FALSE;

	for( int i = 1; i < nSize+1; i++)		{
		pRoom = m_arRoomEventArray.GetData( i );
		if( !pRoom ) continue;
		if( pRoom->m_byStatus == 3 )	continue;	// ���� �������̰ų� ��(clear) ���¶�� �˻����� ����

		bFlag_1 = FALSE; bFlag_2 = FALSE;

		if( pRoom->m_byStatus == 1 )	{			// ���� �ʱ�ȭ ����
			minX = pRoom->m_iInitMinX;		minZ = pRoom->m_iInitMinZ;
			maxX = pRoom->m_iInitMaxX;		maxZ = pRoom->m_iInitMaxZ;
		}
		else if( pRoom->m_byStatus == 2 )	{		// �������� ����
			if( pRoom->m_Logic[0].sNumber != 4)	continue;	// ��ǥ�������� �̵��ϴ°� �ƴ϶��,,
			minX = pRoom->m_iEndMinX;		minZ = pRoom->m_iEndMinZ;
			maxX = pRoom->m_iEndMaxX;		maxZ = pRoom->m_iEndMaxZ;
		}
	
		if( minX < maxX )	{
			if( COMPARE(nX, minX, maxX) )		bFlag_1 = TRUE;
		}
		else	{
			if( COMPARE(nX, maxX, minX) )		bFlag_1 = TRUE;
		}

		if( minZ < maxZ )	{
			if( COMPARE(nZ, minZ, maxZ) )		bFlag_2 = TRUE;
		}
		else	{
			if( COMPARE(nZ, maxZ, minZ) )		bFlag_2 = TRUE;
		}

		if( bFlag_1 == TRUE && bFlag_2 == TRUE )	{
			if( pRoom->m_byStatus == 1 )	{			// ���� �ʱ�ȭ ����
				pRoom->m_byStatus = 2;	// ������ ���·� ����� ��ȯ
				pRoom->m_tDelayTime = UNIXTIME;
				room_number = i;
				TRACE(" Room Check - number = %d, x=%d, z=%d\n", i, nX, nZ);
				//wsprintf(notify, "** �˸� : [%d Zone][%d] �濡 �����Ű��� ȯ���մϴ� **", m_nZoneNumber, pRoom->m_sRoomNumber);
				//g_pMain->SendSystemMsg( notify, PUBLIC_CHAT, SEND_ALL);
			}
			else if( pRoom->m_byStatus == 2 )	{		// �������� ����
				pRoom->m_byStatus = 3;					// Ŭ���� ���·�
				//wsprintf(notify, "** �˸� : [%d Zone][%d] ��ǥ�������� �����ؼ� Ŭ���� �˴ϴ٤� **", m_nZoneNumber, pRoom->m_sRoomNumber);
				//g_pMain->SendSystemMsg( notify, PUBLIC_CHAT, SEND_ALL);
			}

			return room_number;	
		}
	}

	return room_number;
}

CRoomEvent* MAP::SetRoomEvent( int number )
{
	CRoomEvent* pEvent = m_arRoomEventArray.GetData( number );
	if( pEvent )	{
		TRACE("#### SetRoom Error : double event number = %d ####\n", number);
		return NULL;
	}

	pEvent = new CRoomEvent();
	pEvent->m_iZoneNumber = m_nZoneNumber;
	pEvent->m_sRoomNumber = number;
	if( !m_arRoomEventArray.PutData( pEvent->m_sRoomNumber, pEvent) ) {
		delete pEvent;
		pEvent = NULL;
		return NULL;
	}

	return pEvent;
}

BOOL MAP::IsRoomStatusCheck()
{
	int nClearRoom = 1;
	int nTotalRoom = m_arRoomEventArray.GetSize() + 1;

	if( m_byRoomStatus == 2 )	{	// ���� �ʱ�ȭ��
		m_byInitRoomCount++;
	}

	foreach_stlmap (itr, m_arRoomEventArray)
	{
		CRoomEvent *pRoom = itr->second;
		if (pRoom == NULL)
		{
			TRACE("#### IsRoomStatusCheck Error : room empty number = %d ####\n", itr->first);
			continue;
		}

		if( m_byRoomStatus == 1)	{	// �� ������
			if( pRoom->m_byStatus == 3 )	nClearRoom += 1;
			if( m_byRoomType == 0 )	{
				if( nTotalRoom == nClearRoom )	{		// ���� �� Ŭ���� �Ǿ��.. �ʱ�ȭ ���࿩,,
					m_byRoomStatus = 2;
					TRACE("���� �� Ŭ���� �Ǿ��.. �ʱ�ȭ ���࿩,, zone=%d, type=%d, status=%d\n", m_nZoneNumber, m_byRoomType, m_byRoomStatus);
					return TRUE;
				}
			}
		}
		else if( m_byRoomStatus == 2)	{	// ���� �ʱ�ȭ��
			if( m_byInitRoomCount >= 10 ) {
				pRoom->InitializeRoom();		// ���� ���� �ʱ�ȭ
				nClearRoom += 1;
				if( nTotalRoom == nClearRoom )	{		// ���� �ʱ�ȭ �Ǿ��.. 
					m_byRoomStatus = 3;
					TRACE("���� �ʱ�ȭ �Ǿ��..  status=%d\n", m_byRoomStatus);
					return TRUE;
				}
			}
		}
		else if( m_byRoomStatus == 3)	{	// �� �ʱ�ȭ �Ϸ�
			m_byRoomStatus = 1;
			m_byInitRoomCount = 0;
			TRACE("���� �ٽ� ���۵Ǿ�����..  status=%d\n", m_byRoomStatus);
			return TRUE;
		}
	}
	return FALSE;
}

void MAP::InitializeRoom()
{
	foreach_stlmap (itr, m_arRoomEventArray)
	{
		CRoomEvent *pRoom = itr->second;
		if (pRoom == NULL)
		{
			TRACE("#### InitializeRoom Error : room empty number = %d ####\n", itr->first);
			continue;
		}

		pRoom->InitializeRoom();		// ���� ���� �ʱ�ȭ
		m_byRoomStatus = 1;
		m_byInitRoomCount = 0;
	}
}