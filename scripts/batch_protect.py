#!/usr/bin/env python3
"""
Batch Anti-Reverse Engineering Protection Script
Processes all remaining files efficiently
"""

import os
import re

# Files to protect with their specific patterns
FILES_CONFIG = {
    # Aimbot
    'src/game/client/components/shalawa/aimbot/grenade.cpp': {
        'functions': ['Init', 'Reset', 'Tick'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b6\.0f\b', 'OBF_F(6)'), (r'\b0\.001f\b', 'OFR(1,1000)'),
                     (r'\b50\.0f\b', 'OBF_F(50)'), (r'\b10000\.0f\b', 'OBF_F(10000)'), (r'\b< 0\b', '< OI(0)'),
                     (r'\b= 0\b', '= OI(0)'), (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b\+ 1\b', '+ OI(1)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\bvec2\(0,', 'vec2(OI(0),'), (r'\biter = 0\b', 'iter = OI(0)'),
                     (r'\biter < 3\b', 'iter < OI(3)'), (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/aimbot/laser.cpp': {
        'functions': ['Init', 'Reset', 'Tick'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b6\.0f\b', 'OBF_F(6)'), (r'\b0\.001f\b', 'OFR(1,1000)'),
                     (r'\b50\.0f\b', 'OBF_F(50)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'), (r'\b\+ 1\b', '+ OI(1)'),
                     (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'), (r'\bvec2\(0,', 'vec2(OI(0),'),
                     (r'\biter = 0\b', 'iter = OI(0)'), (r'\biter < 3\b', 'iter < OI(3)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/aimbot/walls.cpp': {
        'functions': ['Init', 'Reset', 'Tick'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b6\.0f\b', 'OBF_F(6)'), (r'\b0\.001f\b', 'OFR(1,1000)'),
                     (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b= -1\b', '= -OI(1)'),
                     (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/aimbot/fng.cpp': {
        'functions': ['Init', 'Reset', 'Tick'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b6\.0f\b', 'OBF_F(6)'), (r'\b0\.001f\b', 'OFR(1,1000)'),
                     (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b= -1\b', '= -OI(1)'),
                     (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/aimbot/visuals.cpp': {
        'functions': ['OnRender', 'RenderFov', 'RenderTargetBox', 'RenderSnapline', 'RenderPredictionDot'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b0\.001f\b', 'OFR(1,1000)'), (r'\b< 0\b', '< OI(0)'),
                     (r'\b= 0\b', '= OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)'), (r'\b360\.0f\b', 'OBF_F(360)'),
                     (r'\b180\.0f\b', 'OBF_F(180)'), (r'\b90\.0f\b', 'OBF_F(90)'), (r'\b45\.0f\b', 'OBF_F(45)')]
    },
    # Avoid
    'src/game/client/components/shalawa/avoid/blatant.cpp': {
        'functions': ['AvoidFreeze', 'FindSafeHook', 'EvaluateHookPoint', 'FindPumpHookTarget', 'DetectDrag',
                     'ApplyAntiDrag', 'FindSafeDragTarget', 'SimulateMovement', 'UpdateEscapePath',
                     'FindNearestDanger', 'EvaluateInput', 'ShouldUnhook', 'CheckUserHook', 'UpdatePredictedPath',
                     'FindBestHookTarget', 'SafePredictFreeze', 'GetAvoidMode', 'EvaluateInputCheap', 'NeedHookPump',
                     'FindSafeDirection', 'SimulateWithJump', 'CanPassThrough', 'IsNarrowPassage',
                     'GetDistanceToFreeze', 'WillHitCeiling', 'FreezeReset', 'AvoidFreezeHorizontal'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b14\.0f\b', 'OBF_F(14)'), (r'\b32\.0f\b', 'OBF_F(32)'),
                     (r'\b40\.0f\b', 'OBF_F(40)'), (r'\b46\.0f\b', 'OBF_F(46)'), (r'\b48\.0f\b', 'OBF_F(48)'),
                     (r'\b64\.0f\b', 'OBF_F(64)'), (r'\b80\.0f\b', 'OBF_F(80)'), (r'\b100\.0f\b', 'OBF_F(100)'),
                     (r'\b120\.0f\b', 'OBF_F(120)'), (r'\b150\.0f\b', 'OBF_F(150)'), (r'\b180\.0f\b', 'OBF_F(180)'),
                     (r'\b200\.0f\b', 'OBF_F(200)'), (r'\b250\.0f\b', 'OBF_F(250)'), (r'\b280\.0f\b', 'OBF_F(280)'),
                     (r'\b300\.0f\b', 'OBF_F(300)'), (r'\b320\.0f\b', 'OBF_F(320)'), (r'\b350\.0f\b', 'OBF_F(350)'),
                     (r'\b370\.0f\b', 'OBF_F(370)'), (r'\b380\.0f\b', 'OBF_F(380)'), (r'\b3\.0f\b', 'OBF_F(3)'),
                     (r'\b5\.0f\b', 'OBF_F(5)'), (r'\b6\.0f\b', 'OBF_F(6)'), (r'\b8\.0f\b', 'OBF_F(8)'),
                     (r'\b10\.0f\b', 'OBF_F(10)'), (r'\b20\.0f\b', 'OBF_F(20)'), (r'\b30\.0f\b', 'OBF_F(30)'),
                     (r'\b45\.0f\b', 'OBF_F(45)'), (r'\b60\.0f\b', 'OBF_F(60)'), (r'\b90\.0f\b', 'OBF_F(90)'),
                     (r'\b0\.3f\b', 'OFR(3,10)'), (r'\b0\.5f\b', 'OFR(5,10)'), (r'\b0\.95f\b', 'OFR(95,100)'),
                     (r'\b0\.001f\b', 'OFR(1,1000)'), (r'\b50\.0f\b', 'OBF_F(50)'), (r'\b1000\.0f\b', 'OBF_F(1000)'),
                     (r'\b1200\.0f\b', 'OBF_F(1200)'), (r'\b1500\.0f\b', 'OBF_F(1500)'), (r'\b2000\.0f\b', 'OBF_F(2000)'),
                     (r'\b3000\.0f\b', 'OBF_F(3000)'), (r'\b5000\.0f\b', 'OBF_F(5000)'), (r'\b8000\.0f\b', 'OBF_F(8000)'),
                     (r'\b10000\.0f\b', 'OBF_F(10000)'), (r'\b99999\.0f\b', 'OBF_F(99999)'), (r'\b9999\.0f\b', 'OBF_F(9999)'),
                     (r'\b999\.0f\b', 'OBF_F(999)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b= -2\b', '= -OI(2)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b== -1\b', '== -OI(1)'), (r'\b!= 0\b', '!= OI(0)'), (r'\b!= -1\b', '!= -OI(1)'),
                     (r'\b\+ 1\b', '+ OI(1)'), (r'\b\+ 2\b', '+ OI(2)'), (r'\b- 1\b', '- OI(1)'),
                     (r'\b- 2\b', '- OI(2)'), (r'\b\* 1\b', '* OI(1)'), (r'\b/ 1\b', '/ OI(1)'),
                     (r'\b\+= 1\b', '+= OI(1)'), (r'\b-= 1\b', '-= OI(1)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\bvec2\(0,', 'vec2(OI(0),'), (r'\bvec2\(1, 0\)', 'vec2(OI(1), OI(0))'),
                     (r'\bvec2\(0, -1\)', 'vec2(OI(0), -OI(1))'), (r'\bfor\(int i = 0\b', 'for(int i = OI(0)'),
                     (r'\bfor\(int tick = 0\b', 'for(int tick = OI(0)'), (r'\bfor\(int T = 0\b', 'for(int T = OI(0)'),
                     (r'\bfor\(int t = 0\b', 'for(int t = OI(0)'), (r'\bTargetId = -1\b', 'TargetId = -OI(1)'),
                     (r'\bBestTarget = -1\b', 'BestTarget = -OI(1)'), (r'\b>= 0\b', '>= OI(0)'),
                     (r'\b<= 0\b', '<= OI(0)'), (r'\b&& 1\b', '&& OI(1)'), (r'\b\|\| 0\b', '|| OI(0)'),
                     (r'\breturn 0\b', 'return OI(0)'), (r'\breturn -1\b', 'return -OI(1)'),
                     (r'\breturn false\b', 'return OI(0)'), (r'\breturn true\b', 'return OI(1)'),
                     (r'\b\[0\]', '[OI(0)]'), (r'\b\[1\]', '[OI(1)]'), (r'\b\[2\]', '[OI(2)]')]
    },
    # Movement
    'src/game/client/components/shalawa/movement/unfreeze.cpp': {
        'functions': ['OnTick', 'FindUnfreezeTarget', 'EvaluateUnfreezePath', 'ShouldActivateUnfreeze', 'ApplyUnfreeze'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/movement/balance.cpp': {
        'functions': ['OnTick', 'FindBalanceTarget', 'EvaluateBalancePosition', 'ShouldActivateBalance', 'ApplyBalance',
                       'PredictTargetPosition', 'ShouldHook', 'IsOnHead'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/movement/hookride.cpp': {
        'functions': ['OnTick', 'FindHookRideTarget', 'EvaluateHookRidePath', 'ShouldActivateHookRide', 'ApplyHookRide'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    # Visuals
    'src/game/client/components/shalawa/visuals/chinahat.cpp': {
        'functions': ['OnRender', 'RenderChinaHat', 'ShouldRenderChinaHat'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'), (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/visuals/hitparticles.cpp': {
        'functions': ['OnRender', 'RenderHitParticles', 'ShouldRenderHitParticles', 'SpawnHitParticle'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'), (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/visuals/hud.cpp': {
        'functions': ['OnRender', 'RenderWatermark', 'RenderArraylist', 'RenderKeybinds', 'RenderSpeedometer',
                     'RenderTargetInfo', 'RenderNotifications', 'UpdateElementPositions', 'DrawBlurredShadow',
                     'RenderPanel', 'RenderElement', 'SnapElement', 'RenderWatermarkAtPosition'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'), (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)'),
                     (r'\b10\.0f\b', 'OBF_F(10)'), (r'\b100\.0f\b', 'OBF_F(100)'), (r'\b200\.0f\b', 'OBF_F(200)')]
    },
    # Misc
    'src/game/client/components/shalawa/misc/autoanswer.cpp': {
        'functions': ['OnTick', 'ProcessChatMessage', 'ShouldAutoAnswer', 'SendAutoAnswer'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/misc/chatspam.cpp': {
        'functions': ['OnTick', 'ShouldChatSpam', 'SendChatSpam', 'GenerateSpamMessage'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/misc/cmdslikewar.cpp': {
        'functions': ['OnTick', 'ProcessCommand', 'ShouldActivate', 'ExecuteCommand'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/misc/watermark.cpp': {
        'functions': ['OnRender', 'RenderWatermark', 'ShouldRenderWatermark', 'UpdateWatermarkPosition'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)'), (r'\b10\.0f\b', 'OBF_F(10)')]
    },
    # Other
    'src/game/client/components/shalawa/dummyesconnector/dummy_manager.cpp': {
        'functions': ['OnTick', 'UpdateDummy', 'ProcessDummyInput', 'ShouldActivateDummy', 'SendDummyCommand'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b= -1\b', '= -OI(1)'),
                     (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/shalawa/errorspizdelka/errorspizdelka.cpp': {
        'functions': ['OnTick', 'ProcessError', 'ShouldActivate', 'GenerateError'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/controls.cpp': {
        'functions': ['OnPlayerInput', 'OnDirectInput', 'OnPredictedInput', 'ProcessInput', 'UpdateControls',
                     'HandleAimbotInput', 'HandleAvoidInput', 'HandleMovementInput'],
        'constants': [(r'\b28\.0f\b', 'OBF_F(28)'), (r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'),
                     (r'\b= -1\b', '= -OI(1)'), (r'\b== 0\b', '== OI(0)'), (r'\bvec2\(0, 0\)', 'vec2(OI(0), OI(0))'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/engine/client/imgui_manager.cpp': {
        'functions': ['OnRender', 'RenderMenu', 'UpdateUI', 'ProcessUIInput', 'RenderAimbotMenu', 'RenderAvoidMenu',
                     'RenderVisualsMenu', 'RenderMiscMenu'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/engine/client/imgui_manage1.cpp': {
        'functions': ['OnRender', 'RenderSubMenu', 'UpdateSubUI', 'ProcessSubInput'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/engine/client/imgui_startmenu.cpp': {
        'functions': ['OnRender', 'RenderStartMenu', 'UpdateStartUI', 'ProcessStartInput', 'RenderLoginScreen', 'RenderLoaderScreen'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
    'src/game/client/components/gp4o_menu.cpp': {
        'functions': ['OnRender', 'RenderGP4OMenu', 'UpdateGP4OUI', 'ProcessGP4OInput', 'RenderMainTab',
                     'RenderAimbotTab', 'RenderVisualsTab', 'RenderMiscTab'],
        'constants': [(r'\b< 0\b', '< OI(0)'), (r'\b= 0\b', '= OI(0)'), (r'\b== 0\b', '== OI(0)'),
                     (r'\b> 0\b', '> OI(0)'), (r'\b!= 0\b', '!= OI(0)')]
    },
}

INCLUDES = '''#include <base/antidisasm.h>
#include <base/obfuscate.h>
#include <base/obf_constants.h>
'''

def protect_file(filepath, config):
    """Apply protection to a single file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Skip if already protected
        if '#include <base/antidisasm.h>' in content:
            print(f"  Skipping {filepath} (already protected)")
            return True
        
        # Add includes after existing includes
        lines = content.split('\n')
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i
        
        if last_include_idx >= 0:
            new_includes = INCLUDES.strip().split('\n')
            for inc in reversed(new_includes):
                lines.insert(last_include_idx + 1, inc)
            # Add empty line
            if last_include_idx + len(new_includes) + 1 < len(lines):
                if lines[last_include_idx + len(new_includes) + 1].strip() != '':
                    lines.insert(last_include_idx + len(new_includes) + 1, '')
            content = '\n'.join(lines)
        
        # Add ANTI_DISASM_BLOCK() to functions
        for func_name in config['functions']:
            # Match function definitions with various return types
            patterns = [
                rf'((?:bool|void|int|float|vec2|const\s+\w+)\s+[\w:]+{func_name}\s*\([^)]*\)\s*(?:const)?\s*{{)\s*\n',
                rf'((?:bool|void|int|float|vec2)\s+[\w:]+:{func_name}\s*\([^)]*\)\s*{{)\s*\n',
            ]
            for pattern in patterns:
                replacement = r'\1\n    ANTI_DISASM_BLOCK();'
                content = re.sub(pattern, replacement, content, flags=re.MULTILINE)
        
        # Obfuscate constants
        for pattern, replacement in config['constants']:
            content = re.sub(pattern, replacement, content)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"  Protected {filepath}")
        return True
    
    except Exception as e:
        print(f"  ERROR protecting {filepath}: {e}")
        return False

def main():
    print("Batch Anti-Reverse Engineering Protection...")
    print("=" * 60)
    
    success_count = 0
    fail_count = 0
    
    for filepath, config in FILES_CONFIG.items():
        if os.path.exists(filepath):
            if protect_file(filepath, config):
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
    import sys
    sys.exit(main())
