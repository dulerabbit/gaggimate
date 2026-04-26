Import("env")
from SCons.Script import ARGUMENTS, SetOption

# Prevent intermittent Windows archive races where object files vanish before archiving.
ARGUMENTS["num_jobs"] = "1"
SetOption("num_jobs", 1)
env.SetOption("num_jobs", 1)
print("[pio] Forced num_jobs=1 for stable build/upload")
