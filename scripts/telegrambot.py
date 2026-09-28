#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Original rifsxd <https://github.com/KernelSU-Next/KernelSU-Next>
# Copyright (C) 2026 poqdavid <https://github.com/poqdavid/android_kernel_samsung_sma155f>
#
# This file is licensed under the GNU General Public License v3.0.
# See the top-level LICENSE file for details.

import asyncio
import os
import sys
import time
import requests

# Telethon sometimes can't reach Telegram from GitHub-hosted runners
# ("Server closed the connection: 0 bytes read on a total of 8 expected bytes").
# Retry the whole login + upload a few times before giving up.
TELETHON_ATTEMPTS = 3
TELETHON_BACKOFF_SECONDS = 30   # waits 30s, then 60s between attempts

# Bot API sendDocument can only upload files up to 50 MB (our .tar is ~64 MB).
BOT_API_MAX_UPLOAD = 50 * 1024 * 1024

# Environment Variables (set from the workflow or repository secrets)
API_ID = os.environ.get("API_ID")
API_HASH = os.environ.get("API_HASH")
BOT_TOKEN = os.environ.get("BOT_TOKEN")
CHAT_ID = os.environ.get("CHAT_ID")
MESSAGE_THREAD_ID = os.environ.get("MESSAGE_THREAD_ID")
COMMIT_URL = os.environ.get("COMMIT_URL")
COMMIT_MESSAGE = os.environ.get("COMMIT_MESSAGE")
RUN_URL = os.environ.get("RUN_URL")
TITLE = os.environ.get("TITLE")
VERSION = os.environ.get("VERSION")

MSG_TEMPLATE = """
**{title}**
{version}

```{commit_message}
```
[Release]({commit_url})
[Workflow run]({run_url})
""".strip()

def strip_gpg_signature(text: str) -> str:
    """Remove a PGP signature block from text if present."""
    if not text:
        return ""
    start_marker = "-----BEGIN PGP SIGNATURE-----"
    end_marker = "-----END PGP SIGNATURE-----"
    start = text.find(start_marker)
    if start == -1:
        return text.strip()
    end = text.find(end_marker, start)
    if end == -1:
        # No explicit end marker: drop from start marker onward
        return text[:start].strip()
    # remove through the end marker
    end += len(end_marker)
    # Remove the block and any immediately surrounding blank lines
    new_text = (text[:start] + text[end:]).strip()
    return new_text

def get_caption():
    """
    Build caption while respecting Telegram's 1024-character limit.
    Steps:
      - Strip PGP signature from the commit message.
      - Compute available characters for commit_message after substituting title/version/links.
      - Truncate only the commit_message (append "...") if needed.
      - If even the header+links exceed the limit, return COMMIT_URL as a fallback.
    """
    MAX_CAPTION = 1024

    title_val = TITLE or ""
    version_val = VERSION or ""
    commit_url_val = COMMIT_URL or ""
    run_url_val = RUN_URL or ""
    commit_raw = COMMIT_MESSAGE or ""

    # Strip any PGP signature block from the commit annotation
    commit_clean = strip_gpg_signature(commit_raw)

    # Build the base template with an empty commit_message to see how much space remains
    base_msg = MSG_TEMPLATE.format(
        title=title_val,
        version=version_val,
        commit_message="",
        commit_url=commit_url_val,
        run_url=run_url_val,
    )

    # Characters available for the commit_message
    allowed_for_commit = MAX_CAPTION - len(base_msg)
    if allowed_for_commit <= 0:
        # Header + links already exceed the caption limit; fall back to URL-only
        return commit_url_val or ""

    # If commit message fits, use it as-is
    if len(commit_clean) <= allowed_for_commit:
        final_commit = commit_clean
        print("[+] Commit message fits within caption limit.")
    else:
        print("[+] Commit message exceeds caption limit; truncating.")
        # Truncate commit message to allowed size minus space for ellipsis
        ellipsis = "..."
        truncated_len = max(0, allowed_for_commit - len(ellipsis))
        final_commit = commit_clean[:truncated_len].rstrip() + ellipsis

    msg = MSG_TEMPLATE.format(
        title=title_val,
        version=version_val,
        commit_message=final_commit,
        commit_url=commit_url_val,
        run_url=run_url_val,
    )

    # Final safety: if still somehow > MAX_CAPTION, fall back to the commit URL
    if len(msg) > MAX_CAPTION:
        print("[+] Final message exceeds caption limit even after truncation; using commit URL only.")
        return commit_url_val or ""

    return msg


def get_plain_text(limit):
    """
    Plain-text version of the caption for the Bot API (no parse mode, so
    characters like < > * _ in the tag message can't break entity parsing).
    Only the commit message is truncated to fit `limit` characters.
    """
    header = "\n".join(p for p in (TITLE or "", VERSION or "") if p)
    footer = "\n".join(p for p in (
        f"Release: {COMMIT_URL}" if COMMIT_URL else "",
        f"Workflow run: {RUN_URL}" if RUN_URL else "",
    ) if p)
    body = strip_gpg_signature(COMMIT_MESSAGE or "")

    allowed = limit - len(header) - len(footer) - 4  # 4 = two blank-line separators
    if allowed <= 0:
        body = ""
    elif len(body) > allowed:
        body = body[:max(0, allowed - 3)].rstrip() + "..."

    return "\n\n".join(p for p in (header, body, footer) if p)[:limit]


def check_environ():
    global CHAT_ID, MESSAGE_THREAD_ID, COMMIT_MESSAGE, RUN_URL, TITLE, VERSION
    if BOT_TOKEN is None:
        print("[-] Invalid BOT_TOKEN")
        exit(1)
    if CHAT_ID is None:
        print("[-] Invalid CHAT_ID")
        exit(1)
    else:
        try:
            CHAT_ID = int(CHAT_ID)
        except:
            pass
    if COMMIT_URL is None:
        print("[-] Invalid COMMIT_URL")
    if COMMIT_MESSAGE is None:
        COMMIT_MESSAGE = ""
    if RUN_URL is None:
        RUN_URL = ""
    if TITLE is None:
        TITLE = ""
    if VERSION is None:
        VERSION = ""
    if MESSAGE_THREAD_ID is None or MESSAGE_THREAD_ID == "":
        MESSAGE_THREAD_ID = None
    else:
        try:
            MESSAGE_THREAD_ID = int(MESSAGE_THREAD_ID)
        except:
            MESSAGE_THREAD_ID = None

def use_telethon():
    """Return True only when both API_ID and API_HASH are set and non-empty (not just "")."""
    return (
        API_ID is not None and str(API_ID).strip() != ""
        and API_HASH is not None and str(API_HASH).strip() != ""
    )


async def send_via_telethon(files):
    """Send files using Telethon (requires API_ID and API_HASH)"""
    from telethon import TelegramClient

    print("[+] Using Telethon (API_ID + API_HASH method)")

    # Validate API_ID -> int, fallback to Bot API if invalid
    try:
        api_id_int = int(str(API_ID).strip())
    except Exception:
        print("[-] Invalid API_ID; must be an integer. Falling back to Bot API.")
        # run the blocking bot API sender in a thread to avoid blocking the event loop
        await asyncio.to_thread(send_via_bot_api, files)
        return

    script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    session_dir = os.path.join(script_dir, "releasebot")

    async with await TelegramClient(
        session=session_dir,
        api_id=api_id_int,
        api_hash=API_HASH,
        connection_retries=5,   # reconnect attempts per connection
        retry_delay=5,          # seconds between reconnect attempts
        timeout=20,             # seconds per network operation
        request_retries=10,     # retries per API request (e.g. upload parts)
        auto_reconnect=True,
    ).start(bot_token=BOT_TOKEN) as bot:
        caption = [""] * len(files)
        caption[-1] = get_caption()

        print("[+] Caption: ")
        print("---")
        print(caption)
        print("---")
        print("[+] Sending files via Telethon...")

        await bot.send_file(
            entity=CHAT_ID,
            file=files,
            caption=caption,
            reply_to=MESSAGE_THREAD_ID,
            parse_mode="markdown"
        )
        print("[+] Done!")


def send_via_bot_api(files):
    """Send files using Telegram Bot API (requires only BOT_TOKEN)"""
    print("[+] Using Telegram Bot API (BOT_TOKEN only method)")
    
    api_url = f"https://api.telegram.org/bot{BOT_TOKEN}"
    caption = get_plain_text(1024)  # Bot API caption limit

    for file_path in files:
        if not os.path.exists(file_path):
            print(f"[-] File not found: {file_path}")
            continue
        
        file_size = os.path.getsize(file_path)
        file_name = os.path.basename(file_path)
        
        print(f"[+] Uploading: {file_name} ({file_size / (1024*1024):.2f} MB)")
        
        with open(file_path, 'rb') as f:
            files_dict = {'document': f}
            data = {
                'chat_id': CHAT_ID,
                'caption': caption,
            }

            if MESSAGE_THREAD_ID is not None:
                data['message_thread_id'] = MESSAGE_THREAD_ID
            
            response = requests.post(
                f"{api_url}/sendDocument",
                data=data,
                files=files_dict,
                timeout=300
            )
            
            if response.status_code == 200:
                print(f"[+] Successfully uploaded: {file_name}")
            else:
                print(f"[-] Failed to upload {file_name}: {response.text}")
                exit(1)
    
    print("[+] Done!")


def send_link_via_bot_api():
    """Last resort: post the release text + link (no file) via the Bot API."""
    print("[+] Posting release link via Telegram Bot API (no file attached)")

    data = {
        'chat_id': CHAT_ID,
        'text': get_plain_text(4096),  # Bot API message limit
        'disable_web_page_preview': True,
    }
    if MESSAGE_THREAD_ID is not None:
        data['message_thread_id'] = MESSAGE_THREAD_ID

    response = requests.post(
        f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage",
        data=data,
        timeout=60
    )
    if response.status_code != 200:
        print(f"[-] Failed to post release link: {response.text}")
        exit(1)
    print("[+] Release link posted.")


def send_with_retries(files):
    """Try Telethon a few times; if Telegram keeps dropping the connection,
    fall back to the Bot API (file upload if small enough, else link only)."""
    for attempt in range(1, TELETHON_ATTEMPTS + 1):
        try:
            asyncio.run(send_via_telethon(files))
            return
        except Exception as e:
            print(f"[-] Telethon attempt {attempt}/{TELETHON_ATTEMPTS} failed: {e!r}")
            if attempt < TELETHON_ATTEMPTS:
                wait = TELETHON_BACKOFF_SECONDS * attempt
                print(f"[+] Retrying in {wait}s...")
                time.sleep(wait)

    print("[-] Telethon failed on every attempt; falling back to the Bot API.")
    existing = [f for f in files if os.path.exists(f)]
    if existing and all(os.path.getsize(f) <= BOT_API_MAX_UPLOAD for f in existing):
        send_via_bot_api(existing)
    else:
        send_link_via_bot_api()
        # Visible on the workflow run summary without failing the job:
        # the release is published and the channel got the link.
        print("::warning title=Telegram::File could not be uploaded via Telethon; "
              "posted the release link instead.")


def main():
    print("[+] Uploading to telegram")
    check_environ()
    
    files = sys.argv[1:]
    print("[+] Files:", files)
    
    if len(files) <= 0:
        print("[-] No files to upload")
        exit(1)
    
    # Determine which method to use
    if use_telethon():
        print("[+] Telethon method detected (API_ID + API_HASH present)")
        try:
            from telethon import TelegramClient
        except ImportError:
            print("[-] telethon not installed. Install with: pip install telethon")
            exit(1)
        send_with_retries(files)
    else:
        print("[+] Bot API method detected (using BOT_TOKEN only)")
        try:
            import requests
        except ImportError:
            print("[-] requests not installed. Install with: pip install requests")
            exit(1)
        send_via_bot_api(files)


if __name__ == "__main__":
    # Line-buffer stdout so our messages and Telethon's log lines show up in
    # the right order in the GitHub Actions log.
    sys.stdout.reconfigure(line_buffering=True)
    try:
        main()
    except Exception as e:
        print(f"[-] An error occurred: {e}")
        exit(1)