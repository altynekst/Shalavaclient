#!/usr/bin/env python3
"""
Anti-Reverse Engineering Protection Script
Applies protection to all cheat component files:
1. Adds includes (antidisasm.h, obfuscate.h, obf_constants.h)
2. Adds ANTI_DISASM_BLOCK() to key functions
3. Obfuscates constants with OBF_F() and OI()
"""

import os
import re
import sys

# Files to protect
FILES_TO_PROTECT = [
    # Aimbot
    "src/game/client/components/shalawa/aimbot/hammer.cpp",
    "src/game/client/components/shalawa/aimbot/pistol.cpp",
    "src/game/client/components/shalawa/aimbot/shotgun.cpp",
    "src/game/client/components/shalawa/aimbot/grenade.cpp",
    "src/game/client/components/shalawa/aimbot/laser.cpp",
    "src/game/client/components/shalawa/aimbot/walls.cpp",
    "src/game/client/components/shalawa/aimbot/fng.cpp",
    "src/game/client/components/shalawa/aimbot/visuals.cpp",
    # Avoid
    "src/game/client/components/shalawa/avoid/blatant.cpp",
    # Movement
    "src/game/client/components/shalawa/movement/unfreeze.cpp",
    "src/game/client/components/shalawa/movement/balance.cpp",
    "src/game/client/components/shalawa/movement/hookride.cpp",
    # Visuals
    "src/game/client/components/shalawa/visuals/chinahat.cpp",
    "src/game/client/components/shalawa/visuals/hitparticles.cpp",
    "src/game/client/components/shalawa/visuals/hud.cpp",
    # Misc
    "src/game/client/components/shalawa/misc/autoanswer.cpp",
    "src/game/client/components/shalawa/misc/chatspam.cpp",
    "src/game/client/components/shalawa/misc/cmdslikewar.cpp",
    "src/game/client/components/shalawa/misc/watermark.cpp",
    # Other
    "src/game/client/components/shalawa/dummyesconnector/dummy_manager.cpp",
    "src/game/client/components/shalawa/errorspizdelka/errorspizdelka.cpp",
    "src/game/client/components/controls.cpp",
    "src/engine/client/imgui_manager.cpp",
    "src/engine/client/imgui_manage1.cpp",
    "src/engine/client/imgui_startmenu.cpp",
    "src/game/client/components/gp4o_menu.cpp",
]

# Key functions to protect with ANTI_DISASM_BLOCK()
KEY_FUNCTIONS = [
    'Init', 'Reset', 'Tick', 'OnTick', 'OnRender', 'Process', 'Update',
    'FindClosestPlayer', 'SetCurrentTarget', 'IsPlayerValid', 'CanSeePlayer',
    'PredictPlayerPos', 'PredictPlayerPosPhysics', 'CanHammerHit', 'ShouldAutoFire',
    'FindBestHookTarget', 'EvaluateHookPoint', 'SimulateHookPath', 'ShouldActivateHook',
    'ApplyHookAim', 'ShouldActivateHammer', 'ApplyHammerAim', 'FindHammerTarget',
    'EvaluateHammerTarget', 'ShouldActivatePistol', 'ApplyPistolAim', 'FindPistolTarget',
    'EvaluatePistolTarget', 'ShouldActivateShotgun', 'ApplyShotgunAim', 'FindShotgunTarget',
    'EvaluateShotgunTarget', 'ShouldActivateGrenade', 'ApplyGrenadeAim', 'FindGrenadeTarget',
    'EvaluateGrenadeTarget', 'PredictGrenadePath', 'ShouldActivateLaser', 'ApplyLaserAim',
    'FindLaserTarget', 'EvaluateLaserTarget', 'FindWallTarget', 'EvaluateWallShot',
    'ShouldActivateWalls', 'FindFNGTarget', 'EvaluateFNGShot', 'ShouldActivateFNG',
    'ApplyFNGAim', 'RenderFov', 'RenderTargetBox', 'RenderSnapline', 'RenderPredictionDot',
    'FindUnfreezeTarget', 'EvaluateUnfreezePath', 'ShouldActivateUnfreeze', 'ApplyUnfreeze',
    'FindBalanceTarget', 'EvaluateBalancePosition', 'ShouldActivateBalance', 'ApplyBalance',
    'PredictTargetPosition', 'ShouldHook', 'IsOnHead', 'FindHookRideTarget',
    'EvaluateHookRidePath', 'ShouldActivateHookRide', 'ApplyHookRide', 'RenderChinaHat',
    'ShouldRenderChinaHat', 'RenderHitParticles', 'ShouldRenderHitParticles', 'SpawnHitParticle',
    'RenderWatermark', 'RenderArraylist', 'RenderKeybinds', 'RenderSpeedometer',
    'RenderTargetInfo', 'RenderNotifications', 'UpdateElementPositions', 'DrawBlurredShadow',
    'RenderPanel', 'RenderElement', 'SnapElement', 'RenderWatermarkAtPosition',
    'ProcessChatMessage', 'ShouldAutoAnswer', 'SendAutoAnswer', 'ShouldChatSpam',
    'SendChatSpam', 'GenerateSpamMessage', 'ProcessCommand', 'ShouldActivate', 'ExecuteCommand',
    'ShouldRenderWatermark', 'UpdateWatermarkPosition', 'UpdateDummy', 'ProcessDummyInput',
    'ShouldActivateDummy', 'SendDummyCommand', 'ProcessError', 'ShouldActivateError', 'GenerateError',
    'OnPlayerInput', 'OnDirectInput', 'OnPredictedInput', 'ProcessInput', 'UpdateControls',
    'HandleAimbotInput', 'HandleAvoidInput', 'HandleMovementInput', 'RenderMenu', 'UpdateUI',
    'ProcessUIInput', 'RenderAimbotMenu', 'RenderAvoidMenu', 'RenderVisualsMenu', 'RenderMiscMenu',
    'RenderSubMenu', 'UpdateSubUI', 'ProcessSubInput', 'RenderStartMenu', 'UpdateStartUI',
    'ProcessStartInput', 'RenderLoginScreen', 'RenderLoaderScreen', 'RenderGP4OMenu', 'UpdateGP4OUI',
    'ProcessGP4OInput', 'RenderMainTab', 'RenderAimbotTab', 'RenderVisualsTab', 'RenderMiscTab',
    'AvoidFreeze', 'FindSafeHook', 'EvaluateHookPoint', 'FindPumpHookTarget', 'DetectDrag',
    'ApplyAntiDrag', 'FindSafeDragTarget', 'SimulateMovement', 'UpdateEscapePath',
    'FindNearestDanger', 'EvaluateInput', 'ShouldUnhook', 'CheckUserHook', 'UpdatePredictedPath',
    'FindBestHookTarget', 'SafePredictFreeze', 'GetAvoidMode', 'EvaluateInputCheap', 'NeedHookPump',
    'FindSafeDirection', 'SimulateWithJump', 'CanPassThrough', 'IsNarrowPassage',
    'GetDistanceToFreeze', 'WillHitCeiling', 'FreezeReset', 'AvoidFreezeHorizontal',
]

# Constants to obfuscate
FLOAT_CONSTANTS = {
    r'\b40\.0f\b': 'OBF_F(40)',
    r'\b46\.0f\b': 'OBF_F(46)',
    r'\b100\.0f\b': 'OBF_F(100)',
    r'\b120\.0f\b': 'OBF_F(120)',
    r'\b150\.0f\b': 'OBF_F(150)',
    r'\b180\.0f\b': 'OBF_F(180)',
    r'\b200\.0f\b': 'OBF_F(200)',
    r'\b250\.0f\b': 'OBF_F(250)',
    r'\b280\.0f\b': 'OBF_F(280)',
    r'\b320\.0f\b': 'OBF_F(320)',
    r'\b350\.0f\b': 'OBF_F(350)',
    r'\b370\.0f\b': 'OBF_F(370)',
    r'\b380\.0f\b': 'OBF_F(380)',
    r'\b14\.0f\b': 'OBF_F(14)',
    r'\b19\.0f\b': 'OBF_F(19)',
    r'\b20\.0f\b': 'OBF_F(20)',
    r'\b28\.0f\b': 'OBF_F(28)',
    r'\b32\.0f\b': 'OBF_F(32)',
    r'\b3\.0f\b': 'OBF_F(3)',
    r'\b5\.0f\b': 'OBF_F(5)',
    r'\b6\.0f\b': 'OBF_F(6)',
    r'\b8\.0f\b': 'OBF_F(8)',
    r'\b10\.0f\b': 'OBF_F(10)',
    r'\b14\.0f\b': 'OBF_F(14)',
    r'\b30\.0f\b': 'OBF_F(30)',
    r'\b45\.0f\b': 'OBF_F(45)',
    r'\b60\.0f\b': 'OBF_F(60)',
    r'\b90\.0f\b': 'OBF_F(90)',
    r'\b120\.0f\b': 'OBF_F(120)',
    r'\b150\.0f\b': 'OBF_F(150)',
    r'\b180\.0f\b': 'OBF_F(180)',
    r'\b250\.0f\b': 'OBF_F(250)',
    r'\b270\.0f\b': 'OBF_F(270)',
    r'\b290\.0f\b': 'OBF_F(290)',
    r'\b320\.0f\b': 'OBF_F(320)',
    r'\b360\.0f\b': 'OBF_F(360)',
    r'\b0\.3f\b': 'OBF_F(3,10)',
    r'\b0\.5f\b': 'OFR(',
    r'\b0\.95f\b': 'OBF_F(95,100)',
    r'\b99999\.0f\b': 'OBF_F(99999)',
    r'\b9999\.0f\b': 'OBF_F(9999)',
    r'\b999\.0f\b': 'OBF_F(999)',
    r'\b10000\.0f\b': 'OBF_F(10000)',
    r'\b20000\.0f\b': 'OBF_F(20000)',
    r'\b30000\.0f\b': 'OBF_F(30000)',
    r'\b50000\.0f\b': 'OBF_F(50000)',
    r'\b100\.0f\b': 'OBF_F(100)',
    r'\b200\.0f\b': 'OBF_F(200)',
    r'\b300\.0f\b': 'OBF_F(300)',
    r'\b400\.0f\b': 'OBF_F(400)',
    r'\b500\.0f\b': 'OBF_F(500)',
    r'\b600\.0f\b': 'OBF_F(600)',
    r'\b700\.0f\b': 'OBF_F(700)',
    r'\b800\.0f\b': 'OBF_F(800)',
    r'\b900\.0f\b': 'OBF_F(900)',
    r'\b1000\.0f\b': 'OBF_F(1000)',
    r'\b1200\.0f\b': 'OBF_F(1200)',
    r'\b1500\.0f\b': 'OBF_F(1500)',
    r'\b2000\.0f\b': 'OBF_F(2000)',
    r'\b3000\.0f\b': 'OBF_F(3000)',
    r'\b5000\.0f\b': 'OBF_F(5000)',
    r'\b8000\.0f\b': 'OBF_F(8000)',
    r'\b10000\.0f\b': 'OBF_F(10000)',
    r'\b15000\.0f\b': 'OBF_F(15000)',
    r'\b22000\.0f\b': 'OBF_F(22000)',
    r'\b30000\.0f\b': 'OBF_F(30000)',
    r'\b50000\.0f\b': 'OBF_F(50000)',
    r'\b1e6f\b': 'OBF_F(1000000)',
    r'\b< 0\.001f\b': '< OBF_F(1,1000)',
    r'\b> 0\.001f\b': '> OBF_F(1,1000)',
    r'\b< 0\.0001f\b': '< OBF_F(1,10000)',
    r'\b> 0\.0001f\b': '> OBF_F(1,10000)',
    r'\b< 0\.00001f\b': '< OBF_F(1,100000)',
    r'\b> 0\.00001f\b': '> OBF_F(1,100000)',
}

INT_CONSTANTS = {
    r'\bint\s+MaxHookTicks\s*=\s*128\b': 'int MaxHookTicks = OI(128)',
    r'\bint\s+MaxHookTicks\s*=\s*64\b': 'int MaxHookTicks = OI(64)',
    r'\bint\s+MaxTicks\s*=\s*20\b': 'int MaxTicks = OI(20)',
    r'\bint\s+MaxTicks\s*=\s*30\b': 'int MaxTicks = OI(30)',
    r'\bint\s+Steps\s*=\s*20\b': 'int Steps = OI(20)',
    r'\bint\s+Steps\s*=\s*30\b': 'int Steps = OI(30)',
    r'\bint\s+Segments\s*=\s*20\b': 'int Segments = OI(20)',
    r'\bint\s+Segments\s*=\s*24\b': 'int Segments = OI(24)',
    r'\bint\s+Seg\s*=\s*16\b': 'int Seg = OI(16)',
    r'\bint\s+Seg\s*=\s*20\b': 'int Seg = OI(20)',
    r'\bfor\s*\(\s*int\s+i\s*=\s*0\b': 'for(int i = OI(0)',
    r'\bfor\s*\(\s*int\s+t\s*=\s*0\b': 'for(int t = OI(0)',
    r'\bfor\s*\(\s*int\s+T\s*=\s*0\b': 'for(int T = OI(0)',
    r'\bfor\s*\(\s*int\s+tick\s*=\s*0\b': 'for(int tick = OI(0)',
    r'\bfor\s*\(\s*int\s+Tick\s*=\s*0\b': 'for(int Tick = OI(0)',
    r'\b=\s*0\b': '= OI(0)',
    r'\b=\s*1\b(?!\d)': '= OI(1)',
    r'\b=\s*2\b(?!\d)': '= OI(2)',
    r'\b=\s*-1\b': '= -OI(1)',
    r'\b=\s*-2\b': '= -OI(2)',
    r'\b<\s*0\b(?!\d)': '< OI(0)',
    r'\b<\s*1\b(?!\d)': '< OI(1)',
    r'\b<\s*2\b(?!\d)': '< OI(2)',
    r'\b>\s*0\b(?!\d)': '> OI(0)',
    r'\b>\s*1\b(?!\d)': '> OI(1)',
    r'\b>\s*2\b(?!\d)': '> OI(2)',
    r'\b==\s*0\b(?!\d)': '== OI(0)',
    r'\b==\s*1\b(?!\d)': '== OI(1)',
    r'\b==\s*-1\b': '== -OI(1)',
    r'\b!=\s*0\b(?!\d)': '!= OI(0)',
    r'\b!=\s*1\b(?!\d)': '!= OI(1)',
    r'\b!=\s*-1\b': '!= -OI(1)',
    r'\b\+\s*1\b(?!\d)': '+ OI(1)',
    r'\b\+\s*2\b(?!\d)': '+ OI(2)',
    r'\b-\s*1\b(?!\d)': '- OI(1)',
    r'\b-\s*2\b(?!\d)': '- OI(2)',
    r'\b\*\s*1\b(?!\d)': '* OI(1)',
    r'\b\*\s*2\b(?!\d)': '* OI(2)',
    r'\b/\s*1\b(?!\d)': '/ OI(1)',
    r'\b/\s*2\b(?!\d)': '/ OI(2)',
    r'\b\+=\s*1\b(?!\d)': '+= OI(1)',
    r'\b-=\s*1\b(?!\d)': '-= OI(1)',
    r'\b\+=\s*2\b(?!\d)': '+= OI(2)',
    r'\b-=\s*2\b(?!\d)': '-= OI(2)',
    r'\b\[\s*0\s*\]': '[OI(0)]',
    r'\b\[\s*1\s*\]': '[OI(1)]',
    r'\b\[\s*2\s*\]': '[OI(2)]',
    r'\bvec2\s*\(\s*0\s*,\s*0\s*\)': 'vec2(OI(0), OI(0))',
    r'\bvec2\s*\(\s*1\s*,\s*0\s*\)': 'vec2(OI(1), OI(0))',
    r'\bvec2\s*\(\s*0\s*,\s*1\s*\)': 'vec2(OI(0), OI(1))',
    r'\bvec2\s*\(\s*0\s*,\s*-1\s*\)': 'vec2(OI(0), -OI(1))',
    r'\bTargetId\s*=\s*-1\b': 'TargetId = -OI(1)',
    r'\bBestTarget\s*=\s*-1\b': 'BestTarget = -OI(1)',
    r'\bClientId\s*<\s*0\b': 'ClientId < OI(0)',
    r'\bLocalId\s*<\s*0\b': 'LocalId < OI(0)',
    r'\b>=\s*0\b(?!\d)': '>= OI(0)',
    r'\b>=\s*1\b(?!\d)': '>= OI(1)',
    r'\b<=\s*0\b(?!\d)': '<= OI(0)',
    r'\b<=\s*1\b(?!\d)': '<= OI(1)',
    r'\b&&\s*1\b(?!\d)': '&& OI(1)',
    r'\b\|\|\s*0\b(?!\d)': '|| OI(0)',
    r'\breturn\s+0\b(?!\d)': 'return OI(0)',
    r'\breturn\s+1\b(?!\d)': 'return OI(1)',
    r'\breturn\s+-1\b': 'return -OI(1)',
    r'\breturn\s+false\b': 'return OI(0)',
    r'\breturn\s+true\b': 'return OI(1)',
}

def add_includes(content):
    """Add protection includes after existing includes"""
    includes_to_add = [
        '#include <base/antidisasm.h>',
        '#include <base/obfuscate.h>',
        '#include <base/obf_constants.h>',
    ]
    
    # Find the last #include line
    lines = content.split('\n')
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i
    
    if last_include_idx >= 0:
        # Insert new includes after the last include
        for inc in reversed(includes_to_add):
            if inc not in content:
                lines.insert(last_include_idx + 1, inc)
        
        # Add empty line after includes if not present
        if last_include_idx + len(includes_to_add) + 1 < len(lines):
            if lines[last_include_idx + len(includes_to_add) + 1].strip() != '':
                lines.insert(last_include_idx + len(includes_to_add) + 1, '')
        
        return '\n'.join(lines)
    
    return content

def add_anti_disasm(content):
    """Add ANTI_DISASM_BLOCK() to key functions"""
    for func_name in KEY_FUNCTIONS:
        # Match function definitions
        pattern = rf'((?:bool|void|int|float|vec2|const\s+\w+)\s+[\w:]+{func_name}\s*\([^)]*\)\s*(?:const)?\s*{{)\s*\n'
        replacement = r'\1\n    ANTI_DISASM_BLOCK();'
        content = re.sub(pattern, replacement, content, flags=re.MULTILINE)
    
    return content

def obfuscate_constants(content):
    """Obfuscate numeric constants"""
    # Skip if already has OF/OI calls
    if 'OBF_F(' in content or 'OI(' in content:
        return content
    
    # Obfuscate float constants
    for pattern, replacement in FLOAT_CONSTANTS.items():
        content = re.sub(pattern, replacement, content)
    
    # Obfuscate int constants (more careful)
    for pattern, replacement in INT_CONSTANTS.items():
        content = re.sub(pattern, replacement, content)
    
    return content

def protect_file(filepath):
    """Apply all protection measures to a file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Skip if already protected
        if '#include <base/antidisasm.h>' in content:
            print(f"  Skipping {filepath} (already protected)")
            return True
        
        # Apply protections
        content = add_includes(content)
        content = add_anti_disasm(content)
        content = obfuscate_constants(content)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"  Protected {filepath}")
        return True
    
    except Exception as e:
        print(f"  ERROR protecting {filepath}: {e}")
        return False

def main():
    print("Applying Anti-Reverse Engineering Protection...")
    print("=" * 60)
    
    success_count = 0
    fail_count = 0
    
    for filepath in FILES_TO_PROTECT:
        if os.path.exists(filepath):
            if protect_file(filepath):
                success_count += 1
            else:
                fail_count += 1
        else:
            print(f"  File not found: {filepath}")
            fail_count += 1
    
    print("=" * 60)
    print(f"Protection complete: {success_count} succeeded, {fail_count} failed")
    
    return 0 if fail_count == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
