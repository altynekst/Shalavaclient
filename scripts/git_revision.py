import os
import subprocess

git_hash = os.environ.get("DDNET_GIT_SHORTREV_HASH")
if not git_hash:
	git_hash = "cd3187a83855d200ee9022c7a48827a9"
if git_hash is not None:
	definition = f'"{git_hash}"'
else:
	definition = "0"
print("#include <game/version.h>")
print(f"const char *GIT_SHORTREV_HASH = {definition};")
