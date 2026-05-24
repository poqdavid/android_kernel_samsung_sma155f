#!/usr/bin/env python3
"""
Fast Parallel Archive Download

This script implements the logic previously embedded inline in the action YAML.
It expects to be executed with the working directory set to the kernel workspace
where `manifest.xml` resides.
"""
import xml.etree.ElementTree as ET
import subprocess
import os
import shutil
from concurrent.futures import ThreadPoolExecutor
import traceback
import time
import sys


def try_download(url, name):
    aria_cmd = f"aria2c -x16 -s16 -k1M -j5 --file-allocation=none -o {name}.tar.gz '{url}'"
    print(f"  Trying download: {url}")
    result = subprocess.run(aria_cmd, shell=True)
    if result.returncode == 0:
        return True
    print(f"  Download failed, retrying in 10 seconds...")
    time.sleep(10)
    result = subprocess.run(aria_cmd, shell=True)
    return result.returncode == 0


def sync_project(task):
    name, path, url, strip, rev, linkfiles, copyfiles = task
    if path not in ["./", "."]:
        os.makedirs(path, exist_ok=True)
    print(f"Syncing: {name} -> {path}")
    print(f"  Download URL: {url}")
    try:
        downloaded = False
        # Only apply deprecated fallback for googlesource URLs
        if "googlesource.com" in url:
            downloaded = try_download(url, name)
            if not downloaded:
                if "+archive/" in url:
                    parts = url.split("+archive/")
                    branch = parts[1].split(".tar.gz")[0]
                    dep_url = f"{parts[0]}+archive/deprecated/{branch}.tar.gz"
                    print(f"  Main branch failed, trying deprecated branch: {dep_url}")
                    downloaded = try_download(dep_url, name)
        else:
            downloaded = try_download(url, name)

        if not downloaded:
            print(f"Failed to download {name} from all attempted URLs.")
            return False

        nproc = int(subprocess.check_output("nproc", shell=True).strip() or 1)
        tar_cmd = f"tar -I 'pigz -p {nproc} -b 256' -x --record-size=1M -C {path} {strip} -f {name}.tar.gz"
        subprocess.run(tar_cmd, shell=True, check=True)
        os.remove(f"{name}.tar.gz")

        top_dir = os.getcwd()
        for src_rel, dest_rel in linkfiles:
            src_path = os.path.join(top_dir, path, src_rel)
            dest_path = os.path.join(top_dir, dest_rel)
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            if os.path.lexists(dest_path):
                os.remove(dest_path)
            rel_target = os.path.relpath(src_path, os.path.dirname(dest_path))
            os.symlink(rel_target, dest_path)
            print(f"  [Link] {dest_rel} -> {src_rel}")
        for src_rel, dest_rel in copyfiles:
            src_path = os.path.join(top_dir, path, src_rel)
            dest_path = os.path.join(top_dir, dest_rel)
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            shutil.copy2(src_path, dest_path)
            print(f"  [Copy] {dest_rel} from {src_rel}")
        print(f"Synced {name} successfully!")
        return True
    except Exception as e:
        print(f"Failed to sync {name}: {e}")
        traceback.print_exc()
        return False


def main(manifest_path='manifest.xml'):
    if not os.path.exists(manifest_path):
        print(f"ERROR: manifest file not found: {manifest_path}")
        return 2

    with open(manifest_path, 'r') as f:
        manifest_content = f.read()
    root = ET.fromstring(manifest_content)

    remotes = {}
    for r in root.findall('remote'):
        fetch = (r.get('fetch') or '').rstrip('/')
        if fetch == '..':
            fetch = 'https://android.googlesource.com'
        remotes[r.get('name')] = fetch
    default = root.find('default')
    def_remote = default.get('remote') if default is not None else None
    def_rev = default.get('revision') if default is not None else None

    sync_tasks = []
    for project in root.findall('project'):
        name = project.get('name')
        path = project.get('path', name)
        remote_name = project.get('remote', def_remote)
        rev = project.get('revision', def_rev)
        base_url = remotes.get(remote_name)
        if not base_url:
            continue
        if "github.com" in base_url:
            url = f"{base_url}/{name}/archive/{rev}.tar.gz"
            strip = "--strip-components=1"
        elif "googlesource.com" in base_url:
            url = f"{base_url}/{name}/+archive/{rev}.tar.gz"
            strip = ""
        elif "git.codelinaro.org" in base_url:
            url = f"{base_url}/{name}/-/archive/{rev}.tar.gz"
            strip = "--strip-components=1"
        else:
            continue
        linkfiles = [(lf.get('src'), lf.get('dest')) for lf in project.findall('linkfile')]
        copyfiles = [(cf.get('src'), cf.get('dest')) for cf in project.findall('copyfile')]
        sync_tasks.append((name, path, url, strip, rev, linkfiles, copyfiles))

    print(f"Found {len(sync_tasks)} projects to sync.")

    max_workers = (os.cpu_count() or 2) * 4
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        results = list(executor.map(sync_project, sync_tasks))

    if not all(results):
        print("One or more projects failed to sync!")
        return 1
    return 0


if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
