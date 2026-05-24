import json
import os
import sys
from pathlib import Path


PLACEHOLDERS = {
    "{{KSU_VERSION}}": lambda: os.environ.get("KSU_VERSION", "unknown"),
    "{{KSU_GIT_TAG}}": lambda: os.environ.get("KSU_GIT_TAG", "no-tag"),
    "{{KSUN_BRANCH}}": lambda: os.environ.get("KSUN_BRANCH", "dev"),
    "{{KSUN_COMMIT}}": lambda: os.environ.get("KSUN_COMMIT", "unknown"),
    "{{KSU_MANAGER}}": lambda: os.environ.get("KSU_MANAGER", "Placeholder"),
}


def render_markdown(template_path: Path):
    text = template_path.read_text()

    commits_path = template_path.parent / "commits.json"
    commits = json.loads(commits_path.read_text()) if commits_path.exists() else {}

    for placeholder, getter in PLACEHOLDERS.items():
        text = text.replace(placeholder, getter())

    susfs_branches = []
    for branch, commit in commits.get("susfs", {}).items():
        susfs_branches.append(f"**{branch}**\n`{commit}`")

    if "{{SUSFS_BRANCHS}}" in text:
        text = text.replace("{{SUSFS_BRANCHS}}", "\n".join(susfs_branches))
    if "{{SUSFS_BRANCHES}}" in text:
        text = text.replace("{{SUSFS_BRANCHES}}", "\n".join(susfs_branches))

    print(text, end="")


config_path = Path(sys.argv[1])
if config_path.suffix.lower() == ".md":
    render_markdown(config_path)
    sys.exit(0)

# Backward-compatible JSON renderer for older release configs.

def emit(text=""):
    print(text)


def emit_list(items):
    if isinstance(items, list):
        for item in items:
            emit(f"- {item}")


def emit_description(value):
    if isinstance(value, list):
        for line in value:
            emit(line)
    elif value:
        emit(str(value))


data = json.loads(config_path.read_text())

commits_path = config_path.parent / "commits.json"
commits = json.loads(commits_path.read_text()) if commits_path.exists() else {}

emit("**IMPORTANT DISCLAIMER**")
for line in data["release"]["disclaimer"]:
    emit(line)

kernelsu = data.get("kernelsu", {})
emit()
emit(f"## {kernelsu.get('name', 'KernelSU-Next')}")
emit(f"- Version: {os.environ.get('KSU_VERSION', kernelsu.get('version', 'unknown'))}")
emit(f"- Tag: {os.environ.get('KSU_GIT_TAG', kernelsu.get('tag', 'no-tag'))}")
emit(f"- Branch: {os.environ.get('KSUN_BRANCH', kernelsu.get('branch', 'dev'))}")
emit(f"- Commit: {os.environ.get('KSUN_COMMIT', kernelsu.get('commit', 'unknown'))}")
if kernelsu.get("url"):
    emit(f"- URL: {kernelsu['url']}")
if kernelsu.get("manager"):
    emit(f"- Manager: {kernelsu['manager']}")

skip_keys = {"release", "kernelsu"}
for key in data.keys():
    if key in skip_keys:
        continue

    section = data[key]
    emit()
    emit(f"## {section.get('name', key)}")

    if section.get("description"):
        emit_description(section["description"])

    if section.get("version"):
        emit(f"- Version: {section['version']}")
    if section.get("tag"):
        emit(f"- Tag: {section['tag']}")
    if section.get("branch"):
        emit(f"- Branch: {section['branch']}")

    if key == "susfs" and "susfs" in commits:
        emit("- Branches:")
        for branch, commit in commits["susfs"].items():
            emit(f"**{branch}**")
            emit(f"`{commit}`")

    if section.get("items"):
        emit_list(section["items"])

    if section.get("url"):
        emit(f"- URL: {section['url']}")
