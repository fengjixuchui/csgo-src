//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "cbase.h"
#include "hud.h"
#include "clientmode_csnormal.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "buymenu.h"
#include "cliententitylist.h"
#include "filesystem.h"
#include "vgui/IVGui.h"
#include "hud_basechat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#if !defined( CSTRIKE15 )
#include "cstrikeclassmenu.h"
#else
#include "cs_gamerules.h"
#include "classmenu.h"
#include "c_cs_playerresource.h"
#include "smokegrenade_projectile.h"
#endif //!CSTRIKE15
#include "c_props.h"
#include "c_baseplayer.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include "c_soundscape.h"
#include <keyvalues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"
#include "vguicenterprint.h"
#include "physpropclientside.h"
#include "c_weapon__stubs.h"
#include <engine/IEngineSound.h>
#include "c_cs_hostage.h"
#include "bitbuf.h"
#include "usermessages.h"
#include "prediction.h"
#include "datacache/imdlcache.h"
#include "iachievementmgr.h"
#include "achievementmgr.h"
#include "cs_achievementdefs.h"
#include "achievements_cs.h"
#include "hud_macros.h"
#include "c_plantedc4.h"
#include "tier1/fmtstr.h"
#include "history_resource.h"
#include "cs_client_gamestats.h"
#include "viewpostprocess.h"
#include "../../engine/keys.h"
#include "inputsystem/iinputsystem.h"
#include "matchmaking/mm_helpers.h"
#if defined( INCLUDE_SCALEFORM )
#include "Scaleform/messagebox_scaleform.h"
#include "Scaleform/HUD/sfhud_uniquealerts.h"
#include "Scaleform/HUD/sfhud_rosettaselector.h"
#endif
#include "GameStats.h"
#if defined ( _GAMECONSOLE )
#include "GameUI/IGameUI.h"
#include "GameUI/gameui_interface.h"
#endif
#include "platforminputdevice.h"
#include "glow_outline_effect.h"
#include "hltvcamera.h"
#include "basecsgrenade_projectile.h"
#include "hud_chat.h"
#include "hltvreplaysystem.h"
#include "netmessages.h"
#include "playerdecals_signature.h"

#if defined ( _X360 )
#include "ixboxsystem.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

bool __MsgFunc_MatchEndConditions( const CCSUsrMsg_MatchEndConditions &msg );
bool __MsgFunc_DisconnectToLobby( const CCSUsrMsg_DisconnectToLobby &msg );
bool __MsgFunc_WarmupHasEnded( const CCSUsrMsg_WarmupHasEnded &msg );
bool __MsgFunc_ServerRankUpdate( const CCSUsrMsg_ServerRankUpdate &msg );
bool __MsgFunc_ServerRankRevealAll( const CCSUsrMsg_ServerRankRevealAll &msg );
bool __MsgFunc_ScoreLeaderboardData( const CCSUsrMsg_ScoreLeaderboardData &msg );
bool __MsgFunc_GlowPropTurnOff( const CCSUsrMsg_GlowPropTurnOff &msg );
bool __MsgFunc_XpUpdate( const CCSUsrMsg_XpUpdate &msg );
bool __MsgFunc_QuestProgress( const CCSUsrMsg_QuestProgress &msg );
bool __MsgFunc_PlayerDecalDigitalSignature( const CCSUsrMsg_PlayerDecalDigitalSignature &msg );
//
// WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!!
//

class CHudHintDisplay;
class CHudChat;

//-----------------------------------------------------------------------------
ConVar cl_spec_mode(
	"cl_spec_mode",
	"0",
	FCVAR_USERINFO | FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS | FCVAR_SERVER_CAN_EXECUTE,
	"Saves the last viewed spectator mode for use next time we start to spectate" );

ConVar cl_draw_only_deathnotices( "cl_draw_only_deathnotices", "0", FCVAR_CHEAT, "For drawing only the crosshair and death notices (used for moviemaking)" );
ConVar cl_radar_square_with_scoreboard( "cl_radar_square_with_scoreboard", "1", FCVAR_ARCHIVE | FCVAR_RELEASE, "If set, the radar will toggle to square when the scoreboard is visible." );

ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );

static IClientMode *g_pClientMode[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientMode()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_pClientMode[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}


// This is a temporary entity used to render the player's model while drawing the class selection menu.
CHandle<C_BaseAnimatingOverlay> g_ClassImagePlayer;	// player
CHandle<C_BaseAnimating> g_ClassImageWeapon;	// weapon

STUB_WEAPON_CLASS( cycler_weapon,	WeaponCycler,	C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap,	WeaponCubemap,	C_BaseCombatWeapon );

//-----------------------------------------------------------------------------
// HACK: the detail sway convars are archive, and default to 0.  Existing CS:S players thus have no detail
// prop sway.  We'll force them to DoD's default values for now.  What we really need in the long run is
// a system to apply changes to archived convars' defaults to existing players.
extern ConVar cl_detail_max_sway;
extern ConVar cl_detail_avoid_radius;
extern ConVar cl_detail_avoid_force;
extern ConVar cl_detail_avoid_recover_speed;


extern CAchievementMgr g_AchievementMgrCS;


// Player sprays data
struct PlayerSprayClientRequestData_t
{
	uint32 m_nEquipSlot;
	uint32 m_nDefIdx;
	uint32 m_unTintID;
	float m_flCreationTime;
};
static uint64 s_flLastDigitalSignatureTime = 0;
static CUtlMap< int, PlayerSprayClientRequestData_t, int, CDefLess< int > > s_mapClientDigitalSignatureRequests;


//--------------------------------------------------------------------------------------------------------
CON_COMMAND_F( cl_reloadpostprocessparams, "", FCVAR_CHEAT )
{
	// get optional filename
	if ( args.ArgC() == 2 )
	{
		GetClientModeCSNormal()->LoadPostProcessParamsFromFile( args[1] );
	}
	else
	{
		GetClientModeCSNormal()->LoadPostProcessParamsFromFile();
	}
}
CON_COMMAND_F( cl_sos_test_set_opvar, "", FCVAR_CHEAT )
{
	// get optional filename
	if ( args.ArgC() > 2 )
	{
		engine->SOSSetOpvarFloat( args[1], Q_atof( args[2] ));
	}
}
CON_COMMAND_F( cl_sos_test_get_opvar, "", FCVAR_CHEAT )
{
	// get optional filename
	if ( args.ArgC() > 2 )
	{
		float flResult;
		engine->SOSGetOpvarFloat( args[1], flResult );
		DevMsg( "SOS: GetOpvar %s, %f\n", args[1], flResult );
	}
}


#if defined( _PS3 )

CON_COMMAND( cl_write_ps3_bindings, "Used internally for scaleform to tell us to write out our bindings to the title data." )
{
	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage:  cl_write_ps3_bindings <controller index> <device ID>\n" );
		return;
	}

	int iController = atoi( args[1] );
	int iDeviceID = atoi( args[2] );
	GetClientModeCSNormal()->SyncCurrentKeyBindingsToDeviceTitleData( iController, iDeviceID, KEYBINDING_WRITE_TO_TITLEDATA );
}

CON_COMMAND( cl_read_ps3_bindings, "Used internally for scaleform to tell us to read in our bindings from the title data." )
{
	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage:  cl_read_ps3_bindings <controller index> <device ID>\n" );
		return;
	}

	int iController = atoi( args[1] );
	int iDeviceID = atoi( args[2] );
	GetClientModeCSNormal()->SyncCurrentKeyBindingsToDeviceTitleData( iController, iDeviceID, KEYBINDING_READ_FROM_TITLEDATA );
}

CON_COMMAND( cl_reset_ps3_bindings, "Used internally for scaleform to tell us to reset our bindings to their defaults and save them." )
{
	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage:  cl_reset_ps3_bindings <controller index> <device ID(s) ORed together or -1 for all devices>\n" );
		return;
	}

	int iController = atoi( args[1] );
	int iDevicesToReset = atoi( args[2] );

	if ( -1 == iDevicesToReset )
	{
		iDevicesToReset = (int) PlatformInputDevice::GetValidInputDevicesForPlatform();
	}

	int numDevices = PlatformInputDevice::GetInputDeviceCountforPlatform();

	for ( int ii=1; ii<=numDevices; ++ii )
	{
		InputDevice_t eDevice = PlatformInputDevice::GetInputDeviceTypefromPlatformOrdinal( ii );

		// See if we're supposed to reset this particular device.
		if ( ( iDevicesToReset & eDevice ) == 0 ) continue;

		char *cmdBuffer = NULL;
		switch ( eDevice )
		{
			case INPUT_DEVICE_GAMEPAD:
				cmdBuffer = "exec controller_bindings" PLATFORM_EXT ".cfg game\n";
				break;
			case INPUT_DEVICE_PLAYSTATION_MOVE:
				cmdBuffer = "exec controller_move_bindings" PLATFORM_EXT ".cfg game\n";
				break;
			case INPUT_DEVICE_SHARPSHOOTER:
				cmdBuffer = "exec controller_sharp_shooter_bindings" PLATFORM_EXT ".cfg game\n";
				break;
		}

		if ( NULL != cmdBuffer )
		{
			// Save out the settings for each device.
			engine->ExecuteClientCmd( cmdBuffer );
			// Need to use another command to write the settings because these commands are deferred.
			engine->ExecuteClientCmd( VarArgs( "cl_write_ps3_bindings %d %d", iController, (int)eDevice ) );
		}

	}
	
#if defined( _PS3 )
	
	// We need to restore our settings based on our active device since we may have loaded other settings by entering this function.
	InputDevice_t currentDevice = g_pInputSystem->GetCurrentInputDevice();
	if( currentDevice != INPUT_DEVICE_NONE  )
	{
		// Load the bindings for the specific device.
		engine->ExecuteClientCmd( VarArgs( "cl_read_ps3_bindings %d %d", GET_ACTIVE_SPLITSCREEN_SLOT(), (int)currentDevice ) );
	}

#endif // _PS3

}

#endif // _PS3

CON_COMMAND_F( toggleRdrOpt, "", FCVAR_DEVELOPMENTONLY )
{
	static bool s_fastMode = false;
	s_fastMode = !s_fastMode;
	if ( s_fastMode )
	{
		engine->ExecuteClientCmd( "r_csm_fast_path 1" );
		engine->ExecuteClientCmd( "r_renderoverlaybatch 1" );
		engine->ExecuteClientCmd( "cl_radar_fast_transforms 1" );
#if defined( _WIN32 ) || defined( LINUX )
		engine->ExecuteClientCmd( "mat_vtxlit_new_path 1" );
		engine->ExecuteClientCmd( "mat_depthwrite_new_path 1" );
		engine->ExecuteClientCmd( "mat_lmap_new_path 1" );
		engine->ExecuteClientCmd( "mat_unlit_new_path 1" );
#endif
		engine->ExecuteClientCmd( "r_2PassBuildDraw 1");
		engine->ExecuteClientCmd( "r_threaded_buildWRlist 1");
	}
	else
	{
		engine->ExecuteClientCmd( "r_csm_fast_path 0" );
		engine->ExecuteClientCmd( "r_renderoverlaybatch 0" );
		engine->ExecuteClientCmd( "cl_radar_fast_transforms 0" );
#if defined( _WIN32 ) || defined( LINUX )
		engine->ExecuteClientCmd( "mat_vtxlit_new_path 0" );
		engine->ExecuteClientCmd( "mat_depthwrite_new_path 0" );
		engine->ExecuteClientCmd( "mat_lmap_new_path 0" );
		engine->ExecuteClientCmd( "mat_unlit_new_path 0" );
#endif
		engine->ExecuteClientCmd( "r_2PassBuildDraw 0");
		engine->ExecuteClientCmd( "r_threaded_buildWRlist 0");
	}
	Msg( "Rdr opt = %s\n", s_fastMode ? "TRUE":"FALSE" );
}


//--------------------------------------------------------------------------------------------------------
const char* ClientModeCSNormal::ms_postProcessEffectNames[NUM_POST_EFFECTS] = 
{
	"default",
	"low_health",
	"very_low_health",
	"in_buy_menu",
	"death_cam",
	"spectating",
	"in_fire",
	"zoomed_rifle",
	"zoomed_sniper",
	"zoomed_sniper_moving",
	"under_water",
	"round_end_via_bombing",
	"spec_camera_lerping",
	"map_control_unused",
	"death_cam_bodyshot",
	"death_cam_headshot"
};

//--------------------------------------------------------------------------------------------------------
PostProcessParameters_t ClientModeCSNormal::ms_postProcessParams[NUM_POST_EFFECTS] =
{
	//{ 0.5f, 0.0f, 0.8f, 1.1f, 0.0f, 0.0f },	// default
	//{ 2.5f, 0.0f, 0.8f, 1.1f, 0.0f, 0.0f },	// low_health
	//{ 1.8f, -1.0f, 0.6f, 0.95f, 0.0f, 0.0f },	// in_buy_menu
	//{ -0.4f, 0.0f, 0.8f, 1.1f, 0.0f, 0.0f },	// death_cam
	//{ -0.4f, 0.0f, 0.8f, 1.1f, 0.0f, 0.0f },	// spectating
	//{ 1.25f,-.65f, 0.4f, .85f, 0.0f, 0.0f }	// in fire
};

//-----------------------------------------------------------------------------
ConVar cl_autobuy(
	"cl_autobuy",
	"",
	FCVAR_RELEASE,
	"The order in which autobuy will attempt to purchase items" );

//-----------------------------------------------------------------------------
ConVar cl_rebuy(
	"cl_rebuy",
	"",
	FCVAR_RELEASE,
	"The order in which rebuy will attempt to repurchase items" );

//-----------------------------------------------------------------------------
static void SetBuyData( ConVar &buyVar, const char *filename )
{
	// if we already have autobuy data, don't bother re-parsing the text file
	if ( *buyVar.GetString() )
		return;

	// read in the auto buy string file and construct the string we will send to the mp.dll.
	const char *pfile = (char*)UTIL_LoadFileForMe( filename, NULL );
	if (pfile == NULL)
	{
		return;
	}

	char token[256] = {};
	char buystring[256] = {};

	pfile = engine->ParseFile( pfile, token, sizeof(token) );

	bool first = true;

	while (pfile != NULL)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			Q_strncat(buystring, " ", sizeof(buystring), COPY_ALL_CHARACTERS);
		}

		Q_strncat(buystring, token, sizeof(buystring), COPY_ALL_CHARACTERS);

		pfile = engine->ParseFile( pfile, token, sizeof(token) );
	}

	UTIL_FreeFile((byte *)pfile);

	buyVar.SetValue( buystring );
}

bool MsgFunc_KillCam( const CCSUsrMsg_KillCam &msg ) 
{
	C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );

	if ( !pPlayer )
		return true;

	int newMode = msg.obs_mode();

	if ( newMode != g_nKillCamMode )
	{
#if !defined( NO_ENTITY_PREDICTION )
		if ( g_nKillCamMode == OBS_MODE_NONE )
		{
			// kill cam is switch on, turn off prediction
			g_bForceCLPredictOff = true;
		}
		else if ( newMode == OBS_MODE_NONE )
		{
			// kill cam is switched off, restore old prediction setting is we switch back to normal mode
			g_bForceCLPredictOff = false;
		}
#endif
		g_nKillCamMode = newMode;
	}

	g_nKillCamTarget1	= msg.first_target();
	g_nKillCamTarget2	= msg.second_target();

	return true;
}

bool MsgFunc_DisplayInventory( const CCSUsrMsg_DisplayInventory &msg )
{
	C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayer )
		return true;

	bool showPistol = msg.display();
	int nID = msg.user_id();
	if ( pPlayer->GetUserID() == nID )
	{
		pPlayer->DisplayInventory(showPistol);
		return true;	
	}

	return false;
}

// --------------------------------------------------------------------------------- //
// CCSModeManager.
// --------------------------------------------------------------------------------- //

class CCSModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CCSModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;

// --------------------------------------------------------------------------------- //
// CCSModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CCSModeManager::Init()
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ] = GetClientModeNormal();
	}

	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

void CCSModeManager::LevelInit( const char *newmap )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelInit( newmap );
	}

	SetBuyData( cl_autobuy, "autobuy.txt" );
	SetBuyData( cl_rebuy, "rebuy.txt" );

	GetClientModeCSNormal()->SetupStaticCameras();

#if !defined( NO_ENTITY_PREDICTION )
	if ( g_nKillCamMode > OBS_MODE_NONE )
	{
		g_bForceCLPredictOff = false;
	}
#endif

	g_nKillCamMode		= OBS_MODE_NONE;
	g_nKillCamTarget1	= 0;
	g_nKillCamTarget2	= 0;

	// HACK: the detail sway convars are archive, and default to 0.  Existing CS:S players thus have no detail
	// prop sway.  We'll force them to DoD's default values for now.
	if ( !cl_detail_max_sway.GetFloat() &&
		!cl_detail_avoid_radius.GetFloat() &&
		!cl_detail_avoid_force.GetFloat() &&
		!cl_detail_avoid_recover_speed.GetFloat() )
	{
		cl_detail_max_sway.SetValue( "5" );
		cl_detail_avoid_radius.SetValue( "64" );
		cl_detail_avoid_force.SetValue( "0.4" );
		cl_detail_avoid_recover_speed.SetValue( "0.25" );
	}

	// Record a level transition counter
	++ ClientModeCSNormal::s_numLevelTransitions;
}

void CCSModeManager::LevelShutdown( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelShutdown();
	}
}

static void EnableSteamScreenshots( bool bEnable )
{
#if !defined(NO_STEAM)
	if ( steamapicontext && steamapicontext->SteamScreenshots() )
	{
		ConVarRef cl_savescreenshotstosteam( "cl_savescreenshotstosteam" );
		if ( cl_savescreenshotstosteam.IsValid() )
		{
			cl_savescreenshotstosteam.SetValue( bEnable );
			steamapicontext->SteamScreenshots()->HookScreenshots( bEnable );
		}
	}
#endif
}

#if !defined(NO_STEAM)
CON_COMMAND( cl_steamscreenshots, "Enable/disable saving screenshots to Steam" )
{
	bool bEnable = true;
	if ( args.ArgC() == 2 )
		bEnable = atoi(args[1]) ? true : false;
	EnableSteamScreenshots( bEnable );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeCSNormal::ClientModeCSNormal()
{
	m_CCKillCamReplay = INVALID_CLIENT_CCHANDLE;
	m_CCKillCamReplayPercent = 0;
	m_CCDeathHandle = INVALID_CLIENT_CCHANDLE;
	m_CCDeathPercent = 0.0f;
	m_CCFreezePeriodHandle_CT = INVALID_CLIENT_CCHANDLE;
	m_CCFreezePeriodPercent_CT = 0.0f;
	m_CCFreezePeriodHandle_T = INVALID_CLIENT_CCHANDLE;
	m_CCFreezePeriodPercent_T = 0.0f;
	m_CCPlayerFlashedHandle = INVALID_CLIENT_CCHANDLE;
	m_CCPlayerFlashedPercent = 0.0f;

	m_activePostProcessEffect = POST_EFFECT_DEFAULT;
	m_lastPostProcessEffect = POST_EFFECT_DEFAULT;
	m_pActivePostProcessController = NULL;
	m_postProcessLerpStartParams = ms_postProcessParams[ POST_EFFECT_DEFAULT ];
	m_postProcessLerpEndParams = ms_postProcessParams[ POST_EFFECT_DEFAULT ];
	m_postProcessCurrentParams = ms_postProcessParams[ POST_EFFECT_DEFAULT ];

	m_iRoundStatus = ROUND_UNKNOWN;

	m_fDelayedCTWinTime = -1.0f;
	m_nRoundMVP = 0;
}

ClientModeCSNormal::~ClientModeCSNormal()
{
}


void ClientModeCSNormal::Init()
{
	BaseClass::Init();

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_mvp" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_time_warning" );
	ListenForGameEvent( "cs_round_start_beep" );
	ListenForGameEvent( "cs_round_final_beep" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_exploded" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "hostage_follows" );
	ListenForGameEvent( "hostage_killed" );
	ListenForGameEvent( "hostage_hurt" );
	ListenForGameEvent( "write_game_titledata" );
	ListenForGameEvent( "read_game_titledata" );
	ListenForGameEvent( "switch_team" );
	ListenForGameEvent( "tr_show_finish_msgbox" );
	ListenForGameEvent( "tr_show_exit_msgbox" );
	ListenForGameEvent( "reset_player_controls" );		// used for demo purposes
	ListenForGameEvent( "seasoncoin_levelup" );
	ListenForGameEvent( "game_newmap" );

	HOOK_MESSAGE( MatchEndConditions );
	HOOK_MESSAGE( DisconnectToLobby );
	HOOK_MESSAGE( WarmupHasEnded );
	HOOK_MESSAGE( ServerRankUpdate );
	HOOK_MESSAGE( ServerRankRevealAll );
	HOOK_MESSAGE( ScoreLeaderboardData );
	HOOK_MESSAGE( GlowPropTurnOff );
	HOOK_MESSAGE( XpUpdate );
	HOOK_MESSAGE( QuestProgress );
	HOOK_MESSAGE( PlayerDecalDigitalSignature );
	//
	// WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! WARNING!!
	//

	m_UMCMsgKillCam.Bind< CS_UM_KillCam ,CCSUsrMsg_KillCam >( UtlMakeDelegate ( MsgFunc_KillCam ) );

	m_UMCMsgDisplayInventory.Bind< CS_UM_DisplayInventory, CCSUsrMsg_DisplayInventory>( UtlMakeDelegate(MsgFunc_DisplayInventory) );

	CHudElement* hintBox = (CHudElement*)GET_HUDELEMENT( CHudHintDisplay );
	if (hintBox)
	{
		hintBox->RegisterForRenderGroup("hide_for_scoreboard");
		hintBox->RegisterForRenderGroup("hide_for_round_panel");
	}

#if !defined( INCLUDE_SCALEFORM )
	CHudElement* historyResource = (CHudElement*)GET_HUDELEMENT( CHudHistoryResource );
	if (historyResource)
	{
		historyResource->RegisterForRenderGroup("hide_for_scoreboard");		
	}
#endif

	char szName[ MAX_PATH ] = "";

	if ( m_CCKillCamReplay == INVALID_CLIENT_CCHANDLE )
	{
		const char *szRawFile = "materials/correction/cc_deathcamreplay.raw";
		V_snprintf( szName, sizeof( szName ), "%s_ss%d", szRawFile, GET_ACTIVE_SPLITSCREEN_SLOT() );
		m_CCKillCamReplayPercent = 0.0f;
		m_CCKillCamReplay = g_pColorCorrectionMgr->FindColorCorrection( szName );
		if ( m_CCKillCamReplay == INVALID_CLIENT_CCHANDLE )
		{
			m_CCKillCamReplay = g_pColorCorrectionMgr->AddColorCorrection( szName, szRawFile );
		}
	}
	if ( m_CCDeathHandle == INVALID_CLIENT_CCHANDLE )
	{
		const char *szRawFile = "materials/correction/cc_death.raw";
		V_snprintf( szName, sizeof( szName ), "%s_ss%d", szRawFile, GET_ACTIVE_SPLITSCREEN_SLOT() );
		m_CCDeathPercent = 0.0f;
		m_CCDeathHandle = g_pColorCorrectionMgr->FindColorCorrection( szName );
		if ( m_CCDeathHandle == INVALID_CLIENT_CCHANDLE )
		{
			m_CCDeathHandle = g_pColorCorrectionMgr->AddColorCorrection( szName, szRawFile );
		}
	}

	if ( m_CCFreezePeriodHandle_CT == INVALID_CLIENT_CCHANDLE )
	{
		const char *szRawFile = "materials/correction/cc_freeze_ct.raw";
		V_snprintf( szName, sizeof( szName ), "%s_ss%d", szRawFile, GET_ACTIVE_SPLITSCREEN_SLOT() );
		m_CCFreezePeriodPercent_CT = 0.0f;
		m_CCFreezePeriodHandle_CT = g_pColorCorrectionMgr->FindColorCorrection( szName );
		if ( m_CCFreezePeriodHandle_CT == INVALID_CLIENT_CCHANDLE )
		{
			m_CCFreezePeriodHandle_CT = g_pColorCorrectionMgr->AddColorCorrection( szName, szRawFile );
		}
	}

	if ( m_CCFreezePeriodHandle_T == INVALID_CLIENT_CCHANDLE )
	{
		const char *szRawFile = "materials/correction/cc_freeze_t.raw";
		V_snprintf( szName, sizeof( szName ), "%s_ss%d", szRawFile, GET_ACTIVE_SPLITSCREEN_SLOT() );
		m_CCFreezePeriodPercent_T = 0.0f;
		m_CCFreezePeriodHandle_T = g_pColorCorrectionMgr->FindColorCorrection( szName );
		if ( m_CCFreezePeriodHandle_T == INVALID_CLIENT_CCHANDLE )
		{
			m_CCFreezePeriodHandle_T = g_pColorCorrectionMgr->AddColorCorrection( szName, szRawFile );
		}
	}

// 	if ( m_CCPlayerFlashedHandle == INVALID_CLIENT_CCHANDLE )
// 	{
// 		const char *szRawFile = "materials/correction/cc_flashed.raw";
// 		V_snprintf( szName, sizeof( szName ), "%s_ss%d", szRawFile, GET_ACTIVE_SPLITSCREEN_SLOT() );
// 		m_CCPlayerFlashedPercent = 0.0f;
// 		m_CCPlayerFlashedHandle = g_pColorCorrectionMgr->FindColorCorrection( szName );
// 		if ( m_CCPlayerFlashedHandle == INVALID_CLIENT_CCHANDLE )
// 		{
// 			m_CCPlayerFlashedHandle = g_pColorCorrectionMgr->AddColorCorrection( szName, szRawFile );
// 		}
// 	}


	
	/*
	int nTimer = static_cast<int>( ceil( pRules->GetRoundRemainingTime() ) );

	bool bFreezePeriod = pRules->IsFreezePeriod();
	if ( bFreezePeriod )
	{
	// countdown to the start of the round while we're in freeze period
	nTimer = static_cast<int>( ceil( pRules->GetRoundStartTime() - gpGlobals->curtime ) );
	}
	*/
	LoadPostProcessParamsFromFile();

	m_hCurrentColorCorrection = NULL;

#if !defined(NO_STEAM) && !defined (_PS3)
	m_CallbackScreenshotRequested.Register( this, &ClientModeCSNormal::OnScreenshotRequested );
#endif

	EnableSteamScreenshots( true );
}

#if !defined(NO_STEAM) && !defined (_PS3)
void ClientModeCSNormal::OnScreenshotRequested( ScreenshotRequested_t *pParam )
{
	// Steam has requested a screenshot, act as if the key currently bound to screenshots
	// has been pressed (we want tagging and the killcam screenshot behavior if applicable)
	KeyInput( 0, BUTTON_CODE_INVALID, "screenshot" );
	engine->ClientCmd( "screenshot" );
}
#endif


void ClientModeCSNormal::InitViewport()
{
	BaseClass::InitViewport();

	m_pViewport = new CounterStrikeViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

void ClientModeCSNormal::SetupStaticCameras()
{
	m_SpecCameraPositions.RemoveAll();

	char szMapName[MAX_MAP_NAME];
	Q_FileBase( engine->GetLevelName(), szMapName, sizeof(szMapName) );
	Q_strlower( szMapName );

	char szFileName[ MAX_PATH ] = "";
	V_snprintf( szFileName, sizeof( szFileName ), "maps/%s_cameras.txt", szMapName );

	KeyValues *m_pCamKV = new KeyValues( "Cameras" );
	if ( m_pCamKV->LoadFromFile( g_pFullFileSystem, szFileName ) )
	{
		for ( KeyValues *entry = m_pCamKV->GetFirstSubKey(); entry != NULL; entry = entry->GetNextKey() )
		{
			CUtlVector< char* > coordNum;
			V_SplitString( entry->GetString(), " ", coordNum );

			if ( coordNum.Count() < 5 )
			{
				Msg( "Bad coordinate entry in %s (%s).  Each entry needs 5 numbers, 'x y z pitch yaw'", szFileName, entry->GetString() );
				continue;
			}

			SpecCameraPosition_t spot;
			spot.vecPosition = Vector( atoi(coordNum[0]), atoi(coordNum[1]), atoi(coordNum[2]) );
			spot.vecAngles = Vector( atoi(coordNum[3]), atoi(coordNum[4]), 0.0f );
			spot.flWeight = 0.0f;

			m_SpecCameraPositions.AddToTail( spot );

			coordNum.PurgeAndDeleteElements();
		}
	}
}

int ClientModeCSNormalCameraSortFunction( const SpecCameraPosition_t* entry1, const SpecCameraPosition_t* entry2 )
{
	if ( entry1 == NULL )
		return 1;

	if ( entry2 == NULL )
		return -1;

	if ( entry1->flWeight < entry2->flWeight )
		return 1;
	else
		return -1;
}

bool ClientModeCSNormal::GetIdealCameraPosForPlayer( int playerindex )
{
	CUtlVector<SpecCameraPosition_t>	m_IdealCameras;
	CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerindex ) );
	if ( !pPlayer )
		return false;

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return false;

	for ( int i=0; i<m_SpecCameraPositions.Count(); ++i )
	{
		// build a list of cameras that can see the target player
		Vector vecCam = Vector( m_SpecCameraPositions[i].vecPosition[0], m_SpecCameraPositions[i].vecPosition[1], m_SpecCameraPositions[i].vecPosition[2] );
		trace_t tr;
		UTIL_TraceLine( pPlayer->EyePosition(), vecCam, MASK_OPAQUE, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction == 1.0f )
		{
			// build a list of cameras that can see the target player
			Vector vecCam = Vector( m_SpecCameraPositions[i].vecPosition[0], m_SpecCameraPositions[i].vecPosition[1], m_SpecCameraPositions[i].vecPosition[2] );
			Vector forward;
			AngleVectors( QAngle(m_SpecCameraPositions[i].vecAngles[0],m_SpecCameraPositions[i].vecAngles[1],0.0f) , &forward, NULL, NULL );
			Vector toAimSpot = pPlayer->EyePosition() - vecCam;
			toAimSpot.NormalizeInPlace();
			float flCone = DotProduct( toAimSpot, forward );
			float flMaxLength = 600;
			float flLength = clamp( VectorLength( pPlayer->EyePosition() - vecCam ), 0, flMaxLength );
			if ( flCone > 0.6f )
			{
				// weight by more centered
				m_SpecCameraPositions[i].flWeight = (flCone*1.5);
				// weight by closer
				m_SpecCameraPositions[i].flWeight += ((flMaxLength - flLength)/flMaxLength)*2;
				m_IdealCameras.AddToTail( m_SpecCameraPositions[i] );
			}
		}
	}

	if ( m_IdealCameras.Count() > 0 )
	{
		for ( int i=0; i<m_IdealCameras.Count(); ++i )
		{
			// now find all other entities/players in view 
			Vector vecCam = Vector( m_IdealCameras[i].vecPosition[0], m_IdealCameras[i].vecPosition[1], m_IdealCameras[i].vecPosition[2] );
			Vector vecStartPos = pPlayer->GetAbsOrigin();
			Vector vecMinDist = vecStartPos + Vector( -1024.0f, -1024.0f, -256.0f );
			Vector vecMaxDist = vecStartPos + Vector( 1024.0f, 1024.0f, 256.0f );

			CBaseEntity	*pEntList[128];
			int count = UTIL_EntitiesInBox( pEntList, ARRAYSIZE(pEntList), vecMinDist, vecMaxDist, 0 );

			const int nMaxFocusPoints = 32;
			Vector vecFocusPos[nMaxFocusPoints];
			int focusCount = 0;

			// add the main player's focus into the array
			if ( pPlayer )
			{
				Vector vecDirShooting, vecRight, vecUp;
				AngleVectors( pPlayer->GetFinalAimAngle(), &vecDirShooting, &vecRight, &vecUp );
				VectorNormalize( vecDirShooting );
				Vector vecPlayerShootPos = pPlayer->Weapon_ShootPosition();
				Vector vecEnd = vecPlayerShootPos + (vecDirShooting * 600);
				trace_t tr; // main enter bullet trace
				UTIL_TraceLine( vecPlayerShootPos, vecEnd, MASK_OPAQUE, pPlayer, COLLISION_GROUP_NONE, &tr );

				vecFocusPos[0] = vecStartPos;
				vecFocusPos[1] = tr.endpos;
				focusCount = 2;
			}

			for ( int j = 0; j < count; j++ )
			{
				CBaseEntity *pOther = pEntList[j];
				if ( focusCount < nMaxFocusPoints && (dynamic_cast<C_CSPlayer*>(pOther) || 
					(dynamic_cast<C_WeaponCSBase*>(pOther) && (dynamic_cast<C_WeaponCSBase*>(pOther)->GetWeaponType() == WEAPONTYPE_C4)  ) || 
					dynamic_cast<C_PlantedC4*>(pOther) || 
					dynamic_cast<CBaseCSGrenadeProjectile*>(pOther)) )
				{
					Vector vecOtherPos = pOther->IsPlayer() ? pOther->EyePosition() : pOther->GetAbsOrigin();
					Vector forward;
					AngleVectors( QAngle(m_IdealCameras[i].vecAngles[0],m_IdealCameras[i].vecAngles[1],0.0f) , &forward, NULL, NULL );
					Vector toAimSpot = pOther->EyePosition() - vecCam;
					toAimSpot.NormalizeInPlace();
					float flCone = DotProduct( toAimSpot, forward );

					if ( VectorLength( pOther->EyePosition() - vecCam ) > 64.0f )
						m_IdealCameras[i].flWeight += flCone;
					else
						m_IdealCameras[i].flWeight -= 100.0f;  // player is blocking the camera

					if ( flCone > 0.55f )
					{
						vecFocusPos[focusCount] = pOther->GetAbsOrigin();
						focusCount++;
					}
				}
			}

			Vector vecAvPos = Vector( 0, 0, 0 );
			for ( int j = 0; j < focusCount; j++ )
			{
				vecAvPos += vecFocusPos[j];
			}

			Vector vecNewPos = vecStartPos;
			if ( focusCount > 0 )
			{
				vecNewPos = (vecAvPos/focusCount);
			}

			// look into direction of second target
			QAngle cameraAngles;
			Vector forward = vecNewPos - vecCam;
			VectorAngles( forward, cameraAngles );

			// set the stored camera angles to point to the center of the mass of players weighting the selected player
			m_IdealCameras[i].vecAngles[0] = cameraAngles[0];
			m_IdealCameras[i].vecAngles[1] = cameraAngles[1];
		}

		if ( m_IdealCameras.Count() > 1 )
			m_IdealCameras.Sort( ClientModeCSNormalCameraSortFunction );

		int slot = 0;
		char commandBuffer[128];
		char commandB[32] = "spec_goto";
		float flLerpTime = 0.5f;

		Vector vecSpecPos = g_bEngineIsHLTV ? HLTVCamera()->GetCameraPosition() : pLocalPlayer->GetAbsOrigin();
		trace_t tr;
		UTIL_TraceLine( m_IdealCameras[slot].vecPosition, vecSpecPos, MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
		if ( pLocalPlayer && tr.fraction == 1.0f )
			V_snprintf( commandB, sizeof( commandB ), "%s", "spec_lerpto" );

		V_snprintf( commandBuffer, sizeof( commandBuffer ), "%s %f %f %f %f %f %d %f", commandB, m_IdealCameras[slot].vecPosition[0], m_IdealCameras[slot].vecPosition[1], m_IdealCameras[slot].vecPosition[2], m_IdealCameras[slot].vecAngles[0], m_IdealCameras[slot].vecAngles[1], playerindex, flLerpTime );
		
		engine->ClientCmd( commandBuffer );
		m_IdealCameras.RemoveAll();

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeCSNormal::LevelShutdown( void )
{
	BaseClass::LevelShutdown();

	// reset all of the post process effects
	m_lastPostProcessEffect = POST_EFFECT_DEFAULT;
	m_activePostProcessEffect = POST_EFFECT_DEFAULT;
	m_pActivePostProcessController = NULL;
	m_postProcessEffectCountdown.Reset();
	m_postProcessLerpEndParams = ms_postProcessParams[POST_EFFECT_DEFAULT];
	m_postProcessLerpStartParams = ms_postProcessParams[POST_EFFECT_DEFAULT];
	m_postProcessCurrentParams = ms_postProcessParams[POST_EFFECT_DEFAULT];

	// Unregister all glow boxes here otherwise they would leak
	GlowObjectManager().UnregisterAllGlowBoxes();

	// Clear out all uncommitted quest progress
	sm_mapQuestProgressUncommitted.Purge();
	s_ScoreLeaderboardData.Clear();

	// Shutdown outstanding player spray signature requests
	s_mapClientDigitalSignatureRequests.Purge();
	s_flLastDigitalSignatureTime = 0;

	// Shutdown decals system
	extern void OnPlayerDecalsLevelShutdown();
	OnPlayerDecalsLevelShutdown();

	// Increment level transitions counter
	++ s_numLevelTransitions;

	// Remove any lingering debug overlays, since it's possible they won't get cleaned up automatically later.
	// This is in response to anecdotal reports that players can 'mark' the world with showimpacts or grenade trajectories,
	// then use them to their advantage on subsequent games played immediately on the same map.
	debugoverlay->ClearAllOverlays();
}

uint32 ClientModeCSNormal::s_numLevelTransitions = 0;

void ClientModeCSNormal::Update()
{
	BaseClass::Update();

	UpdatePostProcessingEffects();

	// Update decals system
	extern void OnPlayerDecalsUpdate();
	OnPlayerDecalsUpdate();

	// Override the hud's visibility if this is a logo (like E3 demo) map.
	if ( CSGameRules() && CSGameRules()->IsLogoMap() )
		m_pViewport->SetVisible( false );

	if ( ( m_fDelayedCTWinTime > 0.0f ) && ( gpGlobals->curtime >= m_fDelayedCTWinTime  ) )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.CTWin");

		m_fDelayedCTWinTime = -1.0f;
	}
	// halftime music needs a delay thusly
	static bool bStartedHalfTimeMusic = false;
	static float flHalfTimeStart = 0.0;
	
	if( CSGameRules() && ( CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME || CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED)  )
	{
		if( !bStartedHalfTimeMusic && gpGlobals->curtime - flHalfTimeStart > 6.5 )
		{
			bStartedHalfTimeMusic = true;
			CSingleUserRecipientFilter filter(C_BasePlayer::GetLocalPlayer());
			if( CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME )
			{
				PlayMusicSelection(filter, CSMUSIC_HALFTIME);
			}
			else if( m_nRoundMVP != 0 )
			{
				PlayMusicSelection(filter, CSMUSIC_HALFTIME, m_nRoundMVP );
			}
		}
	}
	else
	{
		flHalfTimeStart = gpGlobals->curtime;
		bStartedHalfTimeMusic = false;
	}
#if defined ( _X360 )
	if ( !xboxsystem->IsArcadeTitleUnlocked() )
	{
		const char *levelName = engine->GetLevelName();
		if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
		{
			// verify globals->frametime is not outrageous and subtract from arcade trial timer
			SplitScreenConVarRef xbox_arcade_remaining_trial_time("xbox_arcade_remaining_trial_time");
			if ( gpGlobals->frametime < 5.0f )
			{
				float trialTime = xbox_arcade_remaining_trial_time.GetFloat( GET_ACTIVE_SPLITSCREEN_SLOT() );
				trialTime -= gpGlobals->frametime;
				if ( trialTime < 0.0f )
				{
					trialTime = 0.0f;
				}
				xbox_arcade_remaining_trial_time.SetValue( GET_ACTIVE_SPLITSCREEN_SLOT(), trialTime );
			}
		}
	}
#endif

	if ( HLTVCamera() )
		HLTVCamera()->Update();

	//
	// Check if client is now eligible to stop demo recording
	//
	if ( CSGameRules() && CSGameRules()->m_bMarkClientStopRecordAtRoundEnd )
	{
		char name[ 256 ] = "Cannot stop recording now";
		if ( clientdll->CanStopRecordDemo( name, sizeof( name ) ) )
		{
			CSGameRules()->m_bMarkClientStopRecordAtRoundEnd = false;
			engine->ClientCmd_Unrestricted( "stop;\n" );
		}
	}

	//
	// Check if quests need chat confirmation
	//
	for ( uint32 i = sm_mapQuestProgressUncommitted.FirstInorder();
		i != sm_mapQuestProgressUncommitted.InvalidIndex();
		i = sm_mapQuestProgressUncommitted.NextInorder( i ) )
	{
		CQuestUncommittedProgress_t &questProgress = sm_mapQuestProgressUncommitted.Element( i );
		if ( !questProgress.m_dblNormalPointsProgressTime )
			continue;
		if ( Plat_FloatTime() - questProgress.m_dblNormalPointsProgressTime < 1.6 )
			continue;
		questProgress.m_dblNormalPointsProgressTime = 0;
		if ( questProgress.m_numNormalPointsProgressBaseline < questProgress.m_numNormalPoints )
		{
			if ( const CEconQuestDefinition *pQuestDef = GetItemSchema()->GetQuestDefinition( sm_mapQuestProgressUncommitted.Key( i ) ) )
			{
				uint32 numPointsEarned = questProgress.m_numNormalPoints - questProgress.m_numNormalPointsProgressBaseline;
				char const *fmtToken = ( numPointsEarned > 1 ) ? "#quest_uncommitted_points_chat_plural" : "#quest_uncommitted_points_chat_singular";

				// Quest string keys
				KeyValues *pKVLocalizedQuestStrings = new KeyValues( "LocalizedQuestStrings" );
				KeyValues::AutoDelete autodelete( pKVLocalizedQuestStrings );
				FOR_EACH_SUBKEY( pQuestDef->GetStringTokens(), keyvalue )
				{
					pKVLocalizedQuestStrings->SetWString( keyvalue->GetName(), g_pVGuiLocalize->Find( keyvalue->GetString() ) );
				}
				
				// Points earned
				wchar_t wchPoints[ 64 ] = {};
				V_snwprintf( wchPoints, ARRAYSIZE( wchPoints ), PRI_S_FOR_WS, V_FormatNumber( numPointsEarned ) );
				pKVLocalizedQuestStrings->SetWString( "points", wchPoints );

				if ( questProgress.m_bIsEventQuest )
				{
					fmtToken = "#quest_uncommitted_points_chat_event";

					wchar_t wchQuestName[ 256 ] = {};

					locchar_t * locShortName = L"";

					locShortName = g_pVGuiLocalize->Find( pQuestDef->GetShortNameLocToken( ) );

					if ( !locShortName )
					{
						locShortName = L"QUEST MISSING SHORT NAME";
					}

					g_pVGuiLocalize->ConstructString( wchQuestName, sizeof( wchQuestName ), locShortName, pKVLocalizedQuestStrings );
					pKVLocalizedQuestStrings->SetWString( "quest_short_name", wchQuestName );
					
				}
				else
				{
					if ( ( !pQuestDef->GetQuestPoints().Count() || ( pQuestDef->GetQuestPoints().Head() <= 1 ) )
						&& ( numPointsEarned == 1 )
						&& !questProgress.m_numNormalPointsProgressBaseline )	// Missions requiring a single point to score print a custom string
						fmtToken = "#quest_uncommitted_points_chat_one";
				}

				// Make the message
				wchar_t wchHudMessage[256] = {};
				g_pVGuiLocalize->ConstructString( wchHudMessage, sizeof( wchHudMessage ), fmtToken, pKVLocalizedQuestStrings );
				if ( wchHudMessage[0] )
				{
					if ( CHudChat* pChat = ( CHudChat* )( GetHud( 0 ).FindElement( "CHudChat" ) ) )
					{
						pChat->ChatPrintfW( 0, CHAT_FILTER_NONE, wchHudMessage );
					}
				}

#if defined( INCLUDE_SCALEFORM )
				// Quest notification alert
				if (SFUniqueAlerts* pAlerts = (SFUniqueAlerts*)(GetHud(0).FindElement("SFUniqueAlerts")))
				{
					// REI: Not sure this is correct.  Need to understand why this is a vector, and which "points" from the
					// vector the uncommitted quest progress is referring to
					const CCopyableUtlVector< uint32 >& questPointsVec = pQuestDef->GetQuestPoints();
					uint32 questPoints = 0;
					if (questPointsVec.Count() > 0)
						questPoints = questPointsVec[0];

					pAlerts->ShowQuestProgress(numPointsEarned, questProgress.m_numNormalPoints, questPoints, "weapon_knife", "do the thing"); // TODO: Correctly extract the weapon icon to use
				}
#endif
			}
		}
		questProgress.m_numNormalPointsProgressBaseline = questProgress.m_numNormalPoints;
	}

	// Check if we need anti-addiction chat print out
	static double s_dblLastAntiAddictionCheckPlatFloatTime = 0;
	static int s_nLastTimePlayedConsecutivelyPrinted = 0;
	double dblTimeNow = Plat_FloatTime();
	if ( dblTimeNow > s_dblLastAntiAddictionCheckPlatFloatTime + 50 )
	{
		s_dblLastAntiAddictionCheckPlatFloatTime = dblTimeNow;
		int nTimePlayedConsecutively = 0;//Helper_GetTimePlayedConsecutively();
		if ( ( nTimePlayedConsecutively >= 3600 ) && ( nTimePlayedConsecutively/1200 != s_nLastTimePlayedConsecutivelyPrinted/1200 ) )
		{	// Print in chat after 1 hour of anti-addiction consecutive time every 20 minutes
			if ( CHudChat* pChat = ( CHudChat* ) ( GetHud( 0 ).FindElement( "CHudChat" ) ) )
			{
				s_nLastTimePlayedConsecutivelyPrinted = nTimePlayedConsecutively;

				static wchar_t const * const kwszHour = g_pVGuiLocalize->Find( "#SFUI_Warning_AntiAddiction_Time_Hour" );
				static wchar_t const * const kwszHours = g_pVGuiLocalize->Find( "#SFUI_Warning_AntiAddiction_Time_Hours" );
				static wchar_t const * const kwszMinutes = g_pVGuiLocalize->Find( "#SFUI_Warning_AntiAddiction_Time_Minutes" );
				static wchar_t const * const kwszFmtH = g_pVGuiLocalize->Find( "#SFUI_Warning_AntiAddiction_Time_Format_H" );
				static wchar_t const * const kwszFmtHM = g_pVGuiLocalize->Find( "#SFUI_Warning_AntiAddiction_Time_Format_HM" );

				wchar_t wszHours[64] = {}, wszMinutes[64] = {};
				V_swprintf_safe( wszHours, L"%d", nTimePlayedConsecutively/3600 );
				V_swprintf_safe( wszMinutes, L"%02d", ( nTimePlayedConsecutively%3600 ) / 60 );
				wchar_t const *wszFmtTime = ( ( nTimePlayedConsecutively%3600 ) / 60 >= 5 ) ? kwszFmtHM : kwszFmtH;
				
				wchar_t wszTimeString[256] = {};
				g_pVGuiLocalize->ConstructString( wszTimeString, sizeof( wszTimeString ), wszFmtTime, 4,
					wszHours, ( nTimePlayedConsecutively/3600 > 1 ) ? kwszHours : kwszHour, wszMinutes, kwszMinutes );

				char const *szColor = "Green";
				if ( nTimePlayedConsecutively/3600 >= 5 )
					szColor = "Red";
				else if ( nTimePlayedConsecutively/3600 >= 3 )
					szColor = "Yellow";
				
				wchar_t wszLocalizedAntiAddiction[1024] = {};
				g_pVGuiLocalize->ConstructString( wszLocalizedAntiAddiction, sizeof( wszLocalizedAntiAddiction ), g_pVGuiLocalize->Find( CFmtStr( "#SFUI_Warning_AntiAddiction_%s", szColor ) ), 1, wszTimeString );
				pChat->ChatPrintfW( 0, CHAT_FILTER_NONE, wszLocalizedAntiAddiction );
			}
		}
	}

}

#ifdef _DEBUG
CON_COMMAND_F(quest_ui_test, "Test quest ui", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	int newPoints = 1;
	int totalPoints = 10;
	int maxPoints = 20;

	if (args.ArgC() > 1)
	{
		newPoints = Q_atoi(args.ArgV()[1]);
	}

	if (args.ArgC() > 2)
	{
		totalPoints = Q_atoi(args.ArgV()[2]);
	}

	if (args.ArgC() > 3)
	{
		maxPoints = Q_atoi(args.ArgV()[3]);
	}

	if (SFUniqueAlerts* pAlerts = (SFUniqueAlerts*)(GetHud(0).FindElement("SFUniqueAlerts")))
	{
		pAlerts->ShowQuestProgress(newPoints, totalPoints, maxPoints, "weapon_knife", "quest desc");
	}
}
#endif

ConVar spec_replay_colorcorrection( "spec_replay_colorcorrection", "0.5", FCVAR_CLIENTDLL, "Amount of color correction in deathcam replay" );

//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::UpdateColorCorrectionWeights( void )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pPlayer = ToCSPlayer(pLocalPlayer);

	if ( !pPlayer )
	{
		m_CCKillCamReplayPercent = 0.0f;
		m_CCDeathPercent = 0.0f;
		m_CCFreezePeriodPercent_CT = 0.0f;
		m_CCFreezePeriodPercent_T = 0.0f;
		m_CCPlayerFlashedPercent = 0.0f;
		return;
	}

	m_CCPlayerFlashedPercent = Min( 1.0f, pPlayer->m_flFlashOverlayAlpha + 0.25f );
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		m_CCKillCamReplayPercent = spec_replay_colorcorrection.GetFloat();
		m_CCDeathPercent = 0.0f;
		m_CCPlayerFlashedPercent *= 0.5f;
	}
	else
	{
		m_CCKillCamReplayPercent = 0.0f;
		bool isDying = false;
		if ( !pPlayer->IsAlive() && (pPlayer->GetObserverMode() == OBS_MODE_DEATHCAM) )
		{
			isDying = true;
		}

		m_CCDeathPercent = clamp( m_CCDeathPercent + ((isDying) ? 0.1f : -0.1f), 0.0f, 1.0f );
	}

	float flTimer = 0;

	bool bFreezePeriod = CSGameRules()->IsFreezePeriod();
	bool bImmune = pPlayer->m_bGunGameImmunity;
	if ( bFreezePeriod || bImmune )
	{
		float flFadeBegin = 2.0f;

		// countdown to the start of the round while we're in freeze period
		if ( bImmune )
		{
			// if freeze time is also active and freeze time is longer than immune time, use that time instead
			if ( bFreezePeriod && (CSGameRules()->GetRoundStartTime() - gpGlobals->curtime) > (pPlayer->m_fImmuneToGunGameDamageTime - gpGlobals->curtime))
				flTimer = CSGameRules()->GetRoundStartTime() - gpGlobals->curtime;
			else
			{
				flTimer = pPlayer->m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
				flFadeBegin = 0.5;
			}
		}
		else if ( bFreezePeriod )
		{
			flTimer = CSGameRules()->GetRoundStartTime() - gpGlobals->curtime;
		}

		if ( flTimer > flFadeBegin )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				m_CCFreezePeriodPercent_CT = 1.0f;
				m_CCFreezePeriodPercent_T = 0.0f;
			}
			else
			{
				m_CCFreezePeriodPercent_CT = 0.0f;
				m_CCFreezePeriodPercent_T = 1.0f;
			}
		}
		else
		{
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				m_CCFreezePeriodPercent_CT = clamp( flTimer / flFadeBegin, 0.05f, 1.0f );
				m_CCFreezePeriodPercent_T = 0;
			}
			else
			{
				m_CCFreezePeriodPercent_T = clamp( flTimer / flFadeBegin, 0.05f, 1.0f );
				m_CCFreezePeriodPercent_CT = 0;
			}	
		}
	}
	else
	{
		int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;

 		if ( CSGameRules()->IsPlayingCooperativeGametype() && pPlayer->GetTeamNumber() == nTeam )
 		{
			m_CCFreezePeriodPercent_T = 0;
			m_CCFreezePeriodPercent_CT = MIN( 1.0f, pPlayer->m_flGuardianTooFarDistFrac * 3);
 		}
		else
		{
			m_CCFreezePeriodPercent_T = 0;
			m_CCFreezePeriodPercent_CT = 0;
		}
	}

}

void ClientModeCSNormal::OnColorCorrectionWeightsReset( void )
{
	UpdateColorCorrectionWeights();
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCKillCamReplay, m_CCKillCamReplayPercent );
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, m_CCDeathPercent );
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCFreezePeriodHandle_CT, m_CCFreezePeriodPercent_CT );
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCFreezePeriodHandle_T, m_CCFreezePeriodPercent_T );
	g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCPlayerFlashedHandle, m_CCPlayerFlashedPercent );

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		C_ColorCorrection* pCC = pPlayer->GetActiveColorCorrection();
		if ( pCC != m_hCurrentColorCorrection )
		{
			if ( m_hCurrentColorCorrection )
			{
				m_hCurrentColorCorrection->EnableOnClient( false );
			}
			if ( pCC )
			{
				pCC->EnableOnClient( true, m_hCurrentColorCorrection == NULL );
			}
			m_hCurrentColorCorrection = pCC;
		}
	}
}

float ClientModeCSNormal::GetColorCorrectionScale( void ) const
{
	return 1.0f;
}

//--------------------------------------------------------------------------------------------------------
PostProcessEffect_t ClientModeCSNormal::PostProcessEffectFromName( const char* pName ) const
{
	for ( int i = 0; i < NUM_POST_EFFECTS; i++ )
	{
		if ( V_stricmp( pName, ms_postProcessEffectNames[i] ) == 0 )
		{
			return PostProcessEffect_t( i );
		}
	}
	return NUM_POST_EFFECTS;
}


//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::LoadPostProcessParamsFromFile( const char* pFileName )
{
	if ( !pFileName )
	{
		pFileName = "scripts/postprocess.txt";
	}

	KeyValues *pPPKeys = new KeyValues( "post_process" );
	if ( pPPKeys->LoadFromFile( g_pFullFileSystem, pFileName ) == false )
	{
		Warning( "Error loading postprocessing params from file %s\n" , pFileName );
		pPPKeys->deleteThis();
		return;
	}

	for ( KeyValues* pSubKeys = pPPKeys->GetFirstTrueSubKey(); pSubKeys; pSubKeys = pSubKeys->GetNextTrueSubKey() )
	{
		PostProcessEffect_t effect = PostProcessEffectFromName( pSubKeys->GetName() );
		if ( effect == NUM_POST_EFFECTS )
		{
			Warning( "Unknown postprocess effect type: %s\n", pSubKeys->GetName() );
			continue;
		}

		ms_postProcessParams[effect].m_flParameters[PPPN_FADE_TIME]						= pSubKeys->GetFloat( "fadetime", 0.5f );
		ms_postProcessParams[effect].m_flParameters[PPPN_LOCAL_CONTRAST_STRENGTH]		= pSubKeys->GetFloat( "localcontrast", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_LOCAL_CONTRAST_EDGE_STRENGTH]	= pSubKeys->GetFloat( "edgelocalcontrast", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_VIGNETTE_START]				= pSubKeys->GetFloat( "vignettestart", 1.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_VIGNETTE_END]					= pSubKeys->GetFloat( "vignetteend", 2.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH]		= pSubKeys->GetFloat( "vignetteblur", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_FADE_TO_BLACK_STRENGTH]		= pSubKeys->GetFloat( "fadetoblack", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_DEPTH_BLUR_FOCAL_DISTANCE]		= pSubKeys->GetFloat( "depthblur_focaldist", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_DEPTH_BLUR_STRENGTH]			= pSubKeys->GetFloat( "depthblur_strength", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_SCREEN_BLUR_STRENGTH]			= pSubKeys->GetFloat( "screenblur_strength", 0.0f );
		ms_postProcessParams[effect].m_flParameters[PPPN_FILM_GRAIN_STRENGTH]			= pSubKeys->GetFloat( "filmgrain_strength", 0.0f );
	}

	// update the currently active postprocess type with the new params
	if ( m_activePostProcessEffect < NUM_POST_EFFECTS )
	{
		m_postProcessLerpEndParams = ms_postProcessParams[m_activePostProcessEffect];
	}

	pPPKeys->deleteThis();
}


//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::UpdatePostProcessingEffects()
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pPlayer = ToCSPlayer(pLocalPlayer);

	// If we set off the bomb, run the bomb round end post process effect.
	if ( pPlayer && m_iRoundStatus == ROUND_ENDED_VIA_BOMBING && (pPlayer->GetObserverMode() == OBS_MODE_DEATHCAM || ( pPlayer->GetObserverTarget() && !pPlayer->GetObserverTarget()->IsPlayer() ) ) )
	{
		PostProcessLerpTo( POST_EFFECT_ROUND_END_VIA_BOMBING, 1.0f );
	}
	else if ( !pPlayer )
	{
		PostProcessLerpTo( POST_EFFECT_DEFAULT );
	}
	else if ( pPlayer->GetViewEntity() != NULL )
	{
		// Our view is on a camera
		PostProcessLerpTo( POST_EFFECT_DEFAULT );
	}
	else if ( pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
	{		
		PostProcessLerpTo( POST_EFFECT_SPEC_CAMERA_LERPING, 0.1f );
	}
	else if ( pPlayer->IsBuyMenuOpen() )
	{
		PostProcessLerpTo( POST_EFFECT_IN_BUY_MENU, 0.1f );
	}
	else if ( false ) // [msmith]: Currently in progress...
	{
		PostProcessLerpTo( POST_EFFECT_UNDER_WATER, 0.1f );
	}
	else if ( pPlayer->IsAlive() && pPlayer->GetFOV() != pPlayer->GetDefaultFOV() && pPlayer->m_bIsScoped )
	{
		CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
		if ( pWeapon && pWeapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE )
		{
			float flBaseAccuracy = pWeapon->GetInaccuracyStand();
			float flInacc = MAX( pWeapon->GetInaccuracy() - flBaseAccuracy, 0 );
			if ( flInacc * 100 > 5 )
				PostProcessLerpTo( POST_EFFECT_ZOOMED_SNIPER_MOVING, 0.01f );
			else
				PostProcessLerpTo( POST_EFFECT_ZOOMED_SNIPER, 0.01f );
		}
		else if ( pWeapon )
		{
			PostProcessLerpTo( POST_EFFECT_ZOOMED_RIFLE, 0.01f );
		}
	}
	else if ( !pPlayer->IsAlive() && pPlayer->m_iDeathPostEffect > 0 && mp_forcecamera.GetInt() != OBS_ALLOW_ALL )
	{
		PostProcessEffect_t post_effect = ( PostProcessEffect_t ) pPlayer->m_iDeathPostEffect;

		if ( post_effect == POST_EFFECT_DEATH_CAM_BODYSHOT )
		{
			extern ConVar	spec_freeze_time;
			PostProcessLerpTo( POST_EFFECT_DEATH_CAM_BODYSHOT, spec_freeze_time.GetInt() );
		}
		else if ( post_effect == POST_EFFECT_DEATH_CAM_HEADSHOT )
		{
			PostProcessLerpTo( POST_EFFECT_DEATH_CAM_HEADSHOT, 1.0f );
		}
	}
	else if ( !pPlayer->IsAlive() && pPlayer->GetObserverTarget() == NULL && pPlayer->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		PostProcessLerpTo( POST_EFFECT_DEATH_CAM );
	}

	
	/*
	else if ( pPlayer && pPlayer->GetHealth() <= pPlayer->GetMaxHealth()/3 )
	{
		float flStartHealthFrac = (pPlayer->GetMaxHealth()/3) * 0.01;
		float fHealthFrac = clamp( (float)pPlayer->GetHealth() / (float)pPlayer->GetMaxHealth(), 0.0f, 1.0f );
		float flFXFrac = fHealthFrac / flStartHealthFrac;
		PostProcessParameters_t incapParams;

		// lerp target params based on health state, then let DoPostProcessParamLerp() do the rest

		LerpPostProcessParam( 1.0f - flFXFrac, incapParams, ms_postProcessParams[POST_EFFECT_LOW_HEATH], ms_postProcessParams[POST_EFFECT_VERY_LOW_HEATH] );

		PostProcessLerpTo( POST_EFFECT_DEFAULT, 0.5f, &incapParams );
	}
	else if ( pPlayer->GetEffectEntity() != NULL )
	{
		PostProcessLerpTo( POST_EFFECT_IN_FIRE );
	}*/
	else
	{
		C_PostProcessController* pPPCtrl = pPlayer->GetActivePostProcessController();

		float flFadeTime = 0.5f;
		if ( m_activePostProcessEffect == POST_EFFECT_ZOOMED_SNIPER_MOVING || m_activePostProcessEffect == POST_EFFECT_ZOOMED_SNIPER ||
			 m_activePostProcessEffect == POST_EFFECT_ZOOMED_RIFLE )
		{
			flFadeTime = 0.0f;
		}
		else if ( m_activePostProcessEffect == POST_EFFECT_SPEC_CAMERA_LERPING )
		{
			flFadeTime = 0.1f;
		}
		else if ( !pPPCtrl && engine->IsLevelMainMenuBackground() )
		{
			// FIXME: In the main menu the server is unable to set up the postprocess controller for the player for some reason.
			// Just use the master controller for now.
			pPPCtrl = C_PostProcessController::GetMasterController();
		}

		if ( !pPPCtrl )
		{
			PostProcessLerpTo( POST_EFFECT_DEFAULT, flFadeTime);
		}
		else
		{
			PostProcessLerpTo( POST_EFFECT_MAP_CONTROLLED, pPPCtrl );
		}
	}

	DoPostProcessParamLerp();

	// Apply params to postprocessing code
	PostProcessParameters_t currentParams = m_postProcessCurrentParams;

	SetPostProcessParams( &currentParams );
}

//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::PostProcessLerpTo( PostProcessEffect_t effectID, float fFadeDuration, const PostProcessParameters_t* pTargetParams )
{
	if ( m_activePostProcessEffect == effectID )
	{
		// the target params might still be updated
		if ( pTargetParams )
		{
			m_postProcessLerpEndParams = *pTargetParams;
		}
		return;
	}

	m_lastPostProcessEffect = m_activePostProcessEffect;
	m_activePostProcessEffect = effectID;
	m_pActivePostProcessController = NULL;
	m_postProcessEffectCountdown.Start( fFadeDuration );
	m_postProcessLerpStartParams = m_postProcessCurrentParams;
	if ( pTargetParams )
	{
		m_postProcessLerpEndParams = *pTargetParams;
	}
	else
	{
		m_postProcessLerpEndParams = ms_postProcessParams[ effectID ];
	}
}


//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::PostProcessLerpTo( PostProcessEffect_t effectID, const C_PostProcessController* pPostProcessController )
{
	Assert( pPostProcessController );

	m_lastPostProcessEffect = m_activePostProcessEffect;
	m_activePostProcessEffect = effectID;

	if ( m_pActivePostProcessController != pPostProcessController )
	{
		float flFade = pPostProcessController->m_PostProcessParameters.m_flParameters[PPPN_FADE_TIME];
		// we force the fade back time to be short when coming from the buy menu
		if ( m_lastPostProcessEffect == POST_EFFECT_IN_BUY_MENU )
			flFade = 0.25;
		else if ( m_lastPostProcessEffect == POST_EFFECT_ZOOMED_RIFLE || m_lastPostProcessEffect == POST_EFFECT_ZOOMED_SNIPER || m_lastPostProcessEffect == POST_EFFECT_ZOOMED_SNIPER_MOVING )
			flFade = 0.01;
		else if ( m_lastPostProcessEffect == POST_EFFECT_SPEC_CAMERA_LERPING )
			flFade = 0.1;
		else if ( m_lastPostProcessEffect == POST_EFFECT_DEATH_CAM_BODYSHOT || m_lastPostProcessEffect == POST_EFFECT_DEATH_CAM_HEADSHOT )
			flFade = 0.01;
		m_pActivePostProcessController = pPostProcessController;
		m_postProcessEffectCountdown.Start( flFade );
		m_postProcessLerpStartParams = m_postProcessCurrentParams;
	}

	m_postProcessLerpEndParams = pPostProcessController->m_PostProcessParameters;

}


//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::DoPostProcessParamLerp()
{
	float fAmount = 1.0f - m_postProcessEffectCountdown.GetRemainingRatio();

	// just force it
	if ( fAmount == 1 )
		m_postProcessCurrentParams = m_postProcessLerpEndParams;
	else
	{
#define PP_LERP(x) m_postProcessCurrentParams.x = Lerp( fAmount, m_postProcessLerpStartParams.x, m_postProcessLerpEndParams.x )
		PP_LERP( m_flParameters[PPPN_FADE_TO_BLACK_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_LOCAL_CONTRAST_EDGE_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_LOCAL_CONTRAST_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_VIGNETTE_END] );
		PP_LERP( m_flParameters[PPPN_VIGNETTE_START] );
		PP_LERP( m_flParameters[PPPN_DEPTH_BLUR_FOCAL_DISTANCE] );
		PP_LERP( m_flParameters[PPPN_DEPTH_BLUR_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_SCREEN_BLUR_STRENGTH] );
		PP_LERP( m_flParameters[PPPN_FILM_GRAIN_STRENGTH] );
#undef PP_LERP
	}
}

//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::LerpPostProcessParam( float fAmount, PostProcessParameters_t& result, const PostProcessParameters_t& from,
	const PostProcessParameters_t& to ) const
{
#define PP_LERP(x) result.x = Lerp( fAmount, from.x, to.x )
	PP_LERP( m_flParameters[PPPN_FADE_TO_BLACK_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_LOCAL_CONTRAST_EDGE_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_LOCAL_CONTRAST_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_VIGNETTE_END] );
	PP_LERP( m_flParameters[PPPN_VIGNETTE_START] );
	PP_LERP( m_flParameters[PPPN_DEPTH_BLUR_FOCAL_DISTANCE] );
	PP_LERP( m_flParameters[PPPN_DEPTH_BLUR_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_SCREEN_BLUR_STRENGTH] );
	PP_LERP( m_flParameters[PPPN_FILM_GRAIN_STRENGTH] );
#undef PP_LERP
}


//--------------------------------------------------------------------------------------------------------
void ClientModeCSNormal::GetDefaultPostProcessingParams( C_CSPlayer* pPlayer, PostProcessEffectParams_t* pParams )
{
	Assert( pParams );

	C_PostProcessController* pPPCtrl = NULL;
	if ( pPlayer )
	{
		pPPCtrl = pPlayer->GetActivePostProcessController();
	}

	if ( pPPCtrl )
	{
		pParams->fLocalContrastStrength = pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_LOCAL_CONTRAST_STRENGTH];
		pParams->fLocalContrastEdgeStrength = pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_LOCAL_CONTRAST_EDGE_STRENGTH];
		pParams->fVignetteStart = pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_VIGNETTE_START];
		pParams->fVignetteEnd = pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_VIGNETTE_END];
		pParams->fVignetteBlurStrength = pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_VIGNETTE_BLUR_STRENGTH];
		pParams->fFadeToBlackStrength= pPPCtrl->m_PostProcessParameters.m_flParameters[PPPN_FADE_TO_BLACK_STRENGTH];
	}
	else
	{
		memcpy( pParams, &ms_postProcessParams[ POST_EFFECT_DEFAULT ], sizeof( PostProcessEffectParams_t ) );
	}
}

/*
void ClientModeCSNormal::UpdateSpectatorMode( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return;

	IMapOverview * overviewmap = m_pViewport->GetMapOverviewInterface();

	if ( !overviewmap )
		return;

	overviewmap->SetTime( gpGlobals->curtime );

	int obs_mode = pPlayer->GetObserverMode();

	if ( obs_mode < OBS_MODE_IN_EYE )
		return;

	Vector worldpos = pPlayer->GetLocalOrigin();
	QAngle angles; engine->GetViewAngles( angles );

	C_BaseEntity *target = pPlayer->GetObserverTarget();

	if ( target && (obs_mode == OBS_MODE_IN_EYE || obs_mode == OBS_MODE_CHASE) )
	{
		worldpos = target->GetAbsOrigin();

		if ( obs_mode == OBS_MODE_IN_EYE )
		{
			angles = target->GetAbsAngles();
		}
	}

	Vector2D mappos = overviewmap->WorldToMap( worldpos );

	overviewmap->SetCenter( mappos );
	overviewmap->SetAngle( angles.y );	
	
	for ( int i = 1; i<= MAX_PLAYERS; i++)
	{
		C_BaseEntity *ent = ClientEntityList().GetEnt( i );

		if ( !ent || !ent->IsPlayer() )
			continue;

		C_BasePlayer *p = ToBasePlayer( ent );

		// update position of active players in our PVS
		Vector position = p->GetAbsOrigin();
		QAngle angle = p->GetAbsAngles();

		if ( p->IsDormant() )
		{
			// if player is not in PVS, use PlayerResources data
			position = g_PR->GetPosition( i );
			angles[1] = g_PR->GetViewAngle( i );
		}
		
		overviewmap->SetPlayerPositions( i-1, position, angles );
	}
} */


// Sets convars to tag the current mapname and the player in your crosshairs.
// The player tagged will be overridden for killcam shots to be the killer
static void ScreenshotTaggingKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( pszCurrentBinding && ( FStrEq( pszCurrentBinding, "screenshot" ) || FStrEq( pszCurrentBinding, "jpeg" ) ) )
	{
		// Tag the player in the crosshairs
		C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
		if ( pPlayer )
		{
			C_CSPlayer *pCrosshairs = ToCSPlayer( UTIL_PlayerByIndex( pPlayer->GetIDTarget() ) );
			if ( pCrosshairs && !pCrosshairs->IsBot() )
			{
				CSteamID steamID;
				if ( pCrosshairs->GetSteamID( &steamID ) && steamID.IsValid() )
				{
					ConVarRef cl_screenshotusertag( "cl_screenshotusertag" );
					if ( cl_screenshotusertag.IsValid() )
					{
						cl_screenshotusertag.SetValue( (int)steamID.GetAccountID() );
					}
				}
			}
		}

		// Tag the current map
		ConVarRef cl_screenshotlocation( "cl_screenshotlocation" );
		if ( cl_screenshotlocation.IsValid() )
		{
			char szMapName[MAX_MAP_NAME];
			Q_FileBase( engine->GetLevelName(), szMapName, sizeof(szMapName) );
			Q_strlower( szMapName );
			cl_screenshotlocation.SetValue( szMapName );
		}
	}
}


ConVar cl_scoreboard_mouse_enable_binding( "cl_scoreboard_mouse_enable_binding", "+attack2", FCVAR_ARCHIVE, "Name of the binding to enable mouse selection in the scoreboard" );
//-----------------------------------------------------------------------------
// Purpose: We've received a keypress from the engine. Return 1 if the engine is allowed to handle it.
//-----------------------------------------------------------------------------
int	ClientModeCSNormal::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	// don't process input in LogoMaps
	if( CSGameRules() && CSGameRules()->IsLogoMap() )
		return 1;

	// Applies basic tags if we're going to take a screenshot
	ScreenshotTaggingKeyInput( down, keynum, pszCurrentBinding );

	return BaseClass::KeyInput( down, keynum, pszCurrentBinding );
}


//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		GetHud().InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual void CreateDefaultPanels( void ) { /* don't create any panels yet*/ };
};

ClientModeCSNormal g_ClientModeNormal[ MAX_SPLITSCREEN_PLAYERS ];

IClientMode *GetClientModeNormal()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ClientModeNormal[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}


ClientModeCSNormal* GetClientModeCSNormal()
{
	return assert_cast< ClientModeCSNormal* >( GetClientModeNormal() );
}

int ClientModeCSNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

void ClientModeCSNormal::SyncCurrentKeyBindingsToDeviceTitleData( int iController, int eDevice, const SyncKeyBindingValueDirection_t eOp )
{

#if defined( _GAMECONSOLE )

#define MAX_BINDING_NAME 64

	//Key Names
#define ACTION( name )
#define BINDING( name, cppType ) { #name },
	static char sJoystickNames[][MAX_BINDING_NAME] = {
#include "xlast_csgo/inc_bindings_usr.inc"

// Additional Keyboard Bindings for PS3
#if defined( _PS3 )

#include "xlast_csgo/inc_ps3_key_bindings_usr.inc"

#endif

		{ "" }
	};
#undef BINDING
#undef ACTION

	//Action Names
#define BINDING( name, cppType )
#define ACTION( name ) { #name },
	static char sBindingActionNames[][MAX_BINDING_NAME] = {
#include "xlast_csgo/inc_bindings_usr.inc"
		{ "" }
	};
#undef BINDING
#undef ACTION
	// Get the number of keybindings.
	static int sNumBindings = 0;
	if ( sNumBindings == 0 )
	{
		while ( sBindingActionNames[sNumBindings][0] != '\0') sNumBindings++;
	}

	// Sync the device specific convars as well.
#define CFG( name, scfgType, cppType ) { #name },
	static const char sDeviceSpecificSettings[][MAX_BINDING_NAME] = {
#include "xlast_csgo/inc_gameconsole_device_specific_settings_usr.inc"
#undef CFG
		{ "" }
	};
	static int sNumDeviceSpecificSettings = 0;
	if ( sNumDeviceSpecificSettings == 0 )
	{
		while ( sDeviceSpecificSettings[sNumDeviceSpecificSettings][0] != '\0') sNumDeviceSpecificSettings++;
	}

	// Only process this if it is the controller associated with this clientmodenormal
#ifndef SPLIT_SCREEN_STUBS
	int iSlot = XBX_GetSlotByUserId( iController );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );
#endif
	if ( this != GetClientModeCSNormal() )
		return;

	// get the matchmaking local player
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayerLocal )
		return;

	TitleDataFieldsDescription_t const *pFields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	if ( !pFields )
		return;

#if defined ( _X360 )

	// Check version number
	ConVarRef cl_titledataversionblock3 ( "cl_titledataversionblock3" );
	TitleDataFieldsDescription_t const *versionField = TitleDataFieldsDescriptionFindByString( pFields, "TITLEDATA.BLOCK3.VERSION" );
	if ( !versionField || versionField->m_eDataType != TitleDataFieldsDescription_t::DT_uint16 )
	{
		Warning( "ClientModeCSNormal::SyncCurrentKeyBindingsToDeviceTitleData TITLEDATA.BLOCK3.VERSION is expected to be defined as DT_uint16\n" );
		return;
	}

	if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
	{
		// If we're reading from title data, check the version to make sure it's good. If not, we bail and just use the defaults that were put into the controls earlier.
		int versionNumber = TitleDataFieldsDescriptionGetValue<uint16>( versionField, pPlayerLocal );
		if ( versionNumber != cl_titledataversionblock3.GetInt() )
		{
			Warning ( "ClientModeCSNormal::SyncCurrentKeyBindingsToDeviceTitleData wrong version # for TITLEDATA.BLOCK3.VERSION; expected %d, got %d\n", cl_titledataversionblock3.GetInt(), versionNumber );
			return;
		}
	}
	else if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
	{
		// If we're saving, just write the version out.
		TitleDataFieldsDescriptionSetValue<uint16>( versionField, pPlayerLocal, cl_titledataversionblock3.GetInt() );
	}

#endif // _X360

	char* devicePrefix = "";

#if defined( _PS3 )

	// For PS3, we have 3 different sets of controller bindings.
	// We want to work on the active controller when reading or writing them out.
	// If we're using the primary controller, then we use an empty string as the text string option.
	switch ( eDevice )
	{
		case INPUT_DEVICE_KEYBOARD_MOUSE:
		case INPUT_DEVICE_GAMEPAD:
			break;
		case INPUT_DEVICE_PLAYSTATION_MOVE:
			devicePrefix = TITLE_DATA_DEVICE_MOVE_PREFIX;
			break;
		case INPUT_DEVICE_SHARPSHOOTER:
			devicePrefix = TITLE_DATA_DEVICE_SHARP_SHOOTER_PREFIX;
			break;
	}

#endif // _PS3

	char bindingKeyName[MAX_BINDING_NAME];
	if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
	{
		for (int i=0; sJoystickNames[i][0] != '\0'; ++i)
		{
			Q_snprintf( bindingKeyName, MAX_BINDING_NAME, TITLE_DATA_PREFIX "%sBINDING.%s", devicePrefix, &sJoystickNames[i][0] );
			TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, bindingKeyName );
			if ( NULL == pField )
			{
				Warning( "Could not find TitleDataField for %s\n", bindingKeyName );
				continue;
			}

			if ( pField->m_eDataType != TitleDataFieldsDescription_t::DT_uint8  )
			{
				Warning( "%s is expected to be defined as DT_uint8\n", bindingKeyName );
				continue;
			}

			const char *pBindingName = "";
			uint8 bindingIndex = TitleDataFieldsDescriptionGetValue<uint8>( pField, pPlayerLocal );
			if ( bindingIndex > 0 && bindingIndex < sNumBindings && sBindingActionNames[bindingIndex][0] != '\0' )
			{
				pBindingName = sBindingActionNames[bindingIndex];
			}

			// If the name of the button code is prefixed with "KEY_" then this is a PS3 key binding.  Do the direct translation
			// from key name to ButtonCode_t here.
			ButtonCode_t buttonCode = g_pInputSystem->StringToButtonCode( &sJoystickNames[i][0] );
			if ( buttonCode == BUTTON_CODE_INVALID )
			{
				Warning( "ClientModeCSNormal::SyncCurrentKeyBindingsToDeviceTitleData Unknown joystick name %s\n", &sJoystickNames[i][0] );
			}
			else
			{
				engine->Key_SetBinding( buttonCode, pBindingName );
			}

		}
	}
	else if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
	{
		for (int i=0; sJoystickNames[i][0] != '\0'; ++i)
		{
			Q_snprintf( bindingKeyName, MAX_BINDING_NAME, TITLE_DATA_PREFIX "%sBINDING.%s", devicePrefix, &sJoystickNames[i][0] );

			// find the current binding for the keyname
			ButtonCode_t buttonCode = g_pInputSystem->StringToButtonCode( &sJoystickNames[i][0] );
			
			const char* keyBinding = engine->Key_BindingForKey( buttonCode );
			if ( NULL == keyBinding )
				continue;

			size_t cmpSize = strlen( keyBinding );

			// Find the keybinding index
			uint8 bindingIndex = 0;
			for ( ; keyBinding && bindingIndex < sNumBindings; ++bindingIndex )
			{
				if ( sBindingActionNames[bindingIndex][0] == '\0' )
					continue;

				if ( cmpSize != strlen( sBindingActionNames[bindingIndex] ) )
					continue;

				if ( !Q_strncmp( &sBindingActionNames[bindingIndex][0], keyBinding, cmpSize ) )
					break;
			}

			if ( bindingIndex == sNumBindings )
			{
				bindingIndex = 0;
			}

			TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, bindingKeyName );
			if ( NULL == pField )
			{
				Warning( "Could not find TitleDataField for %s\n", bindingKeyName );
				continue;
			}
			if ( pField->m_eDataType != TitleDataFieldsDescription_t::DT_uint8  )
			{
				Warning( "%s is expected to be defined as DT_uint8\n", bindingKeyName );
				continue;
			}

			// Finally write the value.
			TitleDataFieldsDescriptionSetValue<uint8>( pField, pPlayerLocal, bindingIndex );
		}
	}

	// Now sync all the device specific convars.
	for (int ii=0; ii<sNumDeviceSpecificSettings; ++ii)
	{
		CFmtStr sFieldLookup( TITLE_DATA_PREFIX "%sCFG.usr.%s", devicePrefix, sDeviceSpecificSettings[ii] );
		TitleDataFieldsDescription_t const *pField = TitleDataFieldsDescriptionFindByString( pFields, sFieldLookup );
		ConVarRef conVarRef( sDeviceSpecificSettings[ii] );
		if ( pField )
		{
			switch( pField->m_eDataType )
			{
				case TitleDataFieldsDescription_t::DT_float:
					if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
						TitleDataFieldsDescriptionSetValue<float>( pField, pPlayerLocal, conVarRef.GetFloat() );
					else if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
						conVarRef.SetValue( TitleDataFieldsDescriptionGetValue<float>( pField, pPlayerLocal ) );
					break;

				case TitleDataFieldsDescription_t::DT_uint32:
					if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
						TitleDataFieldsDescriptionSetValue<int32>( pField, pPlayerLocal, conVarRef.GetInt() );
					else if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
						conVarRef.SetValue( TitleDataFieldsDescriptionGetValue<int32>( pField, pPlayerLocal ) );
					break;

				case TitleDataFieldsDescription_t::DT_uint16:
					if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
						TitleDataFieldsDescriptionSetValue<int16>( pField, pPlayerLocal, conVarRef.GetInt() );
					else if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
						conVarRef.SetValue( TitleDataFieldsDescriptionGetValue<int16>( pField, pPlayerLocal ) );
					break;

				case TitleDataFieldsDescription_t::DT_uint8:
					if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
						TitleDataFieldsDescriptionSetValue<int8>( pField, pPlayerLocal, conVarRef.GetInt() );
					else if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
						conVarRef.SetValue( TitleDataFieldsDescriptionGetValue<int8>( pField, pPlayerLocal ) );
					break;

				case TitleDataFieldsDescription_t::DT_BITFIELD:
					if ( eOp == KEYBINDING_WRITE_TO_TITLEDATA )
						TitleDataFieldsDescriptionSetBit( pField, pPlayerLocal, conVarRef.GetBool() );
					else if ( eOp == KEYBINDING_READ_FROM_TITLEDATA )
						conVarRef.SetValue( !!TitleDataFieldsDescriptionGetBit( pField, pPlayerLocal ) );
					break;

				default:
					AssertMsg(false, "Format type not handled in device specific bindings.  Have a programmer add the case in." );
			}
		}
	}


#endif // _GAMECONSOLE

}

void ClientModeCSNormal::FireGameEvent( IGameEvent *event )
{
	if ( GetSplitScreenPlayerSlot() != event->GetInt( "splitscreenplayer" ) )
		return;

	CBaseHudChat *pHudChat = CBaseHudChat::GetHudChat();
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	CLocalPlayerFilter filter;

	// we want to i/o title profile data from menu screens (not just in game) so we bypass checking for C_BasePlayer since we know we have a matchmaking local player
	bool bIgnoreLocalPlayerCheck = false;
	if ( Q_strcmp( "read_game_titledata", event->GetName() ) ==0 || Q_strcmp( "write_game_titledata", event->GetName() ) ==0  )
		bIgnoreLocalPlayerCheck = true;
	
	if ( !bIgnoreLocalPlayerCheck && ( !pLocalPlayer || !pHudChat ) )
		return;

	const char *eventname = event->GetName();

	if ( !eventname || !eventname[0] )
		return;

	if ( Q_strcmp( "reset_player_controls", eventname ) == 0 )
	{
		engine->ExecuteClientCmd( "exec config" PLATFORM_EXT ".cfg game\n" );

#if defined( _X360 )
		engine->ExecuteClientCmd( "exec controller" PLATFORM_EXT ".cfg\n" );
#elif defined( _PS3 )
		engine->ExecuteClientCmd( VarArgs( "cl_reset_ps3_bindings %d %d", GET_ACTIVE_SPLITSCREEN_SLOT(), -1 ) );
#endif
	}
	if ( Q_strcmp( "round_start", eventname ) == 0 )
	{
		m_iRoundStatus = ROUND_STARTED;
		m_nRoundMVP = 0;
		// recreate all client side physics props
		C_PhysPropClientside::RecreateAll();

		// remove hostage ragdolls
		for ( int i=0; i<g_HostageRagdolls.Count(); ++i )
		{
			// double-check that the EHANDLE is still valid
			if ( g_HostageRagdolls[i] )
			{
				g_HostageRagdolls[i]->Remove();
			}
		}
		g_HostageRagdolls.RemoveAll();

		// Just tell engine to clear decals
		engine->ClientCmd( "r_cleardecals\n" );

		//stop any looping sounds
	//	enginesound->StopAllSounds( true );

		CBaseEntity::EmitSound( filter,  pLocalPlayer->entindex(), "Music.StopAllExceptMusic" );

		Soundscape_OnStopAllSounds();	// Tell the soundscape system.

		// Remove any left over particle effects from the last round.
		ParticleMgr()->SetRemoveAllParticleEffects();

		GlowObjectManager().UnregisterAllGlowBoxes();

		// remove any stacked up temporary effect events
		engine->ClearEvents();

#if defined ( _X360 )
		if ( !xboxsystem->IsArcadeTitleUnlocked() )
		{
			int playerInt = event->GetInt( "splitscreenplayer" );
			SplitScreenConVarRef xbox_arcade_remaining_trial_time("xbox_arcade_remaining_trial_time");
			if ( xbox_arcade_remaining_trial_time.GetFloat( playerInt ) < 0.5f )
			{
				// only process this if it is the associated player
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( playerInt );
				if ( this == GetClientModeCSNormal() )
				{
					IGameEvent *event = gameeventmanager->CreateEvent( "trial_time_expired" );
					if ( event )
					{
						event->SetInt( "slot", playerInt );
						gameeventmanager->FireEventClientSide( event );
					}
					engine->ClientCmd_Unrestricted( "disconnect" );
				}
			}
			// verify they have not pulled the MU in an attempt to get past the trial mode time restriction
			CheckTitleDataStorageConnected();
		}
#endif
	}
	if ( V_strcmp( "round_time_warning", eventname ) == 0 )
	{
		if(	!CSGameRules()->m_bBombPlanted )
		{
			PlayMusicSelection( filter, CSMUSIC_ROUNDTEN );
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer* pPlayer = ToCSPlayer(UTIL_PlayerByIndex(i));
				if( !pPlayer )
					continue;
			}	
		}
	}
	else if ( V_strcmp( "cs_round_start_beep", eventname ) == 0 )
	{
		bool bTeamPanelActive = ( GetViewPortInterface()->GetActivePanel() &&  ( V_strcmp( GetViewPortInterface()->GetActivePanel()->GetName(), PANEL_TEAM ) == 0 ) );
	
		if( !bTeamPanelActive )
		{
			CLocalPlayerFilter filter;
			CBaseEntity::EmitSound( filter, 0, "UI.CounterBeep" );
		}
	}
	else if ( V_strcmp( "cs_round_final_beep", eventname ) == 0 )
	{
		bool bTeamPanelActive = ( GetViewPortInterface()->GetActivePanel() && ( V_strcmp( GetViewPortInterface()->GetActivePanel()->GetName(), PANEL_TEAM ) == 0 ) );

		if( !bTeamPanelActive )
		{
			
			CBaseEntity::EmitSound( filter, 0, "UI.CounterDoneBeep" );
		}

		int nObsMode = pLocalPlayer->GetObserverMode();
		if( nObsMode == OBS_MODE_FIXED || nObsMode == OBS_MODE_ROAMING )
		{
			C_CSPlayer *pCSLocalPlayer = ToCSPlayer(pLocalPlayer);
			if(pCSLocalPlayer->GetCurrentMusic() == CSMUSIC_START )
			{
				CLocalPlayerFilter filter;
				PlayMusicSelection(filter, CSMUSIC_ACTION);
				pCSLocalPlayer->SetCurrentMusic(CSMUSIC_ACTION);
			}
		}
	}
	else if ( V_strcmp( "round_mvp", eventname ) == 0 )
	{
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );
		if ( pPlayer )
		{
			int nPlayerIndex = pPlayer->entindex();

			if ( C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR ) )
			{
				int nMusicID = cs_PR->GetMusicID( nPlayerIndex );
				if ( nMusicID > 1 )
				{
					m_nRoundMVP = nPlayerIndex;
					PlayMusicSelection( filter, CSMUSIC_MVP, nPlayerIndex );
				}
			}
		}
	}
	else if ( Q_strcmp( "round_end", eventname ) == 0 )
	{
		int winningTeam = event->GetInt("winner");
		int reason = event->GetInt("reason");

		if ( Target_Bombed == reason )
		{
			m_iRoundStatus = ROUND_ENDED_VIA_BOMBING;
		}
		else
		{
			m_iRoundStatus = ROUND_ENDED;
		}

		if ( reason != Game_Commencing )
		{
			// if spectating play music for team being spectated at that moment
			C_BasePlayer *pTeamPlayer = pLocalPlayer;
			if( pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR || pLocalPlayer->IsHLTV() )
			{
					pTeamPlayer = GetHudPlayer();
			}
			if( winningTeam == pTeamPlayer->GetTeamNumber() )
			{
				PlayMusicSelection( filter, CSMUSIC_WONROUND );
			}
			else
			{
				PlayMusicSelection( filter, CSMUSIC_LOSTROUND );
			}
		}
		// play endround announcer sound
		if ( winningTeam == TEAM_CT )
		{
			if ( reason == Bomb_Defused )
			{
				int nAnnouncementLine = 0;
				static char const * const s_arrAnnouncementLines[] = { "Event.BombDefused",
					"Event.BombDefused_Legacy1", "Event.BombDefused_Legacy2", "Event.BombDefused_Legacy3" };
				if ( pLocalPlayer && ( pLocalPlayer->GetTeamNumber() == TEAM_CT ) )
				{
					int nLegacyBragLine = event->GetInt( "legacy" );
					if ( ( nLegacyBragLine >= 1 ) && ( nLegacyBragLine <= 3 ) )
						nAnnouncementLine = nLegacyBragLine;
				}
				
				// Play "Bomb has been defused" announcement
				C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, s_arrAnnouncementLines[nAnnouncementLine]);
				// Queue up the CT Win audio to play after the bomb defused audio completes
				m_fDelayedCTWinTime = gpGlobals->curtime + C_BaseEntity::GetSoundDuration( s_arrAnnouncementLines[nAnnouncementLine], NULL) + ( nAnnouncementLine ? 0.7 : 0.3 );
			}
			else if ( reason == All_Hostages_Rescued )
			{
				// Queue up the CT Win audio to play after the hostage rescue audio completes
				m_fDelayedCTWinTime = gpGlobals->curtime + C_BaseEntity::GetSoundDuration( "Event.HostageRescued", NULL );
			}
			else
			{
				C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.CTWin");
			}
		}
		else if ( winningTeam == TEAM_TERRORIST )
		{
			if ( reason != Terrorists_Planted )
				C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.TERWin");
		}
		else
		{
			if ( reason != Game_Commencing )
			{
				// Prevent the round draw sound event from playing when the player spawns into an empty game
				C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.RoundDraw");
			}
		}
		
		// [pfreese] Only show centerprint message for game commencing; the rest of 
		// these messages are handled by the end-of-round panel.
		// [Forrest] Show all centerprint messages if the end-of-round panel is disabled.
		static ConVarRef sv_nowinpanel( "sv_nowinpanel" );
		bool isFinishedGunGameRound = CSGameRules()->IsPlayingGunGameProgressive() && (reason == CTs_Win || reason == Terrorists_Win );
		if ( isFinishedGunGameRound || reason == Game_Commencing || sv_nowinpanel.GetBool() )
		{
			// dont show centerprint message anymore

			// we handle this info int he win panel and this info is distracting and redundant
			// GetCenterPrint()->Print( hudtextmessage->LookupString( event->GetString("message") ) );

			// we are starting a new round; store old stats and clear the current match stats
			g_CSClientGameStats.UpdateLastMatchStats();
			g_CSClientGameStats.ResetMatchStats();
		}

		// [jason] Reset the round stats for leaderboards now
		if ( reason == Game_Commencing )
		{
			g_CSClientGameStats.ResetRoundStats();
		}
	}
	else if ( Q_strcmp( "player_team", eventname ) == 0 )
	{
		CBaseHudChat *pHudChat = CBaseHudChat::GetHudChat();
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );
		
		if ( !pHudChat )
			return;

		if ( !pPlayer )
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if ( bDisconnected )
			return;

		int iTeam = event->GetInt("team");

		if ( C_BasePlayer::IsLocalPlayer( pPlayer ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( pPlayer );
			// that's me
			pPlayer->TeamChange( iTeam );
		}

		bool bSilent = event->GetBool( "silent" );

		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() && pPlayer->IsBot() )
			bSilent = true;

		if ( !bSilent )
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			char szLocalized[100];
			bool bIsBot = !!event->GetInt("isbot"); // squelch 'bot has joined the game' messages

			if ( iTeam == TEAM_SPECTATOR && !bIsBot )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#Cstrike_game_join_spectators" ), 1, wszPlayerName );

				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );
				pHudChat->Printf( CHAT_FILTER_NONE, "%s", szLocalized );
			}
			else if ( iTeam == TEAM_TERRORIST && !bIsBot )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#Cstrike_game_join_terrorist" ), 1, wszPlayerName );

				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );
				pHudChat->Printf( CHAT_FILTER_NONE, "%s", szLocalized );
			}
			else if ( iTeam == TEAM_CT && !bIsBot )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#Cstrike_game_join_ct" ), 1, wszPlayerName );

				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );
				pHudChat->Printf( CHAT_FILTER_NONE, "%s", szLocalized );
			}
		}
	}

	else if ( Q_strcmp( "bomb_planted", eventname ) == 0 )
	{
		// show centerprint message when not in training
		if ( !CSGameRules()->IsPlayingTraining() && !CSGameRules()->IsPlayingCooperativeGametype() )
		{
			wchar_t wszLocalized[100];
			wchar_t seconds[4];

			V_swprintf_safe( seconds, L"%d", mp_c4timer.GetInt() );

			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#Cstrike_TitlesTXT_Bomb_Planted" ), 1, seconds );

			GetCenterPrint()->Print( wszLocalized );

			PlayMusicSelection( filter, CSMUSIC_BOMB );
		}

		// play sound
		 C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.BombPlanted" );
		
	}
// 	else if ( Q_strcmp( "bomb_defused", eventname ) == 0 )
// 	{
// 		if ( !CSGameRules()->IsPlayingTraining() )
// 		{
// 			C_CSPlayer *pHudPlayer = GetHudPlayer();
// 			C_CSPlayer *pCSLocalPlayer = ToCSPlayer(pLocalPlayer);
// 
// 			// music logic says: if mvp has music pack, play that. else, if spectating play win/lose and pack of hudplayer.. else play local win/lose and pack.
// 			C_CSPlayer *pMusicPlayer = pCSLocalPlayer ? pCSLocalPlayer : pHudPlayer;
// 			if( pCSLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR || pCSLocalPlayer->IsHLTV() )
// 			{
// 				pMusicPlayer = pHudPlayer;
// 			}
// 
// 			if ( pMusicPlayer->GetTeamNumber() == TEAM_CT)
// 			{
// 				PlayMusicSelection( filter, CSMUSIC_WONROUND );	
// 			}
// 			else
// 			{
// 				PlayMusicSelection( filter, CSMUSIC_LOSTROUND );
// 			}
// 		}
// 	}

	// [menglish] Tell the client side bomb that the bomb has exploding here creating the explosion particle effect
	else if ( Q_strcmp( "bomb_exploded", eventname ) == 0 )
	{
		if ( g_PlantedC4s.Count() > 0 )
		{
			// bomb is planted
			C_PlantedC4 *pC4 = g_PlantedC4s[0];
			pC4->Explode();
		}

		//pLocalPlayer->EmitSound( "Music.StopAllMusic" );

	}

	else if ( Q_strcmp( "hostage_follows", eventname ) == 0 )
	{
		// show centerprint message when not in training
		if ( !CSGameRules()->IsPlayingTraining() )
		{
			GetCenterPrint()->Print( "#Cstrike_TitlesTXT_Hostage_Being_Taken" );

			bool roundWasAlreadyWon = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
			if ( !roundWasAlreadyWon )
			{
				PlayMusicSelection( filter, CSMUSIC_HOSTAGE );
			}
		}
	}

	else if ( Q_strcmp( "hostage_killed", eventname ) == 0 )
	{
		// play sound for spectators and CTs
		if ( pLocalPlayer->IsObserver() || (pLocalPlayer->GetTeamNumber() == TEAM_CT) )
		{
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Event.HostageKilled")  ;
		}

		// Show warning to killer
		if ( pLocalPlayer->GetUserID() == event->GetInt("userid") )
		{
			GetCenterPrint()->Print( "#Cstrike_TitlesTXT_Killed_Hostage" );
		}
	}

	else if ( Q_strcmp( "hostage_hurt", eventname ) == 0 )
	{
		// Let the loacl player know he harmed a hostage
		if ( pLocalPlayer->GetUserID() == event->GetInt("userid") )
		{
			GetCenterPrint()->Print( "#Cstrike_TitlesTXT_Injured_Hostage" );
		}
	}

	else if ( Q_strcmp( "player_death", eventname ) == 0 )
	{
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );

		C_CSPlayer* csPlayer = ToCSPlayer(pPlayer);
		if (csPlayer)
		{
			csPlayer->ClearSoundEvents();
		}

		if ( pPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			// we just died, hide any buy panels
			CSGameRules()->CloseBuyMenu( pLocalPlayer->GetUserID() );
		}
	}

	else if ( Q_strcmp( "player_changename", eventname ) == 0 )
	{
		return; // server sends a colorized text string for this
	}

	else if ( Q_strcmp( "seasoncoin_levelup", eventname ) == 0 )
	{
		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		int iPlayerIndex = event->GetInt( "player" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		//int iCategory = event->GetInt( "category" );
		int iRank = event->GetInt( "rank" );

		if ( !hudChat || !pPlayer )
			return;

		if ( C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR ) )
		{
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
			cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );

			wchar_t wszLocalizedString[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
			if ( iRank == MEDAL_RANK_SILVER )
				g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find( "#SEASONX_Coin_LevelUp_Silver" ), 1, wszPlayerName );
			else if ( iRank == MEDAL_RANK_GOLD )
				g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find( "#SEASONX_Coin_LevelUp_Gold" ), 1, wszPlayerName );

			if ( wszLocalizedString[0] )
			{
				hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
			}
		}
	}

	// [tj] We handle this here instead of in the base class 
	//      The reason is that we don't use string tables to localize.
	//      Instead, we use the steam localization mechanism.
	else if ( Q_strcmp( "achievement_earned", eventname ) == 0 )
	{
		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		int iPlayerIndex = event->GetInt( "player" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		int iAchievement = event->GetInt( "achievement" );

		if ( !hudChat || !pPlayer )
			return;

		if ( !g_AchievementMgrCS.CheckAchievementsEnabled() )
			return;

		IAchievement *pAchievement = g_AchievementMgrCS.GetAchievementByID( iAchievement, GetSplitScreenPlayerSlot() );
		if ( pAchievement )
		{
			if ( pPlayer->ShouldAnnounceAchievement() || C_BasePlayer::IsLocalPlayer( pPlayer ) )
			{
				pPlayer->SetNextAchievementAnnounceTime( gpGlobals->curtime + ACHIEVEMENT_ANNOUNCEMENT_MIN_TIME );

				//Do something for the player - Actually we should probably do this client-side when the achievement is first earned.
				if (pPlayer->IsLocalPlayer()) 
				{
				}
				pPlayer->OnAchievementAchieved( iAchievement );

				if ( C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR ) )
				{
					wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
					cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), (EDecoratedPlayerNameFlag_t)( k_EDecoratedPlayerNameFlag_DontMakeStringSafe |k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );


					wchar_t achievementName[1024];
					const wchar_t* constAchievementName = &achievementName[0];

					constAchievementName = ACHIEVEMENT_LOCALIZED_NAME( pAchievement );

					if (constAchievementName)
					{
						wchar_t wszLocalizedString[2 * MAX_DECORATED_PLAYER_NAME_LENGTH];
						g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find( "#Achievement_Earned" ), 2, wszPlayerName, constAchievementName );

						hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
					}
				}
			}
		}
	}
	else if ( Q_strcmp( "item_found", eventname ) == 0 )
	{
		//
		// LEGACY game event handler to correctly print items uncased in demos before May 2014
		//

		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		int iPlayerIndex = event->GetInt( "player" );
//		int iItemRarity = event->GetInt( "quality" );
		int iMethod = event->GetInt( "method" );
		int iItemDef = event->GetInt( "itemdef" );
		uint64 itemID = event->GetInt( "itemid", -1 );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		const GameItemDefinition_t *pItemDefinition = dynamic_cast<const GameItemDefinition_t *>( GetItemSchema()->GetItemDefinition( iItemDef ) );

		// White list of item acquisitions to print in chat.
 		if ( ( iMethod != ( UNACK_ITEM_FOUND_IN_CRATE - 1 ) ) && 
			 ( iMethod != ( UNACK_ITEM_PURCHASED - 1 ) ) &&
			 ( iMethod != ( UNACK_ITEM_TRADED - 1 ) ) &&
			 ( iMethod != ( UNACK_ITEM_CRAFTED - 1 ) ) &&
			 ( iMethod != ( UNACK_ITEM_GIFTED - 1 ) ) )
			return;

		if ( !pPlayer || !pItemDefinition )
			return;

		if ( C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR ) )
		{
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
			cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );

			if ( iMethod < 0 || iMethod >= ARRAYSIZE( g_pszItemFoundMethodStrings ) )
			{
				iMethod = 0;
			}

			const char *pszLocString = g_pszItemFoundMethodStrings[iMethod];
			wchar_t const *wszItemFound = pszLocString ? g_pVGuiLocalize->Find( pszLocString ) : NULL;
			if ( wszItemFound )
			{
				// TODO: Update the localization strings to only have two format parameters since that's all we need.
				
				CSteamID steamID;
				wchar_t wszLocalizedString[2 * MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
				bool bUsingFullName = false;
				if ( itemID != -1 && pPlayer->GetSteamID( &steamID ) )
				{
					CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetInventoryForPlayer( steamID );
					if ( pPlayerInv )
					{
						CEconItemView* pEconItem = pPlayerInv->GetInventoryItemByItemID( itemID );
						if ( pEconItem )
						{
							int nRarity = pEconItem->GetRarity() - 1;

							wchar_t wszItemName[256];
							_snwprintf( wszItemName, ARRAYSIZE( wszItemName ), L"%c" PRI_WS_FOR_WS L"\01", nRarity + COLOR_RARITY_FIRST, pEconItem->GetItemName() );

							g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszItemFound, 3, wszPlayerName, wszItemName, L"" );
							bUsingFullName = true;
						}
					}
				}
				
				if ( !bUsingFullName )
					g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszItemFound, 3, wszPlayerName, g_pVGuiLocalize->Find( pItemDefinition->GetItemBaseName() ), L"" );

				if ( wszLocalizedString[0] )
				{
					hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
				}
			}
		}
	}
	else if ( Q_strcmp( "items_gifted", eventname ) == 0 )
	{
		if ( event->GetInt( "giftidx" ) == 0 )
		{	// PUBLIC GIFT ANNOUNCEMENT -- print info when event comes about the first gift in the batch
			CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
			int iPlayerIndex = event->GetInt( "player" );
			int iItemDef = event->GetInt( "itemdef" );
			int numGifts = event->GetInt( "numgifts" );
			C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
			const GameItemDefinition_t *pItemDefinition = dynamic_cast< const GameItemDefinition_t* >( GetItemSchema()->GetItemDefinition( iItemDef ) );

			if ( !pPlayer || !pItemDefinition )
				return;

			if ( C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR ) )
			{	// [VITALIY] has given out a gift! ///OR/// [VITALIY] has given out 25 gifts!
				char const *fmtMessageLoc = ( numGifts > 1 ) ? "#Item_GiftsSentMany" : "#Item_GiftsSent1Anon";
				wchar_t wszArg2[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
				V_snwprintf( wszArg2, Q_ARRAYSIZE( wszArg2 ), L"%d", numGifts );

				wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
				cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );

				if ( wchar_t const *wszLocToken = g_pVGuiLocalize->Find( fmtMessageLoc ) )
				{
					wchar_t wszLocalizedString[2 * MAX_DECORATED_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszLocToken, 2, wszPlayerName, wszArg2 );

					hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
				}
			}
		}
		
		// print gift details if a local player is one of the players involved in the gift exchange
		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		int iPlayerIndex = event->GetInt( "player" );
		int iItemDef = event->GetInt( "itemdef" );
		uint32 unAccountID = event->GetInt( "accountid" );
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		const GameItemDefinition_t *pItemDefinition = dynamic_cast< const GameItemDefinition_t* >( GetItemSchema()->GetItemDefinition( iItemDef ) );
		C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

		if ( !pPlayer || !pItemDefinition || !pLocalPlayer || !cs_PR )
			return;

		if ( pLocalPlayer == pPlayer )
		{
			// Local player is the gifter!
			CSteamID steamID;
			CBasePlayer *pFoundRecipient = NULL;
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				C_BasePlayer *pPlayer = ToBasePlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer == NULL )
					continue;

				if ( pPlayer->GetSteamID( &steamID ) == false )
					continue;

				if ( steamID.GetAccountID() == unAccountID )
				{
					pFoundRecipient = pPlayer;
					break;
				}
			}

			// Print who got my gift? (if the person is in my game server, as opposed to spectating via twitch.tv/GOTV)
			if ( pFoundRecipient )
			{
				wchar_t wszArg2[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
				cs_PR->GetDecoratedPlayerName( pFoundRecipient->entindex(), wszArg2, sizeof( wszArg2 ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );
				
				char const *fmtMessageLoc = "#Item_GiftsYouSentGift";
				if ( wchar_t const *wszLocToken = g_pVGuiLocalize->Find( fmtMessageLoc ) )
				{
					wchar_t wszLocalizedString[2 * MAX_DECORATED_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszLocToken, 1, wszArg2 );

					hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
				}
			}
		}
		else if ( unAccountID == steamapicontext->SteamUser()->GetSteamID().GetAccountID() )
		{
			// Local player got a gift!
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH] = {};
			cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );

			char const *fmtMessageLoc = "#Item_GiftsYouGotGift";
			if ( wchar_t const *wszLocToken = g_pVGuiLocalize->Find( fmtMessageLoc ) )
			{
				wchar_t wszLocalizedString[2 * MAX_DECORATED_PLAYER_NAME_LENGTH];
				g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszLocToken, 1, wszPlayerName );

				hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );

				// Play a sound for the local player
				// CLocalPlayerFilter filter;
				// C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "UI.ContractSeal" );
			}
		}
	}
	else if ( Q_strcmp( "write_game_titledata", eventname ) == 0 )
	{
#if !defined( _PS3 )
		SyncCurrentKeyBindingsToDeviceTitleData( event->GetInt( "controllerId" ), INPUT_DEVICE_NONE, KEYBINDING_WRITE_TO_TITLEDATA );
#endif
	}
	else if ( Q_strcmp( "read_game_titledata", eventname ) == 0 )
	{
#if !defined( _PS3 )
		SyncCurrentKeyBindingsToDeviceTitleData( event->GetInt( "controllerId" ), INPUT_DEVICE_NONE, KEYBINDING_READ_FROM_TITLEDATA );
#endif
	}
	else if ( V_strcmp( "switch_team", eventname ) == 0 )
	{
		// Someone in the game joined or changed teams.  Notify matchmaking that it needs to update any
		// properties based on team distribution.
		if ( g_pMatchFramework && g_PR )
		{
			int numPlayers = event->GetInt( "numPlayers" );
			int numSpectators = event->GetInt( "numSpectators" );
			int avgRank = event->GetInt( "timeout" );
			int numTSlotsFree = event->GetInt( "numTSlotsFree" );
			int numCTSlotsFree = event->GetInt( "numCTSlotsFree" );

			KeyValues *pTeamProperties = new KeyValues( "switch_team" );
			KeyValues::AutoDelete autoDeleteEvent( pTeamProperties );

			pTeamProperties->SetInt( "members/numPlayers", numPlayers );
			pTeamProperties->SetInt( "members/numSpectators", numSpectators );
			pTeamProperties->SetInt( "members/timeout", avgRank );
			pTeamProperties->SetInt( "members/numTSlotsFree", numTSlotsFree );
			pTeamProperties->SetInt( "members/numCTSlotsFree", numCTSlotsFree );

			g_pMatchFramework->UpdateTeamProperties( pTeamProperties );
		}
	}
	else
	{
		BaseClass::FireGameEvent( event );
	}
}

bool __MsgFunc_SendPlayerItemFound( const CCSUsrMsg_SendPlayerItemFound &msg )
{
	CBaseHudChat *hudChat = ( CBaseHudChat * ) GET_HUDELEMENT( CHudChat );
	
	int iPlayerIndex = msg.entindex();

	int iMethod = GetUnacknowledgedReason( msg.iteminfo().inventory() ) - 1;
	C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );

	// White list of item acquisitions to print in chat.
	if ( ( iMethod != ( UNACK_ITEM_FOUND_IN_CRATE - 1 ) ) &&
		( iMethod != ( UNACK_ITEM_PURCHASED - 1 ) ) &&
		( iMethod != ( UNACK_ITEM_TRADED - 1 ) ) &&
		( iMethod != ( UNACK_ITEM_CRAFTED - 1 ) ) &&
		( iMethod != ( UNACK_ITEM_GIFTED - 1 ) ) )
		return true;

	if ( !pPlayer )
		return true;

	if ( C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR ) )
	{
		wchar_t wszPlayerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
		cs_PR->GetDecoratedPlayerName( iPlayerIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_DontMakeStringSafe );

		if ( iMethod < 0 || iMethod >= ARRAYSIZE( g_pszItemFoundMethodStrings ) )
		{
			iMethod = 0;
		}

		const char *pszLocString = g_pszItemFoundMethodStrings[ iMethod ];
		wchar_t *wszItemFound = pszLocString ? g_pVGuiLocalize->Find( pszLocString ) : NULL;
		if ( wszItemFound )
		{
			wchar_t wszLocalizedString[ 2 * MAX_DECORATED_PLAYER_NAME_LENGTH ] = {};
			if ( wszLocalizedString[0] )
			{
				hudChat->ChatPrintfW( iPlayerIndex, CHAT_FILTER_ACHIEVEMENT, wszLocalizedString );
			}
		}
	}

	return true;
}

void ClientModeCSNormal::OnEvent( KeyValues *pEvent )
{
	const char *pEventName = pEvent->GetName();

	if ( 0 == Q_strcmp( pEventName, "OnSysStorageDevicesChanged" ) )
	{
		CheckTitleDataStorageConnected();
	}
}


#if defined ( _X360 )
CON_COMMAND_F( boot_to_start_and_reset_config, "Reset the profile for the indexed player and then go to the start screen", FCVAR_DEVELOPMENTONLY )
{
	if ( args.ArgC() <= 1 || args.ArgC() > 2)
	{
		ConMsg( "Usage:  boot_to_start_and_reset_config: <playerIndex>\n" );
		return;
	}

	if ( args.ArgC() == 2 )
	{
		int slot = atoi( args[1] );

		engine->ClientCmd_Unrestricted( VarArgs( "host_reset_config %d", slot ) );
		engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", slot ) );

		engine->ClientCmd_Unrestricted( "disconnect" );
		BasePanel()->SetForceStartScreen();
		BasePanel()->HandleOpenCreateStartScreen();
	}
}

CON_COMMAND_F( boot_to_start, "Go to the start screen", FCVAR_DEVELOPMENTONLY )
{
	engine->ClientCmd_Unrestricted( "disconnect" );
	BasePanel()->SetForceStartScreen();
	BasePanel()->HandleOpenCreateStartScreen();
}
#endif

void ClientModeCSNormal::CheckTitleDataStorageConnected( void )
{
#if defined ( _X360 )
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	if ( this != GetClientModeCSNormal() )
		return;
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
	if ( pPlayer )
	{
		if ( !pPlayer->IsTitleDataStorageConnected() )
		{
			if ( !xboxsystem->IsArcadeTitleUnlocked() )
			{
				// Here we know that you have pulled the storage unit that we need to save your progress; since you are in trial mode, we boot you back to the start screen
				// Need to inform player that they must have a storage device connected to their profile in order to play in trial mode and hence they got kicked

				int iSlot = XBX_GetSlotByUserId( XBX_GetActiveUserId() );

				ECommandMsgBoxSlot slot = CMB_SLOT_FULL_SCREEN;
				if ( GameUI().IsInLevel() )
				{
					if ( iSlot == 0 )
					{
						slot = CMB_SLOT_PLAYER_0;
					}
					else
					{
						slot = CMB_SLOT_PLAYER_1;
					}
				}

				GameUI().CreateCommandMsgBoxInSlot( 
					slot, 
					"#SFUI_TrialMUPullTitle", 
					"#SFUI_TrialMUPullMsg", 
					true, 
					false, 
					"boot_to_start", 
					NULL, 
					NULL, 
					NULL );

			}
			else
			{
				// Here we know that you have pulled the storage unit that we need to save your progress; we need to inform user they wont be able to save stats/etc
				// until they reattach the storage unit
			}
		}
	}
#endif
}


void RemoveClassImageEntity()
{
	C_BaseAnimating *pEnt = g_ClassImagePlayer.Get();
	if ( pEnt )
	{
		pEnt->Remove();
		g_ClassImagePlayer = NULL;
	}

	pEnt = g_ClassImageWeapon.Get();
	if ( pEnt )
	{
		pEnt->Remove();
		g_ClassImagePlayer = NULL;
	}
}


bool ShouldRecreateClassImageEntity( C_BaseAnimating *pEnt, const char *pNewModelName )
{
	if ( !pNewModelName || !pNewModelName[0] )
		return false;

	if ( !pEnt )
		return true;

	const model_t *pModel = pEnt->GetModel();

	if ( !pModel )
		return true;

	const char *pName = modelinfo->GetModelName( pModel );
	if ( !pName )
		return true;

	// reload only if names are different
	const char *pNameNoPath = V_UnqualifiedFileName( pName );
	const char *pNewModelNameNoPath = V_UnqualifiedFileName( pNewModelName );
	return( Q_stricmp( pNameNoPath, pNewModelNameNoPath ) != 0 );
}


#if !defined( CSTRIKE15 )

// Draw a doll of a player character for the team selection VGUI screen.
static void UpdateClassImageEntity( 
	const char *pModelName,
	int x, int y, int width, int height )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	
	if ( !pLocalPlayer )
		return;

	MDLCACHE_CRITICAL_SECTION();

	const char *pWeaponName = "models/weapons/w_rif_ak47.mdl";
	const char *pWeaponSequence = "Walk_Upper_AK";

	int i;
	for ( i=0; i<CTPlayerModels.Count(); ++i )
	{
		if ( Q_strcasecmp( pModelName, CTPlayerModels[i] ) == 0 )
		{
			// give CTs a M4
			pWeaponName = "models/weapons/w_rif_m4a1.mdl";
			pWeaponSequence = "Walk_Upper_M4";
			break;
		}
	}

	if ( pLocalPlayer->IsAlive() && pLocalPlayer->GetActiveWeapon() )
	{
		C_WeaponCSBase *weapon = dynamic_cast< C_WeaponCSBase * >(pLocalPlayer->GetActiveWeapon());
		if ( weapon )
		{
			pWeaponName = weapon->GetWorldModel();
			pWeaponSequence = VarArgs("Walk_Upper_%s", weapon->GetPlayerAnimationExtension());
		}
	}

	C_BaseAnimatingOverlay *pPlayerModel = g_ClassImagePlayer.Get();

	// Does the entity even exist yet?
	bool recreatePlayer = ShouldRecreateClassImageEntity( pPlayerModel, pModelName );
	if ( recreatePlayer )
	{
		if ( pPlayerModel )
			pPlayerModel->Remove();

		pPlayerModel = new C_BaseAnimatingOverlay;
		pPlayerModel->InitializeAsClientEntity( pModelName, false );
		pPlayerModel->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally

		// let player walk ahead
		pPlayerModel->SetSequence( pPlayerModel->LookupSequence( "walk_lower" ) );
		pPlayerModel->SetPoseParameter( "move_yaw", 0.0f ); // move_yaw
		pPlayerModel->SetPoseParameter( "body_pitch", 10.0f ); // body_pitch, look down a bit
		pPlayerModel->SetPoseParameter( "body_yaw", 0.0f ); // body_yaw
		pPlayerModel->SetPoseParameter( "move_y", 0.0f ); // move_y
		pPlayerModel->SetPoseParameter( "move_x", 1.0f ); // move_x, walk forward
		pPlayerModel->m_flAnimTime = gpGlobals->curtime;

		g_ClassImagePlayer = pPlayerModel;
	}

	C_BaseAnimating *pWeaponModel = g_ClassImageWeapon.Get();

	// Does the entity even exist yet?
	if ( recreatePlayer || ShouldRecreateClassImageEntity( pWeaponModel, pWeaponName ) )
	{
		if ( pWeaponModel )
			pWeaponModel->Remove();

		pWeaponModel = new C_BaseAnimating;
		pWeaponModel->InitializeAsClientEntity( pWeaponName, false );
		pWeaponModel->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally
		pWeaponModel->FollowEntity( pPlayerModel ); // attach to player model
		pWeaponModel->m_flAnimTime = gpGlobals->curtime;
		g_ClassImageWeapon = pWeaponModel;
	}

	Vector origin = pLocalPlayer->EyePosition();
	Vector lightOrigin = origin;

	// find a spot inside the world for the dlight's origin, or it won't illuminate the model
	Vector testPos( origin.x - 100, origin.y, origin.z + 100 );
	trace_t tr;
	UTIL_TraceLine( origin, testPos, MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0f )
	{
		lightOrigin = tr.endpos;
	}
	else
	{
		// Now move the model away so we get the correct illumination
		lightOrigin = tr.endpos + Vector( 1, 0, -1 );	// pull out from the solid
		Vector start = lightOrigin;
		Vector end = lightOrigin + Vector( 100, 0, -100 );
		UTIL_TraceLine( start, end, MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
		origin = tr.endpos;
	}

	// move player model in front of our view
	pPlayerModel->SetAbsOrigin( origin );
	pPlayerModel->SetAbsAngles( QAngle( 0, 210, 0 ) );

	// wacky hacky, set upper body animation
	pPlayerModel->m_SequenceTransitioner.CheckForSequenceChange( 
		pPlayerModel->GetModelPtr(),
		pPlayerModel->LookupSequence( "walk_lower" ),
		false,
		true
	);
	pPlayerModel->m_SequenceTransitioner.UpdateCurrent( 
		pPlayerModel->GetModelPtr(),
		pPlayerModel->LookupSequence( "walk_lower" ),
		pPlayerModel->GetCycle(),
		pPlayerModel->GetPlaybackRate(),
		gpGlobals->realtime
	);

	// Now, blend the lower and upper (aim) anims together
	pPlayerModel->SetNumAnimOverlays( 2 );
	int numOverlays = pPlayerModel->GetNumAnimOverlays();
	for ( i=0; i < numOverlays; ++i )
	{
		C_AnimationLayer *layer = pPlayerModel->GetAnimOverlay( i );

		layer->SetCycle( pPlayerModel->GetCycle() );
		if ( i )
			layer->SetSequence( pPlayerModel->LookupSequence( pWeaponSequence ) );
		else
			layer->SetSequence( pPlayerModel->LookupSequence( "walk_lower" ) );

		layer->SetPlaybackRate( 1.0 );
		layer->SetWeight( 1.0f );
		layer->SetOrder( i );
	}

	pPlayerModel->FrameAdvance( gpGlobals->frametime );

	// Now draw it.
	CViewSetup view;
	view.x = x;
	view.y = y;
	view.width = width;
	view.height = height;

	view.m_bOrtho = false;
	view.fov = 54;

	view.origin = origin + Vector( -110, -5, -5 );

	Vector vMins, vMaxs;
	pPlayerModel->C_BaseAnimating::GetRenderBounds( vMins, vMaxs );
	view.origin.z += ( vMins.z + vMaxs.z ) * 0.55f;

	view.angles.Init();
	view.zNear = VIEW_NEARZ;
	view.zFar = 1000;

	Frustum dummyFrustum;
	CMatRenderContextPtr pRenderContext( materials );
	render->Push3DView( pRenderContext, view, 0, NULL, dummyFrustum );

	// [mhansen] We don't want to light the model in the world.  We want it to 
	// always be lit normal like even if you are standing in a dark (or green) area
	// in the world.

	pRenderContext->SetLightingOrigin( vec3_origin );

	LightDesc_t ld;
	ld.InitDirectional( Vector( 0.0f, 0.0f, -1.0f ), Vector( 1.0f, 1.0f, 0.8f ) );
	pRenderContext->SetLights( 1, &ld );

	static Vector white[6] = 
	{
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
		Vector( 0.4, 0.4, 0.4 ),
	};

	g_pStudioRender->SetAmbientLightColors( white );
	g_pStudioRender->SetLocalLights( 0, NULL );

	modelrender->SuppressEngineLighting( true );
	float color[3] = { 1.0f, 1.0f, 1.0f };
	render->SetColorModulation( color );
	render->SetBlend( 1.0f );

	RenderableInstance_t instance;
	instance.m_nAlpha = 255;
	pPlayerModel->DrawModel( STUDIO_RENDER, instance );

	if ( pWeaponModel )
	{
		pWeaponModel->DrawModel( STUDIO_RENDER, instance );
	}

	modelrender->SuppressEngineLighting( false );

	render->PopView( pRenderContext, dummyFrustum );
}

#endif // CSTRIKE15

bool WillPanelBeVisible( vgui::VPANEL hPanel )
{
	while ( hPanel )
	{
		if ( !vgui::ipanel()->IsVisible( hPanel ) )
			return false;

		hPanel = vgui::ipanel()->GetParent( hPanel );
	}
	return true;
}

void ClientModeCSNormal::PreRender(CViewSetup *pSetup)
{
	ClientModeShared::PreRender( pSetup );

	// Make sure preview decals update before the current render pass every frame (if needed of course)
	extern void UpdatePreviewDecal();
	UpdatePreviewDecal();
}

void ClientModeCSNormal::PostRenderVGui()
{
#if !defined( CSTRIKE15 )
	// If the team menu is up, then we will render the model of the character that is currently selected.
	for ( int i=0; i < g_ClassImagePanels.Count(); i++ )
	{
		CCSClassImagePanel *pPanel = g_ClassImagePanels[i];
		if ( WillPanelBeVisible( pPanel->GetVPanel() ) )
		{
			// Ok, we have a visible class image panel.
			int x, y, w, h;
			pPanel->GetBounds( x, y, w, h );
			pPanel->LocalToScreen( x, y );

			// Allow for the border.
			x += 3;
			y += 5;
			w -= 2;
			h -= 10;

			UpdateClassImageEntity( g_ClassImagePanels[i]->m_ModelName, x, y, w, h );
			return;
		}
	}
#endif // !CSTRIKE15
}

bool ClientModeCSNormal::ShouldDrawViewModel( void )
{
	C_CSPlayer *pPlayer = GetHudPlayer();
	
	if( pPlayer && pPlayer->GetFOV() != CSGameRules()->DefaultFOV() && pPlayer->m_bIsScoped )
	{
		CWeaponCSBase *pWpn = pPlayer->GetActiveCSWeapon();

		if( pWpn && pWpn->DoesHideViewModelWhenZoomed() )
		{
			return false;
		}
	}

	return BaseClass::ShouldDrawViewModel();
}


bool ClientModeCSNormal::CanRecordDemo( char *errorMsg, int length ) const
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
	if ( !player )
	{
		return true;
	}

	if ( !player->IsAlive() )
	{
		return true;
	}

	// don't start recording while flashed, as it would remove the flash
	if ( player->m_flFlashBangTime > gpGlobals->curtime )
	{
		Q_strncpy( errorMsg, "Cannot record demos while blind.", length );
		return false;
	}

	// don't start recording while smoke grenades are spewing smoke, as the existing smoke would be destroyed
	C_BaseEntityIterator it;
	C_BaseEntity *ent;
	while ( (ent = it.Next()) != NULL )
	{
		if ( Q_strcmp( ent->GetClassname(), "class C_ParticleSmokeGrenade" ) == 0 )
		{
			Q_strncpy( errorMsg, "Cannot record demos while a smoke grenade is active.", length );
			return false;
		}
	}

	return true;
}

void ClientModeCSNormal::SetBlurFade( float scale )
{
}

void ClientModeCSNormal::DoPostScreenSpaceEffects( const CViewSetup *pSetup ) 
{ 
	GlowObjectManager().RenderGlowEffects( pSetup, GetSplitScreenPlayerSlot() );
}

void ClientModeCSNormal::SetServerName( wchar_t *name )
{
	V_wcsncpy( m_pServerName, name, sizeof( m_pServerName ) );
}

void ClientModeCSNormal::SetMapName( wchar_t *name )
{
	V_wcsncpy( m_pMapName, name, sizeof( m_pMapName ) );
}

// Receive the PlayerIgnited user message and send out a clientside event for achievements to hook.
bool __MsgFunc_MatchEndConditions( const CCSUsrMsg_MatchEndConditions &msg )
{
	int iFragLimit = (int) msg.fraglimit();
	int iMaxRounds = (int) msg.mp_maxrounds();
	int iWinRounds = (int) msg.mp_winlimit();
	int iTimeLimit = (int) msg.mp_timelimit();

	IGameEvent *event = gameeventmanager->CreateEvent( "match_end_conditions" );
	if ( event )
	{
		event->SetInt( "frags", iFragLimit );
		event->SetInt( "max_rounds", iMaxRounds );
		event->SetInt( "win_rounds", iWinRounds );
		event->SetInt( "time", iTimeLimit );
		gameeventmanager->FireEventClientSide( event );
	}

	return true;
}

bool __MsgFunc_ServerRankUpdate( const CCSUsrMsg_ServerRankUpdate &msg )
{
	/* Removed for partner depot */
	return true;
}

bool __MsgFunc_ServerRankRevealAll( const CCSUsrMsg_ServerRankRevealAll &msg )
{
	/* Removed for partner depot */
	return true;
}

//-----------------------------------------------------------------------------
bool __MsgFunc_DisconnectToLobby( const CCSUsrMsg_DisconnectToLobby &msg )
{
	if ( engine->GetDemoPlaybackParameters() )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
		g_pMatchFramework->CloseSession();
	}
	else
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnEngineEndGame", "reason", "gameover" ) );
	}

	return true;
}

bool __MsgFunc_WarmupHasEnded( const CCSUsrMsg_WarmupHasEnded &msg )
{
	if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
	{
		IViewPortPanel *pTextWindow = GetViewPortInterface()->FindPanelByName( PANEL_INFO );
		if ( pTextWindow && pTextWindow->IsVisible() )
		{
			( static_cast< vgui::Frame * >( ( CTextWindow * )pTextWindow ) )->OnCommand( "okay" );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool __MsgFunc_GlowPropTurnOff( const CCSUsrMsg_GlowPropTurnOff &msg )
{
	CDynamicProp *pProp = dynamic_cast<CDynamicProp*>( ClientEntityList().GetEnt( msg.entidx() ) );
	if ( pProp )
		pProp->ForceTurnOffGlow();

	return true;
}

bool __MsgFunc_XpUpdate( const CCSUsrMsg_XpUpdate &msg )
{
	/* Removed for partner depot */
	return true;
}

CUtlMap< uint32, ClientModeCSNormal::CQuestUncommittedProgress_t, uint32, CDefLess< uint32 > > ClientModeCSNormal::sm_mapQuestProgressUncommitted;

bool __MsgFunc_QuestProgress( const CCSUsrMsg_QuestProgress &msg )
{
	uint32 uidxMap = ClientModeCSNormal::sm_mapQuestProgressUncommitted.Find( msg.quest_id() );
	if ( uidxMap == ClientModeCSNormal::sm_mapQuestProgressUncommitted.InvalidIndex() )
	{
		ClientModeCSNormal::CQuestUncommittedProgress_t questProgressDefault;
		V_memset( &questProgressDefault, 0, sizeof( questProgressDefault ) );
		uidxMap = ClientModeCSNormal::sm_mapQuestProgressUncommitted.InsertOrReplace( msg.quest_id(), questProgressDefault );
	}

	ClientModeCSNormal::CQuestUncommittedProgress_t &questProgress = ClientModeCSNormal::sm_mapQuestProgressUncommitted.Element( uidxMap );
	if ( msg.normal_points() > questProgress.m_numNormalPoints )
	{
		if ( !questProgress.m_dblNormalPointsProgressTime )
			questProgress.m_numNormalPointsProgressBaseline = questProgress.m_numNormalPoints;
		questProgress.m_dblNormalPointsProgressTime = Plat_FloatTime();
	}

	questProgress.m_bIsEventQuest = msg.is_event_quest();

	questProgress.m_numNormalPoints = msg.normal_points();

	return true;
}

ScoreLeaderboardData ClientModeCSNormal::s_ScoreLeaderboardData;

bool __MsgFunc_ScoreLeaderboardData( const CCSUsrMsg_ScoreLeaderboardData &msg )
{
	ClientModeCSNormal::s_ScoreLeaderboardData.Clear();
	if ( msg.has_data() )
		ClientModeCSNormal::s_ScoreLeaderboardData.CopyFrom( msg.data() );
	return true;
}

bool __MsgFunc_PlayerDecalDigitalSignature( const CCSUsrMsg_PlayerDecalDigitalSignature &msg )
{
	// Game server requests us to make a digital signature for the message that we requested
	if ( s_flLastDigitalSignatureTime )
	{
		uint64 uiNow = Plat_FloatTime();
		if ( uiNow == s_flLastDigitalSignatureTime )
			return true;	// ignore servers that are trying to guess the trace ID
	}
	s_flLastDigitalSignatureTime = Plat_FloatTime();

	// Find the trace ID that server is referring to
	int idx = s_mapClientDigitalSignatureRequests.Find( msg.data().trace_id() );
	if ( idx == s_mapClientDigitalSignatureRequests.InvalidIndex() )
		return true;

	PlayerSprayClientRequestData_t data = s_mapClientDigitalSignatureRequests.Element( idx );
	s_mapClientDigitalSignatureRequests.RemoveAt( idx );

	static CSteamID s_mysteamid = steamapicontext->SteamUser()->GetSteamID();

	// Ensure that creation time is within reasonable range from when it was requested
	// and all the fields match
	if ( s_mysteamid.GetAccountID() != msg.data().accountid() ) return true;
	if ( data.m_nEquipSlot != msg.data().equipslot() ) return true;
	if ( data.m_nDefIdx != msg.data().tx_defidx() ) return true;
	if ( data.m_unTintID != msg.data().tint_id() ) return true;
	if ( fabs( gpGlobals->curtime - data.m_flCreationTime ) > 10 ) return true;
	if ( fabs( gpGlobals->curtime - msg.data().creationtime() ) > 10 ) return true;

	// Ensure that the decal to spray is around where our local player is? (grace margins due to networking delays)
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	Vector vecCheck;
	if ( !pLocalPlayer ) return true;
	if ( msg.data().startpos_size() != 3 ) return true;
	vecCheck.Init( msg.data().startpos( 0 ), msg.data().startpos( 1 ), msg.data().startpos( 2 ) );
	if ( pLocalPlayer->EyePosition().DistToSqr( vecCheck ) > 128*128 ) return true;
	if ( msg.data().endpos_size() != 3 ) return true;
	vecCheck.Init( msg.data().endpos( 0 ), msg.data().endpos( 1 ), msg.data().endpos( 2 ) );
	if ( pLocalPlayer->EyePosition().DistToSqr( vecCheck ) > 256*256 ) return true;

	//
	// Validate our inventory again so that we could request charge consumption from GC
	//
	CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !pPlayerInv )
		return true;

	CEconItemView* pEconItem = pPlayerInv->GetItemInLoadout( 0, LOADOUT_POSITION_SPRAY0 + data.m_nEquipSlot );
	if ( !pEconItem || !pEconItem->IsValid() )
		return true;

	uint32 unStickerKitID = pEconItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
	if ( !unStickerKitID )
		return true;
	if ( unStickerKitID != data.m_nDefIdx )
		return true;

	static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
	uint32 unTintID = 0;
	if ( !hAttrSprayTintID || !pEconItem->FindAttribute( hAttrSprayTintID, &unTintID ) )
		unTintID = 0;
	if ( unTintID != data.m_unTintID )
		return true;

	// Record our spray into OGS:
	g_CSClientGameStats.AddClientCSGOGameEvent( k_CSClientCsgoGameEventType_SprayApplication, vecCheck, pLocalPlayer->EyeAngles(), ( uint64( unStickerKitID ) << 32 ) | uint64( unTintID ) );

#ifdef _DEBUG
	DevMsg( "Client decal signature (%llu) @(%.2f,%.2f,%.2f) T%u requested\n", cl2gc.Body().itemid(),
		msg.data().endpos( 0 ), msg.data().endpos( 1 ), msg.data().endpos( 2 ), msg.data().rtime() );
#endif

	return true;
}

class ClientJob_EMsgGCCStrike15_v2_ClientPlayerDecalSign : public GCSDK::CGCClientJob
{
public:
	explicit ClientJob_EMsgGCCStrike15_v2_ClientPlayerDecalSign( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
	{
	}

	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ClientPlayerDecalSign> msg( pNetPacket );
#ifdef _DEBUG
		DevMsg( "Client decal signature (%llu) T%u = [%u b]\n", msg.Body().itemid(),
			msg.Body().data().rtime(), msg.Body().data().signature().size() );
#endif
		if ( !msg.Body().data().signature().size() )
			return false;
		if ( !BValidateClientPlayerDecalSignature( msg.Body().data() ) )
			return false;

		CSVCMsg_UserMessage_t um;
		CCSUsrMsg_PlayerDecalDigitalSignature gameMsg;
		gameMsg.mutable_data()->CopyFrom( msg.Body().data() );
		if ( BSerializeUserMessageToSVCMSG( um, CS_UM_PlayerDecalDigitalSignature, gameMsg ) )
			engine->SendMessageToServer( &um );

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ClientPlayerDecalSign, k_EMsgGCCStrike15_v2_ClientPlayerDecalSign );

void PlayerDecalDataSendActionSprayToServer( int nSlot )
{
	CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !pPlayerInv )
		return;

	CEconItemView* pEconItem = pPlayerInv->GetItemInLoadout( 0, LOADOUT_POSITION_SPRAY0 + nSlot );
	if ( !pEconItem || !pEconItem->IsValid() )
		return;

	uint32 unStickerKitID = pEconItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
	if ( !unStickerKitID )
		return;

	static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );

	int nRandomKey = RandomInt( 2, INT_MAX/2 );
	PlayerSprayClientRequestData_t data;
	data.m_nEquipSlot = nSlot;
	data.m_nDefIdx = unStickerKitID;
	if ( !hAttrSprayTintID || !pEconItem->FindAttribute( hAttrSprayTintID, &data.m_unTintID ) )
		data.m_unTintID = 0;
	data.m_flCreationTime = gpGlobals->curtime;
	
	s_flLastDigitalSignatureTime = 0;
	s_mapClientDigitalSignatureRequests.RemoveAll();
	s_mapClientDigitalSignatureRequests.InsertOrReplace( nRandomKey, data );

	static CSteamID s_mysteamid = steamapicontext->SteamUser()->GetSteamID();

	CCSUsrMsg_PlayerDecalDigitalSignature msg;
	PlayerDecalDigitalSignature &dd = *msg.mutable_data();
	dd.set_accountid( s_mysteamid.GetAccountID() );
	dd.set_equipslot( data.m_nEquipSlot );
	dd.set_tx_defidx( data.m_nDefIdx );
	dd.set_tint_id( data.m_unTintID );
	dd.set_creationtime( data.m_flCreationTime );
	dd.set_trace_id( nRandomKey );

	CSVCMsg_UserMessage_t um;
	if ( BSerializeUserMessageToSVCMSG( um, CS_UM_PlayerDecalDigitalSignature, msg ) )
		engine->SendMessageToServer( &um );
}

CON_COMMAND_F( error_message_explain_vac, "Take user to Steam support article", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	vgui::system()->ShellExecute( "open", "https://support.steampowered.com/kb_article.php?ref=2117-ILZV-2837" );
}

CON_COMMAND_F( error_message_explain_pure, "Take user to Steam support article", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	vgui::system()->ShellExecute( "open", "https://support.steampowered.com/kb_article.php?ref=8285-YOAZ-6049" );
}