#!/usr/bin/env python3
import re
import sys
import os

DANGEROUS_WORDS = [
    "aimbot", "aim", "autofire", "auto fire", "hook aim", "hammer",
    "pistol", "shotgun", "grenade", "laser", "balance bot", "hookride",
    "hook ride", "avoid", "blatant", "unfreeze", "watermark", "arraylist",
    "hitparticles", "hit particles", "chinahat", "china hat", "avoid visuals",
    "aim visuals", "tas", "hide cheat", "chatspam", "chat spam",
    "autoanswer", "auto answer", "fng", "wallhack", "esp",
    "silent", "ignore friends", "draw fov", "anti drag", "safe drag",
    "hook pump", "keep hook", "danger zones", "hitbox", "prediction",
    "escape path", "snapline", "ghost aura", "crystals",
    "automatic aiming", "automatic firing", "auto hook",
    "freeze avoidance", "auto unfreeze", "auto balance",
    "on-screen watermark", "module list overlay",
    "conical hat", "tool-assisted", "hide cheat indicators",
    "shalawa", "dummy_manager", "dummy manager",
    "hookride activated", "target found", "bot enabled",
]

SKIP_PATTERNS = [
    r'Localize\s*\(',
    r'#\s*include',
    r'#\s*define',
    r'#\s*pragma',
    r'MACRO_CONFIG',
    r'//.*$',
    r'AY_OBFUSCATE\s*\(',
    r'\bO\s*\(',
]

def should_skip_line(line):
    stripped = line.strip()
    for pat in SKIP_PATTERNS:
        if re.search(pat, stripped):
            return True
    return False

def is_dangerous(s):
    lower = s.lower()
    for word in DANGEROUS_WORDS:
        if word in lower:
            return True
    return False

def process_line(line):
    if should_skip_line(line):
        return line, 0

    count = 0
    result = []
    i = 0

    while i < len(line):
        if line[i] == '"':
            j = i + 1
            while j < len(line):
                if line[j] == '\\':
                    j += 2
                    continue
                if line[j] == '"':
                    break
                j += 1

            if j < len(line):
                string_content = line[i+1:j]

                before = line[:i].rstrip()
                already_wrapped = before.endswith('AY_OBFUSCATE(') or before.endswith('O(')

                if not already_wrapped and is_dangerous(string_content) and len(string_content) > 1:
                    result.append(line[len(''.join(result)):i])
                    result.append(f'O("{string_content}")')
                    i = j + 1
                    count += 1
                    continue

            result.append(line[len(''.join(result)):j+1])
            i = j + 1
        else:
            result.append(line[len(''.join(result)):i+1])
            i += 1

    return ''.join(result) if result else line, count

def process_file(filepath, dry_run=False):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    new_lines = []
    total = 0
    has_obfuscate_include = False

    for line in lines:
        if 'obfuscate.h' in line:
            has_obfuscate_include = True
        new_line, count = process_line(line)
        new_lines.append(new_line)
        total += count

    if total > 0 and not has_obfuscate_include:
        for i, line in enumerate(new_lines):
            if line.strip().startswith('#include'):
                new_lines.insert(i, '#include <base/obfuscate.h>\n')
                break

    if total > 0:
        print(f"  {filepath}: {total} strings wrapped")
        if not dry_run:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.writelines(new_lines)

    return total

def main():
    dry_run = '--dry-run' in sys.argv

    targets = [
        'src/game/client/components/shalawa/',
        'src/engine/client/imgui_manager.cpp',
        'src/engine/client/imgui_manage1.cpp',
        'src/engine/client/imgui_startmenu.cpp',
        'src/game/client/components/gp4o_menu.cpp',
    ]

    total = 0
    for target in targets:
        if os.path.isdir(target):
            for root, dirs, files in os.walk(target):
                for f in files:
                    if f.endswith(('.cpp', '.h')):
                        total += process_file(os.path.join(root, f), dry_run)
        elif os.path.isfile(target):
            total += process_file(target, dry_run)

    mode = "DRY RUN" if dry_run else "DONE"
    print(f"\n[{mode}] Total: {total} strings obfuscated")

if __name__ == '__main__':
    main()