#pragma once

enum ResistanceTypes
{
	NONE_R		= 0,
	FIRE_R		= 1,
	COLD_R		= 2,
	LIGHTNING_R	= 3,
	MAGIC_R		= 4,
	DISEASE_R	= 5,
	POISON_R	= 6,
	LIGHT_R		= 7,
	DARKNESS_R	= 8
};

enum MagicDamageType
{
	FIRE_DAMAGE			= 5,
	ICE_DAMAGE			= 6,
	LIGHTNING_DAMAGE	= 7
};

enum SkillMoral
{
	MORAL_SELF				= 1,
	MORAL_FRIEND_WITHME		= 2,
	MORAL_FRIEND_EXCEPTME	= 3,
	MORAL_PARTY				= 4,
	MORAL_NPC				= 5,
	MORAL_PARTY_ALL			= 6,
	MORAL_ENEMY				= 7,
	MORAL_ALL				= 8,
	MORAL_AREA_ENEMY		= 10,
	MORAL_AREA_FRIEND		= 11,
	MORAL_AREA_ALL			= 12,
	MORAL_SELF_AREA			= 13,
	MORAL_CLAN				= 14,
	MORAL_CLAN_ALL			= 15,
	MORAL_UNDEAD			= 16, // I don't think a lot of these made it to KO.
	MORAL_PET_WITHME		= 17,
	MORAL_PET_ENEMY			= 18,
	MORAL_ANIMAL1			= 19,
	MORAL_ANIMAL2			= 20,
	MORAL_ANIMAL3			= 21,
	MORAL_ANGEL				= 22,
	MORAL_DRAGON			= 23,
	MORAL_CORPSE_FRIEND		= 25,
	MORAL_CORPSE_ENEMY		= 26
};

#define WARP_RESURRECTION		1		// To the resurrection point.

#define REMOVE_TYPE3			1
#define REMOVE_TYPE4			2
#define RESURRECTION			3
#define	RESURRECTION_SELF		4
#define REMOVE_BLESS			5

class Unit;
struct _MAGIC_TABLE;
class MagicInstance
{
public:
	uint8	bOpcode;
	uint32	nSkillID;
	_MAGIC_TABLE * pSkill;
	int16	sCasterID, sTargetID; 
	Unit	*pSkillCaster, *pSkillTarget;
	uint16	sData1, sData2, sData3, sData4, 
			sData5, sData6, sData7, sData8;
	bool	bIsRecastingSavedMagic;

	void Run();

	bool IsAvailable();
	bool UserCanCast();

	bool ExecuteSkill(uint8 bType);
	bool ExecuteType1();	
	bool ExecuteType2();
	bool ExecuteType3();
	bool ExecuteType4();
	bool ExecuteType5();
	bool ExecuteType6();
	bool ExecuteType7();
	bool ExecuteType8();
	bool ExecuteType9();

	void Type3Cancel();
	void Type4Cancel();
	void Type6Cancel();
	void Type9Cancel();
	void Type4Extend();

	short GetMagicDamage(Unit *pTarget, int total_hit, int attribute);
	short GetWeatherDamage(short damage, int attribute);
	void ReflectDamage(int32 damage);
	bool GrantType4Buff(_MAGIC_TYPE4 *pType, CUser *pTarget, bool bGiveBuff = true);

	void SendSkillToAI();
	void SendSkill(int16 pSkillCaster = -1, int16 pSkillTarget = -1, 
					int8 opcode = -1, uint32 nSkillID = 0, 
					int16 sData1 = -999, int16 sData2 = -999, int16 sData3 = -999, int16 sData4 = -999, 
					int16 sData5 = -999, int16 sData6 = -999, int16 sData7 = -999, int16 sData8 = -999);
	void SendSkillFailed();
	void SendTransformationList();
};