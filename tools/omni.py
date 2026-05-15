import sys
import os

# Technodrome Color Palette
PURPLE = '\033[95m'
CYAN = '\033[96m'
GREEN = '\033[92m'
RED = '\033[91m'
BOLD = '\033[1m'
END = '\033[0m'

def show_tldr():
    print(f"""
    {PURPLE}{BOLD}# miuiser (Omni-verse / Northwood Dash){END}
    {CYAN}> Shredder-approved MIUI management for the D9 Syndicate.{END}

    {BOLD}[ BUS 16 : THE BEAUMONT RUN ]{END}
    {GREEN}omni --16{END}           - Purge old log debris and archive the hoard.
    
    {BOLD}[ BUS 33 : THE LONG HAUL ]{END}
    {GREEN}omni --33{END}           - Sniff the pipes for rogue MIUI 'Hurrs'.
                          (Auto-triggers Panic Protocol if detected).
    
    {BOLD}[ BUS 41 : ABBEY ST SPINE ]{END}
    {GREEN}omni --41{END}           - Re-ignite the Technodrome Core.

    {BOLD}[ PANIC PROTOCOL ]{END}
    {RED}omni --hurr{END}         - Shout "There's a Hurr in me Sewer!" & Force Reset.

    {BOLD}[ HARDWARE LINK ]{END}
    {CYAN}omni --flip{END}         - Access the TCP/Unix Socket Bridge (flip_switch).

    {BOLD}[ SENTRY STATUS ]{END}
    {PURPLE}omni --sentry{END}       - Check the Mouser's health in Northwood.
    """)

def main():
    if len(sys.argv) < 2:
        show_tldr()
        return

    cmd = sys.argv[1]

    if cmd in ["--help", "-h", "-tldr"]:
        show_tldr()
    elif cmd in ["--16", "--33", "--41", "--sentry"]:
        # Direct hand-off to the Mouser binary
        os.system(f"~/MiuiserPeruser/bin/mouser {cmd}")
    elif cmd == "--hurr":
        # The manual shout
        os.system("~/MiuiserPeruser/bin/mouser --panic")
    elif cmd == "--flip":
        if os.path.exists("flip_switch.py"):
            os.system("python3 flip_switch.py")
        else:
            print(f"{RED}Error: flip_switch.py not found. Sewer bridge is down!{END}")
    else:
        print(f"{RED}Route '{cmd}' not found in the D9 Lexicon. Try -tldr.{END}")

if __name__ == "__main__":
    main()
