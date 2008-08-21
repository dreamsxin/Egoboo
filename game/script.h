#pragma once

//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

///
/// @file
/// @brief Egoboo Scripting Definitions

#include "egoboo_types.h"

struct sGame;

// These are for the AI script loading/parsing routines
extern int                     iNumAis;

#define PITNOSOUND          -256                     ///< Stop sound at bottom of pits...
#define MSGDISTANCE         2000                  ///< Range for SendMessageNear

#define AILST_COUNT         129                     //
#define MAXCODE             1024                     ///< Number of lines in AICODES.TXT
#define MAXCODENAMESIZE     64                      //
#define AISMAXCOMPILESIZE   (AILST_COUNT*MAXCODE)         // For parsing AI scripts

extern bool_t parseerror;    //Do we have an script error?



enum e_script_opcode
{
  F_IfSpawned = 0,                      ///< Scripted AI functions (v0.10)
  F_IfTimeOut,
  F_IfAtWaypoint,
  F_IfAtLastWaypoint,
  F_IfAttacked,
  F_IfBumped,
  F_IfSignaled,
  F_IfCalledForHelp,
  F_SetContent,
  F_IfKilled,
  F_IfTargetKilled,
  F_ClearWaypoints,
  F_AddWaypoint,
  F_FindPath,
  F_Compass,
  F_GetTargetArmorPrice,
  F_SetTime,
  F_GetContent,
  F_JoinTargetTeam,
  F_SetTargetToNearbyEnemy,
  F_SetTargetToTargetLeftHand,
  F_SetTargetToTargetRightHand,
  F_SetTargetToWhoeverAttacked,
  F_SetTargetToWhoeverBumped,
  F_SetTargetToWhoeverCalledForHelp,
  F_SetTargetToOldTarget,
  F_SetTurnModeToVelocity,
  F_SetTurnModeToWatch,
  F_SetTurnModeToSpin,
  F_SetBumpHeight,
  F_IfTargetHasID,
  F_IfTargetHasItemID,
  F_IfTargetHoldingItemID,
  F_IfTargetHasSkillID,
  F_Else,
  F_Run,
  F_Walk,
  F_Sneak,
  F_DoAction,
  F_KeepAction,
  F_SignalTeam,
  F_DropWeapons,
  F_TargetDoAction,
  F_OpenPassage,
  F_ClosePassage,
  F_IfPassageOpen,
  F_GoPoof,
  F_CostTargetItemID,
  F_DoActionOverride,
  F_IfHealed,
  F_DisplayMessage,
  F_CallForHelp,
  F_AddIDSZ,
  F_End,
  F_SetState,                          ///< Scripted AI functions (v0.20)
  F_GetState,
  F_IfStateIs,
  F_IfTargetCanOpenStuff,              ///< Scripted AI functions (v0.30)
  F_IfGrabbed,
  F_IfDropped,
  F_SetTargetToWhoeverIsHolding,
  F_DamageTarget,
  F_IfXIsLessThanY,
  F_SetWeatherTime,                    ///< Scripted AI functions (v0.40)
  F_GetBumpHeight,
  F_IfReaffirmed,
  F_UnkeepAction,
  F_IfTargetIsOnOtherTeam,
  F_IfTargetIsOnHatedTeam,          ///< Scripted AI functions (v0.50)
  F_PressLatchButton,
  F_SetTargetToTargetOfLeader,
  F_IfLeaderKilled,
  F_BecomeLeader,
  F_ChangeTargetArmor,                ///< Scripted AI functions (v0.60)
  F_GiveMoneyToTarget,
  F_DropKeys,
  F_IfLeaderIsAlive,
  F_IfTargetIsOldTarget,
  F_SetTargetToLeader,
  F_SpawnCharacter,
  F_RespawnCharacter,
  F_ChangeTile,
  F_IfUsed,
  F_DropMoney,
  F_SetOldTarget,
  F_DetachFromHolder,
  F_IfTargetHasVulnerabilityID,
  F_CleanUp,
  F_IfCleanedUp,
  F_IfSitting,
  F_IfTargetIsHurt,
  F_IfTargetIsAPlayer,
  F_PlaySound,
  F_SpawnParticle,
  F_IfTargetIsAlive,
  F_Stop,
  F_DisaffirmCharacter,
  F_ReaffirmCharacter,
  F_IfTargetIsSelf,
  F_IfTargetIsMale,
  F_IfTargetIsFemale,
  F_SetTargetToSelf,            ///< Scripted AI functions (v0.70)
  F_SetTargetToRider,
  F_GetAttackTurn,
  F_GetDamageType,
  F_BecomeSpell,
  F_BecomeSpellbook,
  F_IfScoredAHit,
  F_IfDisaffirmed,
  F_DecodeOrder,
  F_SetTargetToWhoeverWasHit,
  F_SetTargetToWideEnemy,
  F_IfChanged,
  F_IfInWater,
  F_IfBored,
  F_IfTooMuchBaggage,
  F_IfGrogged,
  F_IfDazed,
  F_IfTargetHasSpecialID,
  F_PressTargetLatchButton,
  F_IfInvisible,
  F_IfArmorIs,
  F_GetTargetGrogTime,
  F_GetTargetDazeTime,
  F_SetDamageType,
  F_SetWaterLevel,
  F_EnchantTarget,
  F_EnchantChild,
  F_TeleportTarget,
  F_GiveExperienceToTarget,
  F_IncreaseAmmo,
  F_UnkurseTarget,
  F_GiveExperienceToTargetTeam,
  F_IfUnarmed,
  F_RestockTargetAmmoIDAll,
  F_RestockTargetAmmoIDFirst,
  F_FlashTarget,
  F_SetRedShift,
  F_SetGreenShift,
  F_SetBlueShift,
  F_SetLight,
  F_SetAlpha,
  F_IfHitFromBehind,
  F_IfHitFromFront,
  F_IfHitFromLeft,
  F_IfHitFromRight,
  F_IfTargetIsOnSameTeam,
  F_KillTarget,
  F_UndoEnchant,
  F_GetWaterLevel,
  F_CostTargetMana,
  F_IfTargetHasAnyID,
  F_SetBumpSize,
  F_IfNotDropped,
  F_IfYIsLessThanX,
  F_SetFlyHeight,
  F_IfBlocked,
  F_IfTargetIsDefending,
  F_IfTargetIsAttacking,
  F_IfStateIs0,
  F_IfStateIs1,
  F_IfStateIs2,
  F_IfStateIs3,
  F_IfStateIs4,
  F_IfStateIs5,
  F_IfStateIs6,
  F_IfStateIs7,
  F_IfContentIs,
  F_SetTurnModeToWatchTarget,
  F_IfStateIsNot,
  F_IfXIsEqualToY,
  F_DisplayDebugMessage,
  F_BlackTarget,                        ///< Scripted AI functions (v0.80)
  F_DisplayMessageNear,
  F_IfHitGround,
  F_IfNameIsKnown,
  F_IfUsageIsKnown,
  F_IfHoldingItemID,
  F_IfHoldingRangedWeapon,
  F_IfHoldingMeleeWeapon,
  F_IfHoldingShield,
  F_IfKursed,
  F_IfTargetIsKursed,
  F_IfTargetIsDressedUp,
  F_IfOverWater,
  F_IfThrown,
  F_MakeNameKnown,
  F_MakeUsageKnown,
  F_StopTargetMovement,
  F_SetXY,
  F_GetXY,
  F_AddXY,
  F_MakeAmmoKnown,
  F_SpawnAttachedParticle,
  F_SpawnExactParticle,
  F_AccelerateTarget,
  F_IfDistanceIsMoreThanTurn,
  F_IfCrushed,
  F_MakeCrushValid,
  F_SetTargetToLowestTarget,
  F_IfNotPutAway,
  F_IfTakenOut,
  F_IfAmmoOut,
  F_PlaySoundLooped,
  F_StopSoundLoop,
  F_HealSelf,
  F_Equip,
  F_IfTargetHasItemIDEquipped,
  F_SetOwnerToTarget,
  F_SetTargetToOwner,
  F_SetFrame,
  F_BreakPassage,
  F_SetReloadTime,
  F_SetTargetToWideBlahID,
  F_PoofTarget,
  F_ChildDoActionOverride,
  F_SpawnPoof,
  F_SetSpeedPercent,
  F_SetChildState,
  F_SpawnAttachedSizedParticle,
  F_ChangeArmor,
  F_ShowTimer,
  F_IfFacingTarget,
  F_PlaySoundVolume,
  F_SpawnAttachedFacedParticle,
  F_IfStateIsOdd,
  F_SetTargetToDistantEnemy,
  F_Teleport,
  F_GiveStrengthToTarget,
  F_GiveWisdomToTarget,
  F_GiveIntelligenceToTarget,
  F_GiveDexterityToTarget,
  F_GiveLifeToTarget,
  F_GiveManaToTarget,
  F_ShowMap,
  F_ShowYouAreHere,
  F_ShowBlipXY,
  F_HealTarget,
  F_PumpTarget,
  F_CostAmmo,
  F_MakeSimilarNamesKnown,
  F_SpawnAttachedHolderParticle,
  F_SetTargetReloadTime,
  F_SetFogLevel,
  F_GetFogLevel,
  F_SetFogTAD,
  F_SetFogBottomLevel,
  F_GetFogBottomLevel,
  F_CorrectActionForHand,
  F_IfTargetIsMounted,
  F_SparkleIcon,
  F_UnsparkleIcon,
  F_GetTileXY,
  F_SetTileXY,
  F_SetShadowSize,
  F_SignalTarget,
  F_SetTargetToWhoeverIsInPassage,
  F_IfCharacterWasABook,
  F_SetEnchantBoostValues,               ///< Scripted AI functions (v0.90)
  F_SpawnCharacterXYZ,
  F_SpawnExactCharacterXYZ,
  F_ChangeTargetClass,
  F_PlayFullSound,
  F_SpawnExactChaseParticle,
  F_EncodeOrder,
  F_SignalSpecialID,
  F_UnkurseTargetInventory,
  F_IfTargetIsSneaking,
  F_DropItems,
  F_RespawnTarget,
  F_TargetDoActionSetFrame,
  F_IfTargetCanSeeInvisible,
  F_SetTargetToNearestBlahID,
  F_SetTargetToNearestEnemy,
  F_SetTargetToNearestFriend,
  F_SetTargetToNearestLifeform,
  F_FlashPassage,
  F_FindTileInPassage,
  F_IfHeldInLeftSaddle,
  F_NotAnItem,
  F_SetChildAmmo,
  F_IfHitVulnerable,
  F_IfTargetIsFlying,
  F_IdentifyTarget,
  F_BeatModule,
  F_EndModule,
  F_DisableExport,
  F_EnableExport,
  F_GetTargetState,
  F_SetSpeech,
  F_SetMoveSpeech,
  F_SetSecondMoveSpeech,
  F_SetAttackSpeech,
  F_SetAssistSpeech,
  F_SetTerrainSpeech,
  F_SetSelectSpeech,
  F_ClearEndText,
  F_AddEndText,
  F_PlayMusic,
  F_SetMusicPassage,
  F_MakeCrushInvalid,
  F_StopMusic,
  F_FlashVariable,
  F_AccelerateUp,
  F_FlashVariableHeight,
  F_SetDamageTime,
  F_IfStateIs8,
  F_IfStateIs9,
  F_IfStateIs10,
  F_IfStateIs11,
  F_IfStateIs12,
  F_IfStateIs13,
  F_IfStateIs14,
  F_IfStateIs15,
  F_IfTargetIsAMount,
  F_IfTargetIsAPlatform,
  F_AddStat,
  F_DisenchantTarget,
  F_DisenchantAll,
  F_SetVolumeNearestTeammate,
  F_AddShopPassage,
  F_TargetPayForArmor,
  F_JoinEvilTeam,
  F_JoinNullTeam,
  F_JoinGoodTeam,
  F_PitsKill,
  F_SetTargetToPassageID,
  F_MakeNameUnknown,
  F_SpawnExactParticleEndSpawn,
  F_SpawnPoofSpeedSpacingDamage,
  F_GiveExperienceToGoodTeam,
  F_DoNothing,                          ///< Scripted AI functions (v0.95)
  F_DazeTarget,
  F_GrogTarget,
  F_IfEquipped,
  F_DropTargetMoney,
  F_GetTargetContent,
  F_DropTargetKeys,
  F_JoinTeam,
  F_TargetJoinTeam,
  F_ClearMusicPassage,
  F_AddQuest,                           ///< Scripted AI functions (v1.00)
  F_BeatQuest,
  F_IfTargetHasQuest,
  F_SetQuestLevel,
  F_IfTargetHasNotFullMana,
  F_IfDoingAction,
  F_IfOperatorIsLinux,
  F_IfTargetIsOwner,                    ///< Scripted AI functions (v1.05)
  F_SetCameraSwing,
  F_EnableRespawn,
  F_DisableRespawn,
  F_IfButtonPressed,
  F_IfHolderScoredAHit					        ///< Scripted AI functions (v1.10)
};
typedef enum e_script_opcode OPCODE;

enum e_script_operation
{
  OP_ADD = 0,     ///< +
  OP_SUB,         ///< -
  OP_AND,         ///< &
  OP_SHR,         ///< >
  OP_SHL,         ///< <
  OP_MUL,         ///< *
  OP_DIV,         ///< /
  OP_MOD          ///< %
};
typedef enum e_script_operation OPERATION;

enum e_script_variable
{
  VAR_TMP_X = 0,
  VAR_TMP_Y,
  VAR_TMP_DISTANCE,
  VAR_TMP_TURN,
  VAR_TMP_ARGUMENT,
  VAR_RAND,
  VAR_SELF_X,
  VAR_SELF_Y,
  VAR_SELF_TURN,
  VAR_SELF_COUNTER,
  VAR_SELF_ORDER,
  VAR_SELF_MORALE,
  VAR_SELF_LIFE,
  VAR_TARGET_X,
  VAR_TARGET_Y,
  VAR_TARGET_DISTANCE,
  VAR_TARGET_TURN,
  VAR_LEADER_X,
  VAR_LEADER_Y,
  VAR_LEADER_DISTANCE,
  VAR_LEADER_TURN,
  VAR_GOTO_X,
  VAR_GOTO_Y,
  VAR_GOTO_DISTANCE,
  VAR_TARGET_TURNTO,
  VAR_PASSAGE,
  VAR_WEIGHT,
  VAR_SELF_ALTITUDE,
  VAR_SELF_ID,
  VAR_SELF_HATEID,
  VAR_SELF_MANA,
  VAR_TARGET_STR,
  VAR_TARGET_WIS,
  VAR_TARGET_INT,
  VAR_TARGET_DEX,
  VAR_TARGET_LIFE,
  VAR_TARGET_MANA,
  VAR_TARGET_LEVEL,
  VAR_TARGET_SPEEDX,
  VAR_TARGET_SPEEDY,
  VAR_TARGET_SPEEDZ,
  VAR_SELF_SPAWNX,
  VAR_SELF_SPAWNY,
  VAR_SELF_STATE,
  VAR_SELF_STR,
  VAR_SELF_WIS,
  VAR_SELF_INT,
  VAR_SELF_DEX,
  VAR_SELF_MANAFLOW,
  VAR_TARGET_MANAFLOW,
  VAR_SELF_ATTACHED,
  VAR_SWINGTURN,
  VAR_XYDISTANCE,
  VAR_SELF_Z,
  VAR_TARGET_ALTITUDE,
  VAR_TARGET_Z,
  VAR_SELF_INDEX,
  VAR_OWNER_X,
  VAR_OWNER_Y,
  VAR_OWNER_TURN,
  VAR_OWNER_DISTANCE,
  VAR_OWNER_TURNTO,
  VAR_XYTURNTO,
  VAR_SELF_MONEY,
  VAR_SELF_ACCEL,
  VAR_TARGET_EXP,
  VAR_SELF_AMMO,
  VAR_TARGET_AMMO,
  VAR_TARGET_MONEY,
  VAR_TARGET_TURNAWAY,
  VAR_SELF_LEVEL,
  VAR_SPAWN_DISTANCE,
  VAR_TARGET_MAX_LIFE,
  VAR_SELF_CONTENT
};
typedef enum e_script_variable VARIABLE;

struct sScriptInfo
{
  int    buffer_index;
  Uint32 buffer[AISMAXCOMPILESIZE];

  STRING fname[AILST_COUNT];
  int    offset_count;
  int    offset_stt[AILST_COUNT];
  int    offset_end[AILST_COUNT];
};
typedef struct sScriptInfo ScriptInfo_t;


retval_t run_script( struct sGame * gs, CHR_REF character, float dUpdate );
void run_all_scripts( struct sGame * gs, float dUpdate );

void append_end_text( struct sGame * gs, int message, CHR_REF character );


void load_ai_codes( char* loadname );
Uint32 load_ai_script( ScriptInfo_t * slist, EGO_CONST char * szModpath, EGO_CONST char * szObjectname );
void reset_ai_script(struct sGame * gs);
