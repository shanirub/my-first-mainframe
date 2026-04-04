#!/usr/bin/env python3

import subprocess
import sys
import os
import anthropic

def get_changed_claude_files():
    result = subprocess.run(
        ["git", "diff", "HEAD~1", "HEAD", "--name-only"],
        capture_output=True, text=True
    )
    files = result.stdout.strip().split("\n")
    return [f for f in files if f.endswith("CLAUDE.md") and os.path.exists(f)]

def get_file_diff(filepath):
    result = subprocess.run(
        ["git", "diff", "HEAD~1", "HEAD", "--", filepath],
        capture_output=True, text=True
    )
    return result.stdout.strip()

def get_file_content(filepath):
    with open(filepath, "r") as f:
        return f.read()

def summarize_changes(client, filepath, diff, content):
    prompt = f"""You are helping maintain a Claude memory system for a hardware project.

A file called {filepath} was just updated in git.

Here is the diff:
{diff}

Here is the full current file content:
{content}

Extract 1-3 key facts that changed or were added. Each fact must:
- Be under 100 characters
- Be a single concrete statement (no vague summaries)
- Be useful context for Claude in a future conversation about this project

Reply with ONLY the facts, one per line. No preamble, no numbering, no explanation."""

    message = client.messages.create(
        model="claude-sonnet-4-20250514",
        max_tokens=300,
        messages=[{"role": "user", "content": prompt}]
    )
    return message.content[0].text.strip().split("\n")

def update_memory(client, facts):
    for fact in facts:
        fact = fact.strip()
        if not fact:
            continue
        client.beta.messages.create(
            model="claude-sonnet-4-20250514",
            max_tokens=100,
            messages=[{"role": "user", "content": fact}],
            system="You are a memory management assistant. Store this fact.",
            betas=["memory-2025-01-01"]
        )
        print(f"  → memorized: {fact}")

def main():
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        print("[memory-sync] ANTHROPIC_API_KEY not set — skipping")
        sys.exit(0)

    changed_files = get_changed_claude_files()
    if not changed_files:
        sys.exit(0)

    print(f"[memory-sync] CLAUDE.md changes detected: {changed_files}")
    client = anthropic.Anthropic(api_key=api_key)

    for filepath in changed_files:
        print(f"[memory-sync] processing {filepath}...")
        diff = get_file_diff(filepath)
        content = get_file_content(filepath)
        facts = summarize_changes(client, filepath, diff, content)
        update_memory(client, facts)

    print("[memory-sync] done")

if __name__ == "__main__":
    main()

