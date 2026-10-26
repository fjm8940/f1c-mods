# Sharing your F1 Challenge live telemetry — setup guide

This lets me watch your live race data (positions, laps, tires, damage, etc.) from
my own PC while you play, without opening any ports on your router to the public
internet. We use a private encrypted network (Tailscale) between just our two PCs.

Total one-time setup: about 5 minutes. After that, it's one double-click per race.

---

## Step 1 — Install Tailscale (one-time)

1. Go to https://tailscale.com/download and download the Windows version.
2. Install it, then sign in (Google/Microsoft/GitHub — whatever's easiest for you).
3. I'll send you an invite to join my "tailnet" (our shared private network) — accept
   it when it arrives in your email or in the Tailscale app.
4. Once connected, right-click the Tailscale icon in your system tray → "This device"
   (or open the app) and send me the IP address shown there — it looks like
   `100.x.x.x`. That's your private address, only reachable by devices on our
   tailnet, not the public internet.

You only do this once. It stays connected in the background after that (small tray
icon, no maintenance needed).

---

## Step 2 — Copy 2 files into your game folder

I'll send you a folder (or zip) containing:

- `web_telemetry.dll`
- `inject.exe`
- `Start_Telemetry.bat`

Copy all three into the **same folder as your `F1 Challenge.exe`**.

That's it for setup.

---

## Step 3 — Every time you want to share telemetry

1. Start F1 Challenge like normal and get into a session (practice, quali, race —
   doesn't matter which).
2. Double-click `Start_Telemetry.bat`.
3. A black window pops up, says "Injecting..." then "Done. Dashboard is now live at
   http://localhost:8080" — leave that window open (minimize it, don't close it).
4. Tell me you're live — I'll open `http://<your-tailscale-ip>:8080` on my end and
   see your race in real time.

If you want to watch it yourself too, open `http://localhost:8080` in your own
browser.

---

## Notes / what this actually does

- `web_telemetry.dll` runs a small local web server *inside* the game process and
  reads live race data (positions, lap times, tires, damage, weather) to display it
  as a dashboard. It's read-only for me — I'm just viewing a webpage, not touching
  your PC.
- It's only reachable over our private Tailscale network — not exposed to the
  internet. Nobody else can see it, and nothing is exposed on your router.
- Windows Defender/antivirus may flag `inject.exe` because DLL injection tools
  trigger heuristics; this is expected for unsigned tools. Add an
  exclusion for the folder if it gets quarantined, or tell me and I'll help.
- Closing the game stops the dashboard. Just re-run `Start_Telemetry.bat` next
  session (after the game is open).
- If port 8080 is already in use by something else on your PC, `Start_Telemetry.bat`
  will say so — tell me and I'll give you a version with a different port.

---

## Quick troubleshooting

| Problem | Fix |
|---|---|
| Script says game isn't running | Start F1 Challenge and load into a session first, then run the script |
| Antivirus deletes/blocks `inject.exe` | Restore it from quarantine, add an exclusion for the game folder |
| I can't reach your dashboard | Confirm Tailscale shows "Connected" in your tray, and double-check the IP you sent me |
| Page says "Connecting..." forever | Game is in a menu, not on track — start a session |
