#!/usr/bin/env python3
import os

shalawa_path = 'src/game/client/components/shalawa'
count = 0

for root, dirs, files in os.walk(shalawa_path):
    for file in files:
        if file.endswith(('.cpp', '.h')):
            filepath = os.path.join(root, file)
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                new_content = content.replace('OBF_F', 'OFR')
                
                if new_content != content:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    count += 1
                    print(f'Updated: {filepath}')
            except Exception as e:
                print(f'Error with {filepath}: {e}')

print(f'Total files updated: {count}')
